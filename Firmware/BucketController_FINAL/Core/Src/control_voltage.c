/**
  **********************************************************************************
  * CONTROL VOLTAGE GENERATION ENGINE - DIGILOG CONSOLE
  **********************************************************************************
  * @file control_voltage.c
  * @brief See control_voltage.h.
  *
  * @author
  * @date
  * @version 1.0
  *
  * @attention
  *  Copyright (C) 2026
  *
  *  MIT License:
  *  Permission is hereby granted, free of charge, to any person obtaining a copy
  *  of this software and associated documentation files (the “Software”), to deal
  *  in the Software without restriction, including without limitation the rights
  *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
  *  of the Software, and to permit persons to whom the Software is furnished to do so,
  *  subject to the following conditions:
  *
  *  The above copyright notice and this permission notice shall be included in all
  *  copies or substantial portions of the Software.
  *  THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
  *  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  *  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
  *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
  *  OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  **********************************************************************************
*/

#include "control_voltage.h"
#include "cv_mapping.h"
#include "control_integration/control_interface.h"
#include "hardware_drivers/i2c_driver.h"
#include "hardware_drivers/mcp4728_cache.h"
#include "hardware_drivers/routing_switches.h"
#include "CONFIG.h"
#include <string.h>
#include <stdio.h>

static cv_state_t s_state[CHANNELS];          // mic pad hysteresis, per channel
static cv_params_t s_last[CHANNELS];          // what the codes were last worked out from
static bool s_have_last[CHANNELS];
static volatile float s_comp_extra_db[CHANNELS];

static cv_source_t source_of (channel_input_selection_t selection)
{
    switch (selection)
    {
        case MIC_INPUT: return CV_SOURCE_MIC;
        case HIZ_INPUT: return CV_SOURCE_HIZ;
        default:        return CV_SOURCE_LINE;
    }
}

static cv_eq_band_t band_of (const eq_band_control_t *band)
{
    return (cv_eq_band_t){
        .gain = cv_signed_to_p(band->gain),
        .freq = band->freq,
        .q = band->q,
        .bell = band->bell,
    };
}

void control_voltage_init (void)
{
    routing_switches_init();
}

/** Whether any channel other than this one is soloed. Read without the locks: each flag is one byte,
 *  and being a frame late doesn't matter. */
static bool other_channel_soloed (uint8_t channel)
{
    for (uint8_t other = 0; other < CHANNELS; other++)
    {
        if (other != channel && ((volatile const channel_controls *)&channel_control_vals[other])->solo) return true;
    }
    return false;
}

void control_voltage_update (uint8_t channel)
{
    if (channel >= CHANNELS) return;
    const bool other_soloed = other_channel_soloed(channel);

    // copy what's needed while holding the channel's lock, then do the maths without it. Zeroed first so
    // the padding is known and the whole struct can be compared with the last one.
    cv_params_t params;
    memset(&params, 0, sizeof(params));
    xSemaphoreTake(channel_control_mutexes[channel], portMAX_DELAY);
    const channel_controls *v = &channel_control_vals[channel];
    params.source = source_of(v->input_type_selection);
    params.input_gain = v->input_gain;
    params.lf = band_of(&v->lf_control);
    params.lmf = band_of(&v->lmf_control);
    params.hmf = band_of(&v->hmf_control);
    params.hf = band_of(&v->hf_control);
    for (uint8_t send = 0; send < 4; send++) params.aux_level[send] = v->send_controls[send].gain;
    params.aux_pan[0] = cv_signed_to_p(v->send_controls[0].pan);
    params.aux_pan[1] = cv_signed_to_p(v->send_controls[1].pan);
    params.fader = cv_signed_to_p(v->output_gain);
    params.pan = cv_signed_to_p(v->output_pan);
    params.phantom = v->phantom_48v;
    params.phase = v->phase_flip;
    params.hpf = v->high_pass_filter;
    params.insert = v->ins;
    params.insert_pre = v->pre_fader;
    params.muted = v->muted;
    params.soloed = v->solo;
    params.other_soloed = other_soloed; // solo takes the other channels off the mix without setting their mute
    params.eq_off = v->eq_off;
    params.sends_off = v->sends_off;
    xSemaphoreGive(channel_control_mutexes[channel]);

    // the maths runs only when something changed; this is called every I2C frame
    if (s_have_last[channel] && memcmp(&params, &s_last[channel], sizeof(params)) == 0) return;
    s_last[channel] = params;
    s_have_last[channel] = true;

    cv_result_t result;
    cv_compute(&params, &s_state[channel], &result);

    I2C_TypeDef *bus = i2c_channel_bus(channel);
    for (uint8_t cv = 0; cv < CV_COUNT; cv++)
    {
        // only marks a DAC for rewriting when a value changed
        mcp4728_cache_write(bus, CV_DAC_SLOTS[cv].addr, CV_DAC_SLOTS[cv].output, result.code[cv]);
    }
#if DEVELOPER_MODE
    // what takes a channel off the mix bus, printed whenever its mix switch changes
    if (((routing_switches_get(channel) ^ result.switches) & RS_MIX_ON) != 0U)
    {
        printf("\r\nCH%u mix bus %s: muted %d, fader P %u, soloed %d, other channels soloed %d%d%d%d\n",
               (unsigned)(channel + 1U), (result.switches & RS_MIX_ON) ? "ON" : "OFF", params.muted,
               (unsigned)params.fader, params.soloed, channel_control_vals[0].solo, channel_control_vals[1].solo,
               channel_control_vals[2].solo, channel_control_vals[3].solo);
    }
#endif
    routing_switches_set(channel, result.switches); // only sends when a bit changed
    s_comp_extra_db[channel] = result.comp_extra_db;
}

float control_voltage_comp_extra_db (uint8_t channel)
{
    return channel < CHANNELS ? s_comp_extra_db[channel] : 0.0f;
}
