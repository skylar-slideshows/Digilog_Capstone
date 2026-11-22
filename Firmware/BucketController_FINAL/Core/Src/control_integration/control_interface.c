#include "comp/comp_engine.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "CONFIG.h"
#include "fader_control.h"

#include "control_integration/control_interface.h"

#include "button_control.h"
#include "hardware_sets.h"
#include "knob_control.h"
#include "portmacro.h"


/*=============================== LOGIC CONNECTING HARDWARE TO CONTROL VALUES ================================*/

/*
 * Holds all control values for all channels in the bucket
 * defined in control_interface.h
 */
channel_controls channel_control_vals[CHANNELS];
SemaphoreHandle_t channel_control_mutexes[CHANNELS];

/**
 * @brief Holds info on all control interface IO for all 4 channels in the bucket
 */
channel_control_io_t channel_controls_io[CHANNELS];

static void init_control_io (uint8_t channel)
{
    init_button_controls(channel, &(channel_controls_io[channel]));
    init_knob_controls(channel, &(channel_controls_io[channel]));
    init_fader_controls(channel, &(channel_controls_io[channel]));
}

static channel_controls s_defaults; // filled once by fill_defaults(); every channel starts as a copy

const channel_controls *control_defaults (void)
{
    return &s_defaults;
}

static void fill_defaults (channel_controls *vals)
{
    vals->input_type_selection = LINE_INPUT;
    vals->control_set_selection = EQ_CONTROL;
    vals->input_gain = 0;
#if FADERS_CONNECTED
    vals->output_gain = 0;
#else
    vals->output_gain = (s_scalar_control_t)((int32_t)FADER_0DB_POSITION - 32768); // faders unplugged: sit at 0 dB
#endif
    vals->output_pan = 0;

    vals->phantom_48v = false;
    vals->phase_flip = false;
    vals->high_pass_filter = false;

    vals->muted = false;
    vals->solo = false;
    vals->sel = false;

    vals->ins = false;
    vals->pre_fader = false;
    vals->record = false;

    for (uint8_t send_channel = 0; send_channel < SEND_CHANNELS; send_channel++)
    {
        vals->send_controls[send_channel].gain = 0;
        vals->send_controls[send_channel].pan = 0;
    }

    const eq_band_control_t default_eq_band_control = {.freq = U_SCALAR_CONTROL_MID, .gain = 0, .q = U_SCALAR_CONTROL_MID, .bell = false}; // knobs start at 12 o'clock, LF/HF as shelves
    vals->hf_control = default_eq_band_control;
    vals->hmf_control = default_eq_band_control;
    vals->lmf_control = default_eq_band_control;
    vals->lf_control = default_eq_band_control;

    // compressor: does nothing until RATIO is turned up (1:1), with sensible times and threshold ready
    vals->comp_control.in_gain = comp_p_from_fraction(1.0f / 3.0f);          // INPUT 0 dB (ring a third lit)
    vals->comp_control.attack_time = comp_p_from_fraction(2.0f / 3.0f);      // ATK 10 ms
    vals->comp_control.threshold = comp_p_from_fraction(30.0f / 50.0f);      // THRESH 0 dBu
    vals->comp_control.release_time = comp_p_from_fraction(0.43457f);        // REL 100 ms
    vals->comp_control.out_gain = comp_p_from_fraction(1.0f / 3.0f);         // OUTPUT 0 dB (ring a third lit)
    vals->comp_control.ratio = 0;                                            // RATIO 1:1
    vals->comp_control.de_ess_amt = 0;
    vals->comp_control.rms = false;                                          // peak detector

    vals->eq_off = false;
    vals->sends_off = false;
    vals->comp_off = false;
}

static void init_control_vals (uint8_t channel)
{
    xSemaphoreTake(channel_control_mutexes[channel], portMAX_DELAY);
    channel_control_vals[channel] = s_defaults;
    xSemaphoreGive(channel_control_mutexes[channel]);
}

void init_control_interface (void)
{
    fill_defaults(&s_defaults);

    for (uint8_t channel = 0; channel < CHANNELS; channel++)
    {
        channel_control_mutexes[channel] = xSemaphoreCreateMutex();

        init_control_io(channel);
        init_control_vals(channel);
    }
}

/**
 * Updates control values based on current/previous (cached) input (e.g button/rotary-encoder) states
 * Gpio expanders should have been polled to cache before running this
 */
uint16_t p = 0;
void update_control_values (uint8_t channel)
{
    update_channel_knob_values(
        &(channel_control_mutexes[channel]),
        &(channel_control_vals[channel]),
        &(channel_controls_io[channel]) //
    );
    update_channel_button_values(
        &(channel_control_mutexes[channel]),
        &(channel_control_vals[channel]),
        &(channel_controls_io[channel]) //
    );
    update_channel_fader_values(
        &(channel_control_mutexes[channel]),
        &(channel_control_vals[channel]),
        &(channel_controls_io[channel]) //
    );

    // TODO: non-button/encoder inputs maybe?
}

/*=============================== LOGIC CONNECTING CONTROL VALUES TO LEDS (TODO) ================================*/

/**
 * Updates LEDs / other visual outputs
 */
void update_control_leds (uint8_t channel)
{
    update_channel_knob_leds(
        &(channel_control_mutexes[channel]),
        &(channel_control_vals[channel]),
        &(channel_controls_io[channel]) //
    );
    update_channel_button_leds(
        &(channel_control_mutexes[channel]),
        &(channel_control_vals[channel]),
        &(channel_controls_io[channel]) //
    );
    update_channel_fader_hardware(&(channel_controls_io[channel]));
    // TODO: non-button/encoder inputs maybe?
}
