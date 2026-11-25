/**
  **********************************************************************************
  * SHIFT REGISTER DRIVER - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file 74hc595.c
  * @brief See 74hc595.h. From the bucket controller's driver (Skylar Denno).
  **********************************************************************************
*/

#include "hardware_drivers/74hc595.h"
#include "CONFIG.h"
#include "main.h"

// BSRR register: reset[15:0]set[15:0]. The CubeMX *_Pin labels are GPIO_PIN_x bitmasks, so they drop straight in.
#define PIN_SET(pin)  ((uint32_t)(pin))
#define PIN_RST(pin)  ((uint32_t)(pin) << 16)

// Push-pull output at the fastest slew; CubeMX already enabled the port clock and set the pin low
static void pin_out_fast (GPIO_TypeDef *port, uint32_t pin)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = pin;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(port, &g);
}

void latch_out (void)
{
    LED_LAT_GPIO_Port->BSRR = PIN_SET(LED_LAT_Pin);
    __NOP(); __NOP(); // the 74HCT595 needs about 20 ns; two cycles at 170 MHz is 12 ns, the BSRR writes add more
    __NOP(); __NOP();
    LED_LAT_GPIO_Port->BSRR = PIN_RST(LED_LAT_Pin);
}

void led_shiftreg_init (void)
{
    pin_out_fast(LED_LAT_GPIO_Port, LED_LAT_Pin);
    LED_LAT_GPIO_Port->BSRR = PIN_RST(LED_LAT_Pin);
}
