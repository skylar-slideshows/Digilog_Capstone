/**
  **********************************************************************************
  * MCP4728 REGISTER CONSTRAINTS AND FUNCTIONS HEADER - DIGILOG CONSOLE
  **********************************************************************************
  * @file mcp4728.h
  * @brief MCP4728s are scanned at constant rate on I2C, and no interrupt
  *        pins are used, for simplicity. The necessary parts for this case are included.
  *
  * Transcribed from MCP4728 DS22187E.
  * This file is the transcription boundary: every number below was copied
  * from a PDF by hand. Re-check it against the datasheet revision for the
  * silicon actually being purchased before a production run.
  *
  * @author Darya Petrova (petrov.da@northeastern.edu)
  * @date 2026-09-01
  * @version 1.0
  *
  * @attention
  *  Copyright (C) 2026 Skylar Denno
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

#ifndef INC_MCP4728_H_
#define INC_MCP4728_H_

#include <stdbool.h>

#include "stm32g4xx.h"

/* Commands and Modes */

/**
 * @brief 4-bit number selecting which channels use the internal 2.048V chip reference
 *
 * 0: The chip's VDD input is used as a reference voltage
 * 1: The chip's internal 2.048V reference is used
 *
 * e.g: 0b00001010, DAC channels A and C use the internal reference
 */
typedef uint8_t mcp4728_vref_selection_t;

#define MCP4728_ALL_VREF_EXTERNAL ((mcp4728_vref_selection_t)0x00)
#define MCP4728_ALL_VREF_INTERNAL ((mcp4728_vref_selection_t)0x0F)

/**
 * @brief 4-bit number selecting which channels use a 2x output voltage gain
 *
 * 0: The chip outputs with 1x gain
 * 1: The chip outputs with 2x gain
 *
 * e.g: 0b00001010, DAC channels A and C use 2x gain, while B and D use 1x gain
 */
typedef uint8_t mcp4728_gain_selection_t;
#define MCP4728_ALL_GAINS_1X ((mcp4728_gain_selection_t)0x00)
#define MCP4728_ALL_GAINS_2X ((mcp4728_gain_selection_t)0x0F)

/**
 * @brief 8-bit number (4 sets of 2-bits) setting the power-down selection of each channel
 *
 * 0: The chip outputs with 1x gain
 * 1: The chip outputs with 2x gain
 *
 * e.g: 0b11100100: A uses 3, B uses 2, C uses 1, D uses 0
 */
typedef uint8_t mcp4728_power_mode_selection_t;
#define MCP4728_PWRDWN_ALL_NORMAL 0x00

#define MCP4728_PWRDWN_NORMAL 0x0
#define MCP4728_PWRDWN_1 0x1
#define MCP4728_PWRDWN_2 0x2
#define MCP4728_PWRDWN_3 0x3

/**
 * @brief 12-bit number (least-significant bits of a uint16_t) representing the output voltage
 *
 * A 12-bit number (0 to 4095) representing the analog output voltage of the DAC
 * Uses the 12 least-significant bits of a uint16_t
 */
typedef uint16_t mcp4728_output_value_t;

/**
 * @brief General i2c command used by the mcp4728
 */
typedef uint8_t mcp4728_general_command_t;
#define MCP4728_GENERAL_RESET ((mcp4728_general_command_t)0x06)
#define MCP4728_GENERAL_WKUP ((mcp4728_general_command_t)0x09)
#define MCP4728_GENERAL_SOFTWARE_UPDATE ((mcp4728_general_command_t)0x08)
#define MCP4728_GENERAL_READ_ADDR ((mcp4728_general_command_t)0x0C)

/**
 * @brief 5-bit number (most-significant bits of a uint8_t) matching a command code of the mcp4728
 */
typedef uint8_t mcp4728_command_code;
#define MCP4728_FAST_WRITE ((mcp4728_command_code)0x00)
#define MCP4728_MULTI_WRITE ((mcp4728_command_code)0x40)
#define MCP4728_SEQ_WRITE ((mcp4728_command_code)0x50)
#define MCP4728_SINGLE_WRITE ((mcp4728_command_code)0x58)
#define MCP4728_ADDR_WRITE ((mcp4728_command_code)0x60)
#define MCP4728_VREF_WRITE ((mcp4728_command_code)0x80)
#define MCP4728_GAIN_WRITE ((mcp4728_command_code)0xC0)
#define MCP4728_PWRDOWN_WRITE ((mcp4728_command_code)0xA0)

#define MCP4728_BASE_ADDR 0x60

#define MCP4728_GAIN_1 0x0
#define MCP4728_GAIN_2 0x1

#define MCP4728_CHANNEL_A 0x0
#define MCP4728_CHANNEL_B 0x1
#define MCP4728_CHANNEL_C 0x2
#define MCP4728_CHANNEL_D 0x3

#define MCP4728_UDAC_UPLOAD 0x1
#define MCP4728_UDAC_NOLOAD 0x0

/**
 * @brief Configuration structure containing the DAC values for each channel
 */
typedef struct
{
    bool internal_vref;           //!< See \ref mcp4728_vref_selection_t
    bool double_gain;             //!< See \ref mcp4728_gain_selection_t
    uint8_t power_mode_selection; //!< See \ref mcp4728_power_mode_selection_t
    mcp4728_output_value_t val;   //!< 12-bit output value for the channel

} mcp4728_single_channel_config_t;

/* Function Prototypes */

/**
 * @brief Selects the voltage reference for the MCP4728.
 *
 * This function selects the voltage reference source (either internal or
 * external) for each channel of the MCP4728 device.
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_vrefSelect (
    I2C_TypeDef *bus, //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,     //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_vref_selection_t vref_select_bits
);

/**
 * @brief Selects the gain for the MCP4728.
 *
 * This function selects the gain (either 1 or 2) for each channel of the MCP4728 device.
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_gainSelect (
    I2C_TypeDef *bus, //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,     //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_gain_selection_t gain_select_bits
);

/**
 * @brief Selects the power down mode for the MCP4728.
 *
 * This function sets the power down mode for the specified channels of the
 * MCP4728 device. The power down mode determines the output impedance of the
 * channels.
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_writePwrDownSelect (
    I2C_TypeDef *bus, //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,     //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_power_mode_selection_t power_modes
);

/**
 * @brief Sends a general call command to the MCP4728.
 *
 * This function sends a general call command to all i2c devices on the bus. General
 * call commands can be used to perform actions such as reset or update all DAC
 * channels.
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_generalCall (
    I2C_TypeDef *bus,                 //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    mcp4728_general_command_t command //!< General call command to send
);

/**
 * @brief Performs a fast write operation on the MCP4728.
 *
 * This function performs a fast write operation on the MCP4728 device, updating
 * the DAC output values for all four channels in a single I2C transaction. Gain,
 * voltage reference, power mode options are not configurable. They are not changed
 * in the transaction. Prior values are used.
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_fastWrite (
    I2C_TypeDef *bus,                //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,                    //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_output_value_t output_a, //!< 12-bit output value for channel A
    mcp4728_output_value_t output_b, //!< 12-bit output value for channel B
    mcp4728_output_value_t output_c, //!< 12-bit output value for channel C
    mcp4728_output_value_t output_d  //!< 12-bit output value for channel D
);

/**
 * @brief Performs a multi-write operation on the MCP4728.
 *
 * This function performs a multi-write operation on the MCP4728 device,
 * updating the DAC output values for all specified channels in a single I2C
 * transaction. It then sends a general call command to update the outputs.
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_multiWrite (
    I2C_TypeDef *bus,                             //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,                                 //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_single_channel_config_t *config_list, //!< Ordered configuration structures containing DAC values
    uint8_t *channel_list,                        //!< Ordered channel numbers corresponding to config_list
    uint8_t channel_count                         //! Number of channels in config_list and channel_list
);

/**
 * @brief Performs a sequential write operation on the MCP4728.
 *
 * This function performs a sequential write operation on the MCP4728 device,
 * updating the DAC output values for all specified channels in a single I2C
 * transaction. It then sends a general call command to update the outputs.
 * This command also updates the DAC's EEPROM
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_sequentialWrite (
    I2C_TypeDef *bus,                             //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,                                 //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_single_channel_config_t *config_list, //!< Ordered configuration structures containing DAC values
    uint8_t channel_count,                        //! Number of channels in config_list
    uint8_t start_channel                         //! The first channel number in the sequence before incrementing
);

/**
 * @brief Performs a single write operation on a specified channel of the
 * MCP4728.
 *
 * This function performs a single write operation on a specified channel of the
 * MCP4728 device, updating the DAC output value for that channel. It then sends
 * a general call command to update the output.
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_singleWrite (
    I2C_TypeDef *bus,                       //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,                           //!< 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_single_channel_config_t config, //!< Configuration structure containing DAC values
    uint8_t channel                         //!< The channel to be updated (0 for A, 1 for B, 2 for C, 3 for D)
);

/**
 * @brief TODO
 *
 * TODO
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_init_single_address (
    GPIO_TypeDef *dac_scl_port, //!< GPIO port of the i2c clock to which the chip is connected
    uint8_t dac_scl_pin,        //!< GPIO pin of the i2c clock to which the chip is connected
    GPIO_TypeDef *dac_sda_port, //!< GPIO port of the i2c data line to which the chip is connected
    uint8_t dac_sda_pin,        //!< GPIO pin of the i2c data line to which the chip is connected
    uint8_t ldac_pin_idx,       //!< index on the connected shift-register to which the LDAC pin of this chip is connected
    uint8_t new_addr,           //!< new 3-bit address of the selected DAC
    uint8_t old_addr            //!< old 3-bit address of the selected DAC
);

/**
 * @brief TODO
 *
 * TODO
 *
 * @return uint8_t Error code (0 for success)
 */
uint8_t mcp4728_init_address (
    I2C_TypeDef *bus,     //!< I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t ldac_pin_idx, //!< index on the connected shift-register to which the LDAC pin of this chip is connected
    uint8_t new_addr      //!< new 3-bit address of the selected DAC
);

#endif /* INC_MCP4728_H_ */
