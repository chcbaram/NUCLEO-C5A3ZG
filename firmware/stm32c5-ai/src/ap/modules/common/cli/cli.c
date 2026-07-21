#include "module.h"
#include "driver/cli_net.h"
#include "cdc.h"


#define CLI_NET_PORT      23


static bool cliThreadInit(void);
static void cliThread(void const *arg);
static void cliCmd(cli_args_t *args);

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
  cliAdd("cli", cliCmd);            // 현재 CLI 채널 조회 (cli info)

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

    // CLI 채널 전환: 엄격한 우선순위 UART > USB > NET
    cliNetPoll();

    // NET(telnet) : 최하위 후보
    if (cliNetIsConnected())
    {
      cli_ch = HW_UART_CH_NET;
    }
    else if (cli_ch == HW_UART_CH_NET)
    {
      cli_ch = HW_UART_CH_CLI;
    }

    // USB(CDC) : 포트가 오픈(DTR)되고 통신속도가 115200 이면 NET 보다 우선
    if (cdcIsConnect() && cdcGetBaud() == 115200)
    {
      cli_ch = HW_UART_CH_USB;
    }
    else if (cli_ch == HW_UART_CH_USB)
    {
      cli_ch = HW_UART_CH_CLI;
    }

    // 로컬 UART 입력이 들어오면 항상 최우선
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

void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    const char *ch_name[] = { "UART(SWD)", "NET(TELNET)", "USB(CDC)" };
    uint8_t     cur       = cliGetPort();

    cliPrintf("CLI Channel : %d (%s)\n", cur, (cur < 3) ? ch_name[cur] : "?");
    cliPrintf("UART RX     : %d\n", (int)uartAvailable(HW_UART_CH_CLI));
    cliPrintf("USB  Open   : %d   Baud : %d\n", cdcIsConnect(), (int)cdcGetBaud());
    cliPrintf("NET  Conn   : %d\n", cliNetIsConnected());

    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("cli info\n");
  }
}

