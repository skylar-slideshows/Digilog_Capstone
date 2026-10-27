/**
  **********************************************************************************
  * MASTER CONFIGURATION FILE - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file CONFIG.h
  * @brief Configuration for the master controller, laid out like the bucket's CONFIG.h:
  *        console size first, then which peripherals the firmware uses.
  *
  *        Macros are only kept where a compile-time constant is required (array sizes,
  *        #if, static initializers); everything else is a typed const.
  *
  *        Pins are referenced directly by their CubeMX labels; the bucket chip selects
  *        (CS_Bucket0 ... CS_Bucket7) are listed in bucket_spi.c.
  **********************************************************************************
*/

#ifndef CONFIG_H
#define CONFIG_H

#include "main.h" // CubeMX labels (<Name>_Pin, <Name>_GPIO_Port) and HAL types


/*=============================== CONSOLE SIZE ================================*/
/** @brief The remote app numbers channels 0-31 across the whole console; each bucket
 *         controller knows only its own four, as 0-3. (macros: used for array sizes) */
#define BUCKET_COUNT         8 // bucket controllers on the master SPI, one chip select each
#define CHANNELS_PER_BUCKET  4
#define CONSOLE_CHANNELS     (BUCKET_COUNT * CHANNELS_PER_BUCKET)


/*============================== HARDWARE MAPPING ================================*/

/*------------------------------ MASTER SPI (to the buckets) ------------------------------*/
/** Shared SCK/MOSI/MISO to every bucket, one chip select per bucket. SPI mode 0, MSB first,
 *  8-bit frames, matching the buckets' SPI1 (slave). */
#define BUCKET_SPI_HANDLE    hspi1

// 170 MHz / 64 = 2.66 MHz: a byte every 3 us, slow enough for a bucket to take bytes by
// interrupt and for ribbon cable between boards. A ud command is about 16 bytes (60 us).
static const uint32_t BUCKET_SPI_PRESCALER  = SPI_BAUDRATEPRESCALER_64;
static const uint32_t BUCKET_SPI_TIMEOUT_MS = 10;
static const uint32_t BUCKET_CS_SETUP_US    = 10; // chip select low -> first clock: time for the bucket to get ready
static const uint32_t BUCKET_CS_GAP_US      = 10; // chip select high -> next transfer: time for the bucket to handle the frame

/*------------------------------ LOG UART ------------------------------*/
/** USART2, 921600 baud, asynchronous (set up in CubeMX): text messages for debugging, such as
 *  the DAW's transport changes from daw_control.c. Open it with any terminal at 921600. */
#define LOG_UART_HANDLE      huart2
static const uint32_t LOG_UART_TIMEOUT_MS = 10;


/*=============================== DAW CONTROL (Mackie Control) ================================*/
/** Mackie Control has no "go to start" or "go to end" message, so those two buttons send a key
 *  combination the DAW maps to them: a modifier held around a button, both as Mackie note numbers
 *  (DAW_NO_MODIFIER for none). Shift + Rewind and Shift + Fast Fwd are Cubase's Mackie Control
 *  mapping. Logic goes to the start on a second press of Stop instead; for Logic or other DAWs,
 *  assign a function key in the DAW's control-surface settings and send that here
 *  (F1 = 0x36 ... F8 = 0x3D) with no modifier. (macros: used in a static initializer) */
#define DAW_NO_MODIFIER        0xFF
#define DAW_TO_START_MODIFIER  0x46 // Shift
#define DAW_TO_START_NOTE      0x5B // Rewind
#define DAW_TO_END_MODIFIER    0x46 // Shift
#define DAW_TO_END_NOTE        0x5C // Fast Fwd

#endif // CONFIG_H
