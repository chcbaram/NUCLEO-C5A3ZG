#include "ap.h"




void apInit(void)
{
  // 각 모듈의 init() 실행 (모듈은 여기서 threadCreate 로 스레드를 등록한다)
  //
  moduleInit();
}

void apMain(void)
{
  // 등록된 스레드를 모두 생성한다
  //
  threadBegin();

  logBoot(false);

  while(1)
  {
    ledToggle(_DEF_LED1);
    delay(500);
  }
}
