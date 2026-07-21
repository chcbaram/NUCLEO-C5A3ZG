#include "usb.h"


#ifdef _USE_HW_USB
#include "cdc.h"
#include "cli.h"
#include "log.h"
#include "osal/thread.h"


static bool       is_init     = false;
static UsbMode_t  is_usb_mode = USB_NON_MODE;


#if CLI_USE(HW_USB)
static void cliCmd(cli_args_t *args);
#endif

#if HW_USB_STACK == HW_USB_STACK_TINYUSB
static void usbThread(void const *arg);
#endif


/* STM32C5 USB 48MHz 클럭 설정 (ST 공식 NUCLEO-C5A3ZG 예제 기준)
   - CK48 = HSIDIV3 (HSI/3 = 48MHz), CRS/VddUSB/전용 GPIO 설정 불필요
   - FreeRTOS 에서 ISR->FreeRTOS API 호출을 위해 IRQ 우선순위는 syscall 범위(>=5)로 */
static void usbClockInit(void)
{
  HAL_RCC_USB_EnableClock();
  HAL_RCC_CK48_SetKernelClkSource(HAL_RCC_CK48_CLK_SRC_HSIDIV3);

  HAL_CORTEX_NVIC_SetPriority(USB_DRD_FS_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_7, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(USB_DRD_FS_IRQn);
}


bool usbInit(void)
{
  usbClockInit();

#if HW_USB_STACK == HW_USB_STACK_TINYUSB
  tusb_rhport_init_t dev_init =
  {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };

  tusb_init(BOARD_TUD_RHPORT, &dev_init);
  is_usb_mode = USB_CDC_MODE;

  // TinyUSB 스택은 tud_task() 를 주기적으로 펌핑해야 하므로 전용 스레드로 구동
  threadCreate("usb", usbThread, NULL, _HW_DEF_THREAD_USB_PRI, _HW_DEF_THREAD_USB_STACK);
#else
  usbBegin(USB_CDC_MODE);
#endif

#if CLI_USE(HW_USB)
  cliAdd("usb", cliCmd);
#endif

  is_init = true;
  return true;
}

bool usbBegin(UsbMode_t usb_mode)
{
#if HW_USB_STACK == HW_USB_STACK_TINYUSB
  is_usb_mode = usb_mode;
  return true;
#else
  // TODO: ST USB Device Library backend (usbd_conf.c 신형 HAL) 연동 예정
  (void)usb_mode;
  return false;
#endif
}

void usbDeInit(void)
{
  if (is_init == true)
  {
  }
}

bool usbUpdate(void)
{
#if HW_USB_STACK == HW_USB_STACK_TINYUSB
  tud_task();
#endif
  return true;
}

bool usbIsOpen(void)
{
  return cdcIsConnect();
}

bool usbIsConnect(void)
{
#if HW_USB_STACK == HW_USB_STACK_TINYUSB
  return (tud_connected() && !tud_suspended());
#else
  return false;
#endif
}

UsbMode_t usbGetMode(void)
{
  return is_usb_mode;
}

UsbType_t usbGetType(void)
{
  return (UsbType_t)cdcGetType();
}


#if HW_USB_STACK == HW_USB_STACK_TINYUSB
static void usbThread(void const *arg)
{
  (void)arg;

  logPrintf("[OK] Thread Started : USB\n");

  while (1)
  {
    tud_task();   // OPT_OS_FREERTOS 에서 내부적으로 이벤트 큐를 대기(블로킹)
  }
}

void USB_DRD_FS_IRQHandler(void)
{
  tud_int_handler(BOARD_TUD_RHPORT);
}

// STM32 UID(96bit) 기반 시리얼 문자열 (usb_descriptors.c 에서 사용)
size_t usbGetSerial(uint16_t desc_str1[], size_t max_chars)
{
  volatile uint32_t *stm32_uuid = (volatile uint32_t *)UID_BASE;
  uint8_t            uid[12];
  uint32_t          *id32    = (uint32_t *)(uintptr_t)uid;
  size_t             uid_len = 12;

  id32[0] = stm32_uuid[0];
  id32[1] = stm32_uuid[1];
  id32[2] = stm32_uuid[2];

  if (uid_len > max_chars / 2) uid_len = max_chars / 2;

  for (size_t i = 0; i < uid_len; i++)
  {
    for (size_t j = 0; j < 2; j++)
    {
      const char nibble_to_hex[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
      uint8_t const nibble       = (uid[i] >> (j * 4)) & 0xf;
      desc_str1[i * 2 + (1 - j)] = nibble_to_hex[nibble]; // UTF-16-LE
    }
  }

  return 2 * uid_len;
}
#endif


#if CLI_USE(HW_USB)
void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    while (cliKeepLoop())
    {
      cliPrintf("USB Mode    : %d\n", usbGetMode());
      cliPrintf("USB Type    : %d\n", usbGetType());
      cliPrintf("USB Connect : %d\n", usbIsConnect());   // 케이블 연결/열거 상태
      cliPrintf("USB Open    : %d\n", usbIsOpen());       // 호스트가 포트 오픈(DTR)
      cliPrintf("USB Baud    : %d\n", (int)cdcGetBaud());
      cliMoveUp(5);
      delay(100);
    }
    cliMoveDown(5);

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "tx") == true)
  {
    uint8_t  buf[512];
    uint32_t pre_time;
    uint32_t tx_cnt = 0;

    for (int i = 0; i < (int)sizeof(buf); i++)   // 송신 패턴
      buf[i] = '0' + (i % 10);

    pre_time = millis();
    while (cliKeepLoop())
    {
      if (millis() - pre_time >= 1000)
      {
        pre_time = millis();
        logPrintf("tx : %d KB/s\n", tx_cnt / 1024);
        tx_cnt = 0;
      }
      tx_cnt += cdcWrite(buf, sizeof(buf));
    }

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "rx") == true)
  {
    uint8_t  buf[512];
    uint32_t pre_time;
    uint32_t rx_cnt = 0;

    pre_time = millis();
    while (cliKeepLoop())
    {
      if (millis() - pre_time >= 1000)
      {
        pre_time = millis();
        logPrintf("rx : %d KB/s\n", rx_cnt / 1024);
        rx_cnt = 0;
      }
      rx_cnt += cdcReadBuf(buf, sizeof(buf));   // 벌크 읽기(1바이트씩 X)
    }

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "duplex") == true)
  {
    uint8_t  buf[512];
    uint32_t pre_time;
    uint32_t tx_cnt = 0;
    uint32_t rx_cnt = 0;

    for (int i = 0; i < (int)sizeof(buf); i++)
      buf[i] = '0' + (i % 10);

    pre_time = millis();
    while (cliKeepLoop())
    {
      if (millis() - pre_time >= 1000)
      {
        pre_time = millis();
        logPrintf("tx : %d KB/s, rx : %d KB/s\n", tx_cnt / 1024, rx_cnt / 1024);
        tx_cnt = 0;
        rx_cnt = 0;
      }
      rx_cnt += cdcReadBuf(buf, sizeof(buf));
      tx_cnt += cdcWrite(buf, sizeof(buf));
    }

    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("usb info\n");
    cliPrintf("usb tx\n");
    cliPrintf("usb rx\n");
    cliPrintf("usb duplex\n");
  }
}
#endif

#endif
