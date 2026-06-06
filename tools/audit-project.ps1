# audit-project.ps1
# Rotina de auditoria do projeto CyberGame - busca erros comuns sem precisar compilar.
# Uso: .\tools\audit-project.ps1
#      .\tools\audit-project.ps1 -OutFile audit-report.md   (salva em arquivo Markdown)
#
# O que checa:
#   1. Estado da PSRAM (sdkconfig)
#   2. Allocs em PSRAM quando PSRAM esta desligada
#   3. Stale GPIO references em comentarios
#   4. TODOs/FIXMEs/HACKs no codigo
#   5. Source files orfaos (no folder mas nao no CMakeLists.txt)
#   6. ESP_ERROR_CHECK count (cada um pode abortar o boot)
#   7. Git status (lembrete pre-reset)
#
# Severidade:
#   CRITICAL - vai quebrar em runtime ou impedir build
#   WARNING  - pode causar bug ou confusao
#   INFO     - apenas informativo, nao requer acao

[CmdletBinding()]
param(
    [string]$OutFile = "",
    [switch]$Quiet
)

$ErrorActionPreference = 'Continue'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

$Findings = New-Object System.Collections.ArrayList

function Add-Finding {
    param([string]$Severity, [string]$Category, [string]$Message, [string]$File = "", [int]$Line = 0)
    $null = $Findings.Add([PSCustomObject]@{
        Severity = $Severity
        Category = $Category
        Message  = $Message
        File     = $File
        Line     = $Line
    })
}

function Write-Section {
    param([string]$Title)
    if (-not $Quiet) {
        Write-Host ""
        Write-Host "=== $Title ===" -ForegroundColor Cyan
    }
}

function Get-ProjectFiles {
    param([string[]]$Extensions)
    $files = @()
    foreach ($ext in $Extensions) {
        $files += Get-ChildItem -Path "components" -Recurse -Filter "*.$ext" -ErrorAction SilentlyContinue |
                  Where-Object { $_.FullName -notmatch '\\managed_components\\' }
        $files += Get-ChildItem -Path "main" -Recurse -Filter "*.$ext" -ErrorAction SilentlyContinue
    }
    return $files
}

# === Check 1: PSRAM status ===
Write-Section "1. Estado da PSRAM"
$PSRAMEnabled = $true
if (Test-Path "sdkconfig") {
    $sdkconfigContent = Get-Content "sdkconfig" -Raw
    if ($sdkconfigContent -match "(?m)^#\s*CONFIG_SPIRAM\s+is\s+not\s+set") {
        $PSRAMEnabled = $false
        if (-not $Quiet) { Write-Host "  PSRAM DESLIGADA no sdkconfig (modo bring-up)" -ForegroundColor Yellow }
    } else {
        if (-not $Quiet) { Write-Host "  PSRAM LIGADA no sdkconfig" -ForegroundColor Green }
    }
}

# === Check 2: MALLOC_CAP_SPIRAM (com deteccao de fallback) ===
Write-Section "2. Allocs em PSRAM"
$allCSources = Get-ProjectFiles -Extensions @("c")
$spiramAllocs = $allCSources | Select-String -Pattern "MALLOC_CAP_SPIRAM"
foreach ($match in $spiramAllocs) {
    $relPath = $match.Path.Substring($ProjectRoot.Length + 1)
    $line = $match.Line

    # Distingue allocs (perigoso sem PSRAM) de reads (heap stats, OK)
    $isAlloc = $line -match 'heap_caps_(malloc|calloc|realloc|aligned_alloc)\s*\('
    $isRead  = $line -match '_get_(free|total|minimum_free|largest_free_block)_size\s*\('

    if ($isRead) {
        # Read de stats — sem PSRAM retorna 0, nao crasha
        Add-Finding -Severity "INFO" -Category "SPIRAM_ALLOC" `
                    -Message "Read de stats PSRAM (retorna 0 sem PSRAM, nao crasha)" `
                    -File $relPath -Line $match.LineNumber
        continue
    }

    if (-not $isAlloc) {
        # Outra mencao (comentario, define, etc) — info
        Add-Finding -Severity "INFO" -Category "SPIRAM_ALLOC" `
                    -Message "Mencao a PSRAM (nao eh alloc)" `
                    -File $relPath -Line $match.LineNumber
        continue
    }

    # Eh alloc. Procura fallback nas proximas 5 linhas (mesma file).
    $fileLines = Get-Content $match.Path
    $startIdx = $match.LineNumber  # 1-based; queremos as proximas
    $endIdx = [Math]::Min($fileLines.Length, $startIdx + 5)
    $hasFallback = $false
    for ($i = $startIdx; $i -lt $endIdx; $i++) {
        if ($fileLines[$i] -match 'MALLOC_CAP_(8BIT|DEFAULT|INTERNAL)') {
            $hasFallback = $true
            break
        }
    }

    if ($hasFallback) {
        Add-Finding -Severity "INFO" -Category "SPIRAM_ALLOC" `
                    -Message "Alloc em PSRAM com fallback pra DRAM (OK)" `
                    -File $relPath -Line $match.LineNumber
    } else {
        if ($PSRAMEnabled) {
            Add-Finding -Severity "INFO" -Category "SPIRAM_ALLOC" `
                        -Message "Alloc em PSRAM sem fallback (OK enquanto PSRAM ligada)" `
                        -File $relPath -Line $match.LineNumber
        } else {
            Add-Finding -Severity "CRITICAL" -Category "SPIRAM_ALLOC" `
                        -Message "Alloc em PSRAM SEM FALLBACK e PSRAM DESLIGADA - vai retornar NULL" `
                        -File $relPath -Line $match.LineNumber
        }
    }
}

# === Check 3: Stale GPIO references ===
Write-Section "3. Referencias GPIO antigas em comentarios"
$allSources = Get-ProjectFiles -Extensions @("c", "h")
$gpioPatterns = @{
    'GPIO\s?3\b'  = 'GPIO 3 mencionado (era REC/START antigo; agora REC=13)'
    'GPIO\s?4\b'  = 'GPIO 4 mencionado (era PWR antigo; agora PWR=14)'
    'GPIO10\b|CS=GPIO10|GPIO\s10\b' = 'GPIO 10 mencionado (era NAND CS antigo; agora CS=41)'
}
foreach ($pat in $gpioPatterns.Keys) {
    $hits = $allSources | Select-String -Pattern $pat
    foreach ($h in $hits) {
        # Pula linhas que sao defines reais
        if ($h.Line -match '^\s*#define') { continue }
        # Pula linhas que ja contem o novo pinout (ex: define _A GPIO_NUM_3 nao se aplica)
        $relPath = $h.Path.Substring($ProjectRoot.Length + 1)
        Add-Finding -Severity "WARNING" -Category "STALE_GPIO" -Message $gpioPatterns[$pat] -File $relPath -Line $h.LineNumber
    }
}

# === Check 4: TODOs ===
Write-Section "4. TODOs/FIXMEs/HACKs"
$todoHits = $allSources | Select-String -Pattern "TODO|FIXME|XXX|HACK"
foreach ($h in $todoHits) {
    $trimmed = $h.Line.Trim()
    # Ignora TODOS (portugues plural)
    if ($trimmed -match '\bTODOS\b') { continue }
    $relPath = $h.Path.Substring($ProjectRoot.Length + 1)
    Add-Finding -Severity "INFO" -Category "TODO" -Message $trimmed -File $relPath -Line $h.LineNumber
}

# === Check 5: Source files orfaos ===
Write-Section "5. Arquivos .c orfaos (nao listados em CMakeLists.txt)"
$components = Get-ChildItem -Path "components" -Directory
foreach ($comp in $components) {
    $cmakeFile = Join-Path $comp.FullName "CMakeLists.txt"
    if (-not (Test-Path $cmakeFile)) { continue }

    $cmakeContent = Get-Content $cmakeFile -Raw
    $cFiles = Get-ChildItem -Path $comp.FullName -Recurse -Filter "*.c" -ErrorAction SilentlyContinue

    foreach ($cf in $cFiles) {
        $fileName = $cf.Name
        if ($cmakeContent -notmatch [regex]::Escape($fileName)) {
            $relPath = $cf.FullName.Substring($ProjectRoot.Length + 1)
            Add-Finding -Severity "WARNING" -Category "ORPHAN_SRC" `
                        -Message "Arquivo $($cf.Name) existe em components/$($comp.Name)/ mas nao esta no CMakeLists.txt" `
                        -File $relPath -Line 0
        }
    }
}

# === Check 6: ESP_ERROR_CHECK count ===
Write-Section "6. ESP_ERROR_CHECK"
$ecHits = $allCSources | Select-String -Pattern "ESP_ERROR_CHECK\("
$ecCount = ($ecHits | Measure-Object).Count
Add-Finding -Severity "INFO" -Category "ESP_ERROR_CHECK" `
            -Message "Encontrados $ecCount usos de ESP_ERROR_CHECK (cada um aborta o boot em falha)" -File "" -Line 0

# === Check 7: Git status ===
Write-Section "7. Estado do Git"
$gitStatus = git status --short 2>&1 | Out-String
$modCount = ([regex]::Matches($gitStatus, '(?m)^\s?M ')).Count
$untrackedCount = ([regex]::Matches($gitStatus, '(?m)^\?\?')).Count
$deletedCount = ([regex]::Matches($gitStatus, '(?m)^\s?D ')).Count
if ($modCount -gt 0) {
    Add-Finding -Severity "WARNING" -Category "GIT_UNCOMMITTED" `
                -Message "$modCount arquivos modificados nao commitados (proteger com commit/stash antes de reset)" -File "" -Line 0
}
if ($deletedCount -gt 0) {
    Add-Finding -Severity "WARNING" -Category "GIT_DELETED" `
                -Message "$deletedCount arquivos deletados pendentes" -File "" -Line 0
}
if ($untrackedCount -gt 0) {
    Add-Finding -Severity "INFO" -Category "GIT_UNTRACKED" `
                -Message "$untrackedCount arquivos untracked (git reset --hard NAO toca nesses)" -File "" -Line 0
}

# === RELATORIO ===
Write-Section "RELATORIO FINAL"

$summary = $Findings | Group-Object Severity | ForEach-Object {
    "$($_.Name): $($_.Count)"
}

$report = New-Object System.Collections.ArrayList
$null = $report.Add("# Auditoria do Projeto CyberGame")
$null = $report.Add("")
$null = $report.Add("**Data:** $(Get-Date -Format 'yyyy-MM-dd HH:mm')")
$psramStatus = if ($PSRAMEnabled) { "LIGADA" } else { "DESLIGADA (modo bring-up)" }
$null = $report.Add("**PSRAM:** $psramStatus")
$null = $report.Add("")
$null = $report.Add("## Sumario")
$null = $report.Add("")
foreach ($s in $summary) { $null = $report.Add("- $s") }
$null = $report.Add("")

foreach ($cat in ($Findings | Group-Object Category | Sort-Object Name)) {
    $null = $report.Add("## $($cat.Name) ($($cat.Count))")
    $null = $report.Add("")
    foreach ($f in $cat.Group) {
        $loc = ""
        if ($f.File) {
            if ($f.Line -gt 0) { $loc = "$($f.File):$($f.Line)" } else { $loc = $f.File }
        }
        $line = "- **[$($f.Severity)]** $($f.Message)"
        if ($loc) { $line += "  ``$loc``" }
        $null = $report.Add($line)
    }
    $null = $report.Add("")
}

if ($OutFile) {
    ($report -join "`r`n") | Out-File -FilePath $OutFile -Encoding utf8
    Write-Host ""
    Write-Host "Relatorio salvo em: $OutFile" -ForegroundColor Green
}

if (-not $Quiet) {
    Write-Host ""
    foreach ($s in $summary) {
        $color = "Gray"
        if ($s -like "CRITICAL*") { $color = "Red" }
        elseif ($s -like "WARNING*") { $color = "Yellow" }
        Write-Host "  $s" -ForegroundColor $color
    }
    Write-Host ""
    Write-Host "Para detalhes salve em arquivo: .\tools\audit-project.ps1 -OutFile audit.md" -ForegroundColor DarkGray
}

$criticals = ($Findings | Where-Object { $_.Severity -eq "CRITICAL" }).Count
if ($criticals -gt 0) { exit 1 } else { exit 0 }
