#pragma once

#include "mt3620_rdb.h"

// MT3620 RDB: Connect external INKPAPER DISPLAY to SPI using header 4, pin 5 (MISO), pin 7 (SCLK), pin 9 (CSA), pin 11 (MOSI)
#define SAMPLE_EPAPER_SPI MT3620_RDB_HEADER4_ISU1_SPI

// MT3620 SPI Chip Select (CS) value "A". This is not a peripheral identifier, and so has no meaning in an app manifest.
#define SAMPLE_EPAPER_SPI_CS MT3620_SPI_CHIP_SELECT_A

#define SAMPLE_EPAPER_DATA_CONFIG MT3620_GPIO0
#define SAMPLE_EPAPER_BUSY MT3620_GPIO2
#define SAMPLE_EPAPER_RESET MT3620_GPIO16


/**
 * data
**/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t


