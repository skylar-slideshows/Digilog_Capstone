/** @file mcp4728.h */

/*
 * Copied from https://github.com/arda-kara/MCP4728-DAC-HAL-Driver
 * 06a2047954bc5d6053fb7c4ba4d271389b37b41e
 * on 2026/08/27
 * With minor changes made
 */
 
/*
 * mcp4728.h
 *
 *  Created on: Jul 9, 2024
 *      Author: Arda
 */

#ifndef INC_MCP4728_H_
#define INC_MCP4728_H_

#include "stm32g4xx.h"

/* MCP4728 Channel Configuration Structure */
typedef struct {
    uint8_t vref;      /* 4-bit reference voltage info: 1=2.048V, 0=VDD */
    uint8_t gain;      /* 4-bit gain info: 1=x2, 0=x1 */
    uint16_t val[4];   /* 12-bit numbers specifying outputs for channels A, B, C, D */
} ChannelConfig;

/* Function Prototypes */
HAL_StatusTypeDef mcp4728_vrefSelect(I2C_HandleTypeDef *i2cHandler, uint8_t mcp4728_base_addr, ChannelConfig config);
HAL_StatusTypeDef mcp4728_gainSelect(I2C_HandleTypeDef *i2cHandler, uint8_t mcp4728_base_addr, ChannelConfig config);
HAL_StatusTypeDef mcp4728_writePwrDownSelect(I2C_HandleTypeDef *i2cHandler, uint8_t mcp4728_base_addr, uint8_t power_modes);
HAL_StatusTypeDef mcp4728_generalCall(I2C_HandleTypeDef *i2cHandler, uint8_t command);
HAL_StatusTypeDef mcp4728_fastWrite(I2C_HandleTypeDef *i2cHandler, uint8_t mcp4728_base_addr, ChannelConfig config);
HAL_StatusTypeDef mcp4728_multiWrite(I2C_HandleTypeDef *i2cHandler, uint8_t mcp4728_base_addr, ChannelConfig config, uint8_t channel);
HAL_StatusTypeDef mcp4728_sequentialWrite(I2C_HandleTypeDef *i2cHandler, uint8_t mcp4728_base_addr, ChannelConfig config, uint8_t channel);
HAL_StatusTypeDef mcp4728_singleWrite(I2C_HandleTypeDef *i2cHandler, uint8_t mcp4728_base_addr, ChannelConfig config, uint8_t channel);
HAL_StatusTypeDef mcp4728_newI2CAddress(I2C_HandleTypeDef *i2cHandler, uint8_t mcp4728_base_addr, uint8_t address);
HAL_StatusTypeDef mcp4728_configure(I2C_HandleTypeDef *i2cHandler, uint8_t mcp4728_base_addr, ChannelConfig config);

/* Commands and Modes */
#define MCP4728_GENERAL_RESET           0x06
#define MCP4728_GENERAL_WKUP            0x09
#define MCP4728_GENERAL_SOFTWARE_UPDATE 0x08
#define MCP4728_GENERAL_READ_ADDR       0x0C

#define MCP4728_FAST_WRITE              0x00
#define MCP4728_MULTI_WRITE             0x40
#define MCP4728_SEQ_WRITE               0x50
#define MCP4728_SINGLE_WRITE            0x58
#define MCP4728_ADDR_WRITE              0x60
#define MCP4728_VREF_WRITE              0x80
#define MCP4728_GAIN_WRITE              0xC0
#define MCP4728_PWRDOWN_WRITE           0xA0

// #define MCP4728_BASE_ADDR               (0x60 << 1) // 7-bit address shifted left

#define MCP4728_GAIN_1                  0x0
#define MCP4728_GAIN_2                  0x1

#define MCP4728_CHANNEL_A               0x0
#define MCP4728_CHANNEL_B               0x1
#define MCP4728_CHANNEL_C               0x2
#define MCP4728_CHANNEL_D               0x3

#define MCP4728_PWRDWN_NORMAL           0x0
#define MCP4728_PWRDWN_1                0x1
#define MCP4728_PWRDWN_2                0x2
#define MCP4728_PWRDWN_3                0x3

#define MCP4728_UDAC_UPLOAD             0x1
#define MCP4728_UDAC_NOLOAD             0x0

#endif /* INC_MCP4728_H_ */
