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

if __name__ == "__main__":
    print("Starting packaging process...")
    main()
    print("Packaging completed.")
