---
date: 2026-05-26 20:00
type: add
files:
  - tools/audit-project.ps1
session: bring-up placa nova
---

# Script de auditoria do projeto — rotina pra achar erros comuns sem compilar

## Motivo
Após sessão caótica (boot loop, hard reset, várias inconsistências do HEAD), usuário pediu rotina automatizada pra varrer erros sem precisar compilar. Útil pra rodar antes de cada commit ou quando algo der errado.

## O que faz
Script PowerShell que executa 7 checagens:

1. **Estado da PSRAM** — lê sdkconfig pra saber se está ligada
2. **Allocs em PSRAM** — se PSRAM desligada, qualquer `MALLOC_CAP_SPIRAM` vira CRITICAL
3. **Refs GPIO antigas** — busca padrões "GPIO 3", "GPIO 4", "GPIO 10" em comentários (eram REC, PWR, NAND CS no pinout antigo)
4. **TODOs/FIXMEs** — lista marcadores de trabalho pendente
5. **Arquivos órfãos** — `.c` no diretório do componente mas não no `CMakeLists.txt`
6. **ESP_ERROR_CHECK** — conta usos (cada um é ponto de abort no boot)
7. **Git status** — lembrete de quantos arquivos não commitados antes de ações destrutivas

## Como usar
```powershell
# Roda e mostra resumo no terminal
.\tools\audit-project.ps1

# Roda e salva relatório completo em Markdown
.\tools\audit-project.ps1 -OutFile audit.md

# Modo silencioso (só exit code; 1 se tiver criticals)
.\tools\audit-project.ps1 -Quiet
```

## Severidades
- `CRITICAL` — vai quebrar runtime/build
- `WARNING` — pode causar bug ou confusão
- `INFO` — apenas informativo

Exit code 1 se tiver pelo menos 1 CRITICAL.

## Antes
N/A (arquivo novo).

## Depois
Script completo em `tools/audit-project.ps1`, ~200 linhas.

## Resultado (primeira execução em 2026-05-26 20:00)
- **CRITICAL: 3** (MALLOC_CAP_SPIRAM em asset_loader, dialog_loader, ui_debug)
- **WARNING: 5** (4 stale GPIO + 11 arquivos não commitados)
- **INFO: 7** (TODOs, untracked, 1 ESP_ERROR_CHECK)

## Limitações
- Não compila — não pega erros de sintaxe/tipo/missing-includes
- Heurística de GPIO é simples — pode dar falso positivo se "3", "4", "10" aparecerem em outro contexto
- Não cobre todos os erros possíveis (ver `tools/audit-project.ps1` no topo pra lista exata)

## Links
- Memória: [[skill_changelog_system]]
- Entrada relacionada: [[2026-05-26T1830-disable-psram]] (PSRAM desligada que motivou os CRITICAL de SPIRAM_ALLOC)
