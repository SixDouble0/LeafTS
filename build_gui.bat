@echo off
:: ============================================================
:: build_gui.bat — builds LeafTS_Studio.exe
:: Run from the project root directory:   build_gui.bat
:: ============================================================

echo.
echo ===================================================
echo  LeafTS Studio — building .exe
echo ===================================================
echo.

:: 1. Always refresh leafts_uart.exe (avoid stale backend in package)
echo [INFO] Building/refreshing leafts_uart.exe...
if not exist "build" mkdir build
cmake --preset virtual 2>nul
if errorlevel 1 (
    cmake -S . -B build -G "MinGW Makefiles" 2>nul
    if errorlevel 1 (
        cmake -S . -B build 2>nul
    )
)
if exist "out\build\virtual\leafts_uart.exe" (
    cmake --build out\build\virtual --target leafts_uart
    if errorlevel 1 (
        echo [ERROR] Failed to build leafts_uart.exe (preset virtual)
        pause
        exit /b 1
    )
    copy /Y "out\build\virtual\leafts_uart.exe" "build\leafts_uart.exe" >nul
) else (
    cmake --build build --target leafts_uart
    if errorlevel 1 (
        echo [ERROR] Failed to build leafts_uart.exe
        echo         Build manually: cmake --build build --target leafts_uart
        pause
        exit /b 1
    )
)
echo [OK] leafts_uart.exe ready

:: 2. Install required Python packages if missing
py -c "import PyQt6" 2>nul
if errorlevel 1 (
    echo [INFO] Installing PyQt6...
    pip install PyQt6
)
py -c "import serial" 2>nul
if errorlevel 1 (
    echo [INFO] Installing pyserial...
    pip install pyserial
)
py -c "import PyInstaller" 2>nul
if errorlevel 1 (
    echo [INFO] Installing PyInstaller...
    pip install pyinstaller
)

:: 3. Build exe
echo.
echo [INFO] Building LeafTS_Studio.exe ...
echo        (this may take 1-2 minutes)
echo.
py -m PyInstaller --clean --workpath pyinstaller_build --distpath dist tools\leafts_gui.spec

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed. Check errors above.
    pause
    exit /b 1
)

echo.
echo ===================================================
echo  SUCCESS!
echo  File: dist\LeafTS_Studio.exe  (~36 MB, no Python required)
echo  Copy it anywhere — no installation needed.
echo ===================================================
echo.
pause
