/**
  * usbx_cdc.c : USBX Device CDC-ACM backend for STM32C5 (FreeRTOS)
  *  - 예제의 USB<->UART 브리지를 제거하고, cdc* 링버퍼 모델로 대체.
  *  - TX: cdcWrite -> ux_device_class_cdc_acm_write (블로킹)
  *  - RX: 전용 리더 스레드가 ux_device_class_cdc_acm_read -> q_rx 링버퍼
  */

#include "usbx_cdc.h"


#if defined(_USE_HW_USB) && (HW_USB_STACK == HW_USB_STACK_USBX)

#include "ux_device_class_cdc_acm.h"
#include "ux_stm32_device_descriptors_config.h"
#include "ux_stm32_device_descriptors.h"
#include "ux_dcd_stm32.h"
#include "mx_usb_drd_fs.h"
#include "qbuffer.h"
#include "bsp.h"
#include "log.h"

#include "FreeRTOS.h"
#include "task.h"


/* USB PCD 핸들 (mx_usb_drd_fs) */
hal_pcd_handle_t *p_usb_device = UX_NULL;

static UX_SLAVE_CLASS_CDC_ACM *cdc_acm = UX_NULL;
static TaskHandle_t            ux_device_app_thread;
static TaskHandle_t            ux_cdc_read_thread;

/* RX 링버퍼 + 리더 임시버퍼 */
static qbuffer_t q_rx;
static uint8_t   q_rx_buf[4096];
static uint8_t   rx_tmp[512];
static bool      is_init = false;


/* CDC-ACM 엔드포인트/인터페이스 핸들 (descriptors_config 매크로 기반) */
static USB_DEVICE_ENDPOINT_HANDLE cdc_acm_ctl_endpoints_app[] =
{
  { USBD_CDC_ACM_EPNTF_ADDR, USBD_CDC_ACM_EPNTF_TYPE, USBD_CDC_ACM_EPNTF_FS_MPS, USBD_CDC_ACM_EPNTF_FS_BINTERVAL },
};

static USB_DEVICE_ENDPOINT_HANDLE cdc_acm_data_endpoints_app[] =
{
  { USBD_CDC_ACM_EPIN_ADDR,  USBD_CDC_ACM_EPIN_TYPE,  USBD_CDC_ACM_EPIN_FS_MPS,  USBD_CDC_ACM_EPIN_FS_BINTERVAL },
  { USBD_CDC_ACM_EPOUT_ADDR, USBD_CDC_ACM_EPOUT_TYPE, USBD_CDC_ACM_EPOUT_FS_MPS, USBD_CDC_ACM_EPOUT_FS_BINTERVAL },
};

static USB_DEVICE_INTERFACE_HANDLE cdc_acm_interface_app[] =
{
  { USBD_CDC_ACM_CTL_NUMBERS,  USBD_CDC_ACM_CTL_ATL_SETTING,  USBD_CDC_ACM_CTL_EP_NUMBERS,
    USBD_CDC_ACM_CTL_CLASS,    USBD_CDC_ACM_CTL_SUBCLASS,     USBD_CDC_ACM_CTL_PROTOCOL,
    USBD_CDC_ACM_CTL_STR_DESC_IDX,  0x00U, cdc_acm_ctl_endpoints_app,  0x00U },
  { USBD_CDC_ACM_DATA_NUMBERS, USBD_CDC_ACM_DATA_ATL_SETTING, USBD_CDC_ACM_DATA_EP_NUMBERS,
    USBD_CDC_ACM_DATA_CLASS,   USBD_CDC_ACM_DATA_SUBCLASS,    USBD_CDC_ACM_DATA_PROTOCOL,
    USBD_CDC_ACM_DATA_STR_DESC_IDX, 0x00U, cdc_acm_data_endpoints_app, 0x00U },
};


static VOID usbd_cdc_acm_activate(VOID *cdc_acm_instance);
static VOID usbd_cdc_acm_deactivate(VOID *cdc_acm_instance);
static VOID usbd_cdc_acm_ParameterChange(VOID *cdc_acm_instance);
static VOID app_ux_device_thread_entry(void *argument);
static void ux_cdc_read_thread_entry(void *argument);


/* 리더 스레드에서 device 상태 참조 */
static UX_SLAVE_DEVICE *usbx_device(void)
{
  return &_ux_system_slave->ux_system_slave_device;
}


UINT app_usbx_device_init(VOID)
{
  UINT                              status;
  USB_DESCRIPTOR                    usbd_desc;
  ULONG                             report_desc_len;
  UX_SLAVE_CLASS_CDC_ACM_PARAMETER  cdc_acm_parameter;

  report_desc_len = sizeof(cdc_acm_parameter);
  usb_device_descriptor_register_class(USBD_CLASS_TYPE_CDC_ACM, cdc_acm_interface_app, &report_desc_len);
  usb_device_descriptor_get_framework(&usbd_desc);

  status = ux_device_stack_initialize(usbd_desc.device_high_speed.framework, usbd_desc.device_high_speed.framework_length,
                                      usbd_desc.device_full_speed.framework, usbd_desc.device_full_speed.framework_length,
                                      usbd_desc.string.framework,           usbd_desc.string.framework_length,
                                      usbd_desc.languageid.framework,       usbd_desc.languageid.framework_length,
                                      UX_NULL);
  if (status != UX_SUCCESS)
    return status;

  cdc_acm_parameter.ux_slave_class_cdc_acm_instance_activate   = usbd_cdc_acm_activate;
  cdc_acm_parameter.ux_slave_class_cdc_acm_instance_deactivate = usbd_cdc_acm_deactivate;
  cdc_acm_parameter.ux_slave_class_cdc_acm_parameter_change    = usbd_cdc_acm_ParameterChange;

  status = ux_device_stack_class_register(_ux_system_slave_class_cdc_acm_name, ux_device_class_cdc_acm_entry,
                                          0x01, cdc_acm_interface_app->interface_numbers, &cdc_acm_parameter);
  if (status != UX_SUCCESS)
    return status;

  /* USB 컨트롤러(DCD) 초기화 스레드 (1회 실행 후 self-suspend) */
  if (xTaskCreate(app_ux_device_thread_entry, "ux_dev", 1024, NULL,
                  (UBaseType_t)_HW_DEF_THREAD_USB_PRI, &ux_device_app_thread) != pdPASS)
    return UX_THREAD_ERROR;

  /* RX 리더 스레드 : CLI(osPriorityNormal=24) 보다 높은 USB 우선순위(AboveNormal=40)로 둔다.
     낮으면 usb rx 명령이 cdcReadBuf 로 양보없이 busy-spin 할 때 리더가 굶어 OUT 수신이 멈춘다.
     (리더는 패킷 사이 OUT 세마포어 대기로 CPU 를 양보하므로 CLI 의 q_rx 드레인을 막지 않는다) */
  if (xTaskCreate(ux_cdc_read_thread_entry, "ux_cdc_rd", 1024, NULL,
                  (UBaseType_t)_HW_DEF_THREAD_USB_PRI, &ux_cdc_read_thread) != pdPASS)
    return UX_THREAD_ERROR;

  return UX_SUCCESS;
}


static VOID app_ux_device_thread_entry(void *argument)
{
  UX_PARAMETER_NOT_USED(argument);

  if (p_usb_device == UX_NULL)
  {
    mx_usb_drd_fs_device_init();                 /* HAL_PCD_Init + 클럭 + PMA(5EP) + NVIC */
    p_usb_device = mx_usb_drd_fs_device_gethandle();
    ux_dcd_stm32_initialize(0, (ULONG)p_usb_device);
  }
  vTaskSuspend(ux_device_app_thread);
}


static VOID usbd_cdc_acm_activate(VOID *cdc_acm_instance)
{
  cdc_acm = (UX_SLAVE_CLASS_CDC_ACM *)cdc_acm_instance;
}

static VOID usbd_cdc_acm_deactivate(VOID *cdc_acm_instance)
{
  UX_PARAMETER_NOT_USED(cdc_acm_instance);
  cdc_acm = UX_NULL;
}

static VOID usbd_cdc_acm_ParameterChange(VOID *cdc_acm_instance)
{
  /* baud/DTR 은 인스턴스 필드에서 직접 읽으므로 별도 처리 불필요 */
  UX_PARAMETER_NOT_USED(cdc_acm_instance);
}


/* RX 리더: q_rx 여유만큼 CDC OUT 을 읽어 링버퍼에 적재.
   여유<64 면 읽지 않아(OUT 미수신) 호스트가 NAK -> 흐름제어 */
static void ux_cdc_read_thread_entry(void *argument)
{
  UX_PARAMETER_NOT_USED(argument);

  while (1)
  {
    if (usbx_device()->ux_slave_device_state == UX_DEVICE_CONFIGURED && cdc_acm != UX_NULL)
    {
      uint32_t room = (q_rx.len - qbufferAvailable(&q_rx)) - 1U;
      if (room >= 64U)
      {
        ULONG want   = (room > sizeof(rx_tmp)) ? sizeof(rx_tmp) : room;
        want &= ~63UL;   /* MPS(64) 정수배로 정렬. 비정렬이면 read 내부 마지막 부분패킷 무장(<64)에
                            호스트가 꽉 찬 64B 를 보내 requested_length 언더플로우->rx_tmp 오버런 발생 */
        ULONG actual = 0;
        if (ux_device_class_cdc_acm_read(cdc_acm, rx_tmp, want, &actual) == UX_SUCCESS && actual > 0U)
        {
          qbufferWrite(&q_rx, rx_tmp, actual);
        }
        else
        {
          vTaskDelay(1);
        }
      }
      else
      {
        vTaskDelay(1);
      }
    }
    else
    {
      vTaskDelay(5);
    }
  }
}


//---------------------------------------------------------------------------
//  cdc* 백엔드 API
//---------------------------------------------------------------------------
bool usbxCdcInit(void)
{
  if (is_init == false)
  {
    qbufferCreate(&q_rx, q_rx_buf, sizeof(q_rx_buf));
    is_init = true;
  }
  return true;
}

bool usbxCdcIsConnect(void)
{
  if (cdc_acm == UX_NULL)
    return false;
  if (usbx_device()->ux_slave_device_state != UX_DEVICE_CONFIGURED)
    return false;
  if (cdc_acm->ux_slave_class_cdc_acm_data_dtr_state == 0U)   /* 호스트 포트 오픈(DTR) */
    return false;
  return true;
}

uint32_t usbxCdcAvailable(void)
{
  return qbufferAvailable(&q_rx);
}

uint8_t usbxCdcRead(void)
{
  uint8_t ret = 0;
  qbufferRead(&q_rx, &ret, 1);
  return ret;
}

uint32_t usbxCdcReadBuf(uint8_t *p_data, uint32_t length)
{
  uint32_t avail = qbufferAvailable(&q_rx);

  if (avail > length) avail = length;
  if (avail > 0)
    qbufferRead(&q_rx, p_data, avail);
  return avail;
}

uint32_t usbxCdcWrite(uint8_t *p_data, uint32_t length)
{
  ULONG actual = 0;

  if (usbxCdcIsConnect() != true)
    return 0;

  if (ux_device_class_cdc_acm_write(cdc_acm, p_data, length, &actual) == UX_SUCCESS)
    return (uint32_t)actual;

  return 0;
}

uint32_t usbxCdcGetBaud(void)
{
  if (cdc_acm == UX_NULL)
    return 0;
  return cdc_acm->ux_slave_class_cdc_acm_baudrate;
}

#endif
