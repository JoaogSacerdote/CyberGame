@echo off
cd /d "%~dp0"

if not exist "bin\cybersim.exe" (
    echo ERRO: bin\cybersim.exe nao encontrado.
    echo Execute COMPILAR.bat primeiro.
    pause
    exit /b 1
)

set PATH=C:\msys64\mingw64\bin;%PATH%
start "" /D "%~dp0bin" "%~dp0bin\cybersim.exe"
