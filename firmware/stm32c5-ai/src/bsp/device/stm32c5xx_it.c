/* Includes ------------------------------------------------------------------*/
#include "bsp.h"
#include "fault.h"
#include "stm32c5xx_it.h"
#include "hw_def.h"


/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief Cortex-M 예외 진입 시 스택된 레지스터 프레임(R0-R3,R12,LR,PC,PSR)의
  *        시작 주소를 R0 로 넘겨 faultReset() 에 전달한다.
  *        EXC_RETURN(LR) bit2 로 MSP/PSP 중 스택 사용처를 판별한다.
  */
#ifdef _USE_HW_FAULT
void HardFault_Handler_C(uint32_t *p_stack)
{
  faultReset("HardFault", p_stack);
  while (1)
  {
  }
}

void MemManage_Handler_C(uint32_t *p_stack)
{
  faultReset("MemManage", p_stack);
  while (1)
  {
  }
}

void BusFault_Handler_C(uint32_t *p_stack)
{
  faultReset("BusFault", p_stack);
  while (1)
  {
  }
}

void UsageFault_Handler_C(uint32_t *p_stack)
{
  faultReset("UsageFault", p_stack);
  while (1)
  {
  }
}

__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile(
    "tst   lr, #4               \n"
    "ite   eq                   \n"
    "mrseq r0, msp              \n"
    "mrsne r0, psp              \n"
    "b     HardFault_Handler_C  \n"
  );
}

__attribute__((naked)) void MemManage_Handler(void)
{
  __asm volatile(
    "tst   lr, #4               \n"
    "ite   eq                   \n"
    "mrseq r0, msp              \n"
    "mrsne r0, psp              \n"
    "b     MemManage_Handler_C  \n"
  );
}

__attribute__((naked)) void BusFault_Handler(void)
{
  __asm volatile(
    "tst   lr, #4               \n"
    "ite   eq                   \n"
    "mrseq r0, msp              \n"
    "mrsne r0, psp              \n"
    "b     BusFault_Handler_C   \n"
  );
}

__attribute__((naked)) void UsageFault_Handler(void)
{
  __asm volatile(
    "tst   lr, #4               \n"
    "ite   eq                   \n"
    "mrseq r0, msp              \n"
    "mrsne r0, psp              \n"
    "b     UsageFault_Handler_C \n"
  );
}
#else
void HardFault_Handler(void)
{
  while (1)
  {
  }
}

void MemManage_Handler(void)
{
  while (1)
  {
  }
}

void BusFault_Handler(void)
{
  while (1)
  {
  }
}

void UsageFault_Handler(void)
{
  while (1)
  {
  }
}
#endif

/**
  * @brief This function handles System service call via SWI instruction.
  */
#ifndef _USE_HW_RTOS
void SVC_Handler(void)
{
}
#endif

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief This function handles Pendable request for system service.
  */
#ifndef _USE_HW_RTOS
void PendSV_Handler(void)
{
}
#endif

#ifdef _USE_HW_RTOS
extern void osSystickHandler(void);

void SysTick_Handler(void)
{
  osSystickHandler();
}
#else
extern void swtimerISR(void);

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
  HAL_CORTEX_SYSTICK_IRQHandler();  
}
#endif
