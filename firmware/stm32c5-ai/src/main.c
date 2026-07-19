#include "main.h"



#ifdef _USE_HW_RTOS
static void mainThread(void *arg);


int main(void)
{
  bspInit();

  osKernelInitialize();

  const osThreadAttr_t attr =
  {
    .name       = "main",
    .stack_size = _HW_DEF_THREAD_MAIN_STACK,
    .priority   = _HW_DEF_THREAD_MAIN_PRI,
  };
  if (osThreadNew(mainThread, NULL, &attr) == NULL)
  {
    ledInit();

    while(1)
    {
      ledOn(_DEF_LED1);
      delay(50);
      ledOff(_DEF_LED1);
      delay(50);
      ledOn(_DEF_LED1);
      delay(500);
      ledOff(_DEF_LED1);
      delay(500);
    }
  }

  osKernelStart();
  return 0;
}

void mainThread(void *arg)
{
  UNUSED(arg);

  hwInit();
  apInit();
  apMain();
}
#else
int main(void)
{
  bspInit();

  hwInit();
  apInit();
  apMain();

  return 0;
}
#endif
