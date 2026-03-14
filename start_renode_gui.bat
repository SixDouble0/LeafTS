@echo off
setlocal

set "ROOT=%~dp0"
set "RESC=%ROOT%renode\leafts_gui_demo.resc"

if not exist "%RESC%" (
    echo [ERROR] File not found:
    echo         %RESC%
    pause
    exit /b 1
)

where renode >nul 2>&1
if %errorlevel%==0 (
    echo [INFO] Starting Renode from PATH for GUI mode...
    renode "%RESC%"
    exit /b %errorlevel%
)

set "RENODE_EXE=C:\Program Files\Renode\renode.exe"
if exist "%RENODE_EXE%" (
    echo [INFO] Starting Renode: %RENODE_EXE% for GUI mode...
    "%RENODE_EXE%" "%RESC%"
    exit /b %errorlevel%
)

set "RENODE_EXE=C:\Program Files (x86)\Renode\renode.exe"
if exist "%RENODE_EXE%" (
    echo [INFO] Starting Renode: %RENODE_EXE% for GUI mode...
    "%RENODE_EXE%" "%RESC%"
    exit /b %errorlevel%
)

echo [ERROR] Renode not found.
pause
exit /b 1
