#ifndef BSP_H_
#define BSP_H_

#ifdef __cplusplus
extern "C" {
#endif


#include "def.h"
#include "stm32_hal.h"


typedef enum
{
  SYSTEM_OK                        = 0xEAEAEAEAU,        /* System initialization successfully       */
  SYSTEM_PRESYSTEM_ERROR           = 0xF5F5F5F5U,        /* Error during System pre-initialization   */
  SYSTEM_STARTUP_ERROR             = 0x96969696U,        /* Error during startup initialization      */
  SYSTEM_INTERRUPTS_ERROR          = 0x5A5A5A5AU,        /* Error during interrupts initialization   */
  SYSTEM_CLOCK_ERROR               = 0xA5A5A5A5U,        /* Error during clock initialization        */
  SYSTEM_RESOURCES_ISOLATION_ERROR = 0x3C3C3C3CU,        /* Error during Cortex MPU initialization   */
  SYSTEM_POWER_ERROR               = 0xC3C3C3C3U,        /* Error during power initialization        */
  SYSTEM_PERIPHERAL_ERROR          = 0x6D6D6D6DU,        /* Error during peripherals initialization  */
  SYSTEM_POSTSYSTEM_ERROR          = 0xB2B2B2B2U         /* Error during System post-initialization  */
} system_status_t;


void logPrintf(const char *fmt, ...);



bool bspInit(void);

void delay(uint32_t time_ms);
void delayUs(uint32_t delay_us);
uint32_t millis(void);
uint32_t micros(void);

void Error_Handler(void);


#ifdef __cplusplus
}
#endif

#endif