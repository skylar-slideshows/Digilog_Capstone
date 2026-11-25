/**
  **********************************************************************************
  * USB DEVICE TASK - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/usb_device_task.h
  * @brief TinyUSB glue: the USB interrupts, the device task and the serial number.
  **********************************************************************************
*/

#ifndef USB_DEVICE_TASK_H
#define USB_DEVICE_TASK_H

#include <stddef.h>
#include <stdint.h>

// Creates the USB task. Call from main.c before the scheduler starts.
void usb_device_start (void);

// Writes the chip's unique ID as UTF-16 hex digits, for the USB serial-number string.
// Returns the number of characters written. Replaces board_usb_get_serial() in the
// usb_descriptors.c copied from TinyUSB's examples.
size_t usb_serial_from_uid (uint16_t *out, size_t max_chars);

#endif
