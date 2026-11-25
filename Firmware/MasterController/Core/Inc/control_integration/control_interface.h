/**
  **********************************************************************************
  * CONTROL INTERFACE - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file control_integration/control_interface.h
  * @brief The master's control values, laid out like the bucket's control interface: the
  *        I2C task reads the panel (update_control_values), the LED task shows the values
  *        (update_control_leds), and the outputs (DACs, LCV lines) follow the values
  *        (control_outputs.c). Every value is guarded by master_control_mutex.
  *
  *        The master's panel:
  *          K0-K3   aux return 1-4 level. Turn: level (ring = bar). Hold the knob down to turn
  *                  finer; double press: back to 0 dB; hold 2 s without turning: return in or
  *                  out of the mix (ring flashes; out of the mix the ring shows a single point).
  *          INSERT  mix insert in/out (LCV_INSERT), lit while in.
  *          PLAY, STOP, REC, FRW, FF   transport, straight to the DAW (daw_control.c), lit from
  *                  the DAW's own state. FRW / FF are Mackie REWIND / FAST FWD: the DAW winds
  *                  while they're held.
  *          K4, B0-B3   the screen's knob and buttons: collected as events for the screen
  *                  (control_take_screen_input), nothing else yet. B0-B3 light while held.
  *          B4-B6   no function yet: each toggles a flag and its LED.
  **********************************************************************************
*/

#ifndef CONTROL_INTERFACE_H
#define CONTROL_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "CONFIG.h"

typedef uint16_t u_scalar_control_t;
typedef int16_t s_scalar_control_t;

#define U_SCALAR_CONTROL_MAX UINT16_MAX
#define U_SCALAR_CONTROL_MID (U_SCALAR_CONTROL_MAX >> 1)

#define S_SCALAR_CONTROL_MAX INT16_MAX
#define S_SCALAR_CONTROL_MIN INT16_MIN

#define SCREEN_BUTTONS 4 // B0-B3
#define USER_BUTTONS   3 // B4-B6


/**
 * @brief Every control value on the master
 */
typedef struct
{
    u_scalar_control_t aux_return_level[AUX_RETURNS]; //!< K0-K3: 0-65535 across the VCA's range; AUX_RETURN_UNITY is 0 dB
    bool aux_return_to_mix[AUX_RETURNS];               //!< return summed into the mix (LCV_AUX1-4)
    bool insert;                                       //!< mix insert in (LCV_INSERT)
    bool user_button[USER_BUTTONS];                    //!< B4-B6: latching, no function yet
} master_controls;


/**
 * @brief Holds all control values of the master
 */
extern master_controls master_control_vals;

/**
 * @brief Mutex for master_control_vals
 */
extern SemaphoreHandle_t master_control_mutex;

/**
 * @brief Sets default control values and the panel's hardware descriptions. Call once, after led_init()
 * (the knob rings and button LEDs must exist) and before the I2C and LED tasks start.
 */
void init_control_interface (void);

/**
 * @brief Updates control values from the (cached) knob and button states. The I2C task calls this
 * once per frame, after polling the expanders.
 */
void update_control_values (void);

/**
 * @brief Updates the LEDs from the control values (and the DAW's transport state). The LED task calls this.
 */
void update_control_leds (void);

/**
 * @brief Every control's value at startup. A knob's double press goes back to its value here.
 */
const master_controls *control_defaults (void);


/*=============================== SCREEN INPUTS ================================*/

/** What the screen's knob and buttons did since the screen last asked */
typedef struct
{
    int16_t turns;                          //!< K4 detents: + clockwise, - anticlockwise
    uint8_t knob_presses;                   //!< K4 push presses
    uint8_t button_presses[SCREEN_BUTTONS]; //!< B0-B3 presses
} screen_input_t;

/**
 * @brief Copies what the screen controls did since the last call, and starts counting again from zero.
 */
void control_take_screen_input (screen_input_t *out);


/*=============================== LAST TOUCHED ================================*/

/** Which value was changed last, for the screen's pop-up later (as on the bucket) */
typedef enum
{
    TOUCHED_NOTHING,
    TOUCHED_AUX_RETURN_LEVEL, //!< which says the return, 0-3
    TOUCHED_AUX_RETURN_MIX,   //!< in or out of the mix, which says the return
    TOUCHED_INSERT,
} touched_param_t;

typedef struct
{
    touched_param_t param;
    uint8_t which;
    uint32_t tick; //!< FreeRTOS tick (ms) of the change
} touched_t;

/**
 * @brief The value most recently changed on the panel, and when. param is TOUCHED_NOTHING until something has changed.
 */
touched_t control_last_touched (void);

#endif
