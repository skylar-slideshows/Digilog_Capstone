/**
  **********************************************************************************
  * MCP4728 DIGITAL TO ANALOG CONVERTER - DIGILOG CONSOLE (Bucket)
  **********************************************************************************
  * @file hardware_drivers/mcp4728.c
  * @brief 
  *
  * @author Darya Petrova (petrov.da@northeastern.edu)
  *         
  * @date 2026-08-23
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

#include "CONFIG.h"
#include "hardware_drivers/mcp4728.h"
#include "hardware_drivers/74hc595.h"
#include "hardware_drivers/i2c_driver.h"
#include "main.h"
#include <string.h>

#define PLEASE_DONT_LEAK_MEMORY
#define PLEASE_DONT_SEGFAULT

/*
 * This function selects the voltage reference source (either internal or
 * external) for each channel of the MCP4728 device.
 *
 * returns uint8_t Error code (0 for success)
 */
uint8_t mcp4728_vrefSelect (
    I2C_TypeDef *bus, // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,     // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_vref_selection_t vref_select_bits
)
{
    uint8_t data[1] = {MCP4728_VREF_WRITE | (vref_select_bits & 0x0F)};
    return !i2c_write(bus, addr, data, sizeof(data));
}

/*
 * This function selects the gain (either 1 or 2) for each channel of the MCP4728 device.
 *
 * returns uint8_t Error code (0 for success)
 */
uint8_t mcp4728_gainSelect (
    I2C_TypeDef *bus, // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,     // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_gain_selection_t gain_select_bits
)
{
    uint8_t data[1] = {MCP4728_GAIN_WRITE | (gain_select_bits & 0x0F)};
    return !i2c_write(bus, addr, data, sizeof(data));
}

/*
 * This function sets the power down mode for the specified channels of the
 * MCP4728 device. The power down mode determines the output impedance of the
 * channels.
 *
 * returns uint8_t Error code (0 for success)
 */
uint8_t mcp4728_writePwrDownSelect (
    I2C_TypeDef *bus, // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,     // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_power_mode_selection_t power_modes
)
{
    uint8_t data[2] = {
        MCP4728_PWRDOWN_WRITE | (power_modes >> 4), // C2, C1, C0, X, PD1A x2, PD1C x2
        power_modes << 4                            // PD1C x2, PD1D x2, X x4
    };
    return !i2c_write(bus, addr, data, sizeof(data));
}

/*
 * This function sends a general call command to all i2c devices on the bus. General
 * call commands can be used to perform actions such as reset or update all DAC
 * channels.
 *
 * returns uint8_t Error code (0 for success)
 */
uint8_t mcp4728_generalCall (
    I2C_TypeDef *bus,                 // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    mcp4728_general_command_t command // General call command to send
)
{
    // TODO
}

/*
 * This function performs a fast write operation on the MCP4728 device, updating
 * the DAC output values for all four channels in a single I2C transaction. Gain, voltage
 * reference, power mode options are not configurable. They are not changed in
 * the transaction. Prior values are used.
 * While the chip can accept power-mode settings in this call, we don't use them
 * for our purposes so they are omitted from the arguments
 *
 * returns uint8_t Error code (0 for success)
 */
uint8_t mcp4728_fastWrite (
    I2C_TypeDef *bus,                // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,                    // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_output_value_t output_a, // 12-bit output value for channel A
    mcp4728_output_value_t output_b, // 12-bit output value for channel B
    mcp4728_output_value_t output_c, // 12-bit output value for channel C
    mcp4728_output_value_t output_d  // 12-bit output value for channel D
)
{
    // Mask to 12 least-significant bits
    output_a = output_a & 0x0FFF;
    output_b = output_b & 0x0FFF;
    output_c = output_c & 0x0FFF;
    output_d = output_d & 0x0FFF;

    uint8_t data[8] = {
        MCP4728_FAST_WRITE | (uint8_t)(output_a >> 8),
        (uint8_t)output_a,
        (uint8_t)output_b >> 8,
        (uint8_t)output_b,
        (uint8_t)output_c >> 8,
        (uint8_t)output_c,
        (uint8_t)output_d >> 8,
        (uint8_t)output_d,
    };
    return !i2c_write(bus, addr, data, sizeof(data));
}

/*
 * This function performs a multi-write operation on the MCP4728 device,
 * updating the DAC output values for all specified channels in a single I2C
 * transaction. It then sends a general call command to update the outputs.
 *
 * returns uint8_t Error code (0 for success)
 */
uint8_t mcp4728_multiWrite (
    I2C_TypeDef *bus,                             // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,                                 // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_single_channel_config_t *config_list, // Ordered configuration structures containing DAC values
    uint8_t *channel_list,                        // Ordered channel numbers corresponding to config_list
    uint8_t channel_count                         // Number of channels in config_list and channel_list
)
{
#define MCP4728_MULTIWRITE_BYTES_PER_CHANNEL 3
    // 3 bytes per channel message for this command type
    uint8_t data_bytes = channel_count * MCP4728_MULTIWRITE_BYTES_PER_CHANNEL;
    uint8_t PLEASE_DONT_LEAK_MEMORY data_buffer[16];

    for (uint8_t i = 0; i < channel_count; i++)
    {
        uint8_t channel = (channel_list[i] & 0x03);
        mcp4728_single_channel_config_t config = config_list[i];

        uint8_t data_tmp[MCP4728_MULTIWRITE_BYTES_PER_CHANNEL] = {
            MCP4728_MULTI_WRITE | (channel << 1) | 1,

            (config.internal_vref << 7) |            // VREF in msb
                (config.power_mode_selection << 5) | // PD1, PD0
                (config.double_gain << 4) |          // gain (Gx)
                (uint8_t)(config.val >> 8),          // first 4 bits of output val

            (uint8_t)config.val, // last 8 bits of output val
        };

        PLEASE_DONT_SEGFAULT memcpy(
            data_buffer + i * MCP4728_MULTIWRITE_BYTES_PER_CHANNEL,
            data_tmp,
            MCP4728_MULTIWRITE_BYTES_PER_CHANNEL
        );
    }
    uint8_t i2c_write_result = i2c_write(bus, addr, data_buffer, data_bytes);

    return !i2c_write_result;
}

/*
 * This function performs a sequential write operation on the MCP4728 device,
 * updating the DAC output values for all specified channels in a single I2C
 * transaction. It then sends a general call command to update the outputs.
 * This command also updates the DAC's EEPROM
 *
 * returns uint8_t Error code (0 for success)
 */
uint8_t mcp4728_sequentialWrite (
    I2C_TypeDef *bus,                             // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,                                 // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_single_channel_config_t *config_list, // Ordered configuration structures containing DAC values
    uint8_t channel_count,                        // Number of channels in config_list
    uint8_t start_channel                         // The first channel number in the sequence before incrementing
)
{
#define MCP4728_SEQWRITE_BYTES_PER_CHANNEL 2
    uint8_t data_bytes = 1 + channel_count * MCP4728_SEQWRITE_BYTES_PER_CHANNEL;
    uint8_t PLEASE_DONT_LEAK_MEMORY data_buffer[16];

    // First byte
    data_buffer[0] = MCP4728_SEQ_WRITE | ((start_channel & 0x03) << 1) | 1;

    // Remaining bytes (pairs of 2)
    for (uint8_t i = 0; i < channel_count; i++)
    {
        mcp4728_single_channel_config_t config = config_list[i];

        uint8_t data_tmp[MCP4728_SEQWRITE_BYTES_PER_CHANNEL] = {
            (config.internal_vref << 7) |            // VREF in msb
                (config.power_mode_selection << 5) | // PD1, PD0
                (config.double_gain << 4) |          // gain (Gx)
                (uint8_t)(config.val >> 8),          // first 4 bits of output val

            (uint8_t)config.val, // last 8 bits of output val
        };

        PLEASE_DONT_SEGFAULT memcpy(
            data_buffer + 1 + i * MCP4728_SEQWRITE_BYTES_PER_CHANNEL,
            data_tmp,
            MCP4728_SEQWRITE_BYTES_PER_CHANNEL
        );
    }
    uint8_t i2c_write_result = i2c_write(bus, addr, data_buffer, data_bytes);

    return !i2c_write_result;
}

/*
 * This function performs a single write operation on a specified channel of the
 * MCP4728 device, updating the DAC output value for that channel. It then sends
 * a general call command to update the output.
 *
 * returns uint8_t Error code (0 for success)
 */
uint8_t mcp4728_singleWrite (
    I2C_TypeDef *bus,                       // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t addr,                           // 7-bit addr of the DAC e.g 0x60, 0x61 ... 0x64
    mcp4728_single_channel_config_t config, // Configuration structure containing DAC values
    uint8_t channel                         // The channel to be updated (0 for A, 1 for B, 2 for C, 3 for D)
)
{
    uint8_t data[3] = {
        MCP4728_SINGLE_WRITE | (uint8_t)(channel << 1) | 1,
        (config.internal_vref << 7) |            // VREF in msb
            (config.power_mode_selection << 5) | // PD1, PD0
            (config.double_gain << 4) |          // gain (Gx)
            (uint8_t)(config.val >> 8),          // first 4 bits of output val

        (uint8_t)config.val, // last 8 bits of output val

    };
    return !i2c_write(bus, addr, data, sizeof(data));
}

/*
 * Functions related to changing the address of an mcp4728
 * with its LDAC pin connected to the DAC shift register line
 * returns uint8_t Error code (0 for success)
 */
void mcp4728_output_byte_callback (uint8_t idx)
{
    if (idx != 1) return;
    latch_out(false);
}

uint8_t mcp4728_init_single_address (
    I2C_TypeDef *bus,     // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t ldac_pin_idx, // index on the connected shift-register to which the LDAC pin of this chip is connected
    uint8_t new_addr,     // new 3-bit address of the selected DAC
    uint8_t old_addr      // old 3-bit address of the selected DAC
)
{


    // TODO: double check that CHANNELS matches the number of shift registers connected to the DACs
    // TODO also: validate that (0x01 << ldac_pin_idx) ^ 0xFF changes the correct pin on the shift register
    for(uint8_t i = 0; i < CHANNELS; i++){
        shift_byte(0xFF);
    }
    latch_out(false);

    for(uint8_t i = 0; i < CHANNELS; i++){
        shift_byte((0x01 << ldac_pin_idx) ^ 0xFF); // Prepare LDAC pin to be latched
    }

    uint8_t data[3] = {
        MCP4728_ADDR_WRITE | ((old_addr & 0x07) << 2) | 0x1, //1
        MCP4728_ADDR_WRITE | ((new_addr & 0x07) << 2) | 0x2, //1
        MCP4728_ADDR_WRITE | ((new_addr & 0x07) << 2) | 0x3, //1
    };

    //uint8_t i2c_result = i2c_write_callback(bus, old_addr, data, sizeof(data), &mcp4728_output_byte_callback);

    for(uint8_t i = 0; i < CHANNELS; i++){
        shift_byte(0xFF);
    }
    latch_out(false);

    return 1;
}

uint8_t mcp4728_init_address (
    I2C_TypeDef *bus,     // I2C bus (I2C1 ... I2C4 of I2C_TypeDef)
    uint8_t ldac_pin_idx, // index on the connected shift-register to which the LDAC pin of this chip is connected
    uint8_t new_addr      // new 3-bit address of the selected DAC
)
{
    uint8_t folded_result = 1;
    for (uint8_t target_addr = 0x60; target_addr < 0x68; target_addr++)
    {
        folded_result &= mcp4728_init_single_address(bus, ldac_pin_idx, new_addr, target_addr);
    }

    return folded_result;
}

uint8_t mcp4728_init_all_addresses (void)
{

    uint8_t folded_result = 1;

    bb_claim(I2C1_Clock_GPIO_Port, I2C1_Clock_Pin, I2C1_Data_GPIO_Port, I2C1_Data_Pin);
    bb_claim(I2C2_Clock_GPIO_Port, I2C2_Clock_Pin, I2C2_Data_GPIO_Port, I2C2_Data_Pin);
    bb_claim(I2C3_Clock_GPIO_Port, I2C3_Clock_Pin, I2C3_Data_GPIO_Port, I2C3_Data_Pin);
    bb_claim(I2C4_Clock_GPIO_Port, I2C4_Clock_Pin, I2C4_Data_GPIO_Port, I2C4_Data_Pin);

    // init all addresses one 

    bb_release(I2C1_Clock_GPIO_Port, I2C1_Clock_Pin, I2C1_Data_GPIO_Port, I2C1_Data_Pin, 4); // alternate function id
    bb_release(I2C2_Clock_GPIO_Port, I2C2_Clock_Pin, I2C2_Data_GPIO_Port, I2C2_Data_Pin, 4);
    bb_release(I2C3_Clock_GPIO_Port, I2C3_Clock_Pin, I2C3_Data_GPIO_Port, I2C3_Data_Pin, 8);
    bb_release(I2C4_Clock_GPIO_Port, I2C4_Clock_Pin, I2C4_Data_GPIO_Port, I2C4_Data_Pin, 8);

    return folded_result;
}

void pin_set (
    GPIO_TypeDef *gpio_port, // GPIOA, GPIOB, ...
    uint8_t pin, // 0-15
    bool high
)
{
    if (high)
    {
        gpio_port->BSRR = (1U << pin);
    } else { // set low
        gpio_port->BSRR = ((1U << pin) << 16);
    }
}
