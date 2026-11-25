/**
  **********************************************************************************
  * SD CARD (SPI) - DIGILOG CONSOLE (Master section)
  **********************************************************************************
  * @file hardware_drivers/sd_spi.c
  * @brief An SD card in SPI mode on SPI3, for FatFs.
  *
  * The SPI mode of the SD protocol, briefly:
  *  - Every exchange is a byte out on MOSI and a byte in on MISO at the same time. When
  *    the host only listens it sends 0xFF, and the card sends 0xFF when it has nothing to say.
  *  - A command is 6 bytes: 0x40 | index, a 32-bit argument, and a CRC (only checked for
  *    CMD0 and CMD8 here). The card answers within 8 bytes with R1, one byte whose top
  *    bit is 0; some commands send more bytes after it.
  *  - Data moves in blocks: a start token, 512 bytes, 2 CRC bytes (ignored here).
  *  - After a write the card holds MISO low (busy) until it has stored the block.
  *  - Commands numbered ACMDn are application commands: CMD55, then CMDn.
  *
  * Start-up: at least 74 clocks with chip select high, CMD0 (go idle), CMD8 (tells an
  * SD v2 card from older ones), then ACMD41 repeated until the card leaves idle, CMD58
  * to see whether it is addressed by sector (SDHC/SDXC) or by byte (SDSC), and CMD9
  * for its size. Only then does the clock go up to full speed.
  *
  * Every SPI transfer goes through HAL_SPI_TransmitReceive / HAL_SPI_Transmit, polled.
  **********************************************************************************
*/

#include "hardware_drivers/sd_spi.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

extern SPI_HandleTypeDef SD_SPI_HANDLE;


/*=============================== SD PROTOCOL ================================*/

#define CMD0   (0)         // GO_IDLE_STATE
#define CMD1   (1)         // SEND_OP_COND (MMC)
#define CMD8   (8)         // SEND_IF_COND
#define CMD9   (9)         // SEND_CSD
#define CMD12  (12)        // STOP_TRANSMISSION
#define CMD16  (16)        // SET_BLOCKLEN
#define CMD17  (17)        // READ_SINGLE_BLOCK
#define CMD18  (18)        // READ_MULTIPLE_BLOCK
#define CMD24  (24)        // WRITE_BLOCK
#define CMD25  (25)        // WRITE_MULTIPLE_BLOCK
#define CMD55  (55)        // APP_CMD: the next command is an application command
#define CMD58  (58)        // READ_OCR
#define ACMD13 (0x80 | 13) // SD_STATUS
#define ACMD23 (0x80 | 23) // SET_WR_BLK_ERASE_COUNT
#define ACMD41 (0x80 | 41) // SD_SEND_OP_COND

#define R1_IDLE 0x01 // R1 with only the idle bit set: the card is starting up

#define TOKEN_BLOCK  0xFE // starts a block: reads, and single-block writes
#define TOKEN_MULTI  0xFC // starts each block of a multi-block write
#define TOKEN_STOP   0xFD // ends a multi-block write

// card types
#define CT_MMC   0x01
#define CT_SD1   0x02 // SD version 1
#define CT_SD2   0x04 // SD version 2 or later
#define CT_SDC   (CT_SD1 | CT_SD2)
#define CT_BLOCK 0x08 // addressed by sector (SDHC/SDXC); otherwise by byte

// how long the card may take (ms)
#define INIT_TIMEOUT_MS 1000 // to leave idle after power-up
#define READ_TIMEOUT_MS 200  // to start sending a block
#define BUSY_TIMEOUT_MS 500  // to finish storing a block
#define SD_SPI_TIMEOUT_MS 100
#define SD_SPI_PRESCALER 64
#define SD_SPI_PRESCALER_INIT 64


/*=============================== STATE ================================*/

static uint8_t  s_type;    // CT_ flags; 0 when no card is ready
static uint32_t s_sectors; // card size, from its CSD


/*=============================== SPI ================================*/

static uint8_t xchg (uint8_t out)
{
    uint8_t in = 0xFF;
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &out, &in, 1, SD_SPI_TIMEOUT_MS);
    return in;
}


static void rx_bytes (uint8_t *buf, uint16_t len)
{
    memset(buf, 0xFF, len); // the card wants MOSI high while it sends; received bytes overwrite these
    HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, buf, buf, len, SD_SPI_TIMEOUT_MS);
}


static void tx_bytes (const uint8_t *buf, uint16_t len)
{
    HAL_SPI_Transmit(&SD_SPI_HANDLE, (uint8_t *)buf, len, SD_SPI_TIMEOUT_MS);
}


static void set_clock (uint32_t prescaler)
{
    SD_SPI_HANDLE.Init.BaudRatePrescaler = prescaler;
    HAL_SPI_Init(&SD_SPI_HANDLE);
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL yield_tick : void -> void
  For waits that can run long (a card stores a block in up to hundreds of ms): gives
  the CPU away for a tick instead of spinning, once the scheduler is running.
 ----------------------------------------------------------------------------------
*/
static void yield_tick (void)
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) vTaskDelay(1);
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL wait_ready : timeout -> bool
  Clocks until the card stops holding MISO low (busy) and answers 0xFF
 ----------------------------------------------------------------------------------
*/
static bool wait_ready (uint32_t timeout_ms)
{
    const uint32_t t0 = HAL_GetTick();
    while (xchg(0xFF) != 0xFF)
    {
        const uint32_t elapsed = HAL_GetTick() - t0;
        if (elapsed >= timeout_ms) return false;
        if (elapsed >= 1) yield_tick(); // most waits are over within the first millisecond
    }
    return true;
}


static void card_release (void)
{
    HAL_GPIO_WritePin(DISK_CS_GPIO_Port, DISK_CS_Pin, GPIO_PIN_SET);
    xchg(0xFF); // one more clock, so the card lets go of MISO
}


static bool card_select (void)
{
    HAL_GPIO_WritePin(DISK_CS_GPIO_Port, DISK_CS_Pin, GPIO_PIN_RESET);
    xchg(0xFF);
    if (wait_ready(BUSY_TIMEOUT_MS)) return true;
    card_release();
    return false;
}


/*=============================== COMMANDS AND BLOCKS ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL send_cmd : command, argument -> R1
  Selects the card (except for CMD12, which interrupts a read in progress), sends the
  command and returns its R1, or 0xFF if the card never answered. Leaves the card
  selected, so the caller can read any bytes that follow R1; the caller releases it.
 ----------------------------------------------------------------------------------
*/
static uint8_t send_cmd (uint8_t cmd, uint32_t arg)
{
    if (cmd & 0x80) // ACMDn = CMD55, then CMDn
    {
        cmd &= 0x7F;
        const uint8_t r1 = send_cmd(CMD55, 0);
        if (r1 > R1_IDLE) return r1;
    }

    if (cmd != CMD12)
    {
        card_release();
        if (!card_select()) return 0xFF;
    }

    const uint8_t frame[6] = {
        (uint8_t)(0x40 | cmd),
        (uint8_t)(arg >> 24),
        (uint8_t)(arg >> 16),
        (uint8_t)(arg >> 8),
        (uint8_t)arg,
        (cmd == CMD0) ? 0x95 : (cmd == CMD8) ? 0x87 : 0x01, // only these two CRCs are checked in SPI mode
    };
    tx_bytes(frame, sizeof(frame));

    if (cmd == CMD12) xchg(0xFF); // a stuff byte comes before CMD12's answer

    uint8_t r1;
    uint8_t tries = 10;
    do
    {
        r1 = xchg(0xFF);
    } while ((r1 & 0x80) && --tries);
    return r1;
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL rx_datablock : buffer, length -> bool
  Waits for the start token, then reads the block and skips its CRC
 ----------------------------------------------------------------------------------
*/
static bool rx_datablock (uint8_t *buf, uint16_t len)
{
    const uint32_t t0 = HAL_GetTick();
    uint8_t token;
    while ((token = xchg(0xFF)) == 0xFF)
    {
        const uint32_t elapsed = HAL_GetTick() - t0;
        if (elapsed >= READ_TIMEOUT_MS) return false;
        if (elapsed >= 1) yield_tick();
    }
    if (token != TOKEN_BLOCK) return false; // an error token

    rx_bytes(buf, len);
    xchg(0xFF); // CRC, not checked
    xchg(0xFF);
    return true;
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL tx_datablock : 512 bytes (or NULL), token -> bool
  Sends one block after the card is ready for it, and checks the card accepted it.
  TOKEN_STOP on its own ends a multi-block write.
 ----------------------------------------------------------------------------------
*/
static bool tx_datablock (const uint8_t *buf, uint8_t token)
{
    if (!wait_ready(BUSY_TIMEOUT_MS)) return false;

    xchg(token);
    if (token == TOKEN_STOP) return true;

    tx_bytes(buf, SD_SECTOR_SIZE);
    xchg(0xFF); // CRC, not checked by the card in SPI mode
    xchg(0xFF);
    return (xchg(0xFF) & 0x1F) == 0x05; // data response: accepted
}


/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL read_csd : 16-byte buffer -> bool
 ----------------------------------------------------------------------------------
*/
static bool read_csd (uint8_t csd[16])
{
    const bool ok = (send_cmd(CMD9, 0) == 0) && rx_datablock(csd, 16);
    card_release();
    return ok;
}


static uint32_t sectors_from_csd (const uint8_t csd[16])
{
    if ((csd[0] >> 6) == 1)
    {
        // CSD version 2 (SDHC/SDXC): (C_SIZE + 1) x 512 KB
        const uint32_t c_size = csd[9] | ((uint32_t)csd[8] << 8) | ((uint32_t)(csd[7] & 0x3F) << 16);
        return (c_size + 1U) << 10;
    }

    // CSD version 1 (SDSC, MMC): (C_SIZE + 1) x 2^(C_SIZE_MULT + 2) blocks of 2^READ_BL_LEN bytes
    const uint32_t c_size = (csd[8] >> 6) | ((uint32_t)csd[7] << 2) | ((uint32_t)(csd[6] & 0x03) << 10);
    const uint8_t mult    = (uint8_t)(((csd[10] & 0x80) >> 7) | ((csd[9] & 0x03) << 1));
    const uint8_t bl_len  = csd[5] & 0x0F;
    const uint8_t shift   = (uint8_t)(mult + 2 + bl_len); // log2 of bytes per C_SIZE unit
    return (shift >= 9) ? (c_size + 1U) << (shift - 9) : (c_size + 1U) >> (9 - shift);
}


/*=============================== PUBLIC FUNCTIONS ================================*/

uint8_t sd_spi_init (void)
{
    s_type    = 0;
    s_sectors = 0;

    // Whole bytes (CubeMX's default is 4-bit frames), SPI mode 0, and the start-up clock
    SD_SPI_HANDLE.Init.DataSize    = SPI_DATASIZE_8BIT;
    SD_SPI_HANDLE.Init.CLKPolarity = SPI_POLARITY_LOW;
    SD_SPI_HANDLE.Init.CLKPhase    = SPI_PHASE_1EDGE;
    SD_SPI_HANDLE.Init.NSSPMode    = SPI_NSS_PULSE_DISABLE;
    set_clock(SD_SPI_PRESCALER_INIT);

    // at least 74 clocks with the card deselected, to put it in SPI mode
    HAL_GPIO_WritePin(DISK_CS_GPIO_Port, DISK_CS_Pin, GPIO_PIN_SET);
    for (uint8_t i = 0; i < 10; i++) xchg(0xFF);

    uint8_t type = 0;
    uint8_t r1   = 0xFF;
    for (uint8_t attempt = 0; attempt < 5 && r1 != R1_IDLE; attempt++) r1 = send_cmd(CMD0, 0);

    if (r1 == R1_IDLE)
    {
        const uint32_t t0 = HAL_GetTick();
        uint8_t ocr[4];

        if (send_cmd(CMD8, 0x1AA) == R1_IDLE)
        {
            // SD version 2: the card echoes the check pattern if it runs at 2.7-3.6 V
            rx_bytes(ocr, 4);
            if (ocr[2] == 0x01 && ocr[3] == 0xAA)
            {
                // leave idle, telling the card we handle high-capacity (sector) addressing
                while (send_cmd(ACMD41, 1UL << 30) != 0 && HAL_GetTick() - t0 < INIT_TIMEOUT_MS) yield_tick();

                if (HAL_GetTick() - t0 < INIT_TIMEOUT_MS && send_cmd(CMD58, 0) == 0)
                {
                    rx_bytes(ocr, 4);
                    type = (ocr[0] & 0x40) ? (CT_SD2 | CT_BLOCK) : CT_SD2; // CCS bit
                }
            }
        }
        else
        {
            // SD version 1 answers ACMD41; MMC doesn't, and starts with CMD1 instead
            uint8_t start_cmd;
            if (send_cmd(ACMD41, 0) <= R1_IDLE)
            {
                type      = CT_SD1;
                start_cmd = ACMD41;
            }
            else
            {
                type      = CT_MMC;
                start_cmd = CMD1;
            }
            while (send_cmd(start_cmd, 0) != 0 && HAL_GetTick() - t0 < INIT_TIMEOUT_MS) yield_tick();
            if (HAL_GetTick() - t0 >= INIT_TIMEOUT_MS) type = 0;
        }

        // cards addressed by byte: make sure a block is 512 bytes
        if (type && !(type & CT_BLOCK) && send_cmd(CMD16, SD_SECTOR_SIZE) != 0) type = 0;
    }
    card_release();

    if (type == 0) return 1;

    set_clock(SD_SPI_PRESCALER);

    uint8_t csd[16];
    if (!read_csd(csd)) return 1;
    s_sectors = sectors_from_csd(csd);
    s_type    = type;
    return 0;
}


bool sd_spi_ready (void)
{
    return s_type != 0;
}


uint8_t sd_spi_read (uint8_t *buf, uint32_t sector, uint32_t count)
{
    if (s_type == 0 || buf == NULL || count == 0) return 1;
    if (!(s_type & CT_BLOCK)) sector *= SD_SECTOR_SIZE; // byte address

    if (count == 1)
    {
        if (send_cmd(CMD17, sector) == 0 && rx_datablock(buf, SD_SECTOR_SIZE)) count = 0;
    }
    else if (send_cmd(CMD18, sector) == 0)
    {
        do
        {
            if (!rx_datablock(buf, SD_SECTOR_SIZE)) break;
            buf += SD_SECTOR_SIZE;
        } while (--count);
        send_cmd(CMD12, 0); // stop the card sending
    }
    card_release();

    if (count) s_type = 0; // something went wrong: start the card again before the next use
    return count ? 1 : 0;
}


uint8_t sd_spi_write (const uint8_t *buf, uint32_t sector, uint32_t count)
{
    if (s_type == 0 || buf == NULL || count == 0) return 1;
    if (!(s_type & CT_BLOCK)) sector *= SD_SECTOR_SIZE; // byte address

    if (count == 1)
    {
        if (send_cmd(CMD24, sector) == 0 && tx_datablock(buf, TOKEN_BLOCK)) count = 0;
    }
    else
    {
        if (s_type & CT_SDC) send_cmd(ACMD23, count); // lets the card pre-erase: faster
        if (send_cmd(CMD25, sector) == 0)
        {
            do
            {
                if (!tx_datablock(buf, TOKEN_MULTI)) break;
                buf += SD_SECTOR_SIZE;
            } while (--count);
            if (!tx_datablock(NULL, TOKEN_STOP)) count = 1;
        }
    }
    card_release();

    if (count) s_type = 0;
    return count ? 1 : 0;
}


uint8_t sd_spi_sync (void)
{
    if (s_type == 0) return 1;
    const bool ok = card_select(); // waits for the card to finish storing
    card_release();
    return ok ? 0 : 1;
}


uint32_t sd_spi_sector_count (void)
{
    return s_sectors;
}


uint32_t sd_spi_erase_block (void)
{
    uint32_t sectors = 1;

    if (s_type & CT_SD2)
    {
        // allocation unit from the 64-byte SD status: 16 sectors << AU_SIZE
        uint8_t status[64];
        if (send_cmd(ACMD13, 0) == 0)
        {
            xchg(0xFF); // second byte of the R2 answer
            if (rx_datablock(status, sizeof(status))) sectors = 16UL << (status[10] >> 4);
        }
        card_release();
    }
    else if (s_type & CT_SD1)
    {
        // from the CSD: (SECTOR_SIZE + 1) write blocks of 2^WRITE_BL_LEN bytes
        uint8_t csd[16];
        if (read_csd(csd))
        {
            const uint32_t blocks = (((uint32_t)(csd[10] & 0x3F) << 1) | ((csd[11] & 0x80) >> 7)) + 1U;
            const uint8_t wbl_len = (uint8_t)(((csd[12] & 0x03) << 2) | (csd[13] >> 6));
            sectors = (wbl_len >= 9) ? blocks << (wbl_len - 9) : blocks;
        }
    }
    return sectors;
}
