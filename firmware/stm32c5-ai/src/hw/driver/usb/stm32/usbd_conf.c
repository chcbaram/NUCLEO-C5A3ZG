/**
  ******************************************************************************
  * @file    usbd_conf.c
  * @brief   USB Device LL glue for STM32C5 (차세대 HAL PCD API)
  *
  *  클래식 STM32_USB_Device_Library(usbd_core/ctlreq/ioreq)와 CDC 클래스는 HAL 독립적이며,
  *  오직 USBD_LL_* 함수만 호출한다. 이 파일이 그 USBD_LL_* 를 STM32C5 신형 HAL PCD
  *  (hal_pcd_handle_t / HAL_PCD_SetConfig / HAL_PCD_OpenEndpoint ...) 로 연결한다.
  ******************************************************************************
  */

#include "bsp.h"
#include "usbd_core.h"
#include "usbd_def.h"
#include "usbd_cdc.h"


static hal_pcd_handle_t hpcd_USB_DRD_FS;
static bool             is_connected = false;

/* 신형 HAL 의 endpoint 구조체에는 is_stall 필드가 없어 stall 상태를 자체 추적한다. */
static uint16_t stall_in  = 0;
static uint16_t stall_out = 0;


static USBD_StatusTypeDef status_from_hal(hal_status_t s)
{
  switch (s)
  {
    case HAL_OK:   return USBD_OK;
    case HAL_BUSY: return USBD_BUSY;
    default:       return USBD_FAIL;
  }
}

static USBD_HandleTypeDef *get_pdev(hal_pcd_handle_t *hpcd)
{
  return (USBD_HandleTypeDef *)HAL_PCD_GetUserData(hpcd);
}

bool USBD_is_connected(void)
{
  return is_connected;
}


/*=============================================================================
   PCD -> USBD 콜백 (USE_HAL_PCD_REGISTER_CALLBACKS==0 이므로 weak 함수 오버라이드)
 =============================================================================*/
void HAL_PCD_SofCallback(hal_pcd_handle_t *hpcd)
{
  USBD_LL_SOF(get_pdev(hpcd));
}

void HAL_PCD_SetupStageCallback(hal_pcd_handle_t *hpcd)
{
  USBD_LL_SetupStage(get_pdev(hpcd), (uint8_t *)hpcd->setup);
}

void HAL_PCD_DataOutStageCallback(hal_pcd_handle_t *hpcd, uint8_t ep_num)
{
  USBD_LL_DataOutStage(get_pdev(hpcd), ep_num, hpcd->out_ep[ep_num].p_xfer_buffer);
}

void HAL_PCD_DataInStageCallback(hal_pcd_handle_t *hpcd, uint8_t ep_num)
{
  USBD_LL_DataInStage(get_pdev(hpcd), ep_num, hpcd->in_ep[ep_num].p_xfer_buffer);
}

void HAL_PCD_ResetCallback(hal_pcd_handle_t *hpcd)
{
  USBD_LL_SetSpeed(get_pdev(hpcd), USBD_SPEED_FULL);
  USBD_LL_Reset(get_pdev(hpcd));
}

void HAL_PCD_SuspendCallback(hal_pcd_handle_t *hpcd)
{
  USBD_LL_Suspend(get_pdev(hpcd));
  is_connected = false;
}

void HAL_PCD_ResumeCallback(hal_pcd_handle_t *hpcd)
{
  USBD_LL_Resume(get_pdev(hpcd));
}

void HAL_PCD_ConnectCallback(hal_pcd_handle_t *hpcd)
{
  USBD_LL_DevConnected(get_pdev(hpcd));
}

void HAL_PCD_DisconnectCallback(hal_pcd_handle_t *hpcd)
{
  USBD_LL_DevDisconnected(get_pdev(hpcd));
}


/*=============================================================================
   USBD -> PCD (USBD_LL_* 구현)
 =============================================================================*/
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
  hal_pcd_config_t cfg = {0};

  if (HAL_PCD_Init(&hpcd_USB_DRD_FS, HAL_PCD_DRD_FS) != HAL_OK)
  {
    Error_Handler();
  }

  /* USBD <-> PCD 핸들 상호 링크 */
  pdev->pData = &hpcd_USB_DRD_FS;
  HAL_PCD_SetUserData(&hpcd_USB_DRD_FS, pdev);

  /* USB 48MHz 클럭 (STM32C5: HSI48 없음 -> CK48 = HSIDIV3) */
  HAL_RCC_USB_EnableClock();
  HAL_RCC_CK48_SetKernelClkSource(HAL_RCC_CK48_CLK_SRC_HSIDIV3);

  cfg.pcd_speed                = HAL_PCD_SPEED_FS;
  cfg.phy_interface            = HAL_PCD_PHY_EMBEDDED_FS;
  cfg.sof_enable               = HAL_PCD_SOF_ENABLED;   /* CDC_SoF_ISR(TX/RX 흐름제어) 용 */
  cfg.lpm_enable               = HAL_PCD_LPM_DISABLED;
  cfg.battery_charging_enable  = HAL_PCD_BCD_DISABLED;
  cfg.vbus_sensing_enable      = HAL_PCD_VBUS_SENSE_DISABLED;
  cfg.bulk_doublebuffer_enable = HAL_PCD_BULK_DB_DISABLED;
  cfg.dma_enable               = HAL_PCD_DMA_DISABLED;
  if (HAL_PCD_SetConfig(&hpcd_USB_DRD_FS, &cfg) != HAL_OK)
  {
    Error_Handler();
  }

  /* PMA 배치: EP0(64x2) + CDC IN/OUT/CMD */
  HAL_PCD_PMAConfig(&hpcd_USB_DRD_FS, 0x00,       HAL_PCD_SNG_BUF, 0x20);
  HAL_PCD_PMAConfig(&hpcd_USB_DRD_FS, 0x80,       HAL_PCD_SNG_BUF, 0x60);
  HAL_PCD_PMAConfig(&hpcd_USB_DRD_FS, CDC_IN_EP,  HAL_PCD_SNG_BUF, 0xA0);
  HAL_PCD_PMAConfig(&hpcd_USB_DRD_FS, CDC_OUT_EP, HAL_PCD_SNG_BUF, 0xE0);
  HAL_PCD_PMAConfig(&hpcd_USB_DRD_FS, CDC_CMD_EP, HAL_PCD_SNG_BUF, 0x120);

  /* FreeRTOS 에서 ISR->FreeRTOS API 를 위해 syscall 범위(>=5) 우선순위 */
  HAL_CORTEX_NVIC_SetPriority(USB_DRD_FS_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_7, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(USB_DRD_FS_IRQn);

  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  HAL_PCD_DeInit((hal_pcd_handle_t *)pdev->pData);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  return status_from_hal(HAL_PCD_Start((hal_pcd_handle_t *)pdev->pData));
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  return status_from_hal(HAL_PCD_Stop((hal_pcd_handle_t *)pdev->pData));
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps)
{
  return status_from_hal(HAL_PCD_OpenEndpoint((hal_pcd_handle_t *)pdev->pData,
                                              ep_addr, ep_mps, (hal_pcd_ep_type_t)ep_type));
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return status_from_hal(HAL_PCD_CloseEndpoint((hal_pcd_handle_t *)pdev->pData, ep_addr));
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return status_from_hal(HAL_PCD_FlushEndpoint((hal_pcd_handle_t *)pdev->pData, ep_addr));
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  if ((ep_addr & 0x80U) == 0x80U)
    stall_in |= (uint16_t)(1U << (ep_addr & 0x0FU));
  else
    stall_out |= (uint16_t)(1U << (ep_addr & 0x0FU));

  return status_from_hal(HAL_PCD_SetEndpointStall((hal_pcd_handle_t *)pdev->pData, ep_addr));
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  if ((ep_addr & 0x80U) == 0x80U)
    stall_in &= (uint16_t)~(1U << (ep_addr & 0x0FU));
  else
    stall_out &= (uint16_t)~(1U << (ep_addr & 0x0FU));

  return status_from_hal(HAL_PCD_ClearEndpointStall((hal_pcd_handle_t *)pdev->pData, ep_addr));
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  (void)pdev;
  if ((ep_addr & 0x80U) == 0x80U)
    return (stall_in  & (uint16_t)(1U << (ep_addr & 0x0FU))) ? 1U : 0U;
  else
    return (stall_out & (uint16_t)(1U << (ep_addr & 0x0FU))) ? 1U : 0U;
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
  hal_status_t s = HAL_PCD_SetDeviceAddress((hal_pcd_handle_t *)pdev->pData, dev_addr);
  is_connected = true;
  return status_from_hal(s);
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{
  return status_from_hal(HAL_PCD_SetEndpointTransmit((hal_pcd_handle_t *)pdev->pData, ep_addr, pbuf, size));
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf, uint32_t size)
{
  return status_from_hal(HAL_PCD_SetEndpointReceive((hal_pcd_handle_t *)pdev->pData, ep_addr, pbuf, size));
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return HAL_PCD_EP_GetRxCount((hal_pcd_handle_t *)pdev->pData, ep_addr);
}

USBD_StatusTypeDef USBD_LL_DevConnect(USBD_HandleTypeDef *pdev)
{
  return status_from_hal(HAL_PCD_DeviceConnect((hal_pcd_handle_t *)pdev->pData));
}

USBD_StatusTypeDef USBD_LL_DevDisconnect(USBD_HandleTypeDef *pdev)
{
  return status_from_hal(HAL_PCD_DeviceDisconnect((hal_pcd_handle_t *)pdev->pData));
}

void USBD_LL_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}

void *USBD_static_malloc(uint32_t size)
{
  (void)size;
  static uint32_t mem[(sizeof(USBD_CDC_HandleTypeDef) / 4) + 1]; /* 32-bit 정렬 */
  return mem;
}

void USBD_static_free(void *p)
{
  (void)p;
}


/*=============================================================================
   USB 인터럽트
 =============================================================================*/
void USB_DRD_FS_IRQHandler(void)
{
  HAL_PCD_IRQHandler(&hpcd_USB_DRD_FS);
}
