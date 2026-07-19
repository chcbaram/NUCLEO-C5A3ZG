#ifndef ETH_H_
#define ETH_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_ETH


typedef struct
{
  bool     is_link;         // 링크 업 여부
  bool     is_autoneg;      // 오토네고 완료 여부
  uint16_t speed;           // 10 / 100 (Mbps)
  bool     is_full_duplex;  // 풀듀플렉스 여부
} eth_link_t;


bool ethInit(void);
bool ethIsInit(void);
bool ethGetLink(eth_link_t *p_link);
void ethGetMacAddr(uint8_t *p_mac);
hal_eth_handle_t *ethGetHandle(void);


#ifdef __cplusplus
}
#endif

#endif

#endif
