/**
  **********************************************************************************
  * MIX STATE - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file mix/mix_state.c
  * @brief See mix_state.h.
  **********************************************************************************
*/

#include "mix/mix_state.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/*
 * The bucket's start-up values (bucket firmware: control_interface.c fill_defaults), as the protocol
 * carries them (control_report.c: signed controls + 32768). If the bucket's defaults change, change
 * these too.
 */
static const uint16_t DEFAULT_KNOB[MIX_KNOBS] = {
    0,     //  0 input gain
    0,     //  1 send 1
    32768, //  2 send 1 pan (centre)
    0,     //  3 send 2
    32768, //  4 send 2 pan
    0,     //  5 send 3
    32768, //  6 send 3 pan
    0,     //  7 send 4
    32768, //  8 HF gain (0 dB)
    32767, //  9 HF freq (12 o'clock)
    32768, // 10 HMF gain
    32767, // 11 HMF freq
    32767, // 12 HMF Q
    32768, // 13 LMF gain
    32767, // 14 LMF freq
    32767, // 15 LMF Q
    32768, // 16 LF gain
    32767, // 17 LF freq
    21845, // 18 comp input   (a third: 0 dB)
    43690, // 19 attack       (two thirds: 10 ms)
    39321, // 20 threshold    (0.6: 0 dBu)
    28480, // 21 release      (100 ms)
    0,     // 22 de-ess (not used)
    0,     // 23 ratio        (1:1)
    21845, // 24 comp output  (a third: 0 dB)
    32768, // 25 pan (centre)
    44380, // 26 fader        (0 dB: the bucket's FADER_0DB_POSITION while its faders aren't connected)
};

static mix_channel_t s_mix[CONSOLE_CHANNELS];
static uint16_t s_master[MIX_MASTER];
static volatile uint32_t s_changes;

void mix_state_init (void)
{
    for (uint16_t ch = 0; ch < CONSOLE_CHANNELS; ch++)
    {
        memcpy(s_mix[ch].knob, DEFAULT_KNOB, sizeof(DEFAULT_KNOB));
        memset(s_mix[ch].sw, 0, sizeof(s_mix[ch].sw)); // line input, everything off
        s_mix[ch].sel = false;
    }
    for (uint8_t i = 0; i < AUX_RETURNS; i++)
    {
        s_master[i] = AUX_RETURN_UNITY;                                // 0 dB
        s_master[AUX_RETURNS + i] = AUX_RETURN_TO_MIX_DEFAULT ? 1U : 0U; // in the mix
    }
}

void mix_state_set_master (uint8_t index, uint16_t value)
{
    if (index >= MIX_MASTER) return;
    if (index >= AUX_RETURNS) value = value ? 1U : 0U;
    if (s_master[index] != value) s_changes++;
    s_master[index] = value;
}

uint16_t mix_state_get_master (uint8_t index)
{
    return index < MIX_MASTER ? s_master[index] : 0U;
}

uint32_t mix_state_changes (void)
{
    return s_changes;
}

void mix_state_changed (void)
{
    s_changes++;
}

void mix_state_set_knob (uint16_t channel, uint8_t id, uint16_t value)
{
    if (channel >= CONSOLE_CHANNELS || id >= MIX_KNOBS) return;
    if (s_mix[channel].knob[id] != value) s_changes++;
    s_mix[channel].knob[id] = value; // one 16-bit store: atomic
}

void mix_state_set_switch (uint16_t channel, uint8_t id, uint8_t value)
{
    if (channel >= CONSOLE_CHANNELS || id >= MIX_SWITCHES) return;
    if (id == 0U && value > 2U) return;
    if (id != 0U) value = value ? 1U : 0U;
    if (s_mix[channel].sw[id] != value) s_changes++;
    s_mix[channel].sw[id] = value;
}

void mix_state_set_bucket_select (uint8_t bucket, int16_t selected)
{
    const uint16_t first = (uint16_t)bucket * CHANNELS_PER_BUCKET;
    if (first >= CONSOLE_CHANNELS) return;
    taskENTER_CRITICAL();
    for (uint16_t ch = first; ch < first + CHANNELS_PER_BUCKET && ch < CONSOLE_CHANNELS; ch++)
    {
        s_mix[ch].sel = ((int16_t)ch == selected);
    }
    taskEXIT_CRITICAL();
}

void mix_state_get (uint16_t channel, mix_channel_t *out)
{
    if (out == NULL) return;
    if (channel >= CONSOLE_CHANNELS)
    {
        memcpy(out->knob, DEFAULT_KNOB, sizeof(DEFAULT_KNOB));
        memset(out->sw, 0, sizeof(out->sw));
        out->sel = false;
        return;
    }
    taskENTER_CRITICAL();
    *out = s_mix[channel];
    taskEXIT_CRITICAL();
}

uint16_t mix_state_default_knob (uint8_t id)
{
    return id < MIX_KNOBS ? DEFAULT_KNOB[id] : 0U;
}

uint8_t mix_state_default_switch (uint8_t id)
{
    (void)id;
    return 0U; // line input; phase, +48V, HPF, solo, mute, insert, pre all off
}
