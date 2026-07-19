/**
  * @file    net_phy.c
  * @brief   mx_phy_interface_t 구현. 별도 LAN8742 part driver 대신
  *          hw/driver/eth 의 경량 PHY 접근(ethGetLink)을 래핑한다.
  */
#include "mx_phy1.h"
#include "eth/eth.h"


static void phy1_get_link_mode(mx_phy_link_mode_t *p_link);

static mx_phy_interface_t phy_interface_1 =
{
  .get_link_mode = phy1_get_link_mode
};



static void phy1_get_link_mode(mx_phy_link_mode_t *p_link)
{
  eth_link_t link;

  MX_PHY_LINK_MODE_CLEAR(p_link);

  if ((ethGetLink(&link) == true) && (link.is_link == true))
  {
    p_link->status = MX_PHY_LINK_UP;
    p_link->speed  = (link.speed == 100) ? MX_PHY_LINK_SPEED_100 : MX_PHY_LINK_SPEED_10;
    p_link->duplex = link.is_full_duplex ? MX_PHY_LINK_FULL_DUPLEX : MX_PHY_LINK_HALF_DUPLEX;
  }
}

mx_phy_status_t mx_phy1_init(void)
{
  // PHY 는 ethInit() 에서 이미 리셋/오토네고 시작됨
  return MX_PHY_STATUS_OK;
}

mx_phy_status_t mx_phy1_deinit(void)
{
  return MX_PHY_STATUS_OK;
}

mx_phy_interface_t *mx_phy1_get_interface(void)
{
  return &phy_interface_1;
}
