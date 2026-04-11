![LeafTS Banner](LeafTsBanner-Reversed-removedbg.png)

## What is LeafTS?
A lightweight, high-performance Time-Series Database (TSDB) and command-line engine designed specifically for microcontrollers and bare-metal embedded systems.
LeafTS allows you to store, query, and analyze time-stamped telemetry data (like sensor readings) directly on the edge device. It features an interactive, SQL-like querying system accessible via UART, making data extraction and debugging incredibly simple. 

Whether you are building environmental monitors, industrial trackers, or IoT nodes, LeafTS can handle the flash storage and make your life easier.

## Features
- **No OS Required**: 100% written in standard C, zero dependencies on external OS/RTOS.
- **Cross-Platform HAL**: Ready-to-use plug-and-play ports for **STM32 (F1-H7, G0-G4, L1-L5, WB/WL), ESP32, nRF52,** and **RP2040**.
- **SQL-like UART Interface**: Query your device directly with human-readable commands (e.g., `select ts(...,...) limit 10 avg`).
- **Flexible Storage Engine**: Store continuous numeric telemetry (`float`) or short string flags (e.g., up to 4 characters).
- **Companion Desktop GUI**: Includes a graphical tool to connect via serial port (COM) or TCP emulator, visualize the database, and generate ready-to-compile C projects.
- **Automated Granular Releases**: Download pre-packaged, minimalist `.zip` libraries precisely matching your microcontroller architecture without any unnecessary files.

---

## Installation

The easiest way to add LeafTS to your project is via the **Releases** tab on GitHub!

1. Go to the **Releases** page.
2. Find the zip file corresponding to your exact microcontroller (e.g., `LeafTS-STM32L4.zip` or the generic `LeafTS-Core-Library.zip`).
3. Extract it into your project folder.
4. Add the extracted `src/` directory to your build system (Makefile, CMake, or an IDE like STM32CubeIDE).
5. Add the `include/` directory to your compiler's include paths.

---

## Usage & Configuration

### 1. Initializing the Database in C
It's incredibly simple to set up in your embedded `main.c`. Include the header and initialize the database with your hardware's specific Flash and UART drivers:

```c
#include "leafts.h"
#include "uart_handler.h"
#include "hal_flash.h"

int main(void) {
    // 1. Initialize your hardware setup (Clocks, UART, Flash)
    // hal_uart_init();
    // hal_flash_init();
    
    // 2. Initialize LeafTS Database Engine
    leafts_init();
    
    while(1) {
        // 3. Process incoming UART database commands
        uart_handler_process();
    }
}
```

### 2. Using the Desktop GUI
To interact with the database, visualize data, and test queries, use the built-in GUI:

```bash
# Install Python dependencies
pip install PyQt5

# Run the GUI
python tools/leafts_gui.py
```
*Tip: You can attach to a physical board via COM ports, or test your C logic safely on your PC using the generic **TCP (Emulator)** target!*

### 3. Basic UART Commands
Connect via a Serial Terminal (like PuTTY or the built-in GUI) and type:
- `append 42.5` - Saves a float value with an automatically generated timestamp.
- `insert K` - Saves a short text flag/string to the database.
- `list` or `select *` - Prints out all recorded data.
- `status` - Displays flash memory usage and total record count.
- `help` - To display all avalibe commands.


---

## Developing & Testing Locally

LeafTS includes a robust testing environment so you can develop logic without flashing the hardware:
- **Build the native TCP emulator (Windows/Linux)**: 
  ```bash
  mkdir build && cd build
  cmake ..
  cmake --build .
  ```
- **Run Renode Simulation**: The project contains `.resc` scripts to simulate STM32 targets completely in software using [Renode](https://renode.io/) which I used to test every family of microcontrolers.

## License
This project is licensed under the MIT License.
