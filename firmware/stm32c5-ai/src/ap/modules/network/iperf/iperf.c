#include "module.h"

#include "lwip/tcpip.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"


static bool iperfInit(void);
static void cliIperf(cli_args_t *args);
static void iperfReportFn(void *arg, enum lwiperf_report_type report_type,
                          const ip_addr_t *local_addr, u16_t local_port,
                          const ip_addr_t *remote_addr, u16_t remote_port,
                          u32_t bytes, u32_t ms, u32_t kbps);
static void iperfUdpRecv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                         const ip_addr_t *addr, u16_t port);

MODULE_DEF(iperf)
{
  .name     = "iperf",
  .priority = MODULE_PRI_LOWEST,
  .init     = iperfInit,
};

// lwiperf 클라이언트 세션은 완료 시 스스로 free 되므로 붙잡지 않는다.
// 서버 리스너 핸들만 추적한다.
//
static void          *s_server  = NULL;
static struct udp_pcb *s_udp_pcb = NULL;




bool iperfInit(void)
{
  cliAdd("iperf", cliIperf);
  return true;
}

// lwiperf(TCP) 세션 종료 시 tcpip 스레드에서 호출됨
//
void iperfReportFn(void *arg, enum lwiperf_report_type report_type,
                   const ip_addr_t *local_addr, u16_t local_port,
                   const ip_addr_t *remote_addr, u16_t remote_port,
                   u32_t bytes, u32_t ms, u32_t kbps)
{
  (void)arg; (void)report_type; (void)local_addr; (void)local_port;
  (void)remote_addr; (void)remote_port;

  logPrintf("iperf tcp: %lu bytes, %lu ms, %lu.%02lu Mbps\n",
            (unsigned long)bytes, (unsigned long)ms,
            (unsigned long)(kbps / 1000), (unsigned long)((kbps % 1000) / 10));
}

// UDP 수신 싱크: ~1초 간격으로 처리량 출력 (tcpip 스레드 컨텍스트)
//
void iperfUdpRecv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                  const ip_addr_t *addr, u16_t port)
{
  static u32_t t0  = 0;
  static u32_t acc = 0;
  u32_t now;

  (void)arg; (void)pcb; (void)addr; (void)port;

  if (p == NULL)
    return;

  now  = sys_now();
  acc += p->tot_len;
  pbuf_free(p);

  if (t0 == 0)
  {
    t0 = now;
    return;
  }

  if ((now - t0) >= 1000)
  {
    u32_t kbps = (acc * 8) / (now - t0);   // bits / ms = kbit/s
    logPrintf("iperf udp: %lu.%02lu Mbps\n",
              (unsigned long)(kbps / 1000), (unsigned long)((kbps % 1000) / 10));
    t0  = now;
    acc = 0;
  }
}

void cliIperf(cli_args_t *args)
{
  bool ret = false;


  // TCP 서버 (PC: iperf -c <board_ip>)  -> 보드 RX 처리량
  if (args->argc == 1 && args->isStr(0, "server"))
  {
    LOCK_TCPIP_CORE();
    if (s_server != NULL)
      lwiperf_abort(s_server);
    s_server = lwiperf_start_tcp_server_default(iperfReportFn, NULL);
    UNLOCK_TCPIP_CORE();

    cliPrintf("iperf TCP server on :%d  (PC: iperf -c <board_ip>)\n", LWIPERF_TCP_PORT_DEFAULT);
    cliPrintf("press any key to stop...\n");
    while (cliKeepLoop())
      delay(50);

    LOCK_TCPIP_CORE();
    if (s_server != NULL) { lwiperf_abort(s_server); s_server = NULL; }
    UNLOCK_TCPIP_CORE();
    cliPrintf("iperf server stopped\n");
    ret = true;
  }

  // TCP 클라이언트 (PC: iperf -s)  -> 보드 TX 처리량
  if (args->argc == 2 && args->isStr(0, "client"))
  {
    ip_addr_t remote;

    if (ipaddr_aton(args->getStr(1), &remote))
    {
      LOCK_TCPIP_CORE();
      lwiperf_start_tcp_client_default(&remote, iperfReportFn, NULL);
      UNLOCK_TCPIP_CORE();

      cliPrintf("iperf TCP client -> %s  (PC: iperf -s)\n", args->getStr(1));
      cliPrintf("press any key to stop...\n");
      while (cliKeepLoop())
        delay(50);
      cliPrintf("iperf client done\n");
    }
    else
    {
      cliPrintf("invalid ip\n");
    }
    ret = true;
  }

  // UDP 싱크 (PC: iperf -c <board_ip> -u -b 50M)  -> 보드 UDP RX 처리량
  if (args->argc == 1 && args->isStr(0, "udprx"))
  {
    LOCK_TCPIP_CORE();
    if (s_udp_pcb == NULL)
    {
      s_udp_pcb = udp_new();
      if (s_udp_pcb != NULL)
      {
        udp_bind(s_udp_pcb, IP_ANY_TYPE, LWIPERF_TCP_PORT_DEFAULT);
        udp_recv(s_udp_pcb, iperfUdpRecv, NULL);
      }
    }
    UNLOCK_TCPIP_CORE();

    cliPrintf("iperf UDP sink on :%d  (PC: iperf -c <board_ip> -u -b 50M)\n", LWIPERF_TCP_PORT_DEFAULT);
    cliPrintf("press any key to stop...\n");
    while (cliKeepLoop())
      delay(50);

    LOCK_TCPIP_CORE();
    if (s_udp_pcb != NULL) { udp_remove(s_udp_pcb); s_udp_pcb = NULL; }
    UNLOCK_TCPIP_CORE();
    cliPrintf("iperf udp stopped\n");
    ret = true;
  }

  // UDP TX 블래스터 (PC: iperf -s -u)  -> 보드 UDP 송신
  if (args->argc == 2 && args->isStr(0, "udptx"))
  {
    ip_addr_t remote;

    if (ipaddr_aton(args->getStr(1), &remote))
    {
      struct udp_pcb *pcb = udp_new();
      uint32_t t0    = millis();
      uint32_t bytes = 0;

      cliPrintf("iperf UDP TX -> %s:%d  (PC: iperf -s -u)\n", args->getStr(1), LWIPERF_TCP_PORT_DEFAULT);
      cliPrintf("press any key to stop...\n");

      while (cliKeepLoop() && pcb != NULL)
      {
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 1470, PBUF_RAM);
        err_t        e = ERR_MEM;

        if (p != NULL)
        {
          LOCK_TCPIP_CORE();
          e = udp_sendto(pcb, p, &remote, LWIPERF_TCP_PORT_DEFAULT);
          UNLOCK_TCPIP_CORE();
          pbuf_free(p);
        }

        if (e == ERR_OK)
          bytes += 1470;
        else
          delay(1);   // 자원 부족 시 백오프

        if ((millis() - t0) >= 1000)
        {
          uint32_t kbps = (bytes * 8) / (millis() - t0);
          cliPrintf("iperf udp tx: %lu.%02lu Mbps\n",
                    (unsigned long)(kbps / 1000), (unsigned long)((kbps % 1000) / 10));
          t0    = millis();
          bytes = 0;
        }
      }

      if (pcb != NULL)
      {
        LOCK_TCPIP_CORE();
        udp_remove(pcb);
        UNLOCK_TCPIP_CORE();
      }
      cliPrintf("iperf udp tx stopped\n");
    }
    else
    {
      cliPrintf("invalid ip\n");
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("iperf server          # TCP RX (PC: iperf -c <board_ip>)\n");
    cliPrintf("iperf client [pc_ip]  # TCP TX (PC: iperf -s)\n");
    cliPrintf("iperf udprx           # UDP RX (PC: iperf -c <board_ip> -u -b 50M)\n");
    cliPrintf("iperf udptx [pc_ip]   # UDP TX (PC: iperf -s -u)\n");
    cliPrintf("(press any key to stop a running test)\n");
  }
}
