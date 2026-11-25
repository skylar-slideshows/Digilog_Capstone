/**
  **********************************************************************************
  * USB DESCRIPTORS - DIGILOG CONSOLE (Master controller)
  **********************************************************************************
  * @file FreeRTOS_apps/usb_descriptors.c
  * @brief What the master tells the computer it is: one USB connection carrying two
  *        class-compliant functions (no drivers needed on macOS, Windows or Linux):
  *          - a serial port for the remote app (CDC), handled in remote_cmd_handler.c
  *          - a MIDI device with two ports, or "cables":
  *              cable 0  "Mackie Control"  the DAW's control-surface port
  *              cable 1  "Timecode"        where the DAW sends MIDI Time Code
  *        TinyUSB calls the three tud_descriptor_*_cb() functions when the computer asks.
  **********************************************************************************
*/

#include "FreeRTOS_apps/usb_device_task.h"
#include "tusb.h"

// TinyUSB's test vendor ID, fine on the bench. A shipping product needs its own VID/PID (from
// the USB-IF, or a PID sublicensed under someone else's VID).
#define USB_VID 0xCAFE
#define USB_PID 0x4011 // was 0x4010 when MIDI was alone; changed so computers don't reuse the old layout

#define MIDI_CABLES     2
#define EPNUM_MIDI_OUT  0x01 // computer to console
#define EPNUM_MIDI_IN   0x81 // console to computer
#define EPNUM_CDC_NOTIF 0x82 // serial port status (required by CDC, unused)
#define EPNUM_CDC_OUT   0x03 // remote app to console
#define EPNUM_CDC_IN    0x83 // console to remote app
#define BULK_EP_SIZE    64   // the full-speed maximum for bulk endpoints

enum
{
    ITF_NUM_CDC = 0,        // serial port: control interface
    ITF_NUM_CDC_DATA,       // serial port: data interface
    ITF_NUM_MIDI,           // MIDI: audio control interface
    ITF_NUM_MIDI_STREAMING, // MIDI: streaming interface
    ITF_NUM_TOTAL
};

enum
{
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC,
    STRID_CABLE_MCU, // macOS uses the cable strings as the port names
    STRID_CABLE_MTC,
};


/*=============================== DEVICE ================================*/

static const tusb_desc_device_t s_device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC, // several functions, each grouped by an
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON, // interface association descriptor
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1,
};

uint8_t const *tud_descriptor_device_cb (void)
{
    return (uint8_t const *)&s_device_desc;
}


/*=============================== CONFIGURATION ================================*/

#define MIDI_IAD_LEN     8
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + MIDI_IAD_LEN + TUD_MIDI_DESC_HEAD_LEN + \
                          MIDI_CABLES * TUD_MIDI_DESC_JACK_LEN + 2 * TUD_MIDI_DESC_EP_LEN(MIDI_CABLES))

// Windows remembers a device's layout by VID/PID. If you change the functions, cables or
// endpoints and it still shows the old ones, uninstall the device in Device Manager (or change the PID).
static const uint8_t s_config_desc[] = {
    // The console powers itself; the figure is what it may draw from VBUS (for example a USB
    // isolator's upstream side)
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_SELF_POWERED, 100),

    // Serial port for the remote app. The macro includes its own interface association.
    // interface number, string, notification endpoint and size, data endpoints and size
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, BULK_EP_SIZE),

    // MIDI, grouped by an interface association like the serial port, so the computer
    // treats its two interfaces as one function
    MIDI_IAD_LEN, TUSB_DESC_INTERFACE_ASSOCIATION, ITF_NUM_MIDI, 2,
    TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_FUNC_PROTOCOL_CODE_UNDEF, 0,

    TUD_MIDI_DESC_HEAD(ITF_NUM_MIDI, 0, MIDI_CABLES),
    TUD_MIDI_DESC_JACK_DESC(1, STRID_CABLE_MCU), // the macros number cables from 1; this is cable 0
    TUD_MIDI_DESC_JACK_DESC(2, STRID_CABLE_MTC), // cable 1

    // Each endpoint lists the jacks it carries, and that order sets the cable numbers
    TUD_MIDI_DESC_EP(EPNUM_MIDI_OUT, BULK_EP_SIZE, MIDI_CABLES),
    TUD_MIDI_JACKID_IN_EMB(1), TUD_MIDI_JACKID_IN_EMB(2),
    TUD_MIDI_DESC_EP(EPNUM_MIDI_IN, BULK_EP_SIZE, MIDI_CABLES),
    TUD_MIDI_JACKID_OUT_EMB(1), TUD_MIDI_JACKID_OUT_EMB(2),
};

TU_VERIFY_STATIC(sizeof(s_config_desc) == CONFIG_TOTAL_LEN, "configuration descriptor length is wrong");

uint8_t const *tud_descriptor_configuration_cb (uint8_t index)
{
    (void)index; // there is only one configuration
    return s_config_desc;
}


/*=============================== STRINGS ================================*/

static const char *const STRINGS[] = {
    [STRID_MANUFACTURER] = "Digilog",
    [STRID_PRODUCT]      = "Digilog Console Master",
    [STRID_CDC]          = "Digilog Remote",
    [STRID_CABLE_MCU]    = "Mackie Control",
    [STRID_CABLE_MTC]    = "Timecode",
};

uint16_t const *tud_descriptor_string_cb (uint8_t index, uint16_t langid)
{
    (void)langid;
    static uint16_t desc[1 + 32]; // a header, then up to 32 UTF-16 characters
    uint8_t len;

    if (index == STRID_LANGID)
    {
        desc[1] = 0x0409; // English (US)
        len = 1;
    }
    else if (index == STRID_SERIAL)
    {
        // the chip's unique ID, so two consoles on one computer differ
        len = (uint8_t)usb_serial_from_uid(&desc[1], 32);
    }
    else
    {
        if (index >= TU_ARRAY_SIZE(STRINGS) || STRINGS[index] == NULL) return NULL;
        const char *str = STRINGS[index];
        for (len = 0; str[len] != '\0' && len < 32; len++)
        {
            desc[1 + len] = (uint16_t)str[len]; // ASCII to UTF-16
        }
    }

    desc[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2U * len + 2U)); // type, then length in bytes
    return desc;
}
