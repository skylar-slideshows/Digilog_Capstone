/**
  **********************************************************************************
  * USB DEVICE TASK - DIGILOG CONSOLE (Master controler)
  **********************************************************************************
  * @file FreeRTOS_apps/usb_device_task.c
  * @brief Runs TinyUSB on the STM32G474's full-speed USB port under FreeRTOS.
  *
  * The USB interrupts only record what happened. The work (enumeration, MIDI transfers and
  * TinyUSB's callbacks) happens in usb_task, which sleeps inside tud_task() until there is some.
  *
  * Written against TinyUSB's current API: tusb_init(rhport, &init) and tusb_int_handler().
  * Older versions use tusb_init() with no arguments and tud_int_handler(0). If these calls
  * don't compile, copy what examples/device/midi_test/src/main.c in your clone does.
  **********************************************************************************
*/

#include "FreeRTOS_apps/usb_device_task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tusb.h"

#define USB_RHPORT           0                          // the G4 has one USB port
#define USB_TASK_STACK_WORDS 256
#define USB_TASK_PRIORITY    (configMAX_PRIORITIES - 2) // MIDI timing matters; keep it near the top


/*=============================== INTERRUPTS ================================*/

// TinyUSB enables all three. In CubeMX, untick "Generate IRQ handler" for the USB interrupts
// (System Core > NVIC > Code generation); otherwise stm32g4xx_it.c defines them a second time.

void USB_HP_IRQHandler (void)
{
    tusb_int_handler(USB_RHPORT, true);
}

void USB_LP_IRQHandler (void)
{
    tusb_int_handler(USB_RHPORT, true);
}

void USBWakeUp_IRQHandler (void)
{
    tusb_int_handler(USB_RHPORT, true);
}


/*=============================== TASK ================================*/

static void usb_task (void *arg)
{
    (void)arg;

    // TinyUSB has to be started from the task that runs tud_task(), once the scheduler is
    // running, because its interrupt handler posts to a FreeRTOS queue.
    const tusb_rhport_init_t dev_init = {
        .role  = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL,
    };
    tusb_init(USB_RHPORT, &dev_init);

    for (;;)
    {
        // Sleeps until the interrupt hands it work, then runs TinyUSB's callbacks in this task,
        // including tud_midi_rx_cb() when MIDI arrives from the DAW.
        tud_task();
    }
}


/*=============================== PUBLIC FUNCTIONS ================================*/

void usb_device_start (void)
{
    // The 48 MHz USB clock is HSI48, kept accurate by the CRS locking onto the host's 1 ms
    // start-of-frame packets. CubeMX sets those up (RCC: HSI48 on, CRS sync source USB,
    // USB clock from HSI48). This turns on the peripheral's own clock.
    __HAL_RCC_USB_CLK_ENABLE();

    // The handlers call FreeRTOS's FromISR functions, so none of the three may sit above the
    // kernel's limit. One left at its reset priority (0) would outrank it and trip an assert.
    NVIC_SetPriority(USB_HP_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(USB_LP_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_SetPriority(USBWakeUp_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);

    xTaskCreate(usb_task, "usb", USB_TASK_STACK_WORDS, NULL, USB_TASK_PRIORITY, NULL);
}

size_t usb_serial_from_uid (uint16_t *out, size_t max_chars)
{
    // Stands in for board_usb_get_serial(), which TinyUSB's examples take from their own board
    // support code. The 96-bit unique ID becomes 24 hex digits, most significant word first.
    static const char hex[] = "0123456789ABCDEF";
    const uint32_t uid[3] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2()};

    size_t n = 0;
    for (int word = 2; word >= 0; word--)
    {
        for (int shift = 28; shift >= 0 && n < max_chars; shift -= 4)
        {
            out[n++] = (uint16_t)hex[(uid[word] >> shift) & 0xFU];
        }
    }
    return n;
}
