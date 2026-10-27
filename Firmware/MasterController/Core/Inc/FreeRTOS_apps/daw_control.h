/**
  **********************************************************************************
  * DAW CONTROL - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/daw_control.h
  * @brief Mackie Control over USB MIDI (cable 0, "Mackie Control"): the transport buttons
  *        and the DAW's timecode display, in both directions.
  *
  *        DAW -> console: the play / stop / record LEDs and the timecode digits. Transport
  *                        changes are reported as text on USART2.
  *        console -> DAW: play, stop, record, to start and to end button presses
  *                        (daw_control_button / daw_control_tap; nothing calls them yet).
  **********************************************************************************
*/

#ifndef DAW_CONTROL_H
#define DAW_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

// console's transport buttons (daw control)
typedef enum
{
    DAW_PLAY = 0,
    DAW_STOP,
    DAW_RECORD,
    DAW_TO_START, // No mackie control message for this - config sets the note
    DAW_TO_END, // No mackie control message for this - config sets the note
    DAW_BUTTON_COUNT
} daw_button_t;

// The DAW's transport buttons state
typedef enum
{
    DAW_STOPPED = 0,
    DAW_PLAYING,
    DAW_PAUSED, // play LED flashing
    DAW_RECORDING // playing with record on
} daw_transport_t;

// DAW TIMECODE
typedef enum
{
    DAW_TIME_UNKNOWN = 0, // the DAW hasn't said
    DAW_TIME_SMPTE, // hours, minutes, seconds, frames
    DAW_TIME_BEATS  // bars, beats, sub-divisions, ticks
} daw_time_mode_t;

// The DAW's timecode display: ten digits in groups of 3-2-2-3
typedef struct
{
    char            text[24]; // as the DAW draws it, dots included, outer spaces removed
    uint16_t        field[4]; // the four groups as numbers, left to right (e.g. h, m, s, frames)
    daw_time_mode_t mode;
    bool            valid; // false until the DAW has sent any digits
} daw_timecode_t;


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC daw_control_init : void -> void
  Creates the DAW control freeRTOS task.
 ----------------------------------------------------------------------------------
*/
void daw_control_init (void);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC daw_control_button : button, pressed -> bool
  Sends a press (true) or release (false) to the DAW, as a physical button would on its
  way down and up. Call from tasks, not interrupts. False if USB isn't connected or the
  MIDI transmit buffer is full.
 ----------------------------------------------------------------------------------
*/
bool daw_control_button (daw_button_t button, bool pressed);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC daw_control_tap : button -> bool
  A press, a short pause, then the release. Blocks the calling task for the pause.
 ----------------------------------------------------------------------------------
*/
bool daw_control_tap (daw_button_t button);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC daw_control_transport : record_armed -> daw_transport_t
  The DAW's transport state. record_armed (optional) is set when record is on while
  the transport isn't rolling.
 ----------------------------------------------------------------------------------
*/
daw_transport_t daw_control_transport (bool *record_armed);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC daw_control_timecode : out -> void
  Copies where the playhead is, as the DAW's timecode display last showed it.
 ----------------------------------------------------------------------------------
*/
void daw_control_timecode (daw_timecode_t *out);

#endif // DAW_CONTROL_H
