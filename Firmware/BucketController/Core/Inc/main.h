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
#define Disp3_CS_Pin GPIO_PIN_14
#define Disp3_CS_GPIO_Port GPIOC
#define Disp4_CS_Pin GPIO_PIN_15
#define Disp4_CS_GPIO_Port GPIOC
#define __OSC_IN_Pin GPIO_PIN_0
#define __OSC_IN_GPIO_Port GPIOF
#define __OSC_OUT_Pin GPIO_PIN_1
#define __OSC_OUT_GPIO_Port GPIOF
#define SPI1_CS_IN_Pin GPIO_PIN_0
#define SPI1_CS_IN_GPIO_Port GPIOC
#define LED_PWM_Pin GPIO_PIN_1
#define LED_PWM_GPIO_Port GPIOC
#define Disp_PWM_Pin GPIO_PIN_2
#define Disp_PWM_GPIO_Port GPIOC
#define Comp1_ADC1_C9_Pin GPIO_PIN_3
#define Comp1_ADC1_C9_GPIO_Port GPIOC
#define SHIFTREG_Latch_Pin GPIO_PIN_0
#define SHIFTREG_Latch_GPIO_Port GPIOA
#define Fader1_MotB_Pin GPIO_PIN_1
#define Fader1_MotB_GPIO_Port GPIOA
#define __USART2_TX_Pin GPIO_PIN_2
#define __USART2_TX_GPIO_Port GPIOA
#define __USART2_RX_Pin GPIO_PIN_3
#define __USART2_RX_GPIO_Port GPIOA
#define Fader3_MotB_Pin GPIO_PIN_4
#define Fader3_MotB_GPIO_Port GPIOA
#define Fader1_MotA_Pin GPIO_PIN_5
#define Fader1_MotA_GPIO_Port GPIOA
#define Fader3_MotA_Pin GPIO_PIN_6
#define Fader3_MotA_GPIO_Port GPIOA
#define Comp3_ADC2_C5_Pin GPIO_PIN_4
#define Comp3_ADC2_C5_GPIO_Port GPIOC
#define Comp4_ADC2_C11_Pin GPIO_PIN_5
#define Comp4_ADC2_C11_GPIO_Port GPIOC
#define Fader4_MotA_Pin GPIO_PIN_0
#define Fader4_MotA_GPIO_Port GPIOB
#define Comp2_ADC_C12_Pin GPIO_PIN_1
#define Comp2_ADC_C12_GPIO_Port GPIOB
#define Disp_DC_Pin GPIO_PIN_2
#define Disp_DC_GPIO_Port GPIOB
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
#define SPI3_Clock_Pin GPIO_PIN_10
#define SPI3_Clock_GPIO_Port GPIOC
#define SPI3_MISO_Pin GPIO_PIN_11
#define SPI3_MISO_GPIO_Port GPIOC
#define SPI3_MOSI_Pin GPIO_PIN_12
#define SPI3_MOSI_GPIO_Port GPIOC
#define SPI3_CS1_Pin GPIO_PIN_2
#define SPI3_CS1_GPIO_Port GPIOD
#define SPI3_CS0_Pin GPIO_PIN_3
#define SPI3_CS0_GPIO_Port GPIOB
#define I2C3_Data_Pin GPIO_PIN_5
#define I2C3_Data_GPIO_Port GPIOB
#define Disp2_CS_Pin GPIO_PIN_6
#define Disp2_CS_GPIO_Port GPIOB
#define Disp1_CS_Pin GPIO_PIN_7
#define Disp1_CS_GPIO_Port GPIOB
#define I2C1_Data_Pin GPIO_PIN_9
#define I2C1_Data_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
