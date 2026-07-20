#include "module.h"
#include "driver/cli_net.h"


#define CLI_NET_PORT      23


static bool cliThreadInit(void);
static void cliThread(void const *arg);

static uint8_t  cli_ch   = HW_UART_CH_CLI;
static uint32_t cli_baud = 115200;

MODULE_DEF(cli)
{
  .name = "cli",
  .priority = MODULE_PRI_NORMAL,
  .init = cliThreadInit
};





bool cliThreadInit(void)
{
  bool ret;

  cliNetInit(CLI_NET_PORT);         // 텔넷 CLI 서버 (HW_UART_CH_NET)
  cliOpen(cli_ch, cli_baud);

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

    // 텔넷 서버 관리 및 로컬(SWD)<->네트워크(telnet) CLI 채널 전환
    cliNetPoll();
    if (cliNetIsConnected())
    {
      cli_ch = HW_UART_CH_NET;
    }
    else if (cli_ch == HW_UART_CH_NET)
    {
      cli_ch = HW_UART_CH_CLI;
    }

    // 로컬 UART 입력이 들어오면 항상 로컬 우선
    if (uartAvailable(HW_UART_CH_CLI) > 0)
    {
      cli_ch = HW_UART_CH_CLI;
    }

    if (cliGetPort() != cli_ch)
    {
      cliOpen(cli_ch, cli_baud);
      logOpen(cli_ch, cli_baud);
    }

    delay(5);
  }
}

