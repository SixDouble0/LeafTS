@echo off
setlocal

set "ROOT=%~dp0"
set "RESC=%ROOT%renode\leafts_uart_demo.resc"

if not exist "%RESC%" (
    echo [ERROR] File not found:
    echo         %RESC%
    pause
    exit /b 1
)

where renode >nul 2>&1
if %errorlevel%==0 (
    echo [INFO] Starting Renode from PATH...
    renode "%RESC%"
    exit /b %errorlevel%
)

set "RENODE_EXE=C:\Program Files\Renode\renode.exe"
if exist "%RENODE_EXE%" (
    echo [INFO] Starting Renode: %RENODE_EXE%
    "%RENODE_EXE%" "%RESC%"
    exit /b %errorlevel%
)

set "RENODE_EXE=C:\Program Files (x86)\Renode\renode.exe"
if exist "%RENODE_EXE%" (
    echo [INFO] Starting Renode: %RENODE_EXE%
    "%RENODE_EXE%" "%RESC%"
    exit /b %errorlevel%
)

echo [ERROR] Renode not found.
echo.
echo Add "renode" to PATH or install it to one of the following locations:
echo   C:\Program Files\Renode\renode.exe
echo   C:\Program Files ^(x86^)\Renode\renode.exe
echo.
echo Then run this script again.
