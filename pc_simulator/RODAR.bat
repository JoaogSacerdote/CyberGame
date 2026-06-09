@echo off
cd /d "%~dp0\bin"

if not exist cybersim.exe (
    echo ERRO: cybersim.exe nao encontrado.
    echo Execute COMPILAR.bat primeiro.
    pause
    exit /b 1
)

echo Iniciando CyberSim...
cybersim.exe
pause
