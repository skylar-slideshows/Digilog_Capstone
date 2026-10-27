#include "button_control.h"
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
    xSemaphoreGive(*vals_mutex);
}

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
    I2C_TypeDef *bus = i2c_channel_bus(channel);

    io->input_selection.input_select_button = BUTTONS[0];
    io->input_selection.input_select_button.bus = bus;
    io->input_selection.line_input_led = led_device_at(INPUT_SELECT_LEDS[channel][0]);
    io->input_selection.mic_input_led = led_device_at(INPUT_SELECT_LEDS[channel][1]);
    io->input_selection.hiz_input_led = led_device_at(INPUT_SELECT_LEDS[channel][2]);

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

    led_set_bool(io->eq_button.led, v.control_set_selection == EQ_CONTROL);
    led_set_bool(io->comp_button.led, v.control_set_selection == COMP_CONTROL);
    led_set_bool(io->send_button.led, v.control_set_selection == SENDS_CONTROL);

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

    update_input_select_button(vals_mutex, vals, io);

    update_toggle_button_val_from_info(vals_mutex, &(io->phase_flip_button.button), &(vals->phase_flip));
    update_toggle_button_val_from_info(vals_mutex, &(io->phantom_48v_button.button), &(vals->phantom_48v));
    update_toggle_button_val_from_info(vals_mutex, &(io->high_pass_filter_button.button), &(vals->high_pass_filter));
    update_toggle_button_val_from_info(vals_mutex, &(io->solo_button.button), &(vals->solo));
    update_toggle_button_val_from_info(vals_mutex, &(io->mute_button.button), &(vals->muted));
    update_toggle_button_val_from_info(vals_mutex, &(io->sel_button.button), &(vals->sel));
    update_toggle_button_val_from_info(vals_mutex, &(io->ins_button.button), &(vals->ins));
    update_toggle_button_val_from_info(vals_mutex, &(io->pre_button.button), &(vals->pre_fader));
}
