#ifndef EVENT_THREAD_H_
#define EVENT_THREAD_H_


#ifdef __cplusplus
extern "C" {
#endif

#include "ap_def.h"


typedef struct
{
  uint16_t id;
  uint32_t data;
} event_msg_t;


bool eventInit(void);
bool eventAddCallback(uint16_t event_id, void (*p_func)(event_msg_t *p_msg));
bool eventPut(event_msg_t *p_msg);


#ifdef __cplusplus
}
#endif

#endif