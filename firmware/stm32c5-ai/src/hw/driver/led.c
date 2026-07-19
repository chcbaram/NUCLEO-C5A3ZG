#include "led.h"


#ifdef _USE_HW_LED



const typedef struct 
{
  hal_gpio_t           port;
  uint32_t             pin;
  hal_gpio_pin_state_t on_state;
  hal_gpio_pin_state_t off_state;
} led_tbl_t;

static led_tbl_t led_tbl[LED_MAX_CH] =
{
  {HAL_GPIOG, HAL_GPIO_PIN_1, HAL_GPIO_PIN_RESET, HAL_GPIO_PIN_SET  },
  {HAL_GPIOA, HAL_GPIO_PIN_5, HAL_GPIO_PIN_SET,   HAL_GPIO_PIN_RESET},
  {HAL_GPIOG, HAL_GPIO_PIN_2, HAL_GPIO_PIN_RESET, HAL_GPIO_PIN_SET  },
};




bool ledInit(void)
{
  hal_gpio_config_t gpio_config;


  HAL_RCC_GPIOA_EnableClock();
  HAL_RCC_GPIOG_EnableClock();

  gpio_config.mode        = HAL_GPIO_MODE_OUTPUT;
  gpio_config.speed       = HAL_GPIO_SPEED_FREQ_LOW;
  gpio_config.pull        = HAL_GPIO_PULL_NO;
  gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
  gpio_config.init_state  = HAL_GPIO_PIN_SET;

  for (int i=0; i<LED_MAX_CH; i++)
  {
    HAL_GPIO_Init(led_tbl[i].port,led_tbl[i].pin, &gpio_config);
    ledOff(i);
  }

  return true;
}

void ledOn(uint8_t ch)
{
  if (ch >= LED_MAX_CH) return;

  HAL_GPIO_WritePin(led_tbl[ch].port, led_tbl[ch].pin, led_tbl[ch].on_state);
}

void ledOff(uint8_t ch)
{
  if (ch >= LED_MAX_CH) return;

  HAL_GPIO_WritePin(led_tbl[ch].port, led_tbl[ch].pin, led_tbl[ch].off_state);
}

void ledToggle(uint8_t ch)
{
  if (ch >= LED_MAX_CH) return;

  HAL_GPIO_TogglePin(led_tbl[ch].port, led_tbl[ch].pin);
}
#endif