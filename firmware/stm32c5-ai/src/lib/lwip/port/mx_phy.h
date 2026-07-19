/**
  * @file    mx_phy.h
  * @brief   Generic Ethernet PHY interface (ST MX_PHY_INTERFACE, version 1).
  *          part driver 없이 경량 PHY provider(net_phy.c)에서 이 인터페이스를 구현한다.
  */
#ifndef MX_PHY_H
#define MX_PHY_H

#ifdef __cplusplus
extern "C" {
#endif


#ifndef MX_PHY_INTERFACE_VERSION
#define MX_PHY_INTERFACE_VERSION 1

typedef enum
{
  MX_PHY_STATUS_ERROR = -1,
  MX_PHY_STATUS_OK    = 0
} mx_phy_status_t;

typedef enum
{
  MX_PHY_LINK_DOWN = 0U,
  MX_PHY_LINK_UP   = 1U
} mx_phy_link_status_t;

typedef enum
{
  MX_PHY_LINK_SPEED_NONE = 0U,
  MX_PHY_LINK_SPEED_10   = 10U,
  MX_PHY_LINK_SPEED_100  = 100U,
  MX_PHY_LINK_SPEED_1000 = 1000U
} mx_phy_link_speed_t;

typedef enum
{
  MX_PHY_LINK_DUPLEX_NONE = 0U,
  MX_PHY_LINK_HALF_DUPLEX = 1U,
  MX_PHY_LINK_FULL_DUPLEX = 2U
} mx_phy_link_duplex_t;

typedef struct
{
  mx_phy_link_status_t status;
  mx_phy_link_speed_t  speed;
  mx_phy_link_duplex_t duplex;
} mx_phy_link_mode_t;

typedef void (*mx_phy_get_link_mode_fn)(mx_phy_link_mode_t *);
typedef void (*mx_phy_event_callback_fn)(void);

typedef struct
{
  mx_phy_get_link_mode_fn get_link_mode;
} mx_phy_interface_t;

#define MX_PHY_LINK_MODE_CLEAR(p_mode) do { \
  (p_mode)->status = MX_PHY_LINK_DOWN;      \
  (p_mode)->speed  = MX_PHY_LINK_SPEED_NONE;\
  (p_mode)->duplex = MX_PHY_LINK_DUPLEX_NONE;\
} while (0)

#endif /* MX_PHY_INTERFACE_VERSION */


#ifdef __cplusplus
}
#endif

#endif /* MX_PHY_H */
