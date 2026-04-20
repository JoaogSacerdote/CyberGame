@echo off
cd /d "%~dp0"

echo ================================================
echo  CyberSec: Network Defender - Compilar
echo ================================================

set MINGW=C:\msys64\mingw64\bin
if not exist "%MINGW%\cmake.exe" (
    echo ERRO: CMake nao encontrado em %MINGW%
    echo Instale o MSYS2 conforme o readme.md
    pause
    exit /b 1
)

set PATH=%MINGW%;%PATH%

echo [1/3] Inicializando submodule LVGL...
git submodule update --init --recursive
if errorlevel 1 (
    echo ERRO ao inicializar submodule.
    pause
    exit /b 1
)

echo [2/3] Copiando arquivos do jogo...
copy /Y "..\main\src\cybersec_game.h" "." >nul
copy /Y "..\main\src\cybersec_game.c" "." >nul

echo [3/3] Compilando...
cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -Wno-dev >nul
if errorlevel 1 (
    echo ERRO na configuracao do CMake.
    pause
    exit /b 1
)

cmake --build build --parallel 4
if errorlevel 1 (
    echo ERRO na compilacao.
    pause
    exit /b 1
)

echo.
echo Compilacao concluida com sucesso!
echo Execute RODAR.bat para iniciar o jogo.
pause
