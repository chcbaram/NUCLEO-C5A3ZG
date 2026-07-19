#include "rtos.h"
#include "bsp.h"


// FreeRTOS 는 SysTick 을 시스템 틱으로 사용한다. HAL 은 별도 타이머로 옮기지 않고
// SysTick 을 공유한다(it.c 의 SysTick_Handler 에서 HAL_IncTick() 과
// xPortSysTickHandler() 를 함께 호출). 따라서 여기서는 타임베이스 재설정이 없다.


void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;

  logPrintf("StackOverflow : %s\r\n", pcTaskName);
  while (1);
}

void vApplicationMallocFailedHook(void)
{
  logPrintf("MallocFailed\r\n");
  while (1);
}
