/**
  **********************************************************************************
  * PARAMETER SAVING AND DATA INFRASTRUCTURE - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file console_state_bucket.c
  * @brief See console_state_bucket.h: which console channels this bucket holds (from the master),
  *        and the channel names. Written by the master link task, read by any task.
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

#include "console_state_bucket.h"
#include "CONFIG.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

static volatile uint8_t s_first; // first console channel this bucket holds (1, 5, 9, 13 ...); 0 = not known yet
static volatile bool s_solo;        // no master: channels 01-04 on our own
static volatile bool s_from_master; // the master has told us our channels
static char s_names[CHANNELS][CHANNEL_NAME_MAX + 1];

bool console_state_set_base (uint32_t first)
{
    if (first < 1U || ((first - 1U) % CHANNELS) != 0U || first + CHANNELS - 1U > 99U) return false;
    if (s_solo) // the master turned up after we gave up waiting: it takes over
    {
        s_solo = false;
#if DEVELOPER_MODE
        printf("\r\nMaster link: master found, leaving solo mode\n");
#endif
    }
    s_from_master = true;
    if (s_first != (uint8_t)first)
    {
#if DEVELOPER_MODE
        printf("\r\nMaster link: this is bucket %lu, console channels %02lu-%02lu\n",
               (unsigned long)((first - 1U) / CHANNELS + 1U), (unsigned long)first, (unsigned long)(first + CHANNELS - 1U));
#endif
        s_first = (uint8_t)first;
    }
    return true;
}

bool console_state_known (void)
{
    return s_first != 0U;
}

void console_state_set_solo (void)
{
    if (s_from_master) return; // the master answered meanwhile
    s_solo = true;
    s_first = 1U;
#if DEVELOPER_MODE
    printf("\r\nNo master: bucket solo mode, channels 01-04\n");
#endif
}

bool console_state_solo (void)
{
    return s_solo;
}

bool console_state_from_master (void)
{
    return s_from_master;
}

uint8_t console_bucket (void)
{
    const uint8_t first = s_first;
    return first ? (uint8_t)((first - 1U) / CHANNELS) : 0xFFU;
}

uint8_t console_channel_number (uint8_t local)
{
    const uint8_t first = s_first;
    return (first && local < CHANNELS) ? (uint8_t)(first + local) : 0U;
}

int16_t console_channel_index (uint8_t local)
{
    const uint8_t first = s_first;
    return (first && local < CHANNELS) ? (int16_t)(first - 1U + local) : -1;
}

void console_state_set_name (uint8_t local, const char *text)
{
    if (local >= CHANNELS || text == NULL) return;
    while (*text == ' ' || *text == '\t') text++;
    size_t len = strlen(text);
    while (len > 0U && (text[len - 1U] == ' ' || text[len - 1U] == '\t')) len--;
    if (len > CHANNEL_NAME_MAX) len = CHANNEL_NAME_MAX;

    char name[CHANNEL_NAME_MAX + 1];
    for (size_t i = 0; i < len; i++)
    {
        const unsigned char c = (unsigned char)text[i];
        name[i] = (c >= 0x20U && c < 0x7FU) ? (char)c : '?'; // the screens draw plain ASCII only
    }
    name[len] = '\0';

    taskENTER_CRITICAL();
    memcpy(s_names[local], name, len + 1U);
    taskEXIT_CRITICAL();
}

void console_state_name (uint8_t local, char out[CHANNEL_NAME_MAX + 1])
{
    if (out == NULL) return;
    if (local >= CHANNELS)
    {
        out[0] = '\0';
        return;
    }
    taskENTER_CRITICAL();
    memcpy(out, s_names[local], CHANNEL_NAME_MAX + 1);
    taskEXIT_CRITICAL();
}
