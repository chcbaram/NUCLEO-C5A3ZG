#include "eth.h"


#ifdef _USE_HW_ETH
#include "cli.h"


// LAN8742 PHY 표준 레지스터 (IEEE 802.3 Clause 22)
//
#define PHY_BCR                 0x00    // Basic Control
#define PHY_BSR                 0x01    // Basic Status
#define PHY_ID1                 0x02
#define PHY_ID2                 0x03
#define PHY_SCSR                0x1F    // Special Control/Status (LAN8742)

#define PHY_BCR_SOFT_RESET      0x8000
#define PHY_BCR_AUTONEG_EN      0x1000
#define PHY_BCR_AUTONEG_RESTART 0x0200

#define PHY_BSR_LINK_UP         (1 << 2)
#define PHY_BSR_AUTONEG_DONE    (1 << 5)


#if CLI_USE(HW_ETH)
static void cliEth(cli_args_t *args);
#endif

static bool ethInitHw(void);
static void ethMacAddrFromUID(uint8_t *p_mac);
static bool phyRead(uint8_t reg, uint16_t *p_val);
static bool phyWrite(uint8_t reg, uint16_t val);

static hal_eth_handle_t hETH1;
static uint8_t          mac_addr[6];
static bool             is_init = false;




bool ethInit(void)
{
  bool ret;


  ethMacAddrFromUID(mac_addr);

  ret = ethInitHw();

  if (ret == true)
  {
    // PHY 소프트 리셋 후 오토네고 시작
    //
    uint16_t bcr = 0;
    uint32_t t_start;

    phyWrite(PHY_BCR, PHY_BCR_SOFT_RESET);

    t_start = millis();
    do
    {
      if (phyRead(PHY_BCR, &bcr) != true)
        break;
      if (millis() - t_start > 1000)
        break;
    } while (bcr & PHY_BCR_SOFT_RESET);

    phyWrite(PHY_BCR, PHY_BCR_AUTONEG_EN | PHY_BCR_AUTONEG_RESTART);
  }

  is_init = ret;
  logPrintf("[%s] ethInit()\n", ret ? "OK":"E_");
  if (ret == true)
  {
    logPrintf("     MAC : %02X:%02X:%02X:%02X:%02X:%02X\n",
              mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  }

#if CLI_USE(HW_ETH)
  cliAdd("eth", cliEth);
#endif
  return ret;
}

bool ethIsInit(void)
{
  return is_init;
}

bool ethGetLink(eth_link_t *p_link)
{
  uint16_t bsr = 0;
  uint16_t scsr = 0;

  p_link->is_link        = false;
  p_link->is_autoneg     = false;
  p_link->speed          = 0;
  p_link->is_full_duplex = false;

  // BSR 링크 비트는 래치되므로 두 번 읽어 현재 상태를 얻는다.
  //
  if (phyRead(PHY_BSR, &bsr) != true)
    return false;
  if (phyRead(PHY_BSR, &bsr) != true)
    return false;

  p_link->is_link    = (bsr & PHY_BSR_LINK_UP) ? true : false;
  p_link->is_autoneg = (bsr & PHY_BSR_AUTONEG_DONE) ? true : false;

  if (p_link->is_link == true)
  {
    if (phyRead(PHY_SCSR, &scsr) == true)
    {
      // LAN8742 PHYSCSR bits[4:2] : 001=10HD 101=10FD 010=100HD 110=100FD
      //
      switch ((scsr >> 2) & 0x7)
      {
        case 0x1: p_link->speed = 10;  p_link->is_full_duplex = false; break;
        case 0x5: p_link->speed = 10;  p_link->is_full_duplex = true;  break;
        case 0x2: p_link->speed = 100; p_link->is_full_duplex = false; break;
        case 0x6: p_link->speed = 100; p_link->is_full_duplex = true;  break;
        default: break;
      }
    }
  }

  return true;
}

void ethGetMacAddr(uint8_t *p_mac)
{
  memcpy(p_mac, mac_addr, 6);
}

hal_eth_handle_t *ethGetHandle(void)
{
  return &hETH1;
}

bool phyRead(uint8_t reg, uint16_t *p_val)
{
  return (HAL_ETH_MDIO_C22ReadData(&hETH1, HW_ETH_PHY_ADDR, reg, p_val) == HAL_OK);
}

bool phyWrite(uint8_t reg, uint16_t val)
{
  return (HAL_ETH_MDIO_C22WriteData(&hETH1, HW_ETH_PHY_ADDR, reg, val) == HAL_OK);
}

// UID 기반의 안정적인 locally-administered MAC 주소 생성 (mx_eth1.c 참고)
//
void ethMacAddrFromUID(uint8_t *p_mac)
{
  uint32_t uid[3];
  uint32_t sum1 = 0;
  uint32_t sum2 = 0;
  uint32_t fletcher;
  const uint16_t *data = (const uint16_t *)uid;

  uid[0] = LL_GetUID_Word0();
  uid[1] = LL_GetUID_Word1();
  uid[2] = LL_GetUID_Word2();

  for (uint32_t i = 0; i < (sizeof(uint32_t[3]) / sizeof(uint16_t)); i++)
  {
    sum1 = (sum1 + data[i]) % 0xffff;
    sum2 = (sum2 + sum1)    % 0xffff;
  }
  fletcher = ((sum2 << 16) | sum1);

  p_mac[0] = 0x02;   // locally administered / unicast
  p_mac[1] = 0x00;
  p_mac[2] = (fletcher      ) & 0xFF;
  p_mac[3] = (fletcher >>  8) & 0xFF;
  p_mac[4] = (fletcher >> 16) & 0xFF;
  p_mac[5] = (fletcher >> 24) & 0xFF;
}

bool ethInitHw(void)
{
  hal_eth_config_t            eth_config = {0, };
  hal_eth_mac_config_t        eth_mac_config;
  hal_eth_tx_channel_config_t eth_tx_channel_config;
  hal_eth_rx_channel_config_t eth_rx_channel_config;
  hal_gpio_config_t           gpio_config;

  // 클럭 : ETH1 커널=PSIS, ETH1REF=RMII
  //
  HAL_RCC_ETH1_EnableClock();
  HAL_RCC_ETH1TX_EnableClock();
  HAL_RCC_ETH1RX_EnableClock();
  HAL_RCC_ETH1CK_EnableClock();
  HAL_RCC_SBS_EnableClock();

  if (HAL_RCC_ETH1_SetKernelClkSource(HAL_RCC_ETH1_CLK_SRC_PSIS) != HAL_OK)
    return false;
  if (HAL_RCC_ETH1REF_SetKernelClkSource(HAL_RCC_ETH1REF_CLK_SRC_RMII) != HAL_OK)
    return false;

  // RMII GPIO (NUCLEO-C5A3ZG)
  //   PA10=ETH1_CLK(AF13) PC1=MDC(AF10) PE12=MDIO(AF10) PD1=CRS_DV(AF10)
  //   PA1=REF_CLK(AF10) PC4=RXD0(AF12) PC5=RXD1(AF13) PG11=TX_EN PG13=TXD0 PG12=TXD1(AF10)
  //
  HAL_RCC_GPIOA_EnableClock();
  HAL_RCC_GPIOC_EnableClock();
  HAL_RCC_GPIOE_EnableClock();
  HAL_RCC_GPIOD_EnableClock();
  HAL_RCC_GPIOG_EnableClock();

  gpio_config.mode        = HAL_GPIO_MODE_ALTERNATE;
  gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
  gpio_config.pull        = HAL_GPIO_PULL_NO;
  gpio_config.speed       = HAL_GPIO_SPEED_FREQ_HIGH;

  gpio_config.alternate   = HAL_GPIO_AF_13;
  HAL_GPIO_Init(HAL_GPIOA, HAL_GPIO_PIN_10, &gpio_config);   // ETH1_CLK

  gpio_config.alternate   = HAL_GPIO_AF_10;
  HAL_GPIO_Init(HAL_GPIOC, HAL_GPIO_PIN_1,  &gpio_config);   // MDC
  HAL_GPIO_Init(HAL_GPIOE, HAL_GPIO_PIN_12, &gpio_config);   // MDIO
  HAL_GPIO_Init(HAL_GPIOD, HAL_GPIO_PIN_1,  &gpio_config);   // CRS_DV
  HAL_GPIO_Init(HAL_GPIOA, HAL_GPIO_PIN_1,  &gpio_config);   // REF_CLK

  gpio_config.alternate   = HAL_GPIO_AF_12;
  HAL_GPIO_Init(HAL_GPIOC, HAL_GPIO_PIN_4,  &gpio_config);   // RXD0

  gpio_config.alternate   = HAL_GPIO_AF_13;
  HAL_GPIO_Init(HAL_GPIOC, HAL_GPIO_PIN_5,  &gpio_config);   // RXD1

  gpio_config.alternate   = HAL_GPIO_AF_10;
  HAL_GPIO_Init(HAL_GPIOG, HAL_GPIO_PIN_11 | HAL_GPIO_PIN_13 | HAL_GPIO_PIN_12, &gpio_config); // TX_EN/TXD0/TXD1

  if (HAL_ETH_Init(&hETH1, HAL_ETH1) != HAL_OK)
    return false;

  memcpy(&eth_config.mac_addr[0], mac_addr, 6);
  eth_config.media_interface = HAL_ETH_MEDIA_IF_RMII;
  if (HAL_ETH_SetConfig(&hETH1, &eth_config) != HAL_OK)
  {
    HAL_ETH_DeInit(&hETH1);
    return false;
  }

  HAL_ETH_MAC_GetConfig(&hETH1, &eth_mac_config);
  eth_mac_config.link_config.speed       = HAL_ETH_MAC_SPEED_100M;
  eth_mac_config.link_config.duplex_mode = HAL_ETH_MAC_FULL_DUPLEX_MODE;
  eth_mac_config.loopback_mode           = HAL_ETH_MAC_LOOPBACK_DISABLE;
  if (HAL_ETH_MAC_SetConfig(&hETH1, &eth_mac_config) != HAL_OK)
  {
    HAL_ETH_DeInit(&hETH1);
    return false;
  }

  // DMA Tx 채널 0
  //
  HAL_ETH_GetConfigTxChannel(&hETH1, HAL_ETH_TX_CHANNEL_0, &eth_tx_channel_config);
  eth_tx_channel_config.max_app_buffers_num                      = 10UL;
  eth_tx_channel_config.req_desc_size_align_byte                 = 1UL;
  eth_tx_channel_config.fifo_event_config.event_mode             = HAL_ETH_FIFO_EVENT_ALWAYS;
  eth_tx_channel_config.dma_channel_config.tx_dma_burst_length   = HAL_ETH_DMA_TX_BLEN_4_BEAT;
  eth_tx_channel_config.dma_channel_config.tx_pbl_x8_mode        = HAL_ETH_DMA_TX_PBL_X8_DISABLE;
  eth_tx_channel_config.dma_channel_config.tx_second_pkt_operate = HAL_ETH_DMA_TX_SEC_PKT_OP_ENABLE;
  eth_tx_channel_config.mtl_queue_config.queue_size_byte         = HAL_ETH_MTL_TX_QUEUE_SZ_2048_BYTE;
  eth_tx_channel_config.mtl_queue_config.transmit_queue_mode     = HAL_ETH_MTL_TX_Q_STORE_AND_FORWARD;
  eth_tx_channel_config.mtl_queue_config.queue_op_mode           = HAL_ETH_MTL_TX_QUEUE_ENABLED;
  if (HAL_ETH_SetConfigTxChannel(&hETH1, HAL_ETH_TX_CHANNEL_0, &eth_tx_channel_config) != HAL_OK)
  {
    HAL_ETH_DeInit(&hETH1);
    return false;
  }

  // DMA Rx 채널 0
  //
  HAL_ETH_GetConfigRxChannel(&hETH1, HAL_ETH_RX_CHANNEL_0, &eth_rx_channel_config);
  eth_rx_channel_config.dma_channel_config.rx_buffer_len_byte       = 1520UL;
  eth_rx_channel_config.max_app_buffers_num                         = 10UL;
  eth_rx_channel_config.req_desc_size_align_byte                    = 1UL;
  eth_rx_channel_config.fifo_event_config.event_mode                = HAL_ETH_FIFO_EVENT_ALWAYS;
  eth_rx_channel_config.dma_channel_config.rx_dma_burst_length      = HAL_ETH_DMA_RX_BLEN_4_BEAT;
  eth_rx_channel_config.mtl_queue_config.queue_size_byte            = HAL_ETH_MTL_RX_QUEUE_SZ_2048_BYTE;
  eth_rx_channel_config.mtl_queue_config.receive_queue_mode         = HAL_ETH_MTL_RX_Q_STORE_AND_FORWARD;
  eth_rx_channel_config.mtl_queue_config.queue_op_mode              = HAL_ETH_MTL_RX_QUEUE_ENABLED;
  eth_rx_channel_config.mtl_queue_config.drop_tcp_ip_csum_error_pkt = HAL_ETH_MTL_RX_DROP_CS_ERR_ENABLE;
  eth_rx_channel_config.mtl_queue_config.fwd_error_pkt              = HAL_ETH_MTL_RX_FWD_ERR_PKT_DISABLE;
  eth_rx_channel_config.mtl_queue_config.fwd_undersized_good_pkt    = HAL_ETH_MTL_RX_FWD_USZ_PKT_ENABLE;
  if (HAL_ETH_SetConfigRxChannel(&hETH1, HAL_ETH_RX_CHANNEL_0, &eth_rx_channel_config) != HAL_OK)
  {
    HAL_ETH_DeInit(&hETH1);
    return false;
  }

  HAL_CORTEX_NVIC_SetPriority(ETH1_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_7, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(ETH1_IRQn);

  return true;
}

void ETH1_IRQHandler(void)
{
  HAL_ETH_IRQHandler(&hETH1);
}

#if CLI_USE(HW_ETH)
void cliEth(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("is_init : %d\n", is_init);
    cliPrintf("MAC     : %02X:%02X:%02X:%02X:%02X:%02X\n",
              mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

    while(cliKeepLoop())
    {
      eth_link_t link;

      if (ethGetLink(&link) == true)
      {
        cliPrintf("link:%s autoneg:%s %dMbps %s\r",
                  link.is_link ? "UP  ":"DOWN",
                  link.is_autoneg ? "OK ":"..",
                  link.speed,
                  link.is_full_duplex ? "FD":"HD");
      }
      else
      {
        cliPrintf("MDIO read fail\r");
      }
      delay(500);
    }
    cliPrintf("\n");
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("eth info\n");
  }
}
#endif

#endif
