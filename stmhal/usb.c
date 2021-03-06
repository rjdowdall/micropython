#include <string.h>

#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_interface.h"
#include "usbd_msc.h"
#include "usbd_msc_storage.h"

#include "misc.h"
#include "mpconfig.h"
#include "qstr.h"
#include "obj.h"
#include "usb.h"

#ifdef USE_DEVICE_MODE
USBD_HandleTypeDef hUSBDDevice;
#endif

static int dev_is_enabled = 0;
uint32_t APP_dev_is_connected = 0; /* used by usbd_cdc_vcp */
mp_obj_t mp_const_vcp_interrupt = MP_OBJ_NULL;

void pyb_usb_dev_init(usbd_device_kind_t device_kind, usbd_storage_medium_kind_t medium_kind) {
#ifdef USE_DEVICE_MODE
    if (!dev_is_enabled) {
        // only init USB once in the device's power-lifetime
        switch (device_kind) {
            case USBD_DEVICE_CDC:
                // XXX USBD_CDC_Init (called by one of these functions below) uses malloc,
                // so the memory is invalid after a soft reset (which resets the GC).
                USBD_Init(&hUSBDDevice, &VCP_Desc, 0);
                USBD_RegisterClass(&hUSBDDevice, &USBD_CDC);
                USBD_CDC_RegisterInterface(&hUSBDDevice, (USBD_CDC_ItfTypeDef*)&USBD_CDC_fops);
                USBD_Start(&hUSBDDevice);
                //USBD_Init(&USB_OTG_Core, USB_OTG_FS_CORE_ID, &USR_desc, &USBD_PYB_cb, &USR_cb);
                break;

            case USBD_DEVICE_MSC:
                // XXX USBD_CDC_Init (called by one of these functions below) uses malloc,
                // so the memory is invalid after a soft reset (which resets the GC).
                USBD_Init(&hUSBDDevice, &MSC_Desc, 0);
                USBD_RegisterClass(&hUSBDDevice, &USBD_MSC);
                if (medium_kind == USBD_STORAGE_MEDIUM_FLASH) {
                    USBD_MSC_RegisterStorage(&hUSBDDevice, (USBD_StorageTypeDef*)&USBD_FLASH_STORAGE_fops);
                } else {
                    USBD_MSC_RegisterStorage(&hUSBDDevice, (USBD_StorageTypeDef*)&USBD_SDCARD_STORAGE_fops);
                }
                USBD_Start(&hUSBDDevice);
                break;

            case USBD_DEVICE_HID:
                //USBD_Init(&USB_OTG_Core, USB_OTG_FS_CORE_ID, &USR_desc, &USBD_PYB_HID_cb, &USR_cb);
                // TODO
                break;
        }
    }
    dev_is_enabled = 1;

    // create an exception object for interrupting by VCP
    mp_const_vcp_interrupt = mp_obj_new_exception_msg(&mp_type_OSError, "VCPInterrupt");
#endif
}

bool usb_vcp_is_enabled(void) {
    return dev_is_enabled;
}

bool usb_vcp_is_connected(void) {
    return APP_dev_is_connected;
}

void usb_vcp_set_interrupt_char(int c) {
    if (dev_is_enabled) {
        if (c != VCP_CHAR_NONE) {
            mp_obj_exception_clear_traceback(mp_const_vcp_interrupt);
        }
        USBD_CDC_SetInterrupt(c, mp_const_vcp_interrupt);
    }
}

int usb_vcp_rx_num(void) {
    return USBD_CDC_RxNum();
}

char usb_vcp_rx_get(void) {
    return USBD_CDC_RxGet();
}

void usb_vcp_send_str(const char *str) {
    usb_vcp_send_strn(str, strlen(str));
}

void usb_vcp_send_strn(const char *str, int len) {
#ifdef USE_DEVICE_MODE
    if (dev_is_enabled) {
        USBD_CDC_Tx(str, len);
    }
#endif
}

void usb_vcp_send_strn_cooked(const char *str, int len) {
#ifdef USE_DEVICE_MODE
    if (dev_is_enabled) {
        for (const char *top = str + len; str < top; str++) {
            if (*str == '\n') {
                USBD_CDC_Tx("\r\n", 2);
            } else {
                USBD_CDC_Tx(str, 1);
            }
        }
    }
#endif
}

void usb_hid_send_report(uint8_t *buf) {
#ifdef USE_DEVICE_MODE
    #if 0
    USBD_HID_SendReport(&USB_OTG_Core, buf, 4);
    #endif
#endif
}

/******************************************************************************/
// code for experimental USB OTG support

#ifdef USE_HOST_MODE

#include "led.h"
#include "usbh_core.h"
#include "usbh_usr.h"
#include "usbh_hid_core.h"
#include "usbh_hid_keybd.h"
#include "usbh_hid_mouse.h"

__ALIGN_BEGIN USBH_HOST USB_Host __ALIGN_END ;

static int host_is_enabled = 0;

void pyb_usb_host_init(void) {
    if (!host_is_enabled) {
        // only init USBH once in the device's power-lifetime
        /* Init Host Library */
        USBH_Init(&USB_OTG_Core, USB_OTG_FS_CORE_ID, &USB_Host, &HID_cb, &USR_Callbacks);
    }
    host_is_enabled = 1;
}

void pyb_usb_host_process(void) {
    USBH_Process(&USB_OTG_Core, &USB_Host);
}

uint8_t usb_keyboard_key = 0;

// TODO this is an ugly hack to get key presses
uint pyb_usb_host_get_keyboard(void) {
    uint key = usb_keyboard_key;
    usb_keyboard_key = 0;
    return key;
}

void USR_MOUSE_Init(void) {
    led_state(4, 1);
    USB_OTG_BSP_mDelay(100);
    led_state(4, 0);
}

void USR_MOUSE_ProcessData(HID_MOUSE_Data_TypeDef *data) {
    led_state(4, 1);
    USB_OTG_BSP_mDelay(50);
    led_state(4, 0);
}

void USR_KEYBRD_Init(void) {
    led_state(4, 1);
    USB_OTG_BSP_mDelay(100);
    led_state(4, 0);
}

void USR_KEYBRD_ProcessData(uint8_t pbuf) {
    led_state(4, 1);
    USB_OTG_BSP_mDelay(50);
    led_state(4, 0);
    //lcd_print_strn((char*)&pbuf, 1);
    usb_keyboard_key = pbuf;
}

#endif // USE_HOST_MODE
