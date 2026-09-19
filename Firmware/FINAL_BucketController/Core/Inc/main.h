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
#define DISP_RST_Pin GPIO_PIN_4
#define DISP_RST_GPIO_Port GPIOE
#define F3_MoB_Pin GPIO_PIN_5
#define F3_MoB_GPIO_Port GPIOE
#define DISP_A0_FMC_Pin GPIO_PIN_10
#define DISP_A0_FMC_GPIO_Port GPIOF
#define __OSC_IN_Pin GPIO_PIN_0
#define __OSC_IN_GPIO_Port GPIOF
#define __OSC_OUT_Pin GPIO_PIN_1
#define __OSC_OUT_GPIO_Port GPIOF
#define MASTER_CS_IN_Pin GPIO_PIN_0
#define MASTER_CS_IN_GPIO_Port GPIOC
#define LED_PWM_Pin GPIO_PIN_1
#define LED_PWM_GPIO_Port GPIOC
#define DISP_PWM_Pin GPIO_PIN_2
#define DISP_PWM_GPIO_Port GPIOC
#define COMP_ADC0_Pin GPIO_PIN_3
#define COMP_ADC0_GPIO_Port GPIOC
#define LED_TEST_Pin GPIO_PIN_2
#define LED_TEST_GPIO_Port GPIOF
#define SHIFTREG_Latch_Pin GPIO_PIN_0
#define SHIFTREG_Latch_GPIO_Port GPIOA
#define F0_MoB_Pin GPIO_PIN_1
#define F0_MoB_GPIO_Port GPIOA
#define __USART2_TX_Pin GPIO_PIN_2
#define __USART2_TX_GPIO_Port GPIOA
#define __USART2_RX_Pin GPIO_PIN_3
#define __USART2_RX_GPIO_Port GPIOA
#define F2_MoB_Pin GPIO_PIN_4
#define F2_MoB_GPIO_Port GPIOA
#define F0_MoA_Pin GPIO_PIN_5
#define F0_MoA_GPIO_Port GPIOA
#define F2_MoA_Pin GPIO_PIN_6
#define F2_MoA_GPIO_Port GPIOA
#define MASTER_MOSI_Pin GPIO_PIN_7
#define MASTER_MOSI_GPIO_Port GPIOA
#define COMP_ADC2_Pin GPIO_PIN_4
#define COMP_ADC2_GPIO_Port GPIOC
#define COMP_ADC3_Pin GPIO_PIN_5
#define COMP_ADC3_GPIO_Port GPIOC
#define F3_MoA_Pin GPIO_PIN_0
#define F3_MoA_GPIO_Port GPIOB
#define COMP_ADC1_Pin GPIO_PIN_1
#define COMP_ADC1_GPIO_Port GPIOB
#define DISP_CS0_Pin GPIO_PIN_2
#define DISP_CS0_GPIO_Port GPIOB
#define DISP_D4_FMC_Pin GPIO_PIN_7
#define DISP_D4_FMC_GPIO_Port GPIOE
#define DISP_D5_FMC_Pin GPIO_PIN_8
#define DISP_D5_FMC_GPIO_Port GPIOE
#define DISP_D6_FMC_Pin GPIO_PIN_9
#define DISP_D6_FMC_GPIO_Port GPIOE
#define DISP_D7_FMC_Pin GPIO_PIN_10
#define DISP_D7_FMC_GPIO_Port GPIOE
#define F3_ADC_Pin GPIO_PIN_11
#define F3_ADC_GPIO_Port GPIOE
#define F0_ADC_Pin GPIO_PIN_13
#define F0_ADC_GPIO_Port GPIOE
#define DISP_CS1_Pin GPIO_PIN_14
#define DISP_CS1_GPIO_Port GPIOE
#define DISP_CS2_Pin GPIO_PIN_15
#define DISP_CS2_GPIO_Port GPIOE
#define F1_MoA_Pin GPIO_PIN_10
#define F1_MoA_GPIO_Port GPIOB
#define LED_Data_Pin GPIO_PIN_11
#define LED_Data_GPIO_Port GPIOB
#define LED_Clock_Pin GPIO_PIN_12
#define LED_Clock_GPIO_Port GPIOB
#define COMP_CV1_Pin GPIO_PIN_13
#define COMP_CV1_GPIO_Port GPIOB
#define LED_Latch_Pin GPIO_PIN_14
#define LED_Latch_GPIO_Port GPIOB
#define DISP_CS3_Pin GPIO_PIN_15
#define DISP_CS3_GPIO_Port GPIOB
#define COMP_CV0_Pin GPIO_PIN_9
#define COMP_CV0_GPIO_Port GPIOD
#define F1_ADC_Pin GPIO_PIN_10
#define F1_ADC_GPIO_Port GPIOD
#define COMP_CV2_Pin GPIO_PIN_11
#define COMP_CV2_GPIO_Port GPIOD
#define COMP_CV3_Pin GPIO_PIN_12
#define COMP_CV3_GPIO_Port GPIOD
#define F2_ADC_Pin GPIO_PIN_13
#define F2_ADC_GPIO_Port GPIOD
#define DISP_D0_FMC_Pin GPIO_PIN_14
#define DISP_D0_FMC_GPIO_Port GPIOD
#define DISP_D1_FMC_Pin GPIO_PIN_15
#define DISP_D1_FMC_GPIO_Port GPIOD
#define I2C4_Clock_Pin GPIO_PIN_6
#define I2C4_Clock_GPIO_Port GPIOC
#define I2C4_Data_Pin GPIO_PIN_7
#define I2C4_Data_GPIO_Port GPIOC
#define I2C3_Clock_Pin GPIO_PIN_8
#define I2C3_Clock_GPIO_Port GPIOC
#define I2C2_Data_Pin GPIO_PIN_8
#define I2C2_Data_GPIO_Port GPIOA
#define I2C2_Clock_Pin GPIO_PIN_9
#define I2C2_Clock_GPIO_Port GPIOA
#define F1_MoB_Pin GPIO_PIN_10
#define F1_MoB_GPIO_Port GPIOA
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
#define F3_Touch_Pin GPIO_PIN_10
#define F3_Touch_GPIO_Port GPIOC
#define F2_Touch_Pin GPIO_PIN_11
#define F2_Touch_GPIO_Port GPIOC
#define F1_Touch_Pin GPIO_PIN_12
#define F1_Touch_GPIO_Port GPIOC
#define DISP_D2_FMC_Pin GPIO_PIN_0
#define DISP_D2_FMC_GPIO_Port GPIOD
#define DISP_D3_FMC_Pin GPIO_PIN_1
#define DISP_D3_FMC_GPIO_Port GPIOD
#define F0_Touch_Pin GPIO_PIN_2
#define F0_Touch_GPIO_Port GPIOD
#define DAC_LDAC4_Pin GPIO_PIN_3
#define DAC_LDAC4_GPIO_Port GPIOD
#define DISP_NOE_FMC_Pin GPIO_PIN_4
#define DISP_NOE_FMC_GPIO_Port GPIOD
#define DISP_NWE_FMC_Pin GPIO_PIN_5
#define DISP_NWE_FMC_GPIO_Port GPIOD
#define DAC_LDAC3_Pin GPIO_PIN_6
#define DAC_LDAC3_GPIO_Port GPIOD
#define DISP_NE1_FMC_Pin GPIO_PIN_7
#define DISP_NE1_FMC_GPIO_Port GPIOD
#define MASTER_Clock_Pin GPIO_PIN_3
#define MASTER_Clock_GPIO_Port GPIOB
#define MASTER_MISO_Pin GPIO_PIN_4
#define MASTER_MISO_GPIO_Port GPIOB
#define I2C3_Data_Pin GPIO_PIN_5
#define I2C3_Data_GPIO_Port GPIOB
#define DAC_LDAC2_Pin GPIO_PIN_6
#define DAC_LDAC2_GPIO_Port GPIOB
#define I2C1_Data_Pin GPIO_PIN_9
#define I2C1_Data_GPIO_Port GPIOB
#define DAC_LDAC1_Pin GPIO_PIN_0
#define DAC_LDAC1_GPIO_Port GPIOE
#define DAC_LDAC0_Pin GPIO_PIN_1
#define DAC_LDAC0_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
