#include "module.h"



static bool cliThreadInit(void);
static void cliThread(void const *arg);

MODULE_DEF(cli) 
{
  .name = "cli",
  .priority = MODULE_PRI_NORMAL,
  .init = cliThreadInit
};





bool cliThreadInit(void)
{
  bool ret;

  cliOpen(_DEF_UART1, 115200);  

  ret = threadCreate("cli", cliThread, NULL, _HW_DEF_THREAD_CLI_PRI, _HW_DEF_THREAD_CLI_STACK);
  assert(ret);

  logPrintf("[%s] cliThreadInit()\n", ret ? "OK":"NG");
  return ret;
}

void cliThread(void const *arg)
{
  bool init_ret = true;

  logPrintf("[%s] Thread Started : CLI\n", init_ret ? "OK":"NG" );
  while(1)
  {
    cliMain();
    delay(5);
  }
}

