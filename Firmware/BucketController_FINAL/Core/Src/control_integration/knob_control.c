#include "CONFIG.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/i2c_driver.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"
#include "hardware_drivers/mcp23017.h"
#include "hardware_drivers/rotary_encoder.h"
#include "hardware_sets.h"
#include "hardware_structs.h"
#include "portmacro.h"
#include "stm32g474xx.h"
#include "task.h"
#include <stdbool.h>

/* Functions related to accepting some states and updating control values */

typedef struct
{
    int8_t direction; //!< +1 turned toward A, -1 turned toward B, 0 not turned
    bool held;        //!< Push button is held (fine adjustment)
    bool reset;       //!< Push button was double pressed (reset value)
} knob_input_t;

/*
 * After a double click resets a knob, the knob ignores everything (turning, holding) until
 * RESET_QUIET_MS after its button is let go, so the knob being nudged while it's pressed doesn't
 * move the value straight off its reset. Pressing it again during the quiet time restarts the wait.
 */
#define RESET_QUIET_MS     300U
#define KNOBS_PER_CHANNEL  16U // room for every knob on a channel (there are 10)

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

// One table per channel: each channel's knobs are only read by that channel's I2C task
static knob_lock_t s_knob_locks[CHANNELS][KNOBS_PER_CHANNEL];

/** The physical knob's entry, found by its push button's location. The shared knobs belong to several
 *  control sets, so the same knob can be read through different structs; this keeps one entry for it. */
static knob_lock_t *knob_lock (const encoder_info_t *info)
{
    const uint8_t channel = i2c_bus_index(info->i2c_bus);
    if (channel >= CHANNELS) return NULL;
    const button_info_t *button = &(info->button_info);
    const uint16_t key = (uint16_t)(0x8000U | ((button->addr & 0x7FU) << 8) | ((uint16_t)(button->port - MCP_GPIOA) << 4) | (button->pin & 0x0FU));

    knob_lock_t *table = s_knob_locks[channel];
    for (uint8_t i = 0; i < KNOBS_PER_CHANNEL; i++)
    {
        if (table[i].knob == key) return &table[i];
        if (table[i].knob == 0)
        {
            table[i].knob = key; // first time this knob is read
            table[i].state = KNOB_LIVE;
            return &table[i];
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
        input.direction = 1;
    }
    else if (motion == ENCODER_TURN_B)
    {
        input.direction = -1;
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
    sensitivity *= input->direction;

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
        *out += sensitivity;
    }
    xSemaphoreGive(*out_mutex);
    return true;
}

/** As apply_u_input, for a signed value */
static bool apply_s_input (
    SemaphoreHandle_t *out_mutex,
    const knob_input_t *input,
    s_scalar_control_t *out,
    s_scalar_control_t sensitivity, //!< sensitivity must be < S_SCALAR_CONTROL_MAX
    s_scalar_control_t reset_to
)
{
    if (!input->reset && input->direction == 0)
    {
        return false;
    }
    sensitivity *= input->direction;

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    if (input->reset)
    {
        *out = reset_to;
    }
    if (sensitivity > 0 && *out > S_SCALAR_CONTROL_MAX - sensitivity)
    {
        *out = S_SCALAR_CONTROL_MAX;
    }
    else if (sensitivity < 0 && *out < S_SCALAR_CONTROL_MIN - sensitivity)
    {
        *out = S_SCALAR_CONTROL_MIN;
    }
    else
    {
        *out += sensitivity;
    }
    xSemaphoreGive(*out_mutex);
    return true;
}

// Holding a knob's button makes adjustments finer
static inline s_scalar_control_t fine_if_held (const knob_input_t *input, s_scalar_control_t sensitivity)
{
    return input->held ? sensitivity / 2 : sensitivity;
}

static bool update_u_value_from_encoder (
    SemaphoreHandle_t *out_mutex,
    const encoder_info_t *info,
    u_scalar_control_t *out,
    s_scalar_control_t sensitivity,
    u_scalar_control_t reset_to
)
{
    const knob_input_t input = read_knob(info);
    return apply_u_input(out_mutex, &input, out, fine_if_held(&input, sensitivity), reset_to);
}

static bool update_s_value_from_encoder (
    SemaphoreHandle_t *out_mutex,
    const encoder_info_t *info,
    s_scalar_control_t *out,
    s_scalar_control_t sensitivity,
    s_scalar_control_t reset_to
)
{
    const knob_input_t input = read_knob(info);
    return apply_s_input(out_mutex, &input, out, fine_if_held(&input, sensitivity), reset_to);
}

/*=============================== LAST CHANGED VALUE ================================*/
/* For the screens' value pop-up. Written by the channel's I2C task, read by the display task:
   the kind and band go in one byte, written before the time. */

static volatile uint8_t s_touched_what[CHANNELS];  // touched_param_t << 4 | band
static volatile uint32_t s_touched_tick[CHANNELS];

static void touched (uint8_t channel, touched_param_t param, uint8_t band)
{
    if (channel >= CHANNELS) return;
    s_touched_what[channel] = (uint8_t)((param << 4) | (band & 0x0FU));
    s_touched_tick[channel] = (uint32_t)xTaskGetTickCount();
}

touched_t control_last_touched (uint8_t channel)
{
    touched_t t = {TOUCHED_NOTHING, 0, 0};
    if (channel >= CHANNELS) return t;
    const uint8_t what = s_touched_what[channel];
    t.tick = s_touched_tick[channel];
    t.param = (touched_param_t)(what >> 4);
    t.band = what & 0x0FU;
    return t;
}

/*
 * Each channel has 10 physical knobs: input gain (0), pan (9), and 8 shared knobs (1-8) whose function depends on the
 * selected control set. Each physical knob must only be read once per update, so only the knobs of the selected set are read.
 */

/*
 * LF and HF shelf/bell (and the compressor's peak/RMS on THRESH): holding the knob down for MODE_HOLD_MS switches
 * it. Its ring then flashes fully MODE_FLASHES times to confirm. A slight turn while held (under 1/20 of the
 * range) still switches, and is undone; a bigger turn is fine adjustment and that hold doesn't switch. One switch
 * per hold; let go and hold again to switch back.
 */
#define MODE_HOLD_MS         2000U
#define MODE_FLASHES         3U
#define MODE_FLASH_PERIOD_MS 300U // on for half, off for half

enum
{
    MODE_LF,
    MODE_HF,
    MODE_RMS,   // compressor THRESH knob: peak/RMS detector
    MODE_BANDS
};

typedef struct
{
    TickType_t since;    //!< when the current press started
    bool pressed;        //!< the button was down at the last update
    bool spent;          //!< this press turned the knob too far or already switched: no switch until let go
    int32_t start_value; //!< the knob's value when the press started
} mode_hold_t;

/* A press still counts as a hold while the knob stays within this of where it started (1/20 of its range),
   so a slight accidental turn doesn't stop the switch. When the switch happens, the knob's value is put
   back where it was, undoing the nudge. */
#define MODE_HOLD_TOLERANCE (65536 / 20)

typedef struct
{
    volatile TickType_t start;
    volatile bool active;
} mode_flash_t;

static mode_hold_t s_mode_hold[CHANNELS][MODE_BANDS];   // read and written by the channel's I2C task
static mode_flash_t s_mode_flash[CHANNELS][MODE_BANDS]; // set by the I2C task, shown by the LED task

static uint8_t channel_of (const channel_controls *vals)
{
    return (uint8_t)(vals - channel_control_vals);
}

/** The knob's value, whichever kind it is, as a plain number for the hold tolerance */
static int32_t knob_value (const s_scalar_control_t *s_value, const u_scalar_control_t *u_value)
{
    return s_value != NULL ? (int32_t)*s_value : (int32_t)*u_value;
}

/** Call after the knob's turn has been applied. The knob's value is s_value or u_value (the other NULL). */
static void check_mode_hold (
    uint8_t channel,
    uint8_t band,
    const knob_input_t *input,
    SemaphoreHandle_t *vals_mutex,
    bool *flag,
    s_scalar_control_t *s_value,
    u_scalar_control_t *u_value
)
{
    mode_hold_t *hold = &(s_mode_hold[channel][band]);
    if (!input->held)
    {
        hold->pressed = false;
        return;
    }

    const TickType_t now = xTaskGetTickCount();
    const int32_t value = knob_value(s_value, u_value); // only this task writes it
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
        *flag = !*flag;
        if (s_value != NULL) *s_value = (s_scalar_control_t)hold->start_value; // undo the accidental nudge
        else *u_value = (u_scalar_control_t)hold->start_value;
        xSemaphoreGive(*vals_mutex);
        s_mode_flash[channel][band].start = now;
        s_mode_flash[channel][band].active = true;
        hold->spent = true;
    }
}

static void update_send_knob_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io,
    s_scalar_control_t sensitivity
)
{
    for (uint8_t i = 0; i < SEND_CHANNELS; i++)
    {
        send_channel_interface_t *send_io = &(io->send_channel_interfaces[i]);
        send_channel_control_t *send_vals = &(vals->send_controls[i]);

        const send_channel_control_t *start = &(control_defaults()->send_controls[i]);
        if (update_u_value_from_encoder(vals_mutex, &(send_io->gain_knob.encoder), &(send_vals->gain), sensitivity, start->gain))
        {
            touched(channel_of(vals), TOUCHED_SEND_LEVEL, i);
        }
        if (i < SEND_CHANNELS - 1) // pan is not used for the last send channel
        {
            if (update_s_value_from_encoder(vals_mutex, &(send_io->pan_knob.encoder), &(send_vals->pan), sensitivity, start->pan) &&
                i < 2) // only sends 1 and 2 are stereo; send 3's pan does nothing in the hardware
            {
                touched(channel_of(vals), TOUCHED_SEND_PAN, i);
            }
        }
    }
}

/**
 * @brief Update an EQ band's values. Bands with a q knob share one physical knob between gain and q:
 * turning it adjusts gain, or q while its button is held (so there is no fine adjustment on that knob).
 * Double clicking the gain knob resets gain, on every band; no click resets q.
 * mode_band (MODE_LF / MODE_HF) is for the bands without q: holding their gain knob switches shelf/bell.
 */
static void update_eq_band_knob_values (
    SemaphoreHandle_t *vals_mutex,
    eq_band_control_t *vals,
    eq_band_interface_t *band_io,
    bool has_q,
    s_scalar_control_t sensitivity,
    uint8_t channel,
    uint8_t mode_band,
    uint8_t band,                    //!< for the value pop-up: 0 LF, 1 LMF, 2 HMF, 3 HF
    const eq_band_control_t *start   //!< startup values, which resets go back to
)
{
    knob_input_t input = read_knob(&(band_io->gain_knob.encoder)); // q_knob is the same hardware

    if (has_q)
    {
        if (input.reset)
        {
            // the second click holds the button, which would otherwise make this a q reset
            const knob_input_t reset_gain = {.reset = true};
            apply_s_input(vals_mutex, &reset_gain, &(vals->gain), sensitivity, start->gain);
            touched(channel, TOUCHED_EQ_GAIN, band);
            input.reset = false;
        }
        if (input.held)
        {
            if (apply_u_input(vals_mutex, &input, &(vals->q), sensitivity, start->q)) touched(channel, TOUCHED_EQ_Q, band);
        }
        else
        {
            if (apply_s_input(vals_mutex, &input, &(vals->gain), sensitivity, start->gain)) touched(channel, TOUCHED_EQ_GAIN, band);
        }
    }
    else
    {
        if (apply_s_input(vals_mutex, &input, &(vals->gain), fine_if_held(&input, sensitivity), start->gain))
        {
            touched(channel, TOUCHED_EQ_GAIN, band);
        }
        check_mode_hold(channel, mode_band, &input, vals_mutex, &(vals->bell), &(vals->gain), NULL);
    }

    if (update_u_value_from_encoder(vals_mutex, &(band_io->freq_knob.encoder), &(vals->freq), sensitivity, start->freq))
    {
        touched(channel, TOUCHED_EQ_FREQ, band);
    }
}

static void update_comp_knob_values (
    SemaphoreHandle_t *vals_mutex,
    comp_control_t *vals,
    comp_interface_t *comp_io,
    s_scalar_control_t sensitivity,
    uint8_t channel
)
{
    const comp_control_t *start = &(control_defaults()->comp_control);
    if (update_u_value_from_encoder(vals_mutex, &(comp_io->in_gain_knob.encoder), &(vals->in_gain), sensitivity, start->in_gain))
    {
        touched(channel, TOUCHED_COMP_INPUT, 0);
    }
    if (update_u_value_from_encoder(vals_mutex, &(comp_io->attack_time_knob.encoder), &(vals->attack_time), sensitivity, start->attack_time))
    {
        touched(channel, TOUCHED_COMP_ATTACK, 0);
    }

    // THRESH: turning sets it; holding it 2 s (without turning) switches the detector between peak and RMS
    const knob_input_t thresh = read_knob(&(comp_io->threshold_knob.encoder));
    if (apply_u_input(vals_mutex, &thresh, &(vals->threshold), fine_if_held(&thresh, sensitivity), start->threshold))
    {
        touched(channel, TOUCHED_COMP_THRESH, 0);
    }
    const bool was_rms = vals->rms; // only this task writes it
    check_mode_hold(channel, MODE_RMS, &thresh, vals_mutex, &(vals->rms), NULL, &(vals->threshold));
    if (vals->rms != was_rms) touched(channel, TOUCHED_COMP_MODE, 0);

    if (update_u_value_from_encoder(vals_mutex, &(comp_io->release_time_knob.encoder), &(vals->release_time), sensitivity, start->release_time))
    {
        touched(channel, TOUCHED_COMP_RELEASE, 0);
    }
    if (update_u_value_from_encoder(vals_mutex, &(comp_io->out_gain_knob.encoder), &(vals->out_gain), sensitivity, start->out_gain))
    {
        touched(channel, TOUCHED_COMP_OUTPUT, 0);
    }
    if (update_u_value_from_encoder(vals_mutex, &(comp_io->ratio_knob.encoder), &(vals->ratio), sensitivity, start->ratio))
    {
        touched(channel, TOUCHED_COMP_RATIO, 0);
    }
}

void update_channel_knob_values (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io
)
{
    const s_scalar_control_t sensitivity = S_SCALAR_CONTROL_MAX / 32;

    const channel_controls *start = control_defaults();
    const uint8_t channel = channel_of(vals);

    // Always active, regardless of the selected control set
    if (update_u_value_from_encoder(vals_mutex, &(io->input_gain_knob.encoder), &(vals->input_gain), sensitivity, start->input_gain))
    {
        touched(channel, TOUCHED_INPUT_GAIN, 0);
    }
    if (update_s_value_from_encoder(vals_mutex, &(io->pan.encoder), &(vals->output_pan), sensitivity, start->output_pan))
    {
        touched(channel, TOUCHED_PAN, 0);
    }

    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    const channel_control_set_selection_t selection = vals->control_set_selection;
    xSemaphoreGive(*vals_mutex);

    switch (selection)
    {
        case SENDS_CONTROL:
            update_send_knob_values(vals_mutex, vals, io, sensitivity);
            break;
        case EQ_CONTROL:
            update_eq_band_knob_values(vals_mutex, &(vals->hf_control), &(io->hf_interface), false, sensitivity, channel, MODE_HF, 3, &(start->hf_control));
            update_eq_band_knob_values(vals_mutex, &(vals->hmf_control), &(io->hmf_interface), true, sensitivity, channel, 0, 2, &(start->hmf_control));
            update_eq_band_knob_values(vals_mutex, &(vals->lmf_control), &(io->lmf_interface), true, sensitivity, channel, 0, 1, &(start->lmf_control));
            update_eq_band_knob_values(vals_mutex, &(vals->lf_control), &(io->lf_interface), false, sensitivity, channel, MODE_LF, 0, &(start->lf_control));
            break;
        case COMP_CONTROL:
            update_comp_knob_values(vals_mutex, &(vals->comp_control), &(io->comp_interface), sensitivity, channel);
            break;
    }
}


/*=============================== LEDS ================================*/

// The shared knobs' rings are registered as centered (signed), but some of their parameters are unsigned
static void show_unsigned (led_device_t *ring, u_scalar_control_t value)
{
    if (ring == NULL)
    {
        return;
    }
    if (ring->scale != SCALE_LEFT)
    {
        led_set_scale(ring, SCALE_LEFT);
    }
    led_set(ring, value);
}

static void show_signed (led_device_t *ring, s_scalar_control_t value)
{
    if (ring == NULL)
    {
        return;
    }
    if (ring->scale != SCALE_CENTER)
    {
        led_set_scale(ring, SCALE_CENTER);
    }
    led_set_signed(ring, value);
}

static void hide (led_device_t *ring) { show_unsigned(ring, 0); }

/* Holding the EQ button resets the whole EQ (button_control.c); every EQ ring then flashes like this */
static mode_flash_t s_eq_reset_flash[CHANNELS];

void knob_flash_eq_rings (uint8_t channel)
{
    if (channel >= CHANNELS) return;
    s_eq_reset_flash[channel].start = xTaskGetTickCount();
    s_eq_reset_flash[channel].active = true;
}

static mode_flash_t s_send_reset_flash[CHANNELS];
static mode_flash_t s_comp_reset_flash[CHANNELS];

void knob_flash_comp_rings (uint8_t channel)
{
    if (channel >= CHANNELS) return;
    s_comp_reset_flash[channel].start = xTaskGetTickCount();
    s_comp_reset_flash[channel].active = true;
}

void knob_flash_send_rings (uint8_t channel)
{
    if (channel >= CHANNELS) return;
    s_send_reset_flash[channel].start = xTaskGetTickCount();
    s_send_reset_flash[channel].active = true;
}

/** Where a flash is in its on/off cycle: 1 = lit, 0 = dark, -1 = not flashing (or just finished) */
static int8_t flash_phase (mode_flash_t *flash)
{
    if (!flash->active) return -1;
    const TickType_t elapsed = xTaskGetTickCount() - flash->start;
    const TickType_t period = pdMS_TO_TICKS(MODE_FLASH_PERIOD_MS);
    if (elapsed >= MODE_FLASHES * period)
    {
        flash->active = false;
        return -1;
    }
    return (elapsed % period < period / 2) ? 1 : 0;
}

/** While a band's shelf/bell flash runs, its gain ring blinks fully on and off. Returns false once it's over. */
static bool show_mode_flash (uint8_t channel, uint8_t band, led_device_t *ring)
{
    mode_flash_t *flash = &(s_mode_flash[channel][band]);
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
    if (elapsed % period < period / 2)
    {
        show_unsigned(ring, U_SCALAR_CONTROL_MAX);
    }
    else
    {
        show_unsigned(ring, 0);
    }
    return true;
}

static void show_eq_band (const eq_band_control_t *vals, eq_band_interface_t *band_io, bool has_q)
{
    // Bands with a q knob show q on the shared ring while its button is held
    if (has_q && get_button_held(&(band_io->gain_knob.encoder.button_info)))
    {
        show_unsigned(band_io->q_knob.led_ring, vals->q);
    }
    else
    {
        show_signed(band_io->gain_knob.led_ring, vals->gain);
    }
    show_unsigned(band_io->freq_knob.led_ring, vals->freq);
}

void update_channel_knob_leds (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io
)
{
    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    const channel_controls v = *vals;
    xSemaphoreGive(*vals_mutex);

    show_unsigned(io->input_gain_knob.led_ring, v.input_gain);
    show_signed(io->pan.led_ring, v.output_pan);

    // Physical knobs 7 and 8 are the LF band's gain and freq knobs; the other sets leave them unused
    switch (v.control_set_selection)
    {
        case SENDS_CONTROL:
        {
            // the sends were just reset: every sends ring flashes instead
            const int8_t phase = flash_phase(&s_send_reset_flash[channel_of(vals)]);
            for (uint8_t i = 0; i < SEND_CHANNELS; i++)
            {
                led_device_t *gain_ring = io->send_channel_interfaces[i].gain_knob.led_ring;
                led_device_t *pan_ring = io->send_channel_interfaces[i].pan_knob.led_ring;
                if (phase >= 0)
                {
                    show_unsigned(gain_ring, phase ? U_SCALAR_CONTROL_MAX : 0);
                    if (i < SEND_CHANNELS - 1) show_unsigned(pan_ring, phase ? U_SCALAR_CONTROL_MAX : 0);
                    continue;
                }
                show_unsigned(gain_ring, v.send_controls[i].gain);
                if (i < SEND_CHANNELS - 1) // pan is not used for the last send channel
                {
                    show_signed(pan_ring, v.send_controls[i].pan);
                }
            }
            hide(io->lf_interface.freq_knob.led_ring);
            break;
        }
        case EQ_CONTROL:
        {
            const uint8_t channel = channel_of(vals);
            show_eq_band(&(v.hf_control), &(io->hf_interface), false);
            show_eq_band(&(v.hmf_control), &(io->hmf_interface), true);
            show_eq_band(&(v.lmf_control), &(io->lmf_interface), true);
            show_eq_band(&(v.lf_control), &(io->lf_interface), false);
            // a shelf/bell switch just happened: flash the gain ring over the normal display
            show_mode_flash(channel, MODE_HF, io->hf_interface.gain_knob.led_ring);
            show_mode_flash(channel, MODE_LF, io->lf_interface.gain_knob.led_ring);
            // the whole EQ was just reset: flash every EQ ring
            {
                const int8_t phase = flash_phase(&s_eq_reset_flash[channel]);
                if (phase >= 0)
                {
                    eq_band_interface_t *bands[4] = {&(io->hf_interface), &(io->hmf_interface), &(io->lmf_interface), &(io->lf_interface)};
                    for (uint8_t b = 0; b < 4; b++)
                    {
                        show_unsigned(bands[b]->gain_knob.led_ring, phase ? U_SCALAR_CONTROL_MAX : 0);
                        show_unsigned(bands[b]->freq_knob.led_ring, phase ? U_SCALAR_CONTROL_MAX : 0);
                    }
                }
            }
            break;
        }
        case COMP_CONTROL:
        {
            const uint8_t channel = channel_of(vals);
            show_unsigned(io->comp_interface.in_gain_knob.led_ring, v.comp_control.in_gain);
            show_unsigned(io->comp_interface.threshold_knob.led_ring, v.comp_control.threshold);
            show_unsigned(io->comp_interface.attack_time_knob.led_ring, v.comp_control.attack_time);
            show_unsigned(io->comp_interface.release_time_knob.led_ring, v.comp_control.release_time);
            show_unsigned(io->comp_interface.ratio_knob.led_ring, v.comp_control.ratio);
            show_unsigned(io->comp_interface.out_gain_knob.led_ring, v.comp_control.out_gain);
            hide(io->lf_interface.gain_knob.led_ring);
            hide(io->lf_interface.freq_knob.led_ring);
            // peak/RMS just switched: the THRESH ring flashes over its normal display
            show_mode_flash(channel, MODE_RMS, io->comp_interface.threshold_knob.led_ring);
            // the compressor was just reset: its six rings flash
            const int8_t phase = flash_phase(&s_comp_reset_flash[channel]);
            if (phase >= 0)
            {
                led_device_t *rings[6] = {
                    io->comp_interface.in_gain_knob.led_ring, io->comp_interface.attack_time_knob.led_ring,
                    io->comp_interface.threshold_knob.led_ring, io->comp_interface.release_time_knob.led_ring,
                    io->comp_interface.out_gain_knob.led_ring, io->comp_interface.ratio_knob.led_ring,
                };
                for (uint8_t r = 0; r < 6; r++) show_unsigned(rings[r], phase ? U_SCALAR_CONTROL_MAX : 0);
            }
            break;
        }
    }
}


static const button_info_t KNOB_BUTTONS[10] = {
    {.addr = 0x20, .port = MCP_GPIOA, .pin = 5},
    {.addr = 0x20, .port = MCP_GPIOA, .pin = 6},
    {.addr = 0x20, .port = MCP_GPIOB, .pin = 5},
    {.addr = 0x20, .port = MCP_GPIOB, .pin = 6},
    {.addr = 0x21, .port = MCP_GPIOA, .pin = 5},
    {.addr = 0x21, .port = MCP_GPIOA, .pin = 6},
    {.addr = 0x21, .port = MCP_GPIOB, .pin = 5},
    {.addr = 0x21, .port = MCP_GPIOB, .pin = 6},
    {.addr = 0x22, .port = MCP_GPIOA, .pin = 0},
    {.addr = 0x22, .port = MCP_GPIOA, .pin = 1}
};

static const encoder_info_t KNOB_ENCODERS[10] = {
    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 0,
     .b_register = MCP_GPIOA,
     .b_pin = 0,
     .button_info = KNOB_BUTTONS[0]},

    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 1,
     .b_register = MCP_GPIOA,
     .b_pin = 1,
     .button_info = KNOB_BUTTONS[1]},

    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 2,
     .b_register = MCP_GPIOA,
     .b_pin = 2,
     .button_info = KNOB_BUTTONS[2]},

    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 3,
     .b_register = MCP_GPIOA,
     .b_pin = 3,
     .button_info = KNOB_BUTTONS[3]},

    {.i2c_addr = 0x20,
     .a_register = MCP_GPIOB,
     .a_pin = 4,
     .b_register = MCP_GPIOA,
     .b_pin = 4,
     .button_info = KNOB_BUTTONS[4]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 0,
     .b_register = MCP_GPIOA,
     .b_pin = 0,
     .button_info = KNOB_BUTTONS[5]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 1,
     .b_register = MCP_GPIOA,
     .b_pin = 1,
     .button_info = KNOB_BUTTONS[6]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 2,
     .b_register = MCP_GPIOA,
     .b_pin = 2,
     .button_info = KNOB_BUTTONS[7]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 3,
     .b_register = MCP_GPIOA,
     .b_pin = 3,
     .button_info = KNOB_BUTTONS[8]},

    {.i2c_addr = 0x21,
     .a_register = MCP_GPIOB,
     .a_pin = 4,
     .b_register = MCP_GPIOA,
     .b_pin = 4,
     .button_info = KNOB_BUTTONS[9]}
};


// Index in led_driver.c's init_devices of the ring around each knob, by [channel][knob index in KNOB_ENCODERS].
// Knob 0 is input gain, 1-8 are the FX section rows 1A, 1B, 2A, 2B, 3A, 3B, 4A, 4B, and 9 is pan.
static const uint8_t KNOB_LED_RINGS[CHANNELS][10] = {
    {0, 39, 40, 47, 48, 55, 56, 95, 96, 103},
    {17, 38, 41, 46, 49, 54, 57, 78, 97, 102},
    {18, 37, 42, 45, 50, 53, 58, 77, 98, 101},
    {35, 36, 43, 44, 51, 52, 59, 60, 99, 100},
};

static void init_knob (uint8_t channel, knob_info_t *knob, uint8_t index)
{
    I2C_TypeDef *bus = i2c_channel_bus(channel);

    knob->encoder = KNOB_ENCODERS[index];
    knob->encoder.i2c_bus = bus;
    knob->encoder.button_info.bus = bus;
    knob->encoder.button_info.active_low = true;

    knob->led_ring = led_device_at(KNOB_LED_RINGS[channel][index]);
}


void init_knob_controls (uint8_t channel, channel_control_io_t *io)
{
    // INPUT AND PAN KNOB
    init_knob(channel, &(io->input_gain_knob), 0);
    init_knob(channel, &(io->pan), 9);

    // SENDS
    init_knob(channel, &(io->send_channel_interfaces[0].gain_knob), 1);
    init_knob(channel, &(io->send_channel_interfaces[0].pan_knob), 2);

    init_knob(channel, &(io->send_channel_interfaces[1].gain_knob), 3);
    init_knob(channel, &(io->send_channel_interfaces[1].pan_knob), 4);

    init_knob(channel, &(io->send_channel_interfaces[2].gain_knob), 5);
    init_knob(channel, &(io->send_channel_interfaces[2].pan_knob), 6);

    init_knob(channel, &(io->send_channel_interfaces[3].gain_knob), 7);

    // EQ HF
    init_knob(channel, &(io->hf_interface.gain_knob), 1);
    init_knob(channel, &(io->hf_interface.freq_knob), 2);

    // EQ HMF
    init_knob(channel, &(io->hmf_interface.gain_knob), 3);
    init_knob(channel, &(io->hmf_interface.freq_knob), 4);
    init_knob(channel, &(io->hmf_interface.q_knob), 3);

    // EQ LMF
    init_knob(channel, &(io->lmf_interface.gain_knob), 5);
    init_knob(channel, &(io->lmf_interface.freq_knob), 6);
    init_knob(channel, &(io->lmf_interface.q_knob), 5);

    // EQ LF
    init_knob(channel, &(io->lf_interface.gain_knob), 7);
    init_knob(channel, &(io->lf_interface.freq_knob), 8);

    // COMP, as printed on the faceplate (K7 and K8 unused)
    init_knob(channel, &(io->comp_interface.in_gain_knob), 1);      // K1 INPUT
    init_knob(channel, &(io->comp_interface.attack_time_knob), 2);  // K2 ATK
    init_knob(channel, &(io->comp_interface.threshold_knob), 3);    // K3 THRESH
    init_knob(channel, &(io->comp_interface.release_time_knob), 4); // K4 REL
    init_knob(channel, &(io->comp_interface.out_gain_knob), 5);     // K5 OUTPUT
    init_knob(channel, &(io->comp_interface.ratio_knob), 6);        // K6 RATIO
}
