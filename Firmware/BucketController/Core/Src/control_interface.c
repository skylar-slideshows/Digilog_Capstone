#include "hardware_drivers/rotary_encoder.h"
#include "hardware_drivers/mcp23017.h"
#include "stm32g474xx.h"
#include "control_interface.h"

/**
 * @brief Info on the location of a single button
 */
typedef struct
{
    I2C_TypeDef bus;   //!< I2C bus of the gpio expander which the button is connected to
    uint8_t addr;      //!< I2C address of the gpio expander
    MCP23017_Reg port; //!< Register/Port on the MCP23017 that the button pin is on;
    uint8_t pin;       //!< Pin which the button output is connected to
} button_info_t;

/**
 * @brief Info on the location of a single button LED
 */
typedef struct
{
    // TODO
} button_led_info_t;

/**
 * @brief Info on the location of a single LED ring
 */
typedef struct
{
    uint8_t channel;
    uint8_t knob_num;
} led_ring_info_t;

//! PLACEHOLDER for when the fader driver is created
typedef struct
{
    // TODO / PLACEHOLDER
} fader_info_t;

//! PLACEHOLDER for when the fader driver is created
typedef struct
{
    // TODO / PLACEHOLDER
} fader_led_bar_info_t;


/**
 * @brief Info on physical interfaces related to a single EQ band on a single channel
 */
typedef struct
{
    encoder_info_t gain_encoder;
    led_ring_info_t gain_led_ring;

    encoder_info_t q_encoder;
    led_ring_info_t q_led_ring;

    encoder_info_t freq_encoder;
    led_ring_info_t freq_led_ring;
} eq_band_interface_t;


/**
 * @brief Info on physical interfaces related to a single send channel output from a single console channel
 */
typedef struct
{
    encoder_info_t gain_encoder;
    led_ring_info_t gain_led_ring;

    encoder_info_t pan_encoder;   //!< Note that pan is not used for send channel 4
    led_ring_info_t pan_led_ring; //!< Note that pan is not used for send channel 4
} send_channel_interface_t;

typedef struct
{
    encoder_info_t in_gain_encoder;
    led_ring_info_t in_gain_led_ring;

    encoder_info_t out_gain_encoder;
    led_ring_info_t out_gain_led_ring;


    encoder_info_t attack_time_encoder;
    led_ring_info_t attack_time_led_ring;

    encoder_info_t release_time_encoder;
    led_ring_info_t release_time_led_ring;

    encoder_info_t threshold_encoder;
    led_ring_info_t threshold_led_ring;

    encoder_info_t ratio_encoder;
    led_ring_info_t ratio_led_ring;

    encoder_info_t de_ess_amt_encoder;
    led_ring_info_t de_ess_amt_led_ring;
} comp_interface_t;


typedef struct
{
    button_info_t mic_input_button;
    button_led_info_t mic_input_led;

    button_info_t line_input_button;
    button_led_info_t line_input_led;

    button_info_t hiz_input_button;
    button_led_info_t hiz_input_led;


    encoder_info_t input_gain_encoder;
    led_ring_info_t input_gain_led_ring;


    button_info_t phantom_48v_button;
    button_led_info_t phantom_48v_led;

    button_info_t phase_flip_button;
    button_led_info_t phase_flip_led;

    button_info_t high_pass_filter_button;
    button_led_info_t high_pass_filter_led;


    button_info_t send_button;
    button_led_info_t send_led;

    button_info_t eq_button;
    button_led_info_t eq_led;

    button_info_t comp_button;
    button_led_info_t comp_led;


    send_channel_interface_t send_channel_interfaces[SEND_CHANNELS];


    eq_band_interface_t hf_interface;
    eq_band_interface_t hmf_interface;
    eq_band_interface_t lmf_interface;
    eq_band_interface_t lf_interface;

    comp_interface_t comp_interface;


    button_info_t solo_button;
    button_led_info_t solo_led;

    button_info_t mute_button;
    button_led_info_t mute_led;

    button_info_t sel_button;
    button_led_info_t sel_led;


    button_info_t ins_button;
    button_led_info_t ins_led;

    button_info_t pre_button;
    button_led_info_t pre_led;

    button_info_t rec_button;
    button_led_info_t rec_led;

    fader_info_t fader;
    fader_led_bar_info_t fader_led_bar;
} channel_control_inputs_t;
