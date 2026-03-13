# -*- mode: python ; coding: utf-8 -*-
#
# PyInstaller spec for LeafTS Playground
# Run:  pyinstaller tools/leafts_gui.spec
# Output: dist/LeafTS_Playground.exe  (single portable file)

import sys
from pathlib import Path
import glob

ROOT = Path(SPECPATH).parent          # project root (one level above tools/)

# ----------------------------------------------------------------------------
# Data files bundled into the exe
# All paths:  (source_on_disk, destination_inside_bundle)
# ----------------------------------------------------------------------------
datas = [
    # boards database
    (str(ROOT / "boards" / "boards.json"),    "boards"),

    # backend server
    (str(ROOT / "build"  / "leafts_uart.exe"), "."),

    # library headers & core sources (always copied by generator)
    (str(ROOT / "include" / "leafts.h"),           "include"),
    (str(ROOT / "include" / "uart_handler.h"),     "include"),
    (str(ROOT / "hal" / "hal_flash.h"),            "hal"),
    (str(ROOT / "hal" / "hal_uart.h"),             "hal"),
    (str(ROOT / "src" / "leafts.c"),               "src"),
    (str(ROOT / "src" / "uart_handler.c"),         "src"),
]

# Family HAL files used by the project generator.
# Keep subfolders in bundle (hal/stm32, src/stm32, ...), otherwise GUI reports
# "HAL not implemented" even when files exist in the repository.
for rel in (
    "hal/**/*.h",
    "src/**/*_flash.c",
):
    for file_path in glob.glob(str(ROOT / rel), recursive=True):
        p = Path(file_path)
        if not p.is_file():
            continue
        bundle_dir = str(p.relative_to(ROOT).parent).replace("\\", "/")
        datas.append((str(p), bundle_dir))

a = Analysis(
    [str(ROOT / "tools" / "leafts_gui.py")],
    pathex=[str(ROOT / "tools")],
    binaries=[],
    datas=datas,
    hiddenimports=[
        "serial",
        "serial.tools",
        "serial.tools.list_ports",
        "serial.tools.list_ports_windows",
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
)

pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="LeafTS_Playground",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)
