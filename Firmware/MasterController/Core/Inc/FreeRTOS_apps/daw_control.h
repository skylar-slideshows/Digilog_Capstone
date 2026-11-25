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
    DAW_REWIND,   // Mackie REWIND: the DAW winds back while it's held (the panel's FRW)
    DAW_FORWARD,  // Mackie FAST FWD: the DAW winds forward while it's held (the panel's FF)
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
  @brief PUBLIC daw_control_button_lit : button -> bool
  Whether the DAW has the LED of one of its transport buttons on (or flashing), e.g.
  REWIND / FAST FWD while it winds. Any task.
 ----------------------------------------------------------------------------------
*/
bool daw_control_button_lit (daw_button_t button);


/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC daw_control_timecode : out -> void
  Copies where the playhead is, as the DAW's timecode display last showed it.
 ----------------------------------------------------------------------------------
*/
void daw_control_timecode (daw_timecode_t *out);

/**
 ----------------------------------------------------------------------------------
  @brief PUBLIC daw_control_position_at : tick, out ms -> bool
  Where the playhead is (or was) at a given FreeRTOS tick, in ms from 00:00:00:00: the
  last SMPTE time the DAW showed, taken at the tick its digits arrived, moved on by the
  time since while the transport rolls. The frames group's rate (24, 25, 30 ...) is
  learned from where it wraps. False until the DAW sends a time, or while it says its
  display shows bars and beats (a DAW that never says which is taken as SMPTE).
  Any task.
 ----------------------------------------------------------------------------------
*/
bool daw_control_position_at (uint32_t tick, uint32_t *ms);

/** True while the DAW's transport rolls (playing or recording). Any task. */
bool daw_control_rolling (void);

#endif // DAW_CONTROL_H
