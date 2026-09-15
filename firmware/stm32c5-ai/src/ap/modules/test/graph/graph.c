#include "module.h"

#include <math.h>


static bool graphInit(void);
static void cliGraph(cli_args_t *args);

MODULE_DEF(graph)
{
  .name     = "graph",
  .priority = MODULE_PRI_LOWEST,
  .init     = graphInit,
};

// baram-term 그래프 패널 확인용. Teleplot 형식(>이름:값)으로 한 줄에 하나씩 보낸다.
// nano.specs 는 %f 를 지원하지 않으므로 값은 정수로 찍는다.
//
#define GRAPH_PERIOD_DEF_MS   20
#define GRAPH_PERIOD_MAX_MS   1000

#define GRAPH_PI              3.14159265f




bool graphInit(void)
{
  cliAdd("graph", cliGraph);
  return true;
}

static uint32_t graphRand(uint32_t *p_state)
{
  uint32_t x = *p_state;

  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *p_state = x;

  return x;
}

void cliGraph(cli_args_t *args)
{
  bool ret = false;


  if (args->argc >= 1 && args->isStr(0, "run"))
  {
    int32_t  period_ms = GRAPH_PERIOD_DEF_MS;
    uint32_t seed;
    uint32_t pre_time;

    if (args->argc == 2)
    {
      period_ms = constrain(args->getData(1), 1, GRAPH_PERIOD_MAX_MS);
    }

    seed     = micros() | 1;
    pre_time = millis();

    while(cliKeepLoop())
    {
      float t = (float)(millis() - pre_time) / 1000.0f;

      int32_t sin1 = (int32_t)(1000.0f * sinf(2.0f * GRAPH_PI * 1.0f * t));
      int32_t sin2 = (int32_t)( 700.0f * sinf(2.0f * GRAPH_PI * 0.5f * t + 2.0f * GRAPH_PI / 3.0f));
      int32_t sin3 = (int32_t)( 400.0f * sinf(2.0f * GRAPH_PI * 2.0f * t + 4.0f * GRAPH_PI / 3.0f));
      int32_t rnd  = (int32_t)(graphRand(&seed) % 2001) - 1000;

      cliPrintf(">sin1:%d\n>sin2:%d\n>sin3:%d\n>rand:%d\n", sin1, sin2, sin3, rnd);
      delay(period_ms);
    }
    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("graph run [period_ms 1~%d, def %d]\n", GRAPH_PERIOD_MAX_MS, GRAPH_PERIOD_DEF_MS);
  }
}
