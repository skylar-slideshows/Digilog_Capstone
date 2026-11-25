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
#include <stdbool.h>


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

/*------------------------------ BUCKET LINK ------------------------------*/
/** Every chip-select window with a bucket is a full-duplex exchange of BUCKET_XCHG_BYTES (bucket_link.c):
 *  out on MOSI, an acknowledgement and whatever is queued for that bucket, 0xFF after; back on MISO, the
 *  bucket's packet. Must match LINK_XCHG_BYTES on the buckets. */
#define BUCKET_XCHG_BYTES   64
#define BUCKET_POLL_MS      5    // each bucket that answers gets a window this often
#define BUCKET_LOST_MS      100  // no valid packet for this long: "something's wrong" (yellow on the screen)
#define BUCKET_GIVE_UP_MS   2000 // ...for this long (or never, this long after start-up): taken as unplugged (red)
#define BUCKET_RETRY_MS     100  // an unplugged one still gets a window this often, so it's found when it starts
#define CHANNEL_NAME_MAX    20   // longest channel name sent to the buckets (what their title strips take)

/*------------------------------ LOG UART ------------------------------*/
/** USART2, 921600 baud, asynchronous (set up in CubeMX): text messages for debugging, such as
 *  the DAW's transport changes from daw_control.c. Open it with any terminal at 921600. */
#define LOG_UART_HANDLE      huart2
static const uint32_t LOG_UART_TIMEOUT_MS = 10;


/*=============================== CONTROLS: KNOBS, BUTTONS, DAC ================================*/
/** Copied from the bucket controller's design (control_integration/, hardware_drivers/), on one bus.
 *  I2C2 carries everything:
 *    0x20  U7 MCP23017: K0-K3 (aux return knobs, with their pushes), B3, INSERT
 *    0x21  U8 MCP23017: K4 (screen knob) and its push, B0-B2, B4-B11
 *    0x60  U9 MCP4728:  A-D -> CV0_AUX1 ... CV3_AUX4 (aux return levels, J27)
 *  Pin-by-pin wiring is in control_integration/knob_control.c and button_control.c. */
#define CONTROLS_I2C_BUS      I2C2
#define CONTROLS_I2C_HANDLE   hi2c2

#define MCP23017_KNOBS_ADDR   0x20 // U7
#define MCP23017_PANEL_ADDR   0x21 // U8
#define MCP4728_ADDR          0x60 // U9 (factory address; LDAC is held low on PA10)

// ~390 kHz at PCLK1 = 170 MHz (PRESC 3, SCLDEL 11, SDADEL 2, SCLH 0x21, SCLL 0x3F). CubeMX has I2C2 at
// 100 kHz, too slow to read both expanders every millisecond, so i2c_init() applies this. Set I2C2
// to Fast Mode in CubeMX too, so the two agree.
static const uint32_t CONTROLS_I2C_TIMING = 0x30B2213FU;

static const uint8_t  I2C_MAX_TRANSFER_LEN = 8;   // bytes; the longest transfer is the DAC's 8-byte fast write
static const uint32_t TIMEOUT_US           = 500; // a transfer that takes longer than this has failed

// The bucket ticks these from TIM4 (1.5 kHz) and TIM5 (120 Hz); the master has one small bus, so plain
// FreeRTOS periods do (no extra timers in CubeMX).
static const uint32_t CONTROLS_PERIOD_MS  = 1; // read the expanders, update values and outputs
static const uint32_t LED_FRAME_PERIOD_MS = 8; // redraw the LEDs (125 Hz)

/** Encoders: +1 if turning a knob clockwise raises its value, -1 to flip all five. The table in
 *  knob_control.c follows the encoders' own A/B pads, which the schematic nets name the other way round
 *  (pad A -> K0_B, pad B -> K0_A, on every knob). */
static const int8_t KNOB_DIRECTION = 1;

/** The screen knob (K4) counts one step per detent: EC11 encoder transitions per detent */
static const uint8_t SCREEN_KNOB_TRANSITIONS_PER_STEP = 4;


/*=============================== AUX RETURNS AND INSERT ================================*/
#define AUX_RETURNS 4 // macro: array sizes

/** Knob value (0-65535) for 0 dB: the CV scaler on the analog board gives 0 dB at 1.536 V, 3/4 of the
 *  DAC's 2.048 V range (bottom -35.6 dB, top +11.9 dB). A double press puts a knob back here. */
static const uint16_t AUX_RETURN_UNITY = 49152U;

/** Returns are summed into the mix (LCV_AUXn high) from power-up. Holding a return's knob in switches it. */
static const bool AUX_RETURN_TO_MIX_DEFAULT = true;

/** Switching a return in or out of the mix first turns its level down, then back up, over this long each way */
static const uint32_t AUX_SWITCH_FADE_MS = 10;

/** LCV_INSERT (PE6) drives the insert relay on the analog board. Not set up in CubeMX yet: if it isn't,
 *  these stand in and control_outputs_init() makes it an output. */
#ifndef LCV_INSERT_Pin
#define LCV_INSERT_Pin       GPIO_PIN_6
#define LCV_INSERT_GPIO_Port GPIOE
#endif


/*=============================== LEDS ================================*/
/** As on the bucket: data out over SPI2 with DMA, latched on PB14 (LED_LAT), brightness a PWM on the knob
 *  boards' (active low) output enable from TIM1 channel 2 (PC1, LED_PWM). The two 74HCT595s for the
 *  button LEDs (U5, U6) have their output enable tied low, so brightness only dims the knob rings. */
#define LED_SPI_HANDLE         hspi2
#define LED_SPI_INSTANCE       SPI2
#define LED_OE_TIM_HANDLE      htim1
#define LED_OE_TIM_CHAN        TIM_CHANNEL_2

static const uint32_t BRIGHTNESS_PWM_HZ      = 24000U; // TIM1's period is set from this
static const uint8_t  LED_BRIGHTNESS_DEFAULT = 64U;    // 0-255, the knob rings at power-up

/** Chain order from the MCU: knob boards K0, K1, K2, K3 (J19/J20 ... J25/J26), then U5, then U6.
 *  The knob boards are taken to be the bucket's ring design: 32 LEDs (four 74HC595s) each. */
#define LEDS_PER_KNOB          32 // macro: used for array sizes
#define LED_CHAIN_BITS         (4 * LEDS_PER_KNOB + 16) // macro: array size (four rings + U5 + U6)
#define LED_MAX_DEVICES        24                       // macro: array size (4 rings + 16 button outputs)

// animations (as on the bucket)
#define LED_ANIM_LOAD_PATTERN  0xE0E0E0E0U // macro: used in a static initializer
static const uint8_t  LED_ANIM_BREATHE_MIN   =   1U; // dimmest
static const uint8_t  LED_ANIM_BREATHE_MAX   = 128U; // brightest
static const uint32_t LED_ANIM_BREATHE_STEPS =  64U; // num steps between these values


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
