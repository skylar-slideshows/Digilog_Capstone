/**
  **********************************************************************************
  * KNOB CONTROL - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file control_integration/knob_control.c
  * @brief The bucket's knob handling (knob_control.c), for the master's five knobs:
  *        - K0-K3, aux return 1-4 level. Each transition of the encoder moves the level by
  *          1/64 of its range (as on the bucket); holding the knob down halves that. A double
  *          press puts the level back to 0 dB, then the knob ignores everything until
  *          RESET_QUIET_MS after it's let go. Holding it down for MODE_HOLD_MS without turning
  *          switches the return in or out of the mix (the bucket's shelf/bell switch): the ring
  *          flashes, and while out of the mix it shows a single point instead of a bar.
  *        - K4, the screen's knob: counted in detents for the screen (control_take_screen_input).
  **********************************************************************************
*/

#include "CONFIG.h"
#include "control_integration/control_interface.h"
#include "control_internal.h"
#include "knob_control.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"
#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_sets.h"
#include "hardware_structs.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct
{
    int8_t direction; //!< +1 turned toward A, -1 turned toward B, 0 not turned (KNOB_DIRECTION applied)
    bool held;        //!< Push button is held (fine adjustment)
    bool reset;       //!< Push button was double pressed (reset value)
} knob_input_t;

/*
 * After a double click resets a knob, the knob ignores everything (turning, holding) until
 * RESET_QUIET_MS after its button is let go, so the knob being nudged while it's pressed doesn't
 * move the value straight off its reset. Pressing it again during the quiet time restarts the wait.
 */
#define RESET_QUIET_MS 300U
#define KNOB_COUNT     8U // room for every knob on the master (there are 5)

typedef enum
{
    KNOB_LIVE,          //!< inputs used normally
    KNOB_RESET_HELD,    //!< just reset, button still down: ignore inputs
    KNOB_RESET_RELEASED //!< let go: ignore inputs until RESET_QUIET_MS after release
} knob_lock_state_t;

typedef struct
{
    uint16_t knob; //!< which physical knob: its button's expander address and pin (0 = free entry)
    knob_lock_state_t state;
    TickType_t released_at;
} knob_lock_t;

static knob_lock_t s_knob_locks[KNOB_COUNT]; // only the I2C task reads the knobs

/** The physical knob's entry, found by its push button's location */
static knob_lock_t *knob_lock (const encoder_info_t *info)
{
    const button_info_t *button = &(info->button_info);
    const uint16_t key = (uint16_t)(0x8000U | ((button->addr & 0x7FU) << 8) | ((uint16_t)(button->port - MCP_GPIOA) << 4) | (button->pin & 0x0FU));

    for (uint8_t i = 0; i < KNOB_COUNT; i++)
    {
        if (s_knob_locks[i].knob == key) return &s_knob_locks[i];
        if (s_knob_locks[i].knob == 0)
        {
            s_knob_locks[i].knob = key; // first time this knob is read
            s_knob_locks[i].state = KNOB_LIVE;
            return &s_knob_locks[i];
        }
    }
    return NULL; // table full: no quiet time for this knob
}

static knob_input_t read_knob (const encoder_info_t *info)
{
    knob_input_t input = {0};

    input.reset = update_button(&(info->button_info)) == BUTTON_DOUBLE_PRESSED;
    input.held = get_button_held(&(info->button_info));

    const encoder_turn_action_t motion = update_encoder(info); // always read, so ignored turns don't pile up
    if (motion == ENCODER_TURN_A)
    {
        input.direction = (int8_t)KNOB_DIRECTION;
    }
    else if (motion == ENCODER_TURN_B)
    {
        input.direction = (int8_t)-KNOB_DIRECTION;
    }

    knob_lock_t *lock = knob_lock(info);
    if (lock != NULL)
    {
        const TickType_t now = xTaskGetTickCount();
        if (input.reset)
        {
            lock->state = KNOB_RESET_HELD; // the reset itself still goes through below
        }
        else if (lock->state == KNOB_RESET_HELD && !input.held)
        {
            lock->state = KNOB_RESET_RELEASED;
            lock->released_at = now;
        }
        else if (lock->state == KNOB_RESET_RELEASED)
        {
            if (input.held) lock->state = KNOB_RESET_HELD; // pressed again: wait for this release
            else if (now - lock->released_at >= pdMS_TO_TICKS(RESET_QUIET_MS)) lock->state = KNOB_LIVE;
        }

        if (lock->state != KNOB_LIVE)
        {
            input.direction = 0;
            input.held = false;
        }
    }
    return input;
}

/** Applies a knob's input to an unsigned value. A reset puts it back to reset_to (its startup value).
 *  Returns true if the knob was turned or reset (whether or not the value could move). */
static bool apply_u_input (
    SemaphoreHandle_t *out_mutex,
    const knob_input_t *input,
    u_scalar_control_t *out,
    s_scalar_control_t sensitivity, //!< sensitivity must be < S_SCALAR_CONTROL_MAX
    u_scalar_control_t reset_to
)
{
    if (!input->reset && input->direction == 0)
    {
        return false;
    }
    sensitivity = (s_scalar_control_t)(sensitivity * input->direction);

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    if (input->reset)
    {
        *out = reset_to;
    }
    if (sensitivity > 0 && U_SCALAR_CONTROL_MAX - *out < (u_scalar_control_t)sensitivity)
    {
        *out = U_SCALAR_CONTROL_MAX;
    }
    else if (sensitivity < 0 && (u_scalar_control_t)(-sensitivity) > *out)
    {
        *out = 0;
    }
    else
    {
        *out = (u_scalar_control_t)(*out + sensitivity);
    }
    xSemaphoreGive(*out_mutex);
    return true;
}

// Holding a knob's button makes adjustments finer
static inline s_scalar_control_t fine_if_held (const knob_input_t *input, s_scalar_control_t sensitivity)
{
    return input->held ? (s_scalar_control_t)(sensitivity / 2) : sensitivity;
}


/*=============================== HOLD TO SWITCH (return in / out of the mix) ================================*/
/*
 * As the bucket's shelf/bell switch: holding a knob down for MODE_HOLD_MS switches its return in or out of the
 * mix, and its ring flashes fully MODE_FLASHES times to confirm. A slight turn while held (under 1/20 of the
 * range) still switches, and is undone; a bigger turn is fine adjustment and that hold doesn't switch. One switch
 * per hold; let go and hold again to switch back.
 */
#define MODE_HOLD_MS         2000U
#define MODE_FLASHES         3U
#define MODE_FLASH_PERIOD_MS 300U // on for half, off for half
#define MODE_HOLD_TOLERANCE  (65536 / 20)

typedef struct
{
    TickType_t since;    //!< when the current press started
    bool pressed;        //!< the button was down at the last update
    bool spent;          //!< this press turned the knob too far or already switched: no switch until let go
    int32_t start_value; //!< the knob's value when the press started
} mode_hold_t;

typedef struct
{
    volatile TickType_t start;
    volatile bool active;
} mode_flash_t;

static mode_hold_t s_mode_hold[AUX_RETURNS];   // read and written by the I2C task
static mode_flash_t s_mode_flash[AUX_RETURNS]; // set by the I2C task, shown by the LED task

/** Call after the knob's turn has been applied */
static void check_mode_hold (uint8_t ret, const knob_input_t *input, SemaphoreHandle_t *vals_mutex, master_controls *vals)
{
    mode_hold_t *hold = &(s_mode_hold[ret]);
    if (!input->held)
    {
        hold->pressed = false;
        return;
    }

    const TickType_t now = xTaskGetTickCount();
    const int32_t value = (int32_t)vals->aux_return_level[ret]; // only this task writes it
    if (!hold->pressed)
    {
        hold->pressed = true;
        hold->since = now;
        hold->spent = false;
        hold->start_value = value;
    }
    const int32_t moved = value > hold->start_value ? value - hold->start_value : hold->start_value - value;
    if (moved >= MODE_HOLD_TOLERANCE)
    {
        hold->spent = true; // turned on purpose: no switch this press
    }
    if (!hold->spent && (now - hold->since) >= pdMS_TO_TICKS(MODE_HOLD_MS))
    {
        xSemaphoreTake(*vals_mutex, portMAX_DELAY);
        vals->aux_return_to_mix[ret] = !vals->aux_return_to_mix[ret];
        vals->aux_return_level[ret] = (u_scalar_control_t)hold->start_value; // undo the accidental nudge
        xSemaphoreGive(*vals_mutex);
        s_mode_flash[ret].start = now;
        s_mode_flash[ret].active = true;
        hold->spent = true;
        control_touched(TOUCHED_AUX_RETURN_MIX, ret);
    }
}


/*=============================== THE SCREEN'S KNOB ================================*/

static int8_t s_screen_transitions; // encoder transitions not yet a whole detent

static void update_screen_knob (const knob_info_t *knob)
{
    const encoder_turn_action_t motion = update_encoder(&(knob->encoder));
    if (motion == ENCODER_TURN_A) s_screen_transitions = (int8_t)(s_screen_transitions + KNOB_DIRECTION);
    else if (motion == ENCODER_TURN_B) s_screen_transitions = (int8_t)(s_screen_transitions - KNOB_DIRECTION);

    const int8_t per_step = (int8_t)(SCREEN_KNOB_TRANSITIONS_PER_STEP ? SCREEN_KNOB_TRANSITIONS_PER_STEP : 1);
    if (s_screen_transitions >= per_step)
    {
        s_screen_transitions = (int8_t)(s_screen_transitions - per_step);
        screen_input_turned(1);
    }
    else if (s_screen_transitions <= -per_step)
    {
        s_screen_transitions = (int8_t)(s_screen_transitions + per_step);
        screen_input_turned(-1);
    }

    if (update_button(&(knob->encoder.button_info)) != BUTTON_NO_EVENT) screen_input_knob_pressed();
}


/*=============================== VALUES ================================*/

void update_knob_values (SemaphoreHandle_t *vals_mutex, master_controls *vals, master_control_io_t *io)
{
    const s_scalar_control_t sensitivity = S_SCALAR_CONTROL_MAX / 32; // as on the bucket: 64 transitions end to end
    const master_controls *start = control_defaults();

    for (uint8_t ret = 0; ret < AUX_RETURNS; ret++)
    {
        const knob_input_t input = read_knob(&(io->aux_return_knob[ret].encoder));
        if (apply_u_input(vals_mutex, &input, &(vals->aux_return_level[ret]), fine_if_held(&input, sensitivity),
                          start->aux_return_level[ret]))
        {
            control_touched(TOUCHED_AUX_RETURN_LEVEL, ret);
        }
        check_mode_hold(ret, &input, vals_mutex, vals);
    }

    update_screen_knob(&(io->screen_knob));
}


/*=============================== LEDS ================================*/

static void show_unsigned (led_device_t *ring, u_scalar_control_t value, knob_disp_t mode)
{
    if (ring == NULL)
    {
        return;
    }
    if (ring->scale != SCALE_LEFT)
    {
        led_set_scale(ring, SCALE_LEFT);
    }
    if (ring->disp_mode != mode)
    {
        led_set_disp(ring, mode);
    }
    led_set(ring, value);
}

/** While a return's in/out switch flash runs, its ring blinks fully on and off. Returns false once it's over. */
static bool show_mode_flash (uint8_t ret, led_device_t *ring)
{
    mode_flash_t *flash = &(s_mode_flash[ret]);
    if (!flash->active)
    {
        return false;
    }
    const TickType_t elapsed = xTaskGetTickCount() - flash->start;
    const TickType_t period = pdMS_TO_TICKS(MODE_FLASH_PERIOD_MS);
    if (elapsed >= MODE_FLASHES * period)
    {
        flash->active = false;
        return false;
    }
    show_unsigned(ring, (elapsed % period < period / 2) ? U_SCALAR_CONTROL_MAX : 0, DISP_BAR);
    return true;
}

void update_knob_leds (SemaphoreHandle_t *vals_mutex, master_controls *vals, master_control_io_t *io)
{
    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    const master_controls v = *vals;
    xSemaphoreGive(*vals_mutex);

    for (uint8_t ret = 0; ret < AUX_RETURNS; ret++)
    {
        led_device_t *ring = io->aux_return_knob[ret].led_ring;
        if (show_mode_flash(ret, ring)) continue;
        // in the mix: a bar up to the level; out of the mix: just the point where the level is
        show_unsigned(ring, v.aux_return_level[ret], v.aux_return_to_mix[ret] ? DISP_BAR : DISP_POINT);
    }
}


/*=============================== THE KNOBS' WIRING ================================*/
/*
 * From the schematic (i2c and controls sheets). Each encoder's own pads: A and B are the two contacts,
 * E the push (all to GND when closed, pulled up on the board). The nets are named the other way round
 * from the pads (pad A -> net Kn_B, pad B -> net Kn_A) on every knob; a_pin below is the pin pad A
 * reaches, so clockwise counts up as on the bucket. KNOB_DIRECTION in CONFIG.h flips all five.
 */
static const encoder_info_t KNOB_ENCODERS[5] = {
    // K0: pad A K0_B U7 GPB6, pad B K0_A U7 GPB5, push U7 GPB4
    {.i2c_addr = MCP23017_KNOBS_ADDR, .a_register = MCP_GPIOB, .a_pin = 6, .b_register = MCP_GPIOB, .b_pin = 5,
     .button_info = {.addr = MCP23017_KNOBS_ADDR, .port = MCP_GPIOB, .pin = 4, .active_low = true}},
    // K1: pad A K1_B U7 GPB3, pad B K1_A U7 GPB2, push U7 GPB1
    {.i2c_addr = MCP23017_KNOBS_ADDR, .a_register = MCP_GPIOB, .a_pin = 3, .b_register = MCP_GPIOB, .b_pin = 2,
     .button_info = {.addr = MCP23017_KNOBS_ADDR, .port = MCP_GPIOB, .pin = 1, .active_low = true}},
    // K2: pad A K2_B U7 GPA5, pad B K2_A U7 GPA6, push U7 GPB0
    {.i2c_addr = MCP23017_KNOBS_ADDR, .a_register = MCP_GPIOA, .a_pin = 5, .b_register = MCP_GPIOA, .b_pin = 6,
     .button_info = {.addr = MCP23017_KNOBS_ADDR, .port = MCP_GPIOB, .pin = 0, .active_low = true}},
    // K3: pad A K3_B U7 GPA2, pad B K3_A U7 GPA3, push U7 GPA4
    {.i2c_addr = MCP23017_KNOBS_ADDR, .a_register = MCP_GPIOA, .a_pin = 2, .b_register = MCP_GPIOA, .b_pin = 3,
     .button_info = {.addr = MCP23017_KNOBS_ADDR, .port = MCP_GPIOA, .pin = 4, .active_low = true}},
    // K4 (screen): pad A K4_B U8 GPB0, pad B K4_A U8 GPA0, push U8 GPA6
    {.i2c_addr = MCP23017_PANEL_ADDR, .a_register = MCP_GPIOB, .a_pin = 0, .b_register = MCP_GPIOA, .b_pin = 0,
     .button_info = {.addr = MCP23017_PANEL_ADDR, .port = MCP_GPIOA, .pin = 6, .active_low = true}},
};

// Index in led_driver.c's init_devices of the ring around each aux return knob
static const uint8_t KNOB_LED_RINGS[AUX_RETURNS] = {0, 1, 2, 3};

static void init_knob (knob_info_t *knob, uint8_t index, led_device_t *ring)
{
    knob->encoder = KNOB_ENCODERS[index];
    knob->encoder.i2c_bus = CONTROLS_I2C_BUS;
    knob->encoder.button_info.bus = CONTROLS_I2C_BUS;
    knob->led_ring = ring;
}

void init_knob_controls (master_control_io_t *io)
{
    for (uint8_t ret = 0; ret < AUX_RETURNS; ret++)
    {
        init_knob(&(io->aux_return_knob[ret]), ret, led_device_at(KNOB_LED_RINGS[ret]));
    }
    init_knob(&(io->screen_knob), 4, NULL); // no ring around K4
}
