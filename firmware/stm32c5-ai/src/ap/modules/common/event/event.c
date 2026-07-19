#include "event.h"



#define EVENT_FUNC_MAX        32


#define lock()      xSemaphoreTake(mutex_lock, portMAX_DELAY);
#define unLock()    xSemaphoreGive(mutex_lock);




static void eventThread(void const *arg);


MODULE_DEF(event) 
{
  .name = "event",
  .priority = MODULE_PRI_HIGH,
  .init = eventInit
};

static QueueHandle_t event_queue;
static SemaphoreHandle_t mutex_lock;

static bool is_init = false;
static uint16_t event_func_cnt = 0;
static void (*event_func[EVENT_FUNC_MAX])(event_msg_t *p_msg);
static uint16_t event_id_tbl[EVENT_FUNC_MAX];




bool eventInit(void)
{  
  bool ret = true;

  event_queue = xQueueCreate(16, sizeof(event_msg_t));
  assert(event_queue != NULL);

  assert(mutex_lock = xSemaphoreCreateMutex());  
  assert(threadCreate("event", eventThread, NULL, _HW_DEF_THREAD_EVENT_PRI, _HW_DEF_THREAD_EVENT_STACK));

  if (event_queue == NULL)
  {
    ret = true;
  }  

  logPrintf("[%s] eventInit()\n", ret ? "OK":"E_");

  is_init = ret;

  return ret;
}

bool eventAddCallback(uint16_t event_id, void (*p_func)(event_msg_t *p_msg))
{
  bool ret = false;

  if (event_func_cnt < EVENT_FUNC_MAX)
  {
    taskENTER_CRITICAL();
    event_id_tbl[event_func_cnt] = event_id;
    event_func[event_func_cnt] = p_func;
    event_func_cnt++;
    taskEXIT_CRITICAL();
  }
  return ret;
}

bool eventPut(event_msg_t *p_msg)
{
  bool ret;

  if (xQueueSend(event_queue, p_msg, 10) == pdTRUE) 
  {
    ret = true;
  }
  else
  {
    ret = false;
  }

  return ret;
}

void eventThread(void const *arg)
{
  bool init_ret = true;

  logPrintf("[%s] Thread Started : EVENT\n", init_ret ? "OK":"NG" );

  
  while(1)
  {
    event_msg_t msg;

    if (xQueueReceive(event_queue, &msg, portMAX_DELAY) == pdTRUE)
    {
      #if 0
      // 메세지를 받았을때 로그를 출력한다.
      //
      logPrintf("Msg Received\n");
      logPrintf("    ID : %d\n", msg.id);
      logPrintf("    DATA : %d\n", msg.data);
      #endif

      #if 1
      for (int i=0; i<event_func_cnt; i++)
      {
        if (event_func[i] != NULL)
        {
          if (event_id_tbl[i] == EVENT_ID_ALL)
          {
            event_func[i](&msg);
          }
          else
          {
            for (int id_idx=0; id_idx<EVENT_ID_MAX; id_idx++)
            {
              if (event_id_tbl[i] == msg.id)              
              {
                event_func[i](&msg);
                break;
              }
            }
          }
        }
      }
      #endif
    }    
  }
}