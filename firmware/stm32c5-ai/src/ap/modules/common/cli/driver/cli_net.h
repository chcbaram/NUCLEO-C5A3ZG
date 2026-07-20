#ifndef CLI_NET_H_
#define CLI_NET_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

bool cliNetInit(uint16_t port);   /* 텔넷 CLI 서버 초기화 (HW_UART_CH_NET 에 드라이버 등록) */
void cliNetPoll(void);            /* 주기 호출: listen/accept/연결관리 */
bool cliNetIsConnected(void);     /* 클라이언트 접속 여부 */

#ifdef __cplusplus
}
#endif

#endif
