@echo off
cd /d "%~dp0"

echo ================================================
echo  CyberSim — Compilar (simulador completo)
echo ================================================

set MINGW=C:\msys64\mingw64\bin
if not exist "%MINGW%\cmake.exe" (
    echo ERRO: CMake nao encontrado em %MINGW%
    echo Instale o MSYS2 e adicione mingw64\bin ao PATH.
    pause
    exit /b 1
)

set PATH=%MINGW%;%PATH%

echo [1/2] Configurando CMake...
cmake -B build -S . -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -Wno-dev
if errorlevel 1 (
    echo ERRO na configuracao do CMake.
    pause
    exit /b 1
)

echo [2/2] Compilando...
cmake --build build --parallel 4
if errorlevel 1 (
    echo ERRO na compilacao.
    pause
    exit /b 1
)

echo.
echo Compilacao OK. Execute RODAR.bat para iniciar.
pause
