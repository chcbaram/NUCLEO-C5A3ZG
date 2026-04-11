#include "bsp.h"
#include "hw_def.h"


static void mpuInit(void);
static system_status_t mx_rcc_init(void);
static system_status_t mx_cortex_nvic_init(void);
static system_status_t mx_rcc_peripherals_clock_config(void);
static hal_icache_handle_t hICACHE;




bool bspInit(void)
{
  bool ret = true;


  HAL_Init();

  mx_rcc_init();
  mx_cortex_nvic_init();
  mx_rcc_peripherals_clock_config();

  mpuInit();

  if (HAL_ICACHE_Init(&hICACHE, HAL_ICACHE) != HAL_OK)
  {
    return false;
  }

  /* ICACHE automatically started at startup */
  if (HAL_ICACHE_Start(&hICACHE, HAL_ICACHE_IT_NONE) != HAL_OK)
  {
    return false;
  }

  return ret;
}

void delay(uint32_t ms)
{
#ifdef _USE_HW_RTOS
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
    osDelay(ms);
  }
  else
  {
    HAL_Delay(ms);
  }
#else
  HAL_Delay(ms);
#endif
}

void delayUs(uint32_t delay_us)
{
  uint32_t pre_time = micros();
  
  while(micros()-pre_time <= delay_us)
  {
    //
  } 
}

uint32_t millis(void)
{
  return HAL_GetTick();
}

uint32_t micros(void)
{
  uint32_t       m0  = millis();
  __IO uint32_t  u0  = SysTick->VAL;
  uint32_t       m1  = millis();
  __IO uint32_t  u1  = SysTick->VAL;
  const uint32_t tms = SysTick->LOAD + 1;

  if (m1 != m0)
  {
    return (m1 * 1000 + ((tms - u1) * 1000) / tms);
  }
  else
  {
    return (m0 * 1000 + ((tms - u0) * 1000) / tms);
  }
}

void Error_Handler(void)
{
  if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)  
  { 
    __BKPT(0);
  }
    
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

/**
  * Configure the system core clock only and activate it using the HAL RCC unitary APIs (footprint optimization)
  *         The system Clock is configured as follow :
  *            System Clock source            = HSIS
  *            SYSCLK(Hz)                     = 144000000
  *            HCLK(Hz)                       = 144000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 1
  *            APB2 Prescaler                 = 1
  *            APB3 Prescaler                 = 1
  *            Flash Latency(WS)              = 4
  */
system_status_t mx_rcc_init(void)
{
  if (HAL_RCC_HSIS_Enable() != HAL_OK)
  {
    return SYSTEM_CLOCK_ERROR;
  }

  /** Initializes the CPU, AHB and APB busses clocks */
  hal_rcc_bus_clk_config_t config_bus;
  config_bus.hclk_prescaler  = HAL_RCC_HCLK_PRESCALER1;
  config_bus.pclk1_prescaler = HAL_RCC_PCLK_PRESCALER1;
  config_bus.pclk2_prescaler = HAL_RCC_PCLK_PRESCALER1;
  config_bus.pclk3_prescaler = HAL_RCC_PCLK_PRESCALER1;
  if (HAL_RCC_SetBusClockConfig(&config_bus) != HAL_OK)
  {
    return SYSTEM_CLOCK_ERROR;
  }

  /** Frequency will be increased */
  HAL_FLASH_ITF_SetLatency(HAL_FLASH, HAL_FLASH_ITF_LATENCY_4);

  if (HAL_RCC_SetSYSCLKSource(HAL_RCC_SYSCLK_SRC_HSIS) != HAL_OK)
  {
    return SYSTEM_CLOCK_ERROR;
  }

  HAL_FLASH_ITF_SetProgrammingDelay(HAL_FLASH, HAL_FLASH_ITF_PROGRAM_DELAY_2);

  if (HAL_UpdateCoreClock() != HAL_OK)
  {
    return SYSTEM_CLOCK_ERROR;
  }

  /* No GPIO configuration required for RCC */

  return SYSTEM_OK;
}

system_status_t mx_cortex_nvic_init(void)
{
  /* Enable DebugMonitor exception */
  STM32_SET_BIT(DCB->DEMCR, DCB_DEMCR_MON_EN_Msk);

  /* Configure the Priority grouping */
  HAL_CORTEX_NVIC_SetPriorityGrouping(HAL_CORTEX_NVIC_PRIORITY_GROUP_4);

  /* Debug Monitor */
  HAL_CORTEX_NVIC_SetPriority(DebugMonitor_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_0, HAL_CORTEX_NVIC_SUB_PRIORITY_0);

  /* Pendable request for system service */
  HAL_CORTEX_NVIC_SetPriority(PendSV_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_0, HAL_CORTEX_NVIC_SUB_PRIORITY_0);

  return SYSTEM_OK;
}

system_status_t mx_rcc_peripherals_clock_config(void)
{
  return SYSTEM_OK;
}

static void mpuInit(void)
{
  /* Disables the MPU */
  HAL_CORTEX_MPU_Disable();

  /*
     Initializes and configures the MPU attributes
  */
  HAL_CORTEX_MPU_SetCacheMemAttr(HAL_CORTEX_MPU_MEM_ATTR_0, HAL_CORTEX_MPU_NORMAL_MEM_NCACHEABLE);

  /*
     Initializes and configures the MPU Region
  */
  hal_cortex_mpu_region_config_t p_region_config = {0};

  p_region_config.base_addr   = 0x8FFE000;
  p_region_config.limit_addr  = 0x8FFFFFF;
  p_region_config.access_attr = HAL_CORTEX_MPU_REGION_ALL_RO;
  p_region_config.exec_attr   = HAL_CORTEX_MPU_EXECUTION_ATTR_DISABLE;
  p_region_config.attr_idx    = HAL_CORTEX_MPU_MEM_ATTR_0;
  HAL_CORTEX_MPU_SetConfigRegion(HAL_CORTEX_MPU_REGION_0, &p_region_config);

  HAL_CORTEX_MPU_EnableRegion(HAL_CORTEX_MPU_REGION_0);

  /* Enables the MPU */
  HAL_CORTEX_MPU_Enable(HAL_CORTEX_MPU_HARDFAULT_NMI_DISABLE, HAL_CORTEX_MPU_ACCESS_FAULT_ONLY_PRIV);
}