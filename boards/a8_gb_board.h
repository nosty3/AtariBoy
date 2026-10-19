#pragma once
// Purple RP2040 clone used by A8PicoCart: 16 MiB flash, full GPIO breakout.
// Turbo diagnostic: keep flash below the proven 62.5 MHz: 250/6 or 300/6.
#define PICO_FLASH_SPI_CLKDIV 6
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#include "boards/pico.h"
