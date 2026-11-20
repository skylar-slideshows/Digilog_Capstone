/**
  **********************************************************************************
  * ROUTING BOARD SWITCHES (74HC595 CHAIN) - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file hardware_drivers/routing_switches.c
  * @brief See routing_switches.h. Uses shift_bit() and latch_out(false) from
  *        74hc595.c on the SHIFTREG pins, which shiftreg_init() in main.c sets up.
  **********************************************************************************
*/

#include "hardware_drivers/routing_switches.h"
#include "hardware_drivers/74hc595.h"
#include "CONFIG.h"
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdbool.h>

#define CHAIN_BYTES (2 * CHANNELS) // 8 registers

// SR_OE_EXT on the routing board: PF9, labelled SHIFTREG_OE in CubeMX (main.h). The fallback is only
// for a main.h generated before the label existed.
#ifndef SHIFTREG_OE_Pin
#define SHIFTREG_OE_Pin       GPIO_PIN_9
#define SHIFTREG_OE_GPIO_Port GPIOF
#endif

/** Turns on the clock of whichever port the OE pin is on, so moving the pin in CubeMX needs no change here */
static void enable_port_clock (GPIO_TypeDef *port)
{
    if (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
    else if (port == GPIOD) __HAL_RCC_GPIOD_CLK_ENABLE();
    else if (port == GPIOE) __HAL_RCC_GPIOE_CLK_ENABLE();
    else if (port == GPIOF) __HAL_RCC_GPIOF_CLK_ENABLE();
    else if (port == GPIOG) __HAL_RCC_GPIOG_CLK_ENABLE();
}

static uint8_t s_image[CHAIN_BYTES];   // [2*ch] = first register, [2*ch+1] = second; bit 0 = QA
static StaticSemaphore_t s_lock_buffer;
static SemaphoreHandle_t s_lock;

/** Sends the image and latches it. The first bit sent ends up furthest down the chain
 *  (U12 QH), so send from the last register's QH back to U5's QA. */
static void send_chain (void)
{
    for (int8_t byte = CHAIN_BYTES - 1; byte >= 0; byte--)
    {
        for (int8_t bit = 7; bit >= 0; bit--)
        {
            shift_bit((s_image[byte] >> bit) & 1U);
        }
    }
    latch_out(false);
}

void routing_switches_init (void)
{
    if (s_lock == NULL)
    {
        s_lock = xSemaphoreCreateMutexStatic(&s_lock_buffer);
    }

    // Outputs off (OE high) while a safe state goes in. Set up here as well as by CubeMX, so it works
    // either way. Set PF9's initial output level to High in CubeMX so they're also off from power-up until now.
    enable_port_clock(SHIFTREG_OE_GPIO_Port);
    HAL_GPIO_WritePin(SHIFTREG_OE_GPIO_Port, SHIFTREG_OE_Pin, GPIO_PIN_SET);
    GPIO_InitTypeDef oe = {0};
    oe.Pin = SHIFTREG_OE_Pin;
    oe.Mode = GPIO_MODE_OUTPUT_PP;
    oe.Pull = GPIO_NOPULL;
    oe.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SHIFTREG_OE_GPIO_Port, &oe);

    for (uint8_t i = 0; i < CHAIN_BYTES; i++) s_image[i] = 0;
    send_chain();

    HAL_GPIO_WritePin(SHIFTREG_OE_GPIO_Port, SHIFTREG_OE_Pin, GPIO_PIN_RESET); // outputs on
}

void routing_switches_set (uint8_t channel, uint16_t bits)
{
    if (channel >= CHANNELS || s_lock == NULL) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    const uint8_t first = (uint8_t)(bits & 0xFFU), second = (uint8_t)(bits >> 8);
    if (s_image[2 * channel] != first || s_image[2 * channel + 1] != second)
    {
        s_image[2 * channel] = first;
        s_image[2 * channel + 1] = second;
        send_chain();
    }
    xSemaphoreGive(s_lock);
}

uint16_t routing_switches_get (uint8_t channel)
{
    if (channel >= CHANNELS) return 0;
    return (uint16_t)(s_image[2 * channel] | (s_image[2 * channel + 1] << 8));
}
