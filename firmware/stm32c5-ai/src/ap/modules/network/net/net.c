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

  while(1)
  {
    delay(1000);
  }
}

// tcpip_thread 컨텍스트에서 호출됨
//
void netExtCallback(struct netif *netif, netif_nsc_reason_t reason,
                    const netif_ext_callback_args_t *p_args)
{
  (void)p_args;

  if ((reason & LWIP_NSC_LINK_CHANGED) == LWIP_NSC_LINK_CHANGED)
  {
    if (netif_is_link_up(netif))
    {
      logPrintf("net: link up -> DHCP start\n");
      netifapi_dhcp_start(netif);
    }
    else
    {
      logPrintf("net: link down -> DHCP stop\n");
      netifapi_dhcp_stop(netif);
    }
  }

  if ((reason & LWIP_NSC_IPV4_ADDR_VALID) == LWIP_NSC_IPV4_ADDR_VALID)
  {
    logPrintf("net: IP   %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
    logPrintf("net: MASK %s\n", ip4addr_ntoa(netif_ip4_netmask(netif)));
    logPrintf("net: GW   %s\n", ip4addr_ntoa(netif_ip4_gw(netif)));
  }
}
