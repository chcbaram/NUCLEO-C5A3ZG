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
#define      HW_UART_MAX_CH         1
#define      HW_UART_CH_SWD         _DEF_UART1
#define      HW_UART_CH_CLI         HW_UART_CH_SWD


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
