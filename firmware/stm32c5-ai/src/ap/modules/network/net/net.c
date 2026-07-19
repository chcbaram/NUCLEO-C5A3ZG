#include "module.h"

#include "mx_lwip.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/ip4_addr.h"
#include "lwip/apps/sntp.h"
#include <time.h>


// SNTP : UTC 기준 시각에 타임존 오프셋(KST = UTC+9) 적용 후 RTC 세팅
//
#define SNTP_TZ_OFFSET_SEC    (9 * 3600)
#define SNTP_SERVER_NAME      "pool.ntp.org"


static bool netInit(void);
static void netThread(void const *arg);
static void netExtCallback(struct netif *netif, netif_nsc_reason_t reason,
                           const netif_ext_callback_args_t *p_args);

MODULE_DEF(net)
{
  .name     = "net",
  .priority = MODULE_PRI_LOWEST,
  .init     = netInit,
};

static struct netif          s_netif;
static netif_ext_callback_t  s_netif_ext_cb;
static bool                  s_sntp_started = false;




bool netInit(void)
{
  bool ret;

  ret = threadCreate("net", netThread, NULL, _HW_DEF_THREAD_NET_PRI, _HW_DEF_THREAD_NET_STACK);

  logPrintf("[%s] netInit()\n", ret ? "OK":"NG");
  return ret;
}

void netThread(void const *arg)
{
  err_t err;

  logPrintf("[  ] Thread Started : NET\n");

  // lwIP 스택 + ethernet interface 초기화 (tcpip_thread 생성)
  //
  err = mx_lwip_init();
  if (err != ERR_OK)
  {
    logPrintf("     mx_lwip_init fail (%d)\n", (int)err);
    while(1) delay(1000);
  }

  // IP/링크 변화 이벤트 콜백 등록
  //
  netif_add_ext_callback(&s_netif_ext_cb, netExtCallback);

  // netif 등록 (DMA start + 링크모니터/데이터워커 스레드는 여기서 생성됨)
  //
  netifapi_netif_add(&s_netif,
                     NULL, NULL, NULL,
                     mx_lwip_get_interface_context_1(),
                     &mx_lwip_init_interface_1,
                     &tcpip_input);
  netifapi_netif_set_default(&s_netif);
  netifapi_netif_set_up(&s_netif);

  logPrintf("     net: waiting for link / DHCP...\n");

  // 로깅은 여기(net 스레드)에서 폴링으로 처리한다.
  // ext 콜백은 core-lock 이 걸린 링크모니터 컨텍스트에서 실행되고 스택이 작으므로
  // logPrintf(vsnprintf) 같은 무거운 작업을 넣지 않는다.
  //
  bool prev_link = false;
  bool prev_ip   = false;

  while(1)
  {
    bool link   = netif_is_link_up(&s_netif);
    bool has_ip = !ip4_addr_isany_val(*netif_ip4_addr(&s_netif));

    if (link != prev_link)
    {
      logPrintf("net: link %s\n", link ? "up":"down");
      prev_link = link;
    }

    if (has_ip != prev_ip)
    {
      if (has_ip)
      {
        logPrintf("net: IP   %s\n", ip4addr_ntoa(netif_ip4_addr(&s_netif)));
        logPrintf("net: MASK %s\n", ip4addr_ntoa(netif_ip4_netmask(&s_netif)));
        logPrintf("net: GW   %s\n", ip4addr_ntoa(netif_ip4_gw(&s_netif)));

        // 최초 IP 획득 시 SNTP 시작 (raw API 는 core-lock 하에 호출)
        //
        if (s_sntp_started == false)
        {
          LOCK_TCPIP_CORE();
          sntp_setoperatingmode(SNTP_OPMODE_POLL);
          sntp_setservername(0, SNTP_SERVER_NAME);
          sntp_init();
          UNLOCK_TCPIP_CORE();
          s_sntp_started = true;
          logPrintf("sntp: started (%s)\n", SNTP_SERVER_NAME);
        }
      }
      else
      {
        logPrintf("net: IP released\n");
      }
      prev_ip = has_ip;
    }

    delay(500);
  }
}

// 링크모니터(core-lock) 컨텍스트에서 호출됨 -> DHCP 제어만, 무거운 작업 금지
//
void netExtCallback(struct netif *netif, netif_nsc_reason_t reason,
                    const netif_ext_callback_args_t *p_args)
{
  (void)p_args;

  if ((reason & LWIP_NSC_LINK_CHANGED) == LWIP_NSC_LINK_CHANGED)
  {
    if (netif_is_link_up(netif))
    {
      netifapi_dhcp_start(netif);
    }
    else
    {
      netifapi_dhcp_stop(netif);
    }
  }
}

// lwIP SNTP 클라이언트가 시각 수신 시 호출(tcpip 스레드). sec = Unix(UTC) 초.
//
void sntpSetSystemTime(uint32_t sec, uint32_t usec)
{
  time_t    t = (time_t)sec + SNTP_TZ_OFFSET_SEC;
  struct tm tmv;
  rtc_date_t date;
  rtc_time_t time;

  (void)usec;

  gmtime_r(&t, &tmv);

  date.year  = (uint8_t)((tmv.tm_year + 1900) - 2000);
  date.month = (uint8_t)(tmv.tm_mon + 1);
  date.day   = (uint8_t)tmv.tm_mday;
  time.hours   = (uint8_t)tmv.tm_hour;
  time.minutes = (uint8_t)tmv.tm_min;
  time.seconds = (uint8_t)tmv.tm_sec;

  rtcSetDate(&date);
  rtcSetTime(&time);

  logPrintf("sntp: %04d-%02d-%02d %02d:%02d:%02d (KST)\n",
            tmv.tm_year + 1900, date.month, date.day,
            time.hours, time.minutes, time.seconds);
}
