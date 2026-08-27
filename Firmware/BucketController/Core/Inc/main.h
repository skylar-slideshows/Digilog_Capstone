/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define _DAC_INIT_Latch_Pin GPIO_PIN_13
#define _DAC_INIT_Latch_GPIO_Port GPIOC
#define _OSC_IN_Pin GPIO_PIN_0
#define _OSC_IN_GPIO_Port GPIOF
#define _OSC_OUT_Pin GPIO_PIN_1
#define _OSC_OUT_GPIO_Port GPIOF
#define __MASTER__CS_in_Pin GPIO_PIN_0
#define __MASTER__CS_in_GPIO_Port GPIOC
#define LED_PWM_Pin GPIO_PIN_1
#define LED_PWM_GPIO_Port GPIOC
#define Disp_PWM_Pin GPIO_PIN_2
#define Disp_PWM_GPIO_Port GPIOC
#define Fader1_Touch_Pin GPIO_PIN_3
#define Fader1_Touch_GPIO_Port GPIOC
#define Fader1_ADC_Pin GPIO_PIN_0
#define Fader1_ADC_GPIO_Port GPIOA
#define Fader2_ADC_Pin GPIO_PIN_1
#define Fader2_ADC_GPIO_Port GPIOA
#define Fader3_MotB_Pin GPIO_PIN_4
#define Fader3_MotB_GPIO_Port GPIOA
#define __MASTER__Clock_Pin GPIO_PIN_5
#define __MASTER__Clock_GPIO_Port GPIOA
#define Fader3_MotA_Pin GPIO_PIN_6
#define Fader3_MotA_GPIO_Port GPIOA
#define __MASTER__MOSI_Pin GPIO_PIN_7
#define __MASTER__MOSI_GPIO_Port GPIOA
#define MCP23S17_CS_Pin GPIO_PIN_5
#define MCP23S17_CS_GPIO_Port GPIOC
#define Fader4_MotA_Pin GPIO_PIN_0
#define Fader4_MotA_GPIO_Port GPIOB
#define Fader3_Touch_Pin GPIO_PIN_1
#define Fader3_Touch_GPIO_Port GPIOB
#define Fader4_Touch_Pin GPIO_PIN_2
#define Fader4_Touch_GPIO_Port GPIOB
#define Fader2_MotA_Pin GPIO_PIN_10
#define Fader2_MotA_GPIO_Port GPIOB
#define LED_Data_Pin GPIO_PIN_11
#define LED_Data_GPIO_Port GPIOB
#define LED_Clock_Pin GPIO_PIN_12
#define LED_Clock_GPIO_Port GPIOB
#define Disp_Clock_Pin GPIO_PIN_13
#define Disp_Clock_GPIO_Port GPIOB
#define LED_Latch_Pin GPIO_PIN_14
#define LED_Latch_GPIO_Port GPIOB
#define Disp_MOSI_Pin GPIO_PIN_15
#define Disp_MOSI_GPIO_Port GPIOB
#define I2C4_Clock_Pin GPIO_PIN_6
#define I2C4_Clock_GPIO_Port GPIOC
#define I2C4_Data_Pin GPIO_PIN_7
#define I2C4_Data_GPIO_Port GPIOC
#define I2C3_Clock_Pin GPIO_PIN_8
#define I2C3_Clock_GPIO_Port GPIOC
#define Fader4_MotB_Pin GPIO_PIN_9
#define Fader4_MotB_GPIO_Port GPIOC
#define I2C2_Data_Pin GPIO_PIN_8
#define I2C2_Data_GPIO_Port GPIOA
#define I2C2_Clock_Pin GPIO_PIN_9
#define I2C2_Clock_GPIO_Port GPIOA
#define Fader2_MotB_Pin GPIO_PIN_10
#define Fader2_MotB_GPIO_Port GPIOA
#define _DAC_INIT_Clock_Pin GPIO_PIN_11
#define _DAC_INIT_Clock_GPIO_Port GPIOA
#define _DAC_INIT_Data_Pin GPIO_PIN_12
#define _DAC_INIT_Data_GPIO_Port GPIOA
#define Fader1_MotA_Pin GPIO_PIN_15
#define Fader1_MotA_GPIO_Port GPIOA
#define FlashADC_Clock_Pin GPIO_PIN_10
#define FlashADC_Clock_GPIO_Port GPIOC
#define FlashADC_MISO_Pin GPIO_PIN_11
#define FlashADC_MISO_GPIO_Port GPIOC
#define FlashADC_MOSI_Pin GPIO_PIN_12
#define FlashADC_MOSI_GPIO_Port GPIOC
#define Fader2_Touch_Pin GPIO_PIN_2
#define Fader2_Touch_GPIO_Port GPIOD
#define Fader1_MotB_Pin GPIO_PIN_3
#define Fader1_MotB_GPIO_Port GPIOB
#define __MASTER__MISO_Pin GPIO_PIN_4
#define __MASTER__MISO_GPIO_Port GPIOB
#define I2C3_Data_Pin GPIO_PIN_5
#define I2C3_Data_GPIO_Port GPIOB
#define FlashADC_CS_ADC_Pin GPIO_PIN_6
#define FlashADC_CS_ADC_GPIO_Port GPIOB
#define FlashADC_CS_Flash_Pin GPIO_PIN_7
#define FlashADC_CS_Flash_GPIO_Port GPIOB
#define I2C1_Clock_Pin GPIO_PIN_8
#define I2C1_Clock_GPIO_Port GPIOB
#define I2C1_Data_Pin GPIO_PIN_9
#define I2C1_Data_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
