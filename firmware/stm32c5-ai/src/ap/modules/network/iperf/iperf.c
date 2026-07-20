#include "module.h"

#include "lwip/tcpip.h"
#include "lwip/apps/lwiperf.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/stats.h"

/* eth_interface_freertos.c 진단 카운터 */
extern volatile uint32_t lwip_hw_event_count;
extern volatile uint32_t lwip_rx_event_drop_count;


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

  // 순수 TCP TX 블래스터 (BSD 소켓, RAM 데이터, lwiperf 미사용)
  //   PC: python3 test/tcp_sink.py [port]
  // lwiperf 클라이언트와 달리 iperf2 프로토콜/FLASH 송신 버퍼를 쓰지 않으므로
  // 보드 TCP TX 성능을 순수하게 측정한다.
  //
  if (args->argc >= 2 && args->isStr(0, "tcptx"))
  {
    static uint8_t     tx_buf[8192];   // .bss RAM (FLASH 아님 -> DMA OK), 한 번에 크게 write
    struct sockaddr_in dst;
    int                sock;
    int                one  = 1;
    int                port = (args->argc >= 3) ? (int)args->getData(2) : LWIPERF_TCP_PORT_DEFAULT;

    sock = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
      cliPrintf("socket() fail\n");
      return;
    }

    memset(&dst, 0, sizeof(dst));
    dst.sin_family      = AF_INET;
    dst.sin_port        = lwip_htons(port);
    dst.sin_addr.s_addr = ipaddr_addr(args->getStr(1));

    cliPrintf("connecting to %s:%d ...\n", args->getStr(1), port);
    if (lwip_connect(sock, (struct sockaddr *)&dst, sizeof(dst)) != 0)
    {
      cliPrintf("connect fail (PC에서 tcp_sink.py 실행 중인가?)\n");
      lwip_close(sock);
      return;
    }

    lwip_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));  // Nagle off

    cliPrintf("connected. sending (RAM, no lwiperf)...  press any key to stop\n");
    {
      uint32_t t_start = millis();
      uint32_t t0      = t_start;
      uint32_t bytes   = 0;
      uint32_t total   = 0;
      uint32_t drop0   = lwip_rx_event_drop_count;
      uint32_t evt0    = lwip_hw_event_count;
      uint32_t xmit0   = lwip_stats.tcp.xmit;
      uint32_t recv0   = lwip_stats.tcp.recv;
      uint32_t tdrop0  = lwip_stats.tcp.drop;
      uint32_t tmem0   = lwip_stats.tcp.memerr;

      while (cliKeepLoop())
      {
        int n = lwip_send(sock, tx_buf, sizeof(tx_buf), 0);

        if (n > 0)
        {
          bytes += n;
          total += n;
        }
        else
        {
          cliPrintf("send() -> %d, 연결 종료\n", n);
          break;
        }

        if ((millis() - t0) >= 1000)
        {
          uint32_t kbps = (bytes * 8) / (millis() - t0);
          cliPrintf("iperf tcp tx: %lu.%02lu Mbps  | hw_evt +%lu, drop +%lu (total drop %lu)\n",
                    (unsigned long)(kbps / 1000), (unsigned long)((kbps % 1000) / 10),
                    (unsigned long)(lwip_hw_event_count - evt0),
                    (unsigned long)(lwip_rx_event_drop_count - drop0),
                    (unsigned long)lwip_rx_event_drop_count);
          t0    = millis();
          bytes = 0;
          drop0 = lwip_rx_event_drop_count;
          evt0  = lwip_hw_event_count;
        }
      }

      lwip_close(sock);

      {
        uint32_t dt   = millis() - t_start;
        uint32_t kbps = (dt > 0) ? (total / dt) * 8 : 0;  // bytes/ms*8 근사
        uint32_t segs = total / 1460;                     // 보낸 데이터 세그먼트 수(근사)
        cliPrintf("iperf tcp tx done: %lu bytes, %lu ms, ~%lu.%02lu Mbps\n",
                  (unsigned long)total, (unsigned long)dt,
                  (unsigned long)(kbps / 1000), (unsigned long)((kbps % 1000) / 10));
        cliPrintf("tcp stats: data_segs~%lu | xmit +%lu recv +%lu drop +%lu memerr +%lu\n",
                  (unsigned long)segs,
                  (unsigned long)(lwip_stats.tcp.xmit   - xmit0),
                  (unsigned long)(lwip_stats.tcp.recv   - recv0),
                  (unsigned long)(lwip_stats.tcp.drop   - tdrop0),
                  (unsigned long)(lwip_stats.tcp.memerr - tmem0));
        cliPrintf("  => xmit >> data_segs 이면 재전송 폭주, xmit~=data_segs 이면 순수 cwnd 정체\n");
      }
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("iperf server          # TCP RX (PC: iperf -c <board_ip>)\n");
    cliPrintf("iperf client [pc_ip]  # TCP TX (PC: iperf -s)\n");
    cliPrintf("iperf tcptx [pc_ip]   # TCP TX (PC: python3 test/tcp_sink.py)\n");
    cliPrintf("iperf udprx           # UDP RX (PC: iperf -c <board_ip> -u -b 50M)\n");
    cliPrintf("iperf udptx [pc_ip]   # UDP TX (PC: iperf -s -u)\n");
    cliPrintf("(press any key to stop a running test)\n");
  }
}
