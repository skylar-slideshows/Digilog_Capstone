/**
  **********************************************************************************
  * TINYUSB CONFIGURATION - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file tusb_config.h
  * @brief TinyUSB settings for the STM32G474: device stack only, FreeRTOS, one USB MIDI
  *        interface.
  *
  * TinyUSB includes this file by name, so it must sit in a folder that is already on the
  * include path (Core/Inc works).
  **********************************************************************************
*/

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#define CFG_TUSB_MCU            OPT_MCU_STM32G4  // selects the stm32_fsdev driver and the G4's interrupts
#define CFG_TUSB_OS             OPT_OS_FREERTOS
#define CFG_TUSB_DEBUG          0                // 1-3 prints the stack's log through printf

#define CFG_TUD_ENABLED         1                // device stack; the host stack stays out of the build
#define CFG_TUD_ENDPOINT0_SIZE  64

// One MIDI interface. It can carry up to 16 virtual cables (ports), e.g. Mackie Control plus timecode.
#define CFG_TUD_MIDI            1
#define CFG_TUD_MIDI_RX_BUFSIZE 256              // bytes; the DAW sends its feedback in bursts
#define CFG_TUD_MIDI_TX_BUFSIZE 256

// The remote app's serial port, on the same USB connection as MIDI (see usb_descriptors.c).
// An acknowledgement goes out in one write, so the transmit buffer bounds the longest command
// that can be acknowledged (as the bucket's 256-byte ring does).
#define CFG_TUD_CDC             1
#define CFG_TUD_CDC_RX_BUFSIZE  512
#define CFG_TUD_CDC_TX_BUFSIZE  1024

// TinyUSB's FreeRTOS layer calls pdTICKS_TO_MS(), which FreeRTOS only gained after V10.3.1,
// the version CubeMX ships for the G4. Delete this if CubeMX moves to a FreeRTOS that has it.
#ifndef pdTICKS_TO_MS
#define pdTICKS_TO_MS(xTicks) ((TickType_t)(((uint64_t)(xTicks) * 1000U) / configTICK_RATE_HZ))
#endif

#endif
