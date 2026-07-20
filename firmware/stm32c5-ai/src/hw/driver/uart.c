#include "uart.h"
#include "qbuffer.h"
#include "cli.h"
#if HW_USE_CDC == 1
#include "cdc.h"
#endif

#ifdef _USE_HW_UART


#define UART_RX_BUF_LENGTH        1024



typedef struct
{
  bool is_open;
  uint32_t baud;

  uint8_t  rx_buf[UART_RX_BUF_LENGTH];
  qbuffer_t qbuffer;
  hal_uart_handle_t *p_huart;
  hal_uart_config_t  uart_cfg;

  uint32_t rx_cnt;
  uint32_t tx_cnt;

  uart_driver_t *p_driver;   /* NULL 이면 HW UART, 아니면 커스텀 드라이버(cli_net 등) */
} uart_tbl_t;

typedef struct
{
  const char        *p_msg;
  hal_uart_t         uart_inst;
  hal_uart_handle_t *p_huart;
  bool               is_rs485;
} uart_hw_t;


bool uartInitHw(uint8_t ch);
#if CLI_USE(HW_UART)
static void cliUart(cli_args_t *args);
#endif


static bool is_init = false;

__attribute__((section(".non_cache")))
static uart_tbl_t uart_tbl[UART_MAX_CH];

static hal_uart_handle_t hUSART2;

static hal_dma_node_t DMA_Node_USART2_RX;
static hal_dma_handle_t hLPDMA1_CH0;

const static uart_hw_t uart_hw_tbl[UART_MAX_CH] =
{
  {"USART1 SWD   ", HAL_UART2, &hUSART2, false},
#if HW_UART_MAX_CH >= 2
  {"NET TELNET   ", HAL_UART2, NULL,     false},   /* 가상채널: p_driver(cli_net) 로 입출력, HW 미사용 */
#endif
};


bool uartInit(void)
{
  for (int i=0; i<UART_MAX_CH; i++)
  {
    uart_tbl[i].is_open = false;
    uart_tbl[i].baud = 57600;
    uart_tbl[i].rx_cnt = 0;
    uart_tbl[i].tx_cnt = 0;
    uart_tbl[i].p_driver = NULL;
  }

  is_init = true;

#if CLI_USE(HW_UART)
  cliAdd("uart", cliUart);
#endif
  return true;
}

bool uartDeInit(void)
{
  return true;
}

bool uartIsInit(void)
{
  return is_init;
}

bool uartOpen(uint8_t ch, uint32_t baud)
{
  bool ret = false;
  hal_status_t ret_hal;


  if (ch >= UART_MAX_CH) return false;

  if (uart_tbl[ch].p_driver != NULL)
  {
    uart_tbl[ch].baud    = baud;
    uart_tbl[ch].is_open = uart_tbl[ch].p_driver->open(baud);
    return uart_tbl[ch].is_open;
  }

  if (uart_tbl[ch].is_open == true && uart_tbl[ch].baud == baud)
  {
    return true;
  }

  switch(ch)
  {
    case _DEF_UART1:
      uart_tbl[ch].baud      = baud;
      uart_tbl[ch].p_huart   = uart_hw_tbl[ch].p_huart;

      uart_tbl[ch].uart_cfg.baud_rate        = baud;
      uart_tbl[ch].uart_cfg.clock_prescaler  = HAL_UART_PRESCALER_DIV1;
      uart_tbl[ch].uart_cfg.word_length      = HAL_UART_WORD_LENGTH_8_BIT;
      uart_tbl[ch].uart_cfg.stop_bits        = HAL_UART_STOP_BIT_1;
      uart_tbl[ch].uart_cfg.parity           = HAL_UART_PARITY_NONE;
      uart_tbl[ch].uart_cfg.direction        = HAL_UART_DIRECTION_TX_RX;
      uart_tbl[ch].uart_cfg.hw_flow_ctl      = HAL_UART_HW_CONTROL_NONE;
      uart_tbl[ch].uart_cfg.oversampling     = HAL_UART_OVERSAMPLING_16;
      uart_tbl[ch].uart_cfg.one_bit_sampling = HAL_UART_ONE_BIT_SAMPLE_DISABLE;


      qbufferCreate(&uart_tbl[ch].qbuffer, &uart_tbl[ch].rx_buf[0], UART_RX_BUF_LENGTH);



      if (uart_tbl[ch].is_open)
      {
        HAL_UART_DeInit(uart_hw_tbl[ch].p_huart);
      }

      ret_hal = HAL_UART_Init(uart_hw_tbl[ch].p_huart, uart_hw_tbl[ch].uart_inst);
      if (ret_hal != HAL_OK)
      {
        break;
      }
        
      if (uartInitHw(ch))
      {          
        if(HAL_UART_Receive_DMA(uart_tbl[ch].p_huart, (uint8_t *)&uart_tbl[ch].rx_buf[0], UART_RX_BUF_LENGTH) == HAL_OK)
        {
          DMA_Channel_TypeDef  *p_dma_rx_reg = HAL_DMA_GetLLInstance(uart_tbl[ch].p_huart->hdma_rx);

          ret = true;
          uart_tbl[ch].is_open = true;
          uart_tbl[ch].qbuffer.in  = uart_tbl[ch].qbuffer.len - p_dma_rx_reg->CBR1;
          uart_tbl[ch].qbuffer.out = uart_tbl[ch].qbuffer.in;            
        }
      }
      break;

    case _DEF_UART2:
      uart_tbl[ch].baud    = baud;
      uart_tbl[ch].is_open = true;
      ret = true;
      break;      
  }

  return ret;
}

bool uartClose(uint8_t ch)
{
  if (ch >= UART_MAX_CH) return false;

  if (uart_tbl[ch].p_driver != NULL)
  {
    uart_tbl[ch].p_driver->close();
  }

  uart_tbl[ch].is_open = false;

  return true;
}

bool uartSetDriver(uint8_t ch, uart_driver_t *p_driver)
{
  if (ch >= UART_MAX_CH) return false;

  uart_tbl[ch].p_driver = p_driver;
  return true;
}

bool uartInitHw(uint8_t ch)
{
  if (ch == _DEF_UART1)
  {
    HAL_RCC_USART2_EnableClock();

    if (HAL_RCC_USART2_SetKernelClkSource(HAL_RCC_USART2_CLK_SRC_PCLK1) != HAL_OK)
    {
      return false;
    }

    if (HAL_UART_SetConfig(uart_tbl[ch].p_huart, &uart_tbl[ch].uart_cfg) != HAL_OK)
    {
      return false;
    }

    HAL_RCC_GPIOA_EnableClock();

    hal_gpio_config_t gpio_config;

    /**
      USART2 GPIO Configuration

      [GPIO Pin] ------> [Signal Name]

        PA3     ------>   USART2_RX
        PA2     ------>   USART2_TX
      **/
    gpio_config.mode        = HAL_GPIO_MODE_ALTERNATE;
    gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
    gpio_config.pull        = HAL_GPIO_PULL_NO;
    gpio_config.speed       = HAL_GPIO_SPEED_FREQ_LOW;
    gpio_config.alternate   = HAL_GPIO_AF_7;
    HAL_GPIO_Init(HAL_GPIOA, HAL_GPIO_PIN_3 | HAL_GPIO_PIN_2, &gpio_config);

    /* Configure the DMA RX */
    if (HAL_DMA_Init(&hLPDMA1_CH0, HAL_LPDMA1_CH0) != HAL_OK)
    {
      return false;
    }

    HAL_RCC_LPDMA1_EnableClock();

    hal_dma_direct_xfer_config_t xfer_cfg_usart2_rx_dma;
    xfer_cfg_usart2_rx_dma.request         = HAL_LPDMA1_REQUEST_USART2_RX;
    xfer_cfg_usart2_rx_dma.direction       = HAL_DMA_DIRECTION_PERIPH_TO_MEMORY;
    xfer_cfg_usart2_rx_dma.src_inc         = HAL_DMA_SRC_ADDR_FIXED;
    xfer_cfg_usart2_rx_dma.dest_inc        = HAL_DMA_DEST_ADDR_INCREMENTED;
    xfer_cfg_usart2_rx_dma.src_data_width  = HAL_DMA_SRC_DATA_WIDTH_BYTE;
    xfer_cfg_usart2_rx_dma.dest_data_width = HAL_DMA_DEST_DATA_WIDTH_BYTE;
    xfer_cfg_usart2_rx_dma.priority        = HAL_DMA_PRIORITY_LOW_WEIGHT_LOW;

    if (HAL_DMA_SetConfigPeriphLinkedListCircularXfer(&hLPDMA1_CH0, &DMA_Node_USART2_RX, &xfer_cfg_usart2_rx_dma) != HAL_OK)
    {
      return false;
    }

    /* Link the Receive DMA handle to the UART handle */
    if (HAL_UART_SetRxDMA(&hUSART2, &hLPDMA1_CH0) != HAL_OK)
    {
      return false;
    }

    return true;
  }

  return false;
}

uint32_t uartAvailable(uint8_t ch)
{
  uint32_t ret = 0;

  if (ch >= UART_MAX_CH) return 0;

  if (uart_tbl[ch].p_driver != NULL)
  {
    return uart_tbl[ch].p_driver->available();
  }

  switch(ch)
  {
    case _DEF_UART1:
      uart_tbl[ch].qbuffer.in = (uart_tbl[ch].qbuffer.len - HAL_DMA_GetLLInstance(uart_tbl[ch].p_huart->hdma_rx)->CBR1);
      ret = qbufferAvailable(&uart_tbl[ch].qbuffer);      
      break;

    case _DEF_UART2:
      #if HW_USE_CDC == 1
      ret = cdcAvailable();
      #endif
      break;      
  }

  return ret;
}

bool uartFlush(uint8_t ch)
{
  uint32_t pre_time;

  if (ch >= UART_MAX_CH) return false;

  if (uart_tbl[ch].p_driver != NULL)
  {
    return uart_tbl[ch].p_driver->flush();
  }

  pre_time = millis();
  while(uartAvailable(ch))
  {
    if (millis()-pre_time >= 10)
    {
      break;
    }
    uartRead(ch);
  }

  return true;
}

uint8_t uartRead(uint8_t ch)
{
  uint8_t ret = 0;

  if (ch >= UART_MAX_CH) return 0;

  if (uart_tbl[ch].p_driver != NULL)
  {
    ret = uart_tbl[ch].p_driver->read();
    uart_tbl[ch].rx_cnt++;
    return ret;
  }

  switch(ch)
  {
    case _DEF_UART1:
      qbufferRead(&uart_tbl[ch].qbuffer, &ret, 1);
      break;

    case _DEF_UART2:
      #if HW_USE_CDC == 1
      ret = cdcRead();
      #endif
      break;      
  }
  uart_tbl[ch].rx_cnt++;

  return ret;
}

uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t length)
{
  uint32_t ret = 0;

  if (ch >= UART_MAX_CH) return 0;

  if (uart_tbl[ch].p_driver != NULL)
  {
    ret = uart_tbl[ch].p_driver->write(p_data, length);
    uart_tbl[ch].tx_cnt += ret;
    return ret;
  }

  switch(ch)
  {
    case _DEF_UART1:
      if (HAL_UART_Transmit(uart_tbl[ch].p_huart, p_data, length, 100) == HAL_OK)
      {
        ret = length;
      }
      break;

    case _DEF_UART2:
      #if HW_USE_CDC == 1
      ret = cdcWrite(p_data, length);
      #endif
      break;      
  }
  uart_tbl[ch].tx_cnt += ret;

  return ret;
}

uint32_t uartPrintf(uint8_t ch, const char *fmt, ...)
{
  char buf[256];
  va_list args;
  int len;
  uint32_t ret;

  va_start(args, fmt);
  len = vsnprintf(buf, 256, fmt, args);

  ret = uartWrite(ch, (uint8_t *)buf, len);

  va_end(args);


  return ret;
}

uint32_t uartGetBaud(uint8_t ch)
{
  uint32_t ret = 0;


  if (ch >= UART_MAX_CH) return 0;

  #if HW_USE_CDC == 1
  if (ch == HW_UART_CH_USB)
    ret = cdcGetBaud();
  else
    ret = uart_tbl[ch].baud;
  #else
  ret = uart_tbl[ch].baud;
  #endif
  
  return ret;
}

uint32_t uartGetRxCnt(uint8_t ch)
{
  if (ch >= UART_MAX_CH) return 0;

  return uart_tbl[ch].rx_cnt;
}

uint32_t uartGetTxCnt(uint8_t ch)
{
  if (ch >= UART_MAX_CH) return 0;

  return uart_tbl[ch].tx_cnt;
}

#if CLI_USE(HW_UART)
void cliUart(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    for (int i=0; i<UART_MAX_CH; i++)
    {
      cliPrintf("_DEF_UART%d : %s, %d bps\n", i+1, uart_hw_tbl[i].p_msg, uartGetBaud(i));
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "test"))
  {
    uint8_t uart_ch;

    uart_ch = constrain(args->getData(1), 1, UART_MAX_CH) - 1;

    if (uart_ch != cliGetPort())
    {
      uint8_t rx_data;

      while(1)
      {
        if (uartAvailable(uart_ch) > 0)
        {
          rx_data = uartRead(uart_ch);
          cliPrintf("<- _DEF_UART%d RX : 0x%X\n", uart_ch + 1, rx_data);
        }

        if (cliAvailable() > 0)
        {
          rx_data = cliRead();
          if (rx_data == 'q')
          {
            break;
          }
          else
          {
            uartWrite(uart_ch, &rx_data, 1);
            cliPrintf("-> _DEF_UART%d TX : 0x%X\n", uart_ch + 1, rx_data);            
          }
        }
      }
    }
    else
    {
      cliPrintf("This is cliPort\n");
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("uart info\n");
    cliPrintf("uart test ch[1~%d]\n", HW_UART_MAX_CH);
  }
}
#endif


#endif

