/**
  **********************************************************************************
  * SPI - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file hardware_drivers/bucket_spi.c
  * @brief The master end of the SPI link to the bucket controllers.
  *
  * Every bucket shares SCK, MOSI and MISO; only its chip select is its own. A transfer
  * pulls one chip select low, waits BUCKET_CS_SETUP_US so the bucket can get ready,
  * clocks the bytes out, and raises the chip select again. The bucket treats the rising
  * edge as the end of the frame.
  **********************************************************************************
*/

#include "hardware_drivers/bucket_spi.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "semphr.h"

extern SPI_HandleTypeDef BUCKET_SPI_HANDLE;

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} bucket_cs_t;

// bucket n holds console channels 4n to 4n+3
static const bucket_cs_t BUCKET_CS[BUCKET_COUNT] = {
    {CS_Bucket0_GPIO_Port, CS_Bucket0_Pin},
    {CS_Bucket1_GPIO_Port, CS_Bucket1_Pin},
    {CS_Bucket2_GPIO_Port, CS_Bucket2_Pin},
    {CS_Bucket3_GPIO_Port, CS_Bucket3_Pin},
    {CS_Bucket4_GPIO_Port, CS_Bucket4_Pin},
    {CS_Bucket5_GPIO_Port, CS_Bucket5_Pin},
    {CS_Bucket6_GPIO_Port, CS_Bucket6_Pin},
    {CS_Bucket7_GPIO_Port, CS_Bucket7_Pin},
};

static SemaphoreHandle_t s_lock; // one bus: one transfer at a time
static StaticSemaphore_t s_lock_mem;


/*=============================== TIMING ================================*/

/**
 ----------------------------------------------------------------------------------
  @brief INTERNAL delay_us : microseconds -> void
  Counts CPU cycles; the chip-select delays are far shorter than a FreeRTOS tick.
 ----------------------------------------------------------------------------------
*/
static void delay_us (uint32_t us)
{
    const uint32_t cycles = (SystemCoreClock / 1000000U) * us;
    const uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < cycles)
    {
    }
}


/*=============================== PUBLIC FUNCTIONS ================================*/

uint8_t bucket_spi_init (void)
{
    // CubeMX starts every chip select low, which selects all eight buckets at once
    for (uint8_t b = 0; b < BUCKET_COUNT; b++)
    {
        HAL_GPIO_WritePin(BUCKET_CS[b].port, BUCKET_CS[b].pin, GPIO_PIN_SET);
    }

    // the delays count CPU cycles
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    // Set here as well as in CubeMX, so a regenerate can't silently break the link: whole
    // bytes (CubeMX's default is 4-bit frames) at a clock the buckets can keep up with. The
    // chip selects are GPIOs, so the hardware NSS pulse is off.
    BUCKET_SPI_HANDLE.Init.DataSize          = SPI_DATASIZE_8BIT;
    BUCKET_SPI_HANDLE.Init.BaudRatePrescaler = BUCKET_SPI_PRESCALER;
    BUCKET_SPI_HANDLE.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
    if (HAL_SPI_Init(&BUCKET_SPI_HANDLE) != HAL_OK) return 1;

    s_lock = xSemaphoreCreateMutexStatic(&s_lock_mem);
    return (s_lock != NULL) ? 0 : 1;
}


uint8_t bucket_spi_send (uint8_t bucket, const uint8_t *data, uint16_t len)
{
    if (bucket >= BUCKET_COUNT || data == NULL || len == 0) return 1;

    xSemaphoreTake(s_lock, portMAX_DELAY);

    HAL_GPIO_WritePin(BUCKET_CS[bucket].port, BUCKET_CS[bucket].pin, GPIO_PIN_RESET);
    delay_us(BUCKET_CS_SETUP_US);

    // Returns once the last byte has left the shift register. The bytes the bucket clocks
    // back on MISO at the same time are discarded.
    const HAL_StatusTypeDef status =
        HAL_SPI_Transmit(&BUCKET_SPI_HANDLE, (uint8_t *)data, len, BUCKET_SPI_TIMEOUT_MS);

    HAL_GPIO_WritePin(BUCKET_CS[bucket].port, BUCKET_CS[bucket].pin, GPIO_PIN_SET);
    delay_us(BUCKET_CS_GAP_US);

    xSemaphoreGive(s_lock);
    return (status == HAL_OK) ? 0 : 1;
}
