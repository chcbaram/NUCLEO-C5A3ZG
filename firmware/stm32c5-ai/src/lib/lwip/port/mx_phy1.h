/**
  * @file    mx_phy1.h
  * @brief   PHY1 generic interface (net_phy.c 에서 경량 LAN8742 접근으로 구현).
  */
#ifndef MX_PHY1_H
#define MX_PHY1_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mx_phy.h"


mx_phy_status_t     mx_phy1_init(void);
mx_phy_status_t     mx_phy1_deinit(void);
mx_phy_interface_t *mx_phy1_get_interface(void);


#ifdef __cplusplus
}
#endif

#endif /* MX_PHY1_H */
