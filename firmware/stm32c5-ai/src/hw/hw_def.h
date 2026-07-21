#ifndef HW_DEF_H_
#define HW_DEF_H_



#include "bsp.h"
#include "assert_def.h"


#define _DEF_FIRMWATRE_VERSION    "V260411R1"
#define _DEF_BOARD_NAME           "STM32C5-FW"



// #define _USE_HW_ASSERT
#define _USE_HW_FAULT


//-- RTOS
//
#define _USE_HW_RTOS

#define _USE_HW_THREAD
#define      HW_THREAD_MAX_CNT          32

#define _HW_DEF_THREAD_MAIN_PRI         osPriorityNormal
#define _HW_DEF_THREAD_MAIN_STACK       (2*1024)
#define _HW_DEF_THREAD_CLI_PRI          osPriorityNormal
#define _HW_DEF_THREAD_CLI_STACK        (8*1024)
#define _HW_DEF_THREAD_EVENT_PRI        osPriorityNormal
#define _HW_DEF_THREAD_EVENT_STACK      (4*1024)
#define _HW_DEF_THREAD_NET_PRI          osPriorityNormal
#define _HW_DEF_THREAD_NET_STACK        (2*1024)
/* USB(tud_task)는 이벤트 큐에서 블로킹하며, 이벤트 발생 시 CLI 소비 스레드를 즉시
   선점해 스택을 서비스해야 처리량이 나온다. 그래서 CLI 보다 우선순위를 높인다. */
#define _HW_DEF_THREAD_USB_PRI          osPriorityAboveNormal
#define _HW_DEF_THREAD_USB_STACK        (4*1024)


#define _USE_HW_LED
#define      HW_LED_MAX_CH          3

#define _USE_HW_RTC
#define      HW_RTC_RESET_BITS      1
#define      HW_RTC_BOOT_MODE       2

#define _USE_HW_RESET
#define      HW_RESET_BOOT          1

#define _USE_HW_ETH
#define      HW_ETH_PHY_ADDR        0

#define _USE_HW_UART
#define      HW_UART_MAX_CH         3
#define      HW_UART_CH_SWD         _DEF_UART1
#define      HW_UART_CH_NET         _DEF_UART2   /* 가상 채널: cli_net 텔넷 소켓 */
#define      HW_UART_CH_USB         _DEF_UART3   /* 가상 채널: USB CDC (cdc*) */
#define      HW_UART_CH_CLI         HW_UART_CH_SWD


//-- USB (CDC)
//
#define _USE_HW_USB
#define _USE_HW_CDC

/* USB 스택 선택: 0 = TinyUSB, 1 = ST USB Device Library
   빌드에서 -DHW_USB_STACK=1 로 오버라이드 가능 */
#define      HW_USB_STACK_TINYUSB   0
#define      HW_USB_STACK_ST        1
#ifndef      HW_USB_STACK
#define      HW_USB_STACK           HW_USB_STACK_TINYUSB
#endif

#define      HW_USE_CDC             1
#define      HW_USE_MSC             0


#define _USE_HW_CLI
#define      HW_CLI_CMD_LIST_MAX    32
#define      HW_CLI_CMD_NAME_MAX    16
#define      HW_CLI_LINE_HIS_MAX    8
#define      HW_CLI_LINE_BUF_MAX    64

#define _USE_HW_CLI_GUI
#define      HW_CLI_GUI_WIDTH       80
#define      HW_CLI_GUI_HEIGHT      24

#define _USE_HW_LOG
#define      HW_LOG_CH              HW_UART_CH_SWD
#define      HW_LOG_BOOT_BUF_MAX    2048
#define      HW_LOG_LIST_BUF_MAX    4096



//-- CLI
//
#define _USE_CLI_HW_LOG             1
#define _USE_CLI_HW_ASSERT          1
#define _USE_CLI_HW_UART            1
#define _USE_CLI_HW_USB             1
#define _USE_CLI_HW_RTC             1
#define _USE_CLI_HW_RESET           1
#define _USE_CLI_HW_ETH             1


// typedef enum
// {
//   W6300_RST,
//   W6300_INT,
//   W6300_CS,
//   GPIO_PIN_MAX
// } GpioPinName_t;

#endif
