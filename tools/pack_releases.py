import os
import zipfile
import glob
import shutil

DIST_DIR = "dist_releases"

CORE_FILES = [
    "include/leafts.h",
    "include/platform_hal.h",
    "include/uart_handler.h",
    "src/leafts.c",
    "src/platform_hal.c",
    "src/uart_handler.c",
    "src/hal_vflash.c",
    "src/hal_vuart.c",
    "hal/hal_flash.h",
    "hal/hal_uart.h",
    "hal/hal_vflash.h",
    "hal/hal_vuart.h",
]

STUDIO_FILES = [
    "tools/leafts_gui.py",
    "tools/boards.json",
    "tools/renode_serial_bridge.py",
    "tools/build_gui.bat",
    "tools/start_renode.bat",
    "tools/start_renode_gui.bat",
    "pyinstaller_build/leafts_gui.exe",
    "pyinstaller_build/leafts_gui_linux",
]

# Add all .py files in tools/
for f in glob.glob("tools/*.py"):
    if f not in STUDIO_FILES:
        STUDIO_FILES.append(f)

# Add all .resc files in tools/
for f in glob.glob("tools/*.resc"):
    STUDIO_FILES.append(f)

# Add all .bat files in tools/
for f in glob.glob("tools/*.bat"):
    if f not in STUDIO_FILES:
        STUDIO_FILES.append(f)

# Add all .sh files in tools/
for f in glob.glob("tools/*.sh"):
    STUDIO_FILES.append(f)

def ensure_dist_dir():
    if os.path.exists(DIST_DIR):
        shutil.rmtree(DIST_DIR)
    os.makedirs(DIST_DIR)

def create_zip(zip_name, files):
    zip_path = os.path.join(DIST_DIR, zip_name)
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for f in files:
            if os.path.exists(f):
                zipf.write(f)
            else:
                print(f"Warning: {f} not found!")
    print(f"Created {zip_name} with {len(files)} files.")

def get_boards():
    boards = []
    # Match patterns like hal/stm32/hal_stm32l4_flash.h
    for flash_h in glob.glob("hal/*/hal_*_flash.h"):
        # e.g., hal\stm32\hal_stm32l4_flash.h
        parts = flash_h.replace("\\", "/").split('/')
        group = parts[1] # stm32
        filename = parts[2] # hal_stm32l4_flash.h
        board = filename.replace("hal_", "").replace("_flash.h", "")
        boards.append((group, board))
    return boards

def main():
    ensure_dist_dir()
    
    # 1. Create Core Library Zip
    create_zip("LeafTS-Core-Library.zip", CORE_FILES)
    
    # 2. Create Board Zips
    boards = get_boards()
    for group, board in boards:
        board_files = CORE_FILES.copy()
        
        # Board specific files
        board_files.append(f"hal/{group}/hal_{board}_flash.h")
        board_files.append(f"hal/{group}/hal_{board}_uart.h")
        board_files.append(f"src/{group}/hal_{board}_flash.c")
        board_files.append(f"src/{group}/hal_{board}_uart.c")
        
        create_zip(f"LeafTS-{board.upper()}.zip", board_files)

    # Studio package
    create_zip("LeafTS-Studio.zip", STUDIO_FILES)

if __name__ == "__main__":
    print("Starting packaging process...")
    main()
    print("Packaging completed.")
