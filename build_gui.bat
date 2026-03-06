@echo off
:: ============================================================
:: build_gui.bat — buduje LeafTS_Playground.exe
:: Uruchom z katalogu glownego projektu:   build_gui.bat
:: ============================================================

echo.
echo ===================================================
echo  LeafTS Playground — budowanie .exe
echo ===================================================
echo.

:: 1. Upewnij sie ze leafts_uart.exe jest zbudowany
if not exist "build\leafts_uart.exe" (
    echo [INFO] Budowanie leafts_uart.exe...
    cmake -S . -B build -G "MinGW Makefiles" 2>nul
    if errorlevel 1 (
        cmake -S . -B build 2>nul
    )
    cmake --build build --target leafts_uart
    if errorlevel 1 (
        echo [ERROR] Nie udalo sie zbudowac leafts_uart.exe
        echo         Zbuduj recznie: cmake --build build --target leafts_uart
        pause
        exit /b 1
    )
)
echo [OK] leafts_uart.exe gotowy

:: 2. Zainstaluj wymagane pakiety Python jesli brak
python -c "import PyQt6" 2>nul
if errorlevel 1 (
    echo [INFO] Instalowanie PyQt6...
    pip install PyQt6
)
python -c "import serial" 2>nul
if errorlevel 1 (
    echo [INFO] Instalowanie pyserial...
    pip install pyserial
)
python -c "import PyInstaller" 2>nul
if errorlevel 1 (
    echo [INFO] Instalowanie PyInstaller...
    pip install pyinstaller
)

:: 3. Buduj exe
echo.
echo [INFO] Budowanie LeafTS_Playground.exe ...
echo        (moze zajac 1-2 minuty)
echo.
python -m PyInstaller --clean --workpath pyinstaller_build --distpath dist tools\leafts_gui.spec

if errorlevel 1 (
    echo.
    echo [ERROR] Budowanie nie powiodlo sie. Sprawdz bledy powyzej.
    pause
    exit /b 1
)

echo.
echo ===================================================
echo  SUKCES!
echo  Plik: dist\LeafTS_Playground.exe  (~36 MB, dziala bez instalacji Pythona)
echo  Skopiuj go gdziekolwiek — nie wymaga instalacji.
echo ===================================================
echo.
pause
