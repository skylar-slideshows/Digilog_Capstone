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
#define __OSC_IN_Pin GPIO_PIN_0
#define __OSC_IN_GPIO_Port GPIOF
#define __OSC_OUT_Pin GPIO_PIN_1
#define __OSC_OUT_GPIO_Port GPIOF
#define F1_ADC_Pin GPIO_PIN_0
#define F1_ADC_GPIO_Port GPIOC
#define LED_PWM_Pin GPIO_PIN_1
#define LED_PWM_GPIO_Port GPIOC
#define F2_ADC_Pin GPIO_PIN_2
#define F2_ADC_GPIO_Port GPIOC
#define F3_ADC_Pin GPIO_PIN_3
#define F3_ADC_GPIO_Port GPIOC
#define SHIFTREG_Latch_Pin GPIO_PIN_0
#define SHIFTREG_Latch_GPIO_Port GPIOA
#define F0_MB_Pin GPIO_PIN_1
#define F0_MB_GPIO_Port GPIOA
#define __USART2_TX_Pin GPIO_PIN_2
#define __USART2_TX_GPIO_Port GPIOA
#define __USART2_RX_Pin GPIO_PIN_3
#define __USART2_RX_GPIO_Port GPIOA
#define F2_MB_Pin GPIO_PIN_4
#define F2_MB_GPIO_Port GPIOA
#define F0_MA_Pin GPIO_PIN_5
#define F0_MA_GPIO_Port GPIOA
#define F2_MA_Pin GPIO_PIN_6
#define F2_MA_GPIO_Port GPIOA
#define F0_ADC_Pin GPIO_PIN_4
#define F0_ADC_GPIO_Port GPIOC
#define F3_Touch_Pin GPIO_PIN_5
#define F3_Touch_GPIO_Port GPIOC
#define F3_MA_Pin GPIO_PIN_0
#define F3_MA_GPIO_Port GPIOB
#define F3_MB_Pin GPIO_PIN_1
#define F3_MB_GPIO_Port GPIOB
#define F2_Touch_Pin GPIO_PIN_2
#define F2_Touch_GPIO_Port GPIOB
#define F1_MA_Pin GPIO_PIN_10
#define F1_MA_GPIO_Port GPIOB
#define F1_MB_Pin GPIO_PIN_11
#define F1_MB_GPIO_Port GPIOB
#define F1_Touch_Pin GPIO_PIN_12
#define F1_Touch_GPIO_Port GPIOB
#define LED_Clock_Pin GPIO_PIN_13
#define LED_Clock_GPIO_Port GPIOB
#define LED_Latch_Pin GPIO_PIN_14
#define LED_Latch_GPIO_Port GPIOB
#define LED_Data_Pin GPIO_PIN_15
#define LED_Data_GPIO_Port GPIOB
#define I2C4_Clock_Pin GPIO_PIN_6
#define I2C4_Clock_GPIO_Port GPIOC
#define I2C4_Data_Pin GPIO_PIN_7
#define I2C4_Data_GPIO_Port GPIOC
#define I2C3_Clock_Pin GPIO_PIN_8
#define I2C3_Clock_GPIO_Port GPIOC
#define I2C3_Data_Pin GPIO_PIN_9
#define I2C3_Data_GPIO_Port GPIOC
#define I2C2_Data_Pin GPIO_PIN_8
#define I2C2_Data_GPIO_Port GPIOA
#define I2C2_Clock_Pin GPIO_PIN_9
#define I2C2_Clock_GPIO_Port GPIOA
#define F0_Touch_Pin GPIO_PIN_10
#define F0_Touch_GPIO_Port GPIOA
#define SHIFTREG_Clock_Pin GPIO_PIN_11
#define SHIFTREG_Clock_GPIO_Port GPIOA
#define SHIFTREG_Data_Pin GPIO_PIN_12
#define SHIFTREG_Data_GPIO_Port GPIOA
#define __SWDIO_Pin GPIO_PIN_13
#define __SWDIO_GPIO_Port GPIOA
#define __SWCLK_Pin GPIO_PIN_14
#define __SWCLK_GPIO_Port GPIOA
#define I2C1_Clock_Pin GPIO_PIN_15
#define I2C1_Clock_GPIO_Port GPIOA
#define LDAC4_Pin GPIO_PIN_10
#define LDAC4_GPIO_Port GPIOC
#define LDAC3_Pin GPIO_PIN_11
#define LDAC3_GPIO_Port GPIOC
#define LDAC2_Pin GPIO_PIN_12
#define LDAC2_GPIO_Port GPIOC
#define LDAC1_Pin GPIO_PIN_2
#define LDAC1_GPIO_Port GPIOD
#define SPI1_Clock_Pin GPIO_PIN_3
#define SPI1_Clock_GPIO_Port GPIOB
#define SPI1_CS_IN_Pin GPIO_PIN_5
#define SPI1_CS_IN_GPIO_Port GPIOB
#define LDAC0_Pin GPIO_PIN_6
#define LDAC0_GPIO_Port GPIOB
#define I2C1_Data_Pin GPIO_PIN_9
#define I2C1_Data_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
