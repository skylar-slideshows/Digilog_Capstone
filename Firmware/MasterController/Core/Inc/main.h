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
#define _OSC_IN_Pin GPIO_PIN_0
#define _OSC_IN_GPIO_Port GPIOF
#define _OSC_OUT_Pin GPIO_PIN_1
#define _OSC_OUT_GPIO_Port GPIOF
#define CS_Bucket0_Pin GPIO_PIN_0
#define CS_Bucket0_GPIO_Port GPIOC
#define LED_PWM_Pin GPIO_PIN_1
#define LED_PWM_GPIO_Port GPIOC
#define Disp_PWM_Pin GPIO_PIN_2
#define Disp_PWM_GPIO_Port GPIOC
#define CS_Bucket1_Pin GPIO_PIN_0
#define CS_Bucket1_GPIO_Port GPIOA
#define CS_Bucket2_Pin GPIO_PIN_1
#define CS_Bucket2_GPIO_Port GPIOA
#define CS_Bucket3_Pin GPIO_PIN_2
#define CS_Bucket3_GPIO_Port GPIOA
#define __MASTER_Clock_Pin GPIO_PIN_5
#define __MASTER_Clock_GPIO_Port GPIOA
#define __MASTER_MISO_Pin GPIO_PIN_6
#define __MASTER_MISO_GPIO_Port GPIOA
#define __MASTER_MOSI_Pin GPIO_PIN_7
#define __MASTER_MOSI_GPIO_Port GPIOA
#define CS_Bucket4_Pin GPIO_PIN_4
#define CS_Bucket4_GPIO_Port GPIOC
#define CS_Bucket5_Pin GPIO_PIN_5
#define CS_Bucket5_GPIO_Port GPIOC
#define Disp_CS_Pin GPIO_PIN_12
#define Disp_CS_GPIO_Port GPIOB
#define Disp_Clock_Pin GPIO_PIN_13
#define Disp_Clock_GPIO_Port GPIOB
#define Disp_DC_Pin GPIO_PIN_14
#define Disp_DC_GPIO_Port GPIOB
#define Disp_MOSI_Pin GPIO_PIN_15
#define Disp_MOSI_GPIO_Port GPIOB
#define CS_Bucket6_Pin GPIO_PIN_6
#define CS_Bucket6_GPIO_Port GPIOC
#define CS_Bucket7_Pin GPIO_PIN_7
#define CS_Bucket7_GPIO_Port GPIOC
#define I2C2_Data_Pin GPIO_PIN_8
#define I2C2_Data_GPIO_Port GPIOA
#define I2C2_Clock_Pin GPIO_PIN_9
#define I2C2_Clock_GPIO_Port GPIOA
#define SWD_SWDIO_Pin GPIO_PIN_13
#define SWD_SWDIO_GPIO_Port GPIOA
#define SWD_SWCLK_Pin GPIO_PIN_14
#define SWD_SWCLK_GPIO_Port GPIOA
#define I2C1_Clock_Pin GPIO_PIN_15
#define I2C1_Clock_GPIO_Port GPIOA
#define SC_SCK_Pin GPIO_PIN_10
#define SC_SCK_GPIO_Port GPIOC
#define SD_MISO_Pin GPIO_PIN_11
#define SD_MISO_GPIO_Port GPIOC
#define SD_MOSI_Pin GPIO_PIN_12
#define SD_MOSI_GPIO_Port GPIOC
#define SD_CS_Pin GPIO_PIN_2
#define SD_CS_GPIO_Port GPIOD
#define I2C1_Data_Pin GPIO_PIN_7
#define I2C1_Data_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
