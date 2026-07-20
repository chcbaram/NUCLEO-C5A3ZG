/*
 * FreeRTOS Kernel V10.2.1
 * Copyright (C) 2019 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * FreeRTOSConfig for STM32C5A3 (Cortex-M33, non-secure) + CMSIS-RTOS v2.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Ensure stdint is only used by the compiler, and not the assembler. */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
 #include <stdint.h>
 extern uint32_t SystemCoreClock;
#endif

/* CMSIS device header required by the CMSIS-RTOS v2 wrapper (freertos_os2.h). */
#define CMSIS_device_header               "stm32c5xx.h"

#define configENABLE_TRUSTZONE            0
#define configENABLE_FPU                  1
#define configENABLE_MPU                  0

#define configUSE_PREEMPTION              1
#define configUSE_IDLE_HOOK               1
#define configUSE_TICK_HOOK               1
/* CMSIS-RTOS v2 uses 56 priority levels (osPriority_t), so port-optimised
 * task selection must be off and configMAX_PRIORITIES must be 56. */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configMAX_PRIORITIES              (56)
#define configCPU_CLOCK_HZ                (SystemCoreClock)
#define configTICK_RATE_HZ                ((TickType_t)1000)
#define configMINIMAL_STACK_SIZE          ((uint16_t)128)
#define configTOTAL_HEAP_SIZE             ((size_t)(64 * 1024))
#define configMAX_TASK_NAME_LEN           (16)
#define configUSE_TRACE_FACILITY          1
#define configUSE_16_BIT_TICKS            0
#define configIDLE_SHOULD_YIELD           1
#define configUSE_MUTEXES                 1
#define configUSE_RECURSIVE_MUTEXES       1
#define configUSE_COUNTING_SEMAPHORES     1
#define configUSE_TASK_NOTIFICATIONS      1
#define configQUEUE_REGISTRY_SIZE         8
#define configCHECK_FOR_STACK_OVERFLOW    2
#define configUSE_MALLOC_FAILED_HOOK      1
#define configUSE_APPLICATION_TASK_TAG    1
#define configGENERATE_RUN_TIME_STATS     0
#define configUSE_STATS_FORMATTING_FUNCTIONS 1

#define configSUPPORT_STATIC_ALLOCATION   1
#define configSUPPORT_DYNAMIC_ALLOCATION  1

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES             0
#define configMAX_CO_ROUTINE_PRIORITIES   (2)

/* Software timer definitions (disabled). */
#define configUSE_TIMERS                  0
#define configTIMER_TASK_PRIORITY         (2)
#define configTIMER_QUEUE_LENGTH          10
#define configTIMER_TASK_STACK_DEPTH      (configMINIMAL_STACK_SIZE * 2)

/* CMSIS-RTOS v2 wrapper options: event flags from ISR need software timers,
 * which we keep disabled, so exclude that path. */
#define configUSE_OS2_EVENTFLAGS_FROM_ISR 0

/* it.c owns SysTick_Handler (HAL_IncTick + xPortSysTickHandler), so tell the
 * CMSIS-RTOS v2 wrapper not to define its own. */
#define USE_CUSTOM_SYSTICK_HANDLER_IMPLEMENTATION 1

/* API functions required by the CMSIS-RTOS v2 wrapper (freertos_os2.h). */
#define INCLUDE_vTaskPrioritySet          1
#define INCLUDE_uxTaskPriorityGet         1
#define INCLUDE_vTaskDelete               1
#define INCLUDE_vTaskCleanUpResources     0
#define INCLUDE_vTaskSuspend              1
#define INCLUDE_vTaskDelayUntil           1
#define INCLUDE_vTaskDelay                1
#define INCLUDE_xTaskGetSchedulerState    1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xSemaphoreGetMutexHolder  1
#define INCLUDE_eTaskGetState             1

/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
 #define configPRIO_BITS                  __NVIC_PRIO_BITS
#else
 #define configPRIO_BITS                  4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY      0xf
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY     ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* Assert routed to logPrintf. */
void logPrintf(const char *fmt, ...);
#define configASSERT( x ) if( ( x ) == 0 ) { logPrintf("configASSERT()\n  %s\n", __FUNCTION__); taskDISABLE_INTERRUPTS(); for( ;; ); }

/* Map the FreeRTOS port handlers to the CMSIS standard names.
 * SysTick is deliberately left unmapped: it.c owns SysTick_Handler and routes
 * it to xPortSysTickHandler while also keeping HAL_IncTick() alive. */
#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler
// #define xPortSysTickHandler SysTick_Handler

/* CPU-load monitor hooks (cpu_utils.c). */
#define traceTASK_SWITCHED_IN()  extern void StartIdleMonitor(void); StartIdleMonitor()
#define traceTASK_SWITCHED_OUT() extern void EndIdleMonitor(void);   EndIdleMonitor()

#endif /* FREERTOS_CONFIG_H */
