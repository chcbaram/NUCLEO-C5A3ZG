#ifndef USBX_CDC_H_
#define USBX_CDC_H_


#ifdef __cplusplus
extern "C" {
#endif


#include "hw_def.h"


#if defined(_USE_HW_USB) && (HW_USB_STACK == HW_USB_STACK_USBX)

#include "ux_api.h"


UINT app_usbx_device_init(VOID);   /* mx_usbx_app.c(app_usbx_init) 에서 호출 */

/* cdc* 백엔드용 API (cdc.c USBX 분기에서 사용) */
bool     usbxCdcInit(void);
bool     usbxCdcIsConnect(void);
uint32_t usbxCdcAvailable(void);
uint8_t  usbxCdcRead(void);
uint32_t usbxCdcReadBuf(uint8_t *p_data, uint32_t length);
uint32_t usbxCdcWrite(uint8_t *p_data, uint32_t length);
uint32_t usbxCdcGetBaud(void);

#endif


#ifdef __cplusplus
}
#endif


#endif /* USBX_CDC_H_ */
