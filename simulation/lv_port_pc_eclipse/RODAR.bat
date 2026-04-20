@echo off
cd /d "%~dp0"

echo ================================================
echo  CyberSec: Network Defender
echo ================================================

if not exist "bin\main.exe" (
    echo ERRO: bin\main.exe nao encontrado.
    echo Execute COMPILAR.bat primeiro.
    pause
    exit /b 1
)

if not exist "bin\SDL2.dll" (
    echo Copiando DLLs necessarios...
    copy "C:\msys64\mingw64\bin\SDL2.dll"           "bin\" >nul 2>&1
    copy "C:\msys64\mingw64\bin\libgcc_s_seh-1.dll" "bin\" >nul 2>&1
    copy "C:\msys64\mingw64\bin\libstdc++-6.dll"    "bin\" >nul 2>&1
    if errorlevel 1 (
        echo ERRO: DLLs nao encontradas em C:\msys64\mingw64\bin\
        echo Instale o MSYS2 conforme o readme.md
        pause
        exit /b 1
    )
)

echo Iniciando o jogo...
set PATH=C:\msys64\mingw64\bin;%PATH%
start "" /D "%~dp0bin" "%~dp0bin\main.exe"
