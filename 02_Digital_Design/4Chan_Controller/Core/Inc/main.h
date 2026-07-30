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
#define LED_CL_Pin GPIO_PIN_13
#define LED_CL_GPIO_Port GPIOC
#define BTNS_I2C2_DA_Pin GPIO_PIN_0
#define BTNS_I2C2_DA_GPIO_Port GPIOF
#define DISP12_SPI2_CL_Pin GPIO_PIN_1
#define DISP12_SPI2_CL_GPIO_Port GPIOF
#define LED_DA_Pin GPIO_PIN_2
#define LED_DA_GPIO_Port GPIOC
#define LED_LATCH_Pin GPIO_PIN_3
#define LED_LATCH_GPIO_Port GPIOC
#define FADER1_ADC_Pin GPIO_PIN_0
#define FADER1_ADC_GPIO_Port GPIOA
#define FADER2_ADC_Pin GPIO_PIN_1
#define FADER2_ADC_GPIO_Port GPIOA
#define FADER3_ADC_Pin GPIO_PIN_2
#define FADER3_ADC_GPIO_Port GPIOA
#define FADER4_ADC_Pin GPIO_PIN_3
#define FADER4_ADC_GPIO_Port GPIOA
#define FADER2_MOTOR2_Pin GPIO_PIN_4
#define FADER2_MOTOR2_GPIO_Port GPIOA
#define CV_SPI1_CL_Pin GPIO_PIN_5
#define CV_SPI1_CL_GPIO_Port GPIOA
#define FADER2_MOTOR1_Pin GPIO_PIN_6
#define FADER2_MOTOR1_GPIO_Port GPIOA
#define CV_SPI1_MOSI_Pin GPIO_PIN_7
#define CV_SPI1_MOSI_GPIO_Port GPIOA
#define BTNS_I2C2_CL_Pin GPIO_PIN_4
#define BTNS_I2C2_CL_GPIO_Port GPIOC
#define FADER4_MOTOR1_Pin GPIO_PIN_2
#define FADER4_MOTOR1_GPIO_Port GPIOB
#define FADER1_TOUCH_Pin GPIO_PIN_11
#define FADER1_TOUCH_GPIO_Port GPIOB
#define FADER2_TOUCH_Pin GPIO_PIN_12
#define FADER2_TOUCH_GPIO_Port GPIOB
#define FADER3_TOUCH_Pin GPIO_PIN_13
#define FADER3_TOUCH_GPIO_Port GPIOB
#define FADER4_TOUCH_Pin GPIO_PIN_14
#define FADER4_TOUCH_GPIO_Port GPIOB
#define DISP12_SPI2_MOSI_Pin GPIO_PIN_15
#define DISP12_SPI2_MOSI_GPIO_Port GPIOB
#define MASTER_I2C3_CL_Pin GPIO_PIN_8
#define MASTER_I2C3_CL_GPIO_Port GPIOC
#define MASTER_I2C3_DA_Pin GPIO_PIN_9
#define MASTER_I2C3_DA_GPIO_Port GPIOC
#define LED_BRIGHT_PWM_Pin GPIO_PIN_8
#define LED_BRIGHT_PWM_GPIO_Port GPIOA
#define DISP_BRIGHT_PWM_Pin GPIO_PIN_9
#define DISP_BRIGHT_PWM_GPIO_Port GPIOA
#define FADER3_MOTOR1_Pin GPIO_PIN_11
#define FADER3_MOTOR1_GPIO_Port GPIOA
#define FADER3_MOTOR2_Pin GPIO_PIN_12
#define FADER3_MOTOR2_GPIO_Port GPIOA
#define FADER1_MOTOR1_Pin GPIO_PIN_15
#define FADER1_MOTOR1_GPIO_Port GPIOA
#define DISP34_SPI3_CL_Pin GPIO_PIN_10
#define DISP34_SPI3_CL_GPIO_Port GPIOC
#define FADER4_MOTOR2_Pin GPIO_PIN_12
#define FADER4_MOTOR2_GPIO_Port GPIOC
#define FADER1_MOTOR2_Pin GPIO_PIN_3
#define FADER1_MOTOR2_GPIO_Port GPIOB
#define DISP34_SPI3_MOSI_Pin GPIO_PIN_5
#define DISP34_SPI3_MOSI_GPIO_Port GPIOB
#define KNOB_I2C1_DA_Pin GPIO_PIN_7
#define KNOB_I2C1_DA_GPIO_Port GPIOB
#define KNOB_I2C1_CL_Pin GPIO_PIN_8
#define KNOB_I2C1_CL_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
