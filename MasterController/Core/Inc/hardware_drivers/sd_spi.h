/**
  **********************************************************************************
  * SD CARD (SPI) - DIGILOG CONSOLE (Master section)
  **********************************************************************************
  * @file hardware_drivers/sd_spi.h
  * @brief An SD card in SPI mode on SPI3 (the DISK_ pins), in 512-byte sectors.
  *
  *        Written for Adafruit's SPI Flash SD Card (product 4899, an XTSD chip that
  *        behaves as an SD card), and handles any SD v1, SD v2 (SDSC, SDHC, SDXC) or
  *        MMC card. FatFs reaches it through FATFS/Target/user_diskio.c; the rest of
  *        the firmware should use FatFs, not these functions.
  *
  *        Not thread-safe on its own: FatFs (with _FS_REENTRANT) serialises access.
  **********************************************************************************
*/

#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdbool.h>
#include <stdint.h>

#define SD_SECTOR_SIZE 512


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC sd_spi_init : void -> uint8_t
  Wakes the card, works out what kind it is, and reads its size. Call from a task.
  Returns 0 when the card is ready, 1 if no card answered or it didn't finish starting.
 ----------------------------------------------------------------------------------
*/
uint8_t sd_spi_init (void);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC sd_spi_ready : void -> bool
  True once sd_spi_init() has succeeded (and no read or write has failed since)
 ----------------------------------------------------------------------------------
*/
bool sd_spi_ready (void);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC sd_spi_read / sd_spi_write : buffer, first sector, sector count -> uint8_t
  count sectors of SD_SECTOR_SIZE bytes. Return 0 on success, 1 on failure.
 ----------------------------------------------------------------------------------
*/
uint8_t sd_spi_read (uint8_t *buf, uint32_t sector, uint32_t count);
uint8_t sd_spi_write (const uint8_t *buf, uint32_t sector, uint32_t count);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC sd_spi_sync : void -> uint8_t
  Waits for the card to finish storing the last write. Returns 0 when it has.
 ----------------------------------------------------------------------------------
*/
uint8_t sd_spi_sync (void);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC sd_spi_sector_count : void -> uint32_t
  The card's size in sectors, from its CSD register (0 before sd_spi_init succeeds)
 ----------------------------------------------------------------------------------
*/
uint32_t sd_spi_sector_count (void);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC sd_spi_erase_block : void -> uint32_t
  The card's erase block in sectors, which f_mkfs aligns the file system to (1 if unknown)
 ----------------------------------------------------------------------------------
*/
uint32_t sd_spi_erase_block (void);

#endif // SD_SPI_H
