#include "button_control.h"
#include "knob_control.h"
#include "FreeRTOS.h"
#include "task.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/i2c_driver.h"
#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"
#include "hardware_sets.h"
#include "portmacro.h"

static void update_toggle_button_val_from_info (
    SemaphoreHandle_t *out_mutex,
    const button_info_t *info,
    bool *val_out //
)
{
    if (update_button(info) == BUTTON_NO_EVENT)
    {
        return;
    }

    xSemaphoreTake(*out_mutex, portMAX_DELAY);
    *val_out = !*val_out;
    xSemaphoreGive(*out_mutex);
}

/*
 * The EQ and SENDS buttons, besides selecting what the knobs control:
 *  - held for RESET_HOLD_MS: every setting of that section goes back to its startup value (EQ: all
 *    four bands' gain, frequency and Q, and LF/HF back to shelves; SENDS: all four sends' level and
 *    pan), and its rings flash. Once per hold.
 *  - double tapped: that section is bypassed (EQ flat; sends at their lowest gain) without changing
 *    its settings, and its button flashes until it's double tapped again. The knobs still change the
 *    settings while it's bypassed; they're heard once the bypass is lifted.
 */
#define RESET_HOLD_MS       2000U
#define BYPASS_FLASH_MS     250U  // a bypassed section's button: on this long, then off this long

typedef struct
{
    TickType_t since; //!< when the current press started
    bool pressed;
    bool done;        //!< already reset during this press
} hold_t;

static hold_t s_eq_hold[CHANNELS];
static hold_t s_sends_hold[CHANNELS];

static uint8_t channel_of (const channel_controls *vals)
{
    return (uint8_t)(vals - channel_control_vals);
}

/** True once per press, when the button has been held RESET_HOLD_MS */
static bool held_long_enough (hold_t *hold, const button_info_t *button)
{
    if (!get_button_held(button))
    {
        hold->pressed = false;
        return false;
    }
    const TickType_t now = xTaskGetTickCount();
    if (!hold->pressed)
    {
        hold->pressed = true;
        hold->since = now;
        hold->done = false;
    }
    if (!hold->done && (now - hold->since) >= pdMS_TO_TICKS(RESET_HOLD_MS))
    {
        hold->done = true;
        return true;
    }
    return false;
}

static void check_section_buttons (SemaphoreHandle_t *vals_mutex, channel_controls *vals, channel_control_io_t *io)
{
    const uint8_t channel = channel_of(vals);
    if (channel >= CHANNELS) return;
    const channel_controls *start = control_defaults();

    if (held_long_enough(&s_eq_hold[channel], &(io->eq_button.button)))
    {
        xSemaphoreTake(*vals_mutex, portMAX_DELAY);
        vals->hf_control = start->hf_control;
        vals->hmf_control = start->hmf_control;
        vals->lmf_control = start->lmf_control;
        vals->lf_control = start->lf_control;
        xSemaphoreGive(*vals_mutex);
        knob_flash_eq_rings(channel);
    }
    if (held_long_enough(&s_sends_hold[channel], &(io->send_button.button)))
    {
        xSemaphoreTake(*vals_mutex, portMAX_DELAY);
        for (uint8_t i = 0; i < SEND_CHANNELS; i++) vals->send_controls[i] = start->send_controls[i];
        xSemaphoreGive(*vals_mutex);
        knob_flash_send_rings(channel);
    }

    // double taps toggle the bypasses (each button is read for presses exactly once per update, here)
    const bool eq_double = update_button(&(io->eq_button.button)) == BUTTON_DOUBLE_PRESSED;
    const bool sends_double = update_button(&(io->send_button.button)) == BUTTON_DOUBLE_PRESSED;
    if (eq_double || sends_double)
    {
        xSemaphoreTake(*vals_mutex, portMAX_DELAY);
        if (eq_double) vals->eq_off = !vals->eq_off;
        if (sends_double) vals->sends_off = !vals->sends_off;
        xSemaphoreGive(*vals_mutex);
    }
}

/*
 * +48 V: the button must be held PHANTOM_HOLD_MS to turn it on, and only on the mic input. A press turns
 * it off straight away (and that same press can't turn it back on). It also goes off whenever the input
 * isn't mic (see update_input_select_button). Whether the routing board actually switches it is up to
 * CV_ENABLE_PHANTOM in cv_mapping.c.
 */
#define PHANTOM_HOLD_MS 2000U

static hold_t s_phantom_hold[CHANNELS];

static void update_phantom_button (SemaphoreHandle_t *vals_mutex, channel_controls *vals, channel_control_io_t *io)
{
    const uint8_t channel = channel_of(vals);
    if (channel >= CHANNELS) return;
    hold_t *hold = &s_phantom_hold[channel];
    const button_info_t *button = &(io->phantom_48v_button.button);

    const bool new_press = update_button(button) != BUTTON_NO_EVENT;
    const bool held = get_button_held(button);
    const TickType_t now = xTaskGetTickCount();

    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    const bool mic = vals->input_type_selection == MIC_INPUT;
    if (!mic) vals->phantom_48v = false;

    if (new_press)
    {
        hold->pressed = true;
        hold->since = now;
        hold->done = false;
        if (vals->phantom_48v)
        {
            vals->phantom_48v = false; // off at once
            hold->done = true;         // and this press can't turn it on again
        }
    }
    if (!held)
    {
        hold->pressed = false;
    }
    else if (hold->pressed && !hold->done && mic && (now - hold->since) >= pdMS_TO_TICKS(PHANTOM_HOLD_MS))
    {
        vals->phantom_48v = true;
        hold->done = true;
    }
    xSemaphoreGive(*vals_mutex);
}

static void update_radio_buttons (SemaphoreHandle_t *vals_mutex, channel_controls *vals, channel_control_io_t *io)
{
    channel_control_set_selection_t selection;
    if (get_button_held(&(io->send_button.button)))
    {
        selection = SENDS_CONTROL;
    }
    else if (get_button_held(&(io->eq_button.button)))
    {
        selection = EQ_CONTROL;
    }
    else if (get_button_held(&(io->comp_button.button)))
    {
        selection = COMP_CONTROL;
    }
    else
    {
        return;
    }

    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    vals->control_set_selection = selection;
    xSemaphoreGive(*vals_mutex);
}

/*
 * The SEL buttons are one set of radio buttons across the bucket's channels: pressing one selects that
 * channel and deselects the others. Pressing the selected channel's button again keeps it selected.
 * s_select_lock makes each change happen in one go, even if two buttons are pressed at the same moment.
 */
static StaticSemaphore_t s_select_lock_buffer;
static SemaphoreHandle_t s_select_lock;

static void update_select_button (const channel_controls *vals, channel_control_io_t *io)
{
    if (update_button(&(io->sel_button.button)) == BUTTON_NO_EVENT)
    {
        return;
    }

    xSemaphoreTake(s_select_lock, portMAX_DELAY);
    for (uint8_t channel = 0; channel < CHANNELS; channel++)
    {
        xSemaphoreTake(channel_control_mutexes[channel], portMAX_DELAY);
        channel_control_vals[channel].sel = (&channel_control_vals[channel] == vals);
        xSemaphoreGive(channel_control_mutexes[channel]);
    }
    xSemaphoreGive(s_select_lock);
}

static void update_input_select_button (SemaphoreHandle_t *vals_mutex, channel_controls *vals, channel_control_io_t *io)
{
    if (update_button(&(io->input_selection.input_select_button)) == BUTTON_NO_EVENT)
    {
        return;
    }

    // Cycles in the order the LEDs are laid out on the board: line, mic, hi-z
    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    switch (vals->input_type_selection)
    {
    case LINE_INPUT:
        vals->input_type_selection = MIC_INPUT;
        break;
    case MIC_INPUT:
        vals->input_type_selection = HIZ_INPUT;
        break;
    default:
        vals->input_type_selection = LINE_INPUT;
        break;
    }
    if (vals->input_type_selection != MIC_INPUT)
    {
        vals->phantom_48v = false; // +48 V only on the mic input
    }
    xSemaphoreGive(*vals_mutex);
}

// The input type cycle button's own LED (lit while it's held down)
static const uint8_t INPUT_CYCLE_BUTTON_LEDS[CHANNELS] = {4, 12, 22, 30};
static led_device_t *s_input_cycle_led[CHANNELS];

static const uint8_t INPUT_SELECT_LEDS[CHANNELS][3] = {
    {1, 2, 3}, // line, mic, hi-z
    {9, 10, 11},
    {19, 20, 21},
    {27, 28, 29}
};

// LED device indexes (see init_devices in led_driver.c) for:
// phase, phantom, HPF, eq, comp, sends, solo, mute, sel, insert, insert pre
static const uint8_t TOGGLE_BUTTON_LEDS[CHANNELS][11] = {
    {5, 6, 7, 8, 79, 80, 81, 82, 83, 84, 85},
    {13, 14, 15, 16, 87, 88, 89, 90, 91, 92, 93},
    {23, 24, 25, 26, 61, 62, 63, 64, 65, 66, 67},
    {31, 32, 33, 34, 69, 70, 71, 72, 73, 74, 75}
};

// Same MCP23017 locations on every channel's bus (all on the 0x22 expander). Buttons are debounced in hardware and
// pull the pin low while held.
static const button_info_t BUTTONS[12] = {
    {.addr = 0x22, .port = MCP_GPIOA, .pin = 2, .active_low = true}, // input select
    {.addr = 0x22, .port = MCP_GPIOA, .pin = 3, .active_low = true}, // phase flip
    {.addr = 0x22, .port = MCP_GPIOA, .pin = 4, .active_low = true}, // phantom 48v
    {.addr = 0x22, .port = MCP_GPIOA, .pin = 5, .active_low = true}, // high pass filter
    {.addr = 0x22, .port = MCP_GPIOB, .pin = 1, .active_low = true}, // eq
    {.addr = 0x22, .port = MCP_GPIOB, .pin = 0, .active_low = true}, // comp
    {.addr = 0x22, .port = MCP_GPIOA, .pin = 6, .active_low = true}, // sends
    {.addr = 0x22, .port = MCP_GPIOB, .pin = 2, .active_low = true}, // solo
    {.addr = 0x22, .port = MCP_GPIOB, .pin = 3, .active_low = true}, // mute
    {.addr = 0x22, .port = MCP_GPIOB, .pin = 4, .active_low = true}, // sel
    {.addr = 0x22, .port = MCP_GPIOB, .pin = 5, .active_low = true}, // insert (on the fader card connector)
    {.addr = 0x22, .port = MCP_GPIOB, .pin = 6, .active_low = true}  // insert pre (on the fader card connector)
};

void init_button_controls (uint8_t channel, channel_control_io_t *io)
{
    if (s_select_lock == NULL)
    {
        s_select_lock = xSemaphoreCreateMutexStatic(&s_select_lock_buffer);
    }

    I2C_TypeDef *bus = i2c_channel_bus(channel);

    io->input_selection.input_select_button = BUTTONS[0];
    io->input_selection.input_select_button.bus = bus;
    io->input_selection.line_input_led = led_device_at(INPUT_SELECT_LEDS[channel][0]);
    io->input_selection.mic_input_led = led_device_at(INPUT_SELECT_LEDS[channel][1]);
    io->input_selection.hiz_input_led = led_device_at(INPUT_SELECT_LEDS[channel][2]);
    s_input_cycle_led[channel] = led_device_at(INPUT_CYCLE_BUTTON_LEDS[channel]);

    io->phase_flip_button.button = BUTTONS[1];
    io->phase_flip_button.button.bus = bus;
    io->phase_flip_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][0]);

    io->phantom_48v_button.button = BUTTONS[2];
    io->phantom_48v_button.button.bus = bus;
    io->phantom_48v_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][1]);

    io->high_pass_filter_button.button = BUTTONS[3];
    io->high_pass_filter_button.button.bus = bus;
    io->high_pass_filter_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][2]);

    io->eq_button.button = BUTTONS[4];
    io->eq_button.button.bus = bus;
    io->eq_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][3]);

    io->comp_button.button = BUTTONS[5];
    io->comp_button.button.bus = bus;
    io->comp_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][4]);

    io->send_button.button = BUTTONS[6];
    io->send_button.button.bus = bus;
    io->send_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][5]);

    io->solo_button.button = BUTTONS[7];
    io->solo_button.button.bus = bus;
    io->solo_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][6]);

    io->mute_button.button = BUTTONS[8];
    io->mute_button.button.bus = bus;
    io->mute_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][7]);

    io->sel_button.button = BUTTONS[9];
    io->sel_button.button.bus = bus;
    io->sel_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][8]);

    io->ins_button.button = BUTTONS[10];
    io->ins_button.button.bus = bus;
    io->ins_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][9]);

    io->pre_button.button = BUTTONS[11];
    io->pre_button.button.bus = bus;
    io->pre_button.led = led_device_at(TOGGLE_BUTTON_LEDS[channel][10]);
}

void update_channel_button_leds (
    SemaphoreHandle_t *vals_mutex,
    channel_controls *vals,
    channel_control_io_t *io //
)
{
    xSemaphoreTake(*vals_mutex, portMAX_DELAY);
    const channel_controls v = *vals;
    xSemaphoreGive(*vals_mutex);

    led_set_bool(io->input_selection.hiz_input_led, v.input_type_selection == HIZ_INPUT);
    led_set_bool(io->input_selection.line_input_led, v.input_type_selection == LINE_INPUT);
    led_set_bool(io->input_selection.mic_input_led, v.input_type_selection == MIC_INPUT);
    const uint8_t channel = channel_of(vals);
    if (channel < CHANNELS && s_input_cycle_led[channel] != NULL)
    {
        led_set_bool(s_input_cycle_led[channel], get_button_held(&(io->input_selection.input_select_button)));
    }

    // a bypassed section's button flashes, whether or not it's selected
    const bool flash_lit = ((xTaskGetTickCount() / pdMS_TO_TICKS(BYPASS_FLASH_MS)) & 1U) == 0U;
    led_set_bool(io->eq_button.led, v.eq_off ? flash_lit : (v.control_set_selection == EQ_CONTROL));
    led_set_bool(io->comp_button.led, v.control_set_selection == COMP_CONTROL);
    led_set_bool(io->send_button.led, v.sends_off ? flash_lit : (v.control_set_selection == SENDS_CONTROL));

    led_set_bool(io->phase_flip_button.led, v.phase_flip);
    led_set_bool(io->phantom_48v_button.led, v.phantom_48v);
    led_set_bool(io->high_pass_filter_button.led, v.high_pass_filter);
    led_set_bool(io->solo_button.led, v.solo);
    led_set_bool(io->mute_button.led, v.muted);
    led_set_bool(io->sel_button.led, v.sel);
    led_set_bool(io->ins_button.led, v.ins);
    led_set_bool(io->pre_button.led, v.pre_fader);
}

void update_channel_button_values (SemaphoreHandle_t *vals_mutex, channel_controls *vals, channel_control_io_t *io)
{
    update_radio_buttons(vals_mutex, vals, io);
    check_section_buttons(vals_mutex, vals, io);

    update_input_select_button(vals_mutex, vals, io);

    update_toggle_button_val_from_info(vals_mutex, &(io->phase_flip_button.button), &(vals->phase_flip));
    update_phantom_button(vals_mutex, vals, io); // hold 2 s to turn on (mic only); the routing board follows only if CV_ENABLE_PHANTOM
    update_toggle_button_val_from_info(vals_mutex, &(io->high_pass_filter_button.button), &(vals->high_pass_filter));
    update_toggle_button_val_from_info(vals_mutex, &(io->solo_button.button), &(vals->solo));
    update_toggle_button_val_from_info(vals_mutex, &(io->mute_button.button), &(vals->muted));
    update_select_button(vals, io); // radio buttons across all channels, not a toggle
    // For now the insert is out and its send taps after the EQ (not pre); see CV_ENABLE_INSERT in cv_mapping.c
    // update_toggle_button_val_from_info(vals_mutex, &(io->ins_button.button), &(vals->ins));
    // update_toggle_button_val_from_info(vals_mutex, &(io->pre_button.button), &(vals->pre_fader));
    update_button(&(io->ins_button.button));
    update_button(&(io->pre_button.button));
}