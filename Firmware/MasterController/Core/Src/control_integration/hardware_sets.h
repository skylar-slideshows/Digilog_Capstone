#ifndef HARDWARE_SETS_H
#define HARDWARE_SETS_H

/**
 * This header file is internal to control_integration, separated for organization (as on the bucket).
 * It should not have to be included anywhere outside of control_integration; it is not a public header.
 */

#include "CONFIG.h"
#include "control_integration/control_interface.h"
#include "FreeRTOS_apps/daw_control.h"

#include "hardware_drivers/button_driver.h"
#include "hardware_drivers/led_driver.h"
#include "hardware_structs.h" // Only to be included for control_interface related code


/*=============================== THE MASTER'S PANEL ================================*/

#define TRANSPORT_BUTTONS 5 // PLAY, STOP, REC, FRW, FF

/**
 * @brief Every knob and button on the master, with its LED where it has one
 */
typedef struct
{
    knob_info_t aux_return_knob[AUX_RETURNS];        //!< K0-K3, each with its ring
    knob_info_t screen_knob;                          //!< K4, no ring

    lit_button_info_t screen_button[SCREEN_BUTTONS]; //!< B0-B3
    lit_button_info_t user_button[USER_BUTTONS];     //!< B4-B6
    lit_button_info_t transport[TRANSPORT_BUTTONS];  //!< PLAY, STOP, REC, FRW, FF (button_control.c says which DAW button each sends)
    lit_button_info_t insert_button;                 //!< INSERT
} master_control_io_t;

#endif
