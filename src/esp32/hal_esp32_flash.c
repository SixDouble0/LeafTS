// ESP32 Flash HAL for LeafTS
// Reference: ESP-IDF esp_flash.h, ESP32 Technical Reference Manual §11
//
// ESP32 flash is accessed through the SPI0/SPI1 bus via the ESP-IDF
// esp_flash API (which handles cache disabling / re-enabling automatically).
//
// When ESP_PLATFORM is defined (ESP-IDF build):
//   - Uses esp_flash_read / esp_flash_write / esp_flash_erase_region.
//   - flash_base is a byte offset within the default flash chip.
// When ESP_PLATFORM is NOT defined (host compile-check):
//   - Stubs return -1 so the file compiles on any host toolchain.
//
// Sector size (erase granularity): 4096 bytes.
// Page size  (write granularity) : 256 bytes.
// Erased state: 0xFF.

#include <string.h>
#include <stdint.h>
#include "../../hal/esp32/hal_esp32_flash.h"

#define SECTOR_SIZE    4096u
#define PAGE_SIZE      256u

static uint32_t g_base;
static uint32_t g_size;

#ifdef ESP_PLATFORM
// ---------------------------------------------------------------------------
// Real ESP32 implementation using ESP-IDF esp_flash API
// ---------------------------------------------------------------------------
#include "esp_flash.h"

static int esp_read(uint32_t address, uint8_t *buffer, size_t size)
{
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    return (esp_flash_read(NULL, buffer, address, (uint32_t)size) == ESP_OK) ? 0 : -1;
}

static int esp_write(uint32_t address, const uint8_t *buffer, size_t size)
{
    if (!buffer || address < g_base || address + size > g_base + g_size) return -1;
    return (esp_flash_write(NULL, buffer, address, (uint32_t)size) == ESP_OK) ? 0 : -1;
}

static int esp_erase(uint32_t sector_address)
{
    if (sector_address < g_base ||
        sector_address + SECTOR_SIZE > g_base + g_size ||
        sector_address % SECTOR_SIZE != 0) return -1;
    return (esp_flash_erase_region(NULL, sector_address, SECTOR_SIZE) == ESP_OK) ? 0 : -1;
}

#else
// ---------------------------------------------------------------------------
// Host stub — compile-check only; not for actual MCU use
// ---------------------------------------------------------------------------
static int esp_read(uint32_t a, uint8_t *b, size_t s)
{
    (void)a; (void)b; (void)s; return -1;
}
static int esp_write(uint32_t a, const uint8_t *b, size_t s)
{
    (void)a; (void)b; (void)s; return -1;
}
static int esp_erase(uint32_t a)
{
    (void)a; return -1;
}
#endif  // ESP_PLATFORM

int esp32_flash_init(hal_flash_t *flash, uint32_t flash_base, uint32_t total_size)
{
    if (!flash || total_size == 0 || total_size % SECTOR_SIZE != 0) return -1;
    g_base = flash_base; g_size = total_size;
    flash->read  = esp_read;
    flash->write = esp_write;
    flash->erase = esp_erase;
    flash->total_size  = total_size;
    flash->sector_size = SECTOR_SIZE;
    flash->page_size   = PAGE_SIZE;
    return 0;
}
