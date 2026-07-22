#include "cdc.h"


#ifdef _USE_HW_CDC
#include "usb/usb.h"


static bool is_init = false;


//---------------------------------------------------------------------------
//  TinyUSB backend (HW_USB_STACK == HW_USB_STACK_TINYUSB)
//---------------------------------------------------------------------------
#if HW_USB_STACK == HW_USB_STACK_TINYUSB

static volatile uint32_t cdc_baud = 0;   // 호스트가 설정한 통신속도 (SET_LINE_CODING)

bool cdcInit(void)
{
  is_init = true;
  return true;
}

bool cdcIsInit(void)
{
  return is_init;
}

bool cdcIsConnect(void)
{
  // tud_cdc_n_connected() 은 DTR 기준 -> 호스트가 포트를 오픈한 상태
  return tud_cdc_n_connected(0);
}

uint32_t cdcAvailable(void)
{
  return tud_cdc_n_available(0);
}

uint8_t cdcRead(void)
{
  uint8_t buf[1] = {0, };

  tud_cdc_n_read(0, buf, 1);

  return buf[0];
}

uint32_t cdcReadBuf(uint8_t *p_data, uint32_t length)
{
  return tud_cdc_n_read(0, p_data, length);
}

uint32_t cdcWrite(uint8_t *p_data, uint32_t length)
{
  uint32_t pre_time;
  uint32_t sent_len;
  uint32_t w;


  if (cdcIsConnect() != true)
    return 0;

  sent_len = 0;

  // tud_task() 펌핑은 전용 usb 스레드가 담당한다. TX FIFO 에 적재만 하고, FIFO 가
  // 차서 더 못 넣을 때만 flush 로 밀어내고 양보한다(매 호출 flush = 부분패킷 남발 방지).
  pre_time = millis();
  while (sent_len < length)
  {
    w = tud_cdc_n_write(0, &p_data[sent_len], length - sent_len);
    sent_len += w;

    if (w == 0)
    {
      tud_cdc_n_write_flush(0);
      delay(1);
    }

    if (cdcIsConnect() != true)
      break;

    if (millis() - pre_time >= 100)
      break;
  }

  tud_cdc_n_write_flush(0);

  return sent_len;
}

uint32_t cdcGetBaud(void)
{
  return cdc_baud;
}

uint8_t cdcGetType(void)
{
  return (cdc_baud == 115200) ? 1 : 0;
}

// 호스트가 통신속도(SET_LINE_CODING)를 설정할 때 호출된다. -> baud 감지
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p_line_coding)
{
  (void)itf;
  cdc_baud = p_line_coding->bit_rate;
}

// DTR/RTS 상태 변경 (touch1200 부트 진입 등 처리용, 현재는 미사용)
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void)itf;
  (void)dtr;
  (void)rts;
}

//---------------------------------------------------------------------------
//  ST USB Device Library backend (HW_USB_STACK == HW_USB_STACK_ST)
//---------------------------------------------------------------------------
#elif HW_USB_STACK == HW_USB_STACK_ST
#include "stm32/usb_cdc/usbd_cdc_if.h"

bool cdcInit(void)
{
  is_init = cdcIfInit();
  return is_init;
}

bool cdcIsInit(void)
{
  return is_init;
}

bool cdcIsConnect(void)
{
  return cdcIfIsConnected();
}

uint32_t cdcAvailable(void)
{
  return cdcIfAvailable();
}

uint8_t cdcRead(void)
{
  return cdcIfRead();
}

uint32_t cdcReadBuf(uint8_t *p_data, uint32_t length)
{
  return cdcIfReadBuf(p_data, length);
}

uint32_t cdcWrite(uint8_t *p_data, uint32_t length)
{
  return cdcIfWrite(p_data, length);
}

uint32_t cdcGetBaud(void)
{
  return cdcIfGetBaud();
}

uint8_t cdcGetType(void)
{
  return cdcIfGetType();
}

//---------------------------------------------------------------------------
//  Azure RTOS USBX backend (HW_USB_STACK == HW_USB_STACK_USBX)
//---------------------------------------------------------------------------
#else
#include "usb/usbx/usbx_cdc.h"

bool cdcInit(void)
{
  is_init = usbxCdcInit();
  return is_init;
}

bool cdcIsInit(void)
{
  return is_init;
}

bool cdcIsConnect(void)
{
  return usbxCdcIsConnect();
}

uint32_t cdcAvailable(void)
{
  return usbxCdcAvailable();
}

uint8_t cdcRead(void)
{
  return usbxCdcRead();
}

uint32_t cdcReadBuf(uint8_t *p_data, uint32_t length)
{
  return usbxCdcReadBuf(p_data, length);
}

uint32_t cdcWrite(uint8_t *p_data, uint32_t length)
{
  return usbxCdcWrite(p_data, length);
}

uint32_t cdcGetBaud(void)
{
  return usbxCdcGetBaud();
}

uint8_t cdcGetType(void)
{
  return (usbxCdcGetBaud() == 115200) ? 1 : 0;
}

#endif

#endif
