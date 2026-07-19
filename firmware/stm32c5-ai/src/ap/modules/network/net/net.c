#include "module.h"

#include "mx_lwip.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/ip4_addr.h"


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
