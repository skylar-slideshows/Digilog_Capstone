/**
  **********************************************************************************
  * BUCKET SPI - DIGILOG CONSOLE (Master section)
  **********************************************************************************
  * @file hardware_drivers/bucket_spi.h
  * @brief The master end of the SPI link to the bucket controllers: one shared bus, one
  *        chip select per bucket. Safe to call from any task; one transfer at a time.
  **********************************************************************************
*/

#ifndef BUCKET_SPI_H
#define BUCKET_SPI_H

#include <stdint.h>


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC bucket_spi_init : void -> uint8_t
  Deselects every bucket, sets the SPI to 8-bit frames at BUCKET_SPI_PRESCALER and creates
  the bus lock. Call once before the scheduler starts. Returns 0 on success.
 ----------------------------------------------------------------------------------
*/
uint8_t bucket_spi_init (void);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC bucket_spi_send : bucket, bytes, length -> uint8_t
  Selects one bucket, clocks the bytes out and deselects it again. Blocks the calling
  task until the transfer is done. Returns 0 on success, 1 for a bad bucket number or
  an SPI error.
 ----------------------------------------------------------------------------------
*/
uint8_t bucket_spi_send (uint8_t bucket, const uint8_t *data, uint16_t len);

#endif // BUCKET_SPI_H
