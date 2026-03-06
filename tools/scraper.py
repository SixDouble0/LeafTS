#!/usr/bin/env python3
"""
LeafTS Board Database Scraper
------------------------------
Fetches board definitions from PlatformIO GitHub and generates boards/boards.json.
Run once: python tools/scraper.py
Output: boards/boards.json  (commit it with the rest of the code)

Supported platforms:
  STM32:  F1, F2, F3, F4, F7, H7, G0, G4, L1, L4, L5, WB, WL
  ESP32:  ESP32, S2, S3, C2, C3, C6, H2
  RP2040: Raspberry Pi Pico and clones
  nRF52:  Nordic nRF52832, nRF52840

FAMILY_DATA contains sector layout data from the Reference Manuals.
PlatformIO does not store sector sizes - they have to be hardcoded here.
"""

import json
import re
import sys
import time
import urllib.request
import urllib.error
from pathlib import Path

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

STSTM32_API   = "https://api.github.com/repos/platformio/platform-ststm32/contents/boards"
ESP32_API     = "https://api.github.com/repos/platformio/platform-espressif32/contents/boards"
RASPI_API     = "https://api.github.com/repos/maxgerhardt/platform-raspberrypi/contents/boards"
NRF52_API     = "https://api.github.com/repos/platformio/platform-nordicnrf52/contents/boards"
OUT_FILE      = Path(__file__).parent.parent / "boards" / "boards.json"

# Delay between requests to avoid hammering the GitHub API
REQUEST_DELAY = 0.05   # seconds

# Boards with less flash than this are skipped - too small for LeafTS
MIN_FLASH_KB = 64

# ---------------------------------------------------------------------------
# Hardcoded sector sizes per MCU family (from ST/Nordic/Espressif Reference Manuals)
# Flash base addresses are also taken from the RM.
# ---------------------------------------------------------------------------

# Key: MCU name prefix (lowercase). Longer prefixes take priority over shorter ones.
FAMILY_DATA = {
    # --- STM32F1xx: uniform 1 KB pages (F103 low/medium density) ---
    "stm32f103": {
        "family":       "STM32F1",
        "flash_base":   0x08000000,
        # F103 has uniform pages: 1 KB (low/medium density) or 2 KB (high density)
        # page_size is the minimum erase unit
        "page_size":    1024,
        "sector_sizes": None,   # None = uniform pages, use page_size + sector_count
        "hal_include":  "hal/hal_stm32f1_flash.h",
    },

    # --- STM32F401 ---
    "stm32f401": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        # sectors 0-3: 16 KB, sector 4: 64 KB, sectors 5-7: 128 KB
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },

    # --- STM32F405 / F407 / F415 / F417 ---
    "stm32f405": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072,
                         131072, 131072, 131072, 131072],  # up to 1 MB
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },
    "stm32f407": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072,
                         131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },
    "stm32f415": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072,
                         131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },
    "stm32f417": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072,
                         131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },

    # --- STM32F410 (256 KB max) ---
    "stm32f410": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },

    # --- STM32F411 (512 KB) ---
    "stm32f411": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },

    # --- STM32F412 (1 MB) ---
    "stm32f412": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },

    # --- STM32F413 / F423 (1.5 MB) ---
    "stm32f413": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072,
                         131072, 131072, 131072, 131072, 131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },

    # --- STM32F427 / F429 / F437 / F439 (2 MB, dual bank) ---
    "stm32f427": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536,
                         131072, 131072, 131072, 131072, 131072, 131072, 131072,
                         16384, 16384, 16384, 16384, 65536,
                         131072, 131072, 131072, 131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },
    "stm32f429": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536,
                         131072, 131072, 131072, 131072, 131072, 131072, 131072,
                         16384, 16384, 16384, 16384, 65536,
                         131072, 131072, 131072, 131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },

    # --- STM32F446 (512 KB) ---
    "stm32f446": {
        "family":       "STM32F4",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f4_flash.h",
    },

    # --- STM32F2xx: same sector layout as F4 ---
    "stm32f205": {
        "family":       "STM32F2",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072,
                         131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f2_flash.h",
    },
    "stm32f207": {
        "family":       "STM32F2",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072,
                         131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f2_flash.h",
    },
    "stm32f215": {
        "family":       "STM32F2",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072,
                         131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f2_flash.h",
    },
    "stm32f217": {
        "family":       "STM32F2",
        "flash_base":   0x08000000,
        "sector_sizes": [16384, 16384, 16384, 16384, 65536, 131072, 131072, 131072,
                         131072, 131072, 131072, 131072],
        "hal_include":  "hal/hal_stm32f2_flash.h",
    },

    # --- STM32F3xx: uniform 2 KB pages ---
    "stm32f301": {
        "family":       "STM32F3",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32f3_flash.h",
    },
    "stm32f302": {
        "family":       "STM32F3",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32f3_flash.h",
    },
    "stm32f303": {
        "family":       "STM32F3",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32f3_flash.h",
    },
    "stm32f334": {
        "family":       "STM32F3",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32f3_flash.h",
    },
    "stm32f373": {
        "family":       "STM32F3",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32f3_flash.h",
    },

    # --- STM32F7xx: similar layout to H7 (mixed 32/128/256 KB sectors) ---
    "stm32f745": {
        "family":       "STM32F7",
        "flash_base":   0x08000000,
        "sector_sizes": [32768, 32768, 32768, 32768, 131072, 262144, 262144, 262144],
        "hal_include":  "hal/hal_stm32f7_flash.h",
    },
    "stm32f746": {
        "family":       "STM32F7",
        "flash_base":   0x08000000,
        "sector_sizes": [32768, 32768, 32768, 32768, 131072, 262144, 262144, 262144],
        "hal_include":  "hal/hal_stm32f7_flash.h",
    },
    "stm32f750": {
        "family":       "STM32F7",
        "flash_base":   0x08000000,
        "sector_sizes": [32768, 32768, 32768, 32768, 131072, 262144, 262144, 262144],
        "hal_include":  "hal/hal_stm32f7_flash.h",
    },
    "stm32f756": {
        "family":       "STM32F7",
        "flash_base":   0x08000000,
        "sector_sizes": [32768, 32768, 32768, 32768, 131072, 262144, 262144, 262144],
        "hal_include":  "hal/hal_stm32f7_flash.h",
    },
    "stm32f767": {
        "family":       "STM32F7",
        "flash_base":   0x08000000,
        "sector_sizes": [32768, 32768, 32768, 32768, 131072,
                         262144, 262144, 262144, 262144, 262144, 262144, 262144],
        "hal_include":  "hal/hal_stm32f7_flash.h",
    },
    "stm32f769": {
        "family":       "STM32F7",
        "flash_base":   0x08000000,
        "sector_sizes": [32768, 32768, 32768, 32768, 131072,
                         262144, 262144, 262144, 262144, 262144, 262144, 262144],
        "hal_include":  "hal/hal_stm32f7_flash.h",
    },

    # --- STM32H7xx: 128 KB sectors (H743/H753 are dual bank) ---
    "stm32h743": {
        "family":       "STM32H7",
        "flash_base":   0x08000000,
        "sector_sizes": [131072] * 16,   # 2 MB: bank1 8x128 KB + bank2 8x128 KB
        "hal_include":  "hal/hal_stm32h7_flash.h",
    },
    "stm32h753": {
        "family":       "STM32H7",
        "flash_base":   0x08000000,
        "sector_sizes": [131072] * 16,
        "hal_include":  "hal/hal_stm32h7_flash.h",
    },
    "stm32h750": {
        "family":       "STM32H7",
        "flash_base":   0x08000000,
        "sector_sizes": [131072],        # 128 KB, single sector
        "hal_include":  "hal/hal_stm32h7_flash.h",
    },
    "stm32h7a3": {
        "family":       "STM32H7",
        "flash_base":   0x08000000,
        "sector_sizes": [131072] * 16,
        "hal_include":  "hal/hal_stm32h7_flash.h",
    },
    "stm32h7b0": {
        "family":       "STM32H7",
        "flash_base":   0x08000000,
        "sector_sizes": [131072] * 8,
        "hal_include":  "hal/hal_stm32h7_flash.h",
    },
    "stm32h7b3": {
        "family":       "STM32H7",
        "flash_base":   0x08000000,
        "sector_sizes": [131072] * 16,
        "hal_include":  "hal/hal_stm32h7_flash.h",
    },

    # --- STM32G0xx: uniform 2 KB pages ---
    "stm32g031": {
        "family":       "STM32G0",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g0_flash.h",
    },
    "stm32g041": {
        "family":       "STM32G0",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g0_flash.h",
    },
    "stm32g071": {
        "family":       "STM32G0",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g0_flash.h",
    },
    "stm32g081": {
        "family":       "STM32G0",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g0_flash.h",
    },
    "stm32g0b1": {
        "family":       "STM32G0",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g0_flash.h",
    },
    "stm32g0c1": {
        "family":       "STM32G0",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g0_flash.h",
    },

    # --- STM32G4xx: uniform 2 KB pages ---
    "stm32g431": {
        "family":       "STM32G4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g4_flash.h",
    },
    "stm32g441": {
        "family":       "STM32G4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g4_flash.h",
    },
    "stm32g471": {
        "family":       "STM32G4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g4_flash.h",
    },
    "stm32g473": {
        "family":       "STM32G4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g4_flash.h",
    },
    "stm32g474": {
        "family":       "STM32G4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g4_flash.h",
    },
    "stm32g491": {
        "family":       "STM32G4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g4_flash.h",
    },
    "stm32g4a1": {
        "family":       "STM32G4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32g4_flash.h",
    },

    # --- STM32L1xx: very small 256 B pages ---
    "stm32l151": {
        "family":       "STM32L1",
        "flash_base":   0x08000000,
        "page_size":    256,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l1_flash.h",
    },
    "stm32l152": {
        "family":       "STM32L1",
        "flash_base":   0x08000000,
        "page_size":    256,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l1_flash.h",
    },
    "stm32l162": {
        "family":       "STM32L1",
        "flash_base":   0x08000000,
        "page_size":    256,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l1_flash.h",
    },

    # --- STM32L4xx: uniform 2 KB pages (additional entries) ---
    "stm32l471": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },
    "stm32l475": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },
    "stm32l486": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },

    # --- STM32L5xx: uniform 4 KB pages ---
    "stm32l552": {
        "family":       "STM32L5",
        "flash_base":   0x08000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l5_flash.h",
    },
    "stm32l562": {
        "family":       "STM32L5",
        "flash_base":   0x08000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l5_flash.h",
    },

    # --- STM32WBxx: uniform 4 KB pages ---
    "stm32wb55": {
        "family":       "STM32WB",
        "flash_base":   0x08000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32wb_flash.h",
    },
    "stm32wb35": {
        "family":       "STM32WB",
        "flash_base":   0x08000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32wb_flash.h",
    },

    # --- STM32WLxx: uniform 2 KB pages ---
    "stm32wl55": {
        "family":       "STM32WL",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32wl_flash.h",
    },
    "stm32wle5": {
        "family":       "STM32WL",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32wl_flash.h",
    },

    # --- STM32L4xx: uniform 2 KB pages ---
    "stm32l412": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },
    "stm32l432": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },
    "stm32l433": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },
    "stm32l452": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },
    "stm32l476": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },
    "stm32l496": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    2048,
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },
    "stm32l4r5": {
        "family":       "STM32L4",
        "flash_base":   0x08000000,
        "page_size":    4096,   # L4+ series has 4 KB pages
        "sector_sizes": None,
        "hal_include":  "hal/hal_stm32l4_flash.h",
    },

    # --- ESP32: external SPI flash, 4 KB sectors ---
    "esp32": {
        "family":       "ESP32",
        "flash_base":   0x00000000,   # virtual address, mapped by MMU
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_esp32_flash.h",
    },
    "esp32s2": {
        "family":       "ESP32",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_esp32_flash.h",
    },
    "esp32s3": {
        "family":       "ESP32",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_esp32_flash.h",
    },
    "esp32c2": {
        "family":       "ESP32",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_esp32_flash.h",
    },
    "esp32c3": {
        "family":       "ESP32",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_esp32_flash.h",
    },
    "esp32c6": {
        "family":       "ESP32",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_esp32_flash.h",
    },
    "esp32h2": {
        "family":       "ESP32",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_esp32_flash.h",
    },

    # --- RP2040: external QSPI flash, 4 KB sectors, XIP base 0x10000000 ---
    "rp2040": {
        "family":       "RP2040",
        "flash_base":   0x10000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_rp2040_flash.h",
    },

    # --- RP2350: same external QSPI flash topology as RP2040 ---
    "rp2350": {
        "family":       "RP2350",
        "flash_base":   0x10000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_rp2040_flash.h",  # same HAL as RP2040
    },

    # --- Nordic nRF52: internal flash, 4 KB pages ---
    "nrf52840": {
        "family":       "nRF52",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_nrf52_flash.h",
    },
    "nrf52832": {
        "family":       "nRF52",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_nrf52_flash.h",
    },
    "nrf52833": {
        "family":       "nRF52",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_nrf52_flash.h",
    },
    "nrf5340": {
        "family":       "nRF53",
        "flash_base":   0x00000000,
        "page_size":    4096,
        "sector_sizes": None,
        "hal_include":  "hal/hal_nrf53_flash.h",
    },
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def fetch_json(url: str) -> dict | list:
    """Fetch JSON from URL. Handles GitHub rate-limit (403/429) with a 60s retry."""
    req = urllib.request.Request(url, headers={"User-Agent": "LeafTS-scraper/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        if e.code in (403, 429):
            # Rate limit hit - wait 60s and retry
            print(f"  [rate-limit] waiting 60s...", flush=True)
            time.sleep(60)
            with urllib.request.urlopen(req, timeout=15) as resp:
                return json.loads(resp.read().decode())
        raise


def match_family(mcu: str) -> str | None:
    """Return the FAMILY_DATA key that matches the given MCU string, or None."""
    mcu_low = mcu.lower().replace("-", "").replace("_", "")
    # Sort by key length descending so more specific prefixes match first
    for key in sorted(FAMILY_DATA.keys(), key=len, reverse=True):
        if mcu_low.startswith(key):
            return key
    return None


def parse_flash_size(raw: str | int) -> int:
    """Convert '512KB', '1MB', 524288, etc. to bytes."""
    if isinstance(raw, int):
        return raw
    raw = str(raw).strip().upper().replace(" ", "")
    m = re.match(r"^(\d+(?:\.\d+)?)(K|KB|M|MB)?$", raw)
    if not m:
        return 0
    val = float(m.group(1))
    unit = m.group(2) or ""
    if unit in ("K", "KB"):
        return int(val * 1024)
    if unit in ("M", "MB"):
        return int(val * 1024 * 1024)
    return int(val)


def build_board_entry(board_id: str, raw: dict) -> dict | None:
    """
    Convert a raw PlatformIO board JSON into a boards.json record.
    Returns None if the board is not supported.
    """
    build   = raw.get("build", {})
    upload  = raw.get("upload", {})
    mcu     = build.get("mcu", "").lower()
    name    = raw.get("name", board_id)
    vendor  = raw.get("vendor", "")

    if not mcu:
        return None

    family_key = match_family(mcu)
    if family_key is None:
        return None

    fdata = FAMILY_DATA[family_key]

    # Flash size: PlatformIO stores it as int (bytes) in upload.maximum_size
    # or as a string like "4MB" in upload.flash_size (ESP32 boards)
    flash_size = 0
    if "maximum_size" in upload:
        flash_size = int(upload["maximum_size"])
    elif "flash_size" in upload:
        flash_size = parse_flash_size(upload["flash_size"])

    if flash_size == 0:
        return None
    if flash_size < MIN_FLASH_KB * 1024:
        return None

    sector_sizes = fdata.get("sector_sizes")
    page_size    = fdata.get("page_size")

    if sector_sizes is not None:
        # Non-uniform sectors (STM32F4) - trim the list to actual flash size
        cumulative = 0
        trimmed = []
        for s in sector_sizes:
            if cumulative >= flash_size:
                break
            trimmed.append(s)
            cumulative += s
        sector_sizes = trimmed
        result_sectors = {"sector_sizes": sector_sizes}
    else:
        # Uniform pages (ESP32, STM32L4, STM32F1) - store compactly
        # No need to expand into a huge array - just page_size + sector_count
        ps = page_size if page_size else 4096
        result_sectors = {
            "page_size":    ps,
            "sector_count": flash_size // ps,
        }

    entry = {
        "id":         board_id,
        "name":       name,
        "vendor":     vendor,
        "mcu":        mcu,
        "family":     fdata["family"],
        "flash_base": fdata["flash_base"],
        "flash_size": flash_size,
        "hal_include": fdata["hal_include"],
    }
    entry.update(result_sectors)
    return entry


# ---------------------------------------------------------------------------
# Scraper
# ---------------------------------------------------------------------------

def scrape_platform(api_url: str, label: str) -> list[dict]:
    """Fetch all boards from one PlatformIO platform repository."""
    print(f"\n=== {label} ===")
    print(f"Fetching board list...", flush=True)
    files = fetch_json(api_url)

    boards = []
    total  = len(files)

    for idx, f in enumerate(files, 1):
        fname = f["name"]
        if not fname.endswith(".json"):
            continue

        board_id    = fname[:-5]
        download_url = f["download_url"]

        print(f"  [{idx:3d}/{total}] {board_id:<50s}", end="", flush=True)

        try:
            raw   = fetch_json(download_url)
            entry = build_board_entry(board_id, raw)
            if entry:
                boards.append(entry)
                print(f"  -> {entry['family']} | {entry['flash_size']//1024} KB")
            else:
                print(f"  (skipped)")
        except Exception as exc:
            print(f"  ERROR: {exc}")

        time.sleep(REQUEST_DELAY)

    print(f"\n{label}: {len(boards)} boards added.")
    return boards


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("LeafTS Board Scraper")
    print("=" * 50)

    all_boards: list[dict] = []

    # STM32
    stm_boards = scrape_platform(STSTM32_API, "platform-ststm32")
    all_boards.extend(stm_boards)

    # ESP32
    esp_boards = scrape_platform(ESP32_API, "platform-espressif32")
    all_boards.extend(esp_boards)

    # Raspberry Pi Pico (RP2040)
    raspi_boards = scrape_platform(RASPI_API, "platform-raspberrypi")
    all_boards.extend(raspi_boards)

    # Nordic nRF52
    nrf_boards = scrape_platform(NRF52_API, "platform-nordicnrf52")
    all_boards.extend(nrf_boards)

    # Deduplicate by board id, just in case
    seen = set()
    deduped = []
    for b in all_boards:
        if b["id"] not in seen:
            seen.add(b["id"])
            deduped.append(b)

    # Sort by family first, then by board id
    deduped.sort(key=lambda b: (b["family"], b["id"]))

    # Write output
    OUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT_FILE, "w", encoding="utf-8") as fp:
        json.dump({"boards": deduped}, fp, indent=2, ensure_ascii=False)

    print(f"\n✓ Saved {len(deduped)} boards to: {OUT_FILE}")
    print(f"\nFamily summary:")
    from collections import Counter
    counts = Counter(b["family"] for b in deduped)
    for fam, cnt in sorted(counts.items()):
        print(f"  {fam:<12s}: {cnt:4d} boards")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nCancelled by user.")
        sys.exit(1)
