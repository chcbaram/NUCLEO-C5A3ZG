#include "rtc.h"


#ifdef _USE_HW_RTC
#include "cli.h"
#include "stm32c5xx_ll_tamp.h"
#include <time.h>


// RTC 도메인은 리셋(POR 제외)에도 유지되므로, 최초 1회만 캘린더를 초기화한다.
// 백업 레지스터 0번에 매직값을 기록해두고 다음 부팅 때 초기화 여부를 판단한다.
//
#define RTC_BKP_MAGIC_INDEX       0
#define RTC_BKP_MAGIC_VALUE       0xA5A50001


#if CLI_USE(HW_RTC)
static void cliRtc(cli_args_t *args);
#endif

static bool rtcClockInit(void);
static bool rtcCalendarInit(void);
static bool rtcWriteDateTime(const hal_rtc_date_t *p_date, const hal_rtc_time_t *p_time);
static uint8_t rtcCalcWeekday(const rtc_date_t *rtc_date);

static bool is_init = false;




bool rtcInit(void)
{
  bool ret = true;
  uint32_t magic = 0;


  ret = rtcClockInit();

  if (ret == true)
  {
    // 최초 부팅(또는 배터리 방전 후)에만 캘린더를 초기화한다.
    //
    rtcGetReg(RTC_BKP_MAGIC_INDEX, &magic);
    if (magic != RTC_BKP_MAGIC_VALUE)
    {
      rtc_time_t rtc_time = {0, };
      rtc_date_t rtc_date = {0, };

      ret &= rtcCalendarInit();

      // 기본 시각 : 2026-01-01 00:00:00
      //
      rtc_date.year  = 26;
      rtc_date.month = 1;
      rtc_date.day   = 1;
      rtcSetDate(&rtc_date);

      rtc_time.hours   = 0;
      rtc_time.minutes = 0;
      rtc_time.seconds = 0;
      rtcSetTime(&rtc_time);

      rtcSetReg(RTC_BKP_MAGIC_INDEX, RTC_BKP_MAGIC_VALUE);
    }
  }

  is_init = ret;
  logPrintf("[%s] rtcInit()\n", ret ? "OK":"E_");

#if CLI_USE(HW_RTC)
  cliAdd("rtc", cliRtc);
#endif
  return ret;
}

bool rtcGetInfo(rtc_info_t *rtc_info)
{
  if (rtcGetTime(&rtc_info->time) != true)
    return false;

  if (rtcGetDate(&rtc_info->date) != true)
    return false;

  return true;
}

bool rtcGetTime(rtc_time_t *rtc_time)
{
  hal_rtc_time_t time = {0, };

  if (HAL_RTC_CALENDAR_GetTime(&time) != HAL_OK)
    return false;

  rtc_time->hours   = time.hour;
  rtc_time->minutes = time.min;
  rtc_time->seconds = time.sec;

  return true;
}

bool rtcGetDate(rtc_date_t *rtc_date)
{
  hal_rtc_date_t date = {0, };

  if (HAL_RTC_CALENDAR_GetDate(&date) != HAL_OK)
    return false;

  rtc_date->year  = date.year;
  rtc_date->month = (uint8_t)date.mon;
  rtc_date->day   = date.mday;
  rtc_date->week  = (uint8_t)date.wday;

  return true;
}

bool rtcSetTime(rtc_time_t *rtc_time)
{
  hal_rtc_time_t time = {0, };

  time.am_pm = HAL_RTC_TIME_FORMAT_AM_24H;
  time.hour  = rtc_time->hours;
  time.min   = rtc_time->minutes;
  time.sec   = rtc_time->seconds;

  return rtcWriteDateTime(NULL, &time);
}

bool rtcSetDate(rtc_date_t *rtc_date)
{
  hal_rtc_date_t date = {0, };

  date.year = rtc_date->year;
  date.mon  = (hal_rtc_month_t)rtc_date->month;
  date.mday = rtc_date->day;
  date.wday = (hal_rtc_weekday_t)rtcCalcWeekday(rtc_date);

  return rtcWriteDateTime(&date, NULL);
}

bool rtcSetReg(uint32_t index, uint32_t data)
{
  if (index >= LL_TAMP_BKP_NUMBER)
    return false;

  LL_TAMP_BKP_SetRegister(index, data);
  return true;
}

bool rtcGetReg(uint32_t index, uint32_t *p_data)
{
  if (index >= LL_TAMP_BKP_NUMBER)
    return false;

  *p_data = LL_TAMP_BKP_GetRegister(index);
  return true;
}

bool rtcClockInit(void)
{
  // 백업(RTC) 도메인 쓰기 보호 해제 : LSE, RTC 레지스터에 접근하기 위해 필요
  //
  HAL_PWR_DisableRTCDomainWriteProtection();

  // 외부 32.768kHz 크리스털(LSE) 구동
  //
  if (HAL_RCC_LSE_IsReady() != HAL_RCC_OSC_READY)
  {
    if (HAL_RCC_LSE_Enable(HAL_RCC_LSE_ON, HAL_RCC_LSE_DRIVE_MEDIUMHIGH) != HAL_OK)
      return false;
  }

  // RTC 커널 클럭 소스를 LSE 로 선택
  // (최초 설정 시에만 백업 도메인이 리셋되며, 이후 재부팅에서는 그대로 유지된다)
  //
  if (HAL_RCC_RTC_SetKernelClkSource(HAL_RCC_RTC_CLK_SRC_LSE) != HAL_OK)
    return false;

  HAL_RCC_RTC_EnableKernelClock();
  HAL_RCC_RTCAPB_EnableClock();

  return true;
}

bool rtcCalendarInit(void)
{
  hal_rtc_config_t          config = {0, };
  hal_rtc_calendar_config_t cal_config = {0, };


  // LSE(32768Hz) / (127+1) / (255+1) = 1Hz
  //
  config.mode          = HAL_RTC_MODE_BCD;
  config.asynch_prediv = 127;
  config.synch_prediv  = 255;

  cal_config.hour_format            = HAL_RTC_CALENDAR_HOUR_FORMAT_24;
  cal_config.bypass_shadow_register = HAL_RTC_CALENDAR_SHADOW_REG_BYPASS;

  HAL_RTC_DisableWriteProtection();
  if (HAL_RTC_EnterInitMode() != HAL_OK)
  {
    HAL_RTC_EnableWriteProtection();
    return false;
  }

  HAL_RTC_SetConfig(&config);
  HAL_RTC_CALENDAR_SetConfig(&cal_config);

  HAL_RTC_ExitInitMode();
  HAL_RTC_EnableWriteProtection();

  return true;
}

bool rtcWriteDateTime(const hal_rtc_date_t *p_date, const hal_rtc_time_t *p_time)
{
  // 캘린더(TR/DR) 레지스터 쓰기는 쓰기 보호 해제 + 초기화 모드에서만 가능하다.
  //
  HAL_RTC_DisableWriteProtection();
  if (HAL_RTC_EnterInitMode() != HAL_OK)
  {
    HAL_RTC_EnableWriteProtection();
    return false;
  }

  if (p_time != NULL)
    HAL_RTC_CALENDAR_SetTime(p_time);

  if (p_date != NULL)
    HAL_RTC_CALENDAR_SetDate(p_date);

  HAL_RTC_ExitInitMode();
  HAL_RTC_EnableWriteProtection();

  return true;
}

uint8_t rtcCalcWeekday(const rtc_date_t *rtc_date)
{
  struct tm timeinfo;

  memset(&timeinfo, 0, sizeof(timeinfo));
  timeinfo.tm_year = (2000 + rtc_date->year) - 1900;
  timeinfo.tm_mon  = rtc_date->month - 1;
  timeinfo.tm_mday = rtc_date->day;

  mktime(&timeinfo);

  // tm_wday : 0(일)~6(토)  ->  HAL weekday : 1(월)~7(일)
  //
  return (timeinfo.tm_wday == 0) ? HAL_RTC_WEEKDAY_SUNDAY : (uint8_t)timeinfo.tm_wday;
}

#if CLI_USE(HW_RTC)
void cliRtc(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("is_init : %d\n", is_init);
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "get") && args->isStr(1, "info"))
  {
    rtc_info_t rtc_info;

    while(cliKeepLoop())
    {
      rtcGetInfo(&rtc_info);

      cliPrintf("Y:%02d M:%02d D:%02d W:%d, H:%02d M:%02d S:%02d\n",
                rtc_info.date.year,
                rtc_info.date.month,
                rtc_info.date.day,
                rtc_info.date.week,
                rtc_info.time.hours,
                rtc_info.time.minutes,
                rtc_info.time.seconds);
      delay(1000);
    }
    ret = true;
  }

  if (args->argc == 5 && args->isStr(0, "set") && args->isStr(1, "time"))
  {
    rtc_time_t rtc_time;

    rtc_time.hours   = args->getData(2);
    rtc_time.minutes = args->getData(3);
    rtc_time.seconds = args->getData(4);

    rtcSetTime(&rtc_time);
    cliPrintf("H:%02d M:%02d S:%02d\n",
              rtc_time.hours,
              rtc_time.minutes,
              rtc_time.seconds);
    ret = true;
  }

  if (args->argc == 5 && args->isStr(0, "set") && args->isStr(1, "date"))
  {
    rtc_date_t rtc_date;

    rtc_date.year  = args->getData(2);
    rtc_date.month = args->getData(3);
    rtc_date.day   = args->getData(4);

    rtcSetDate(&rtc_date);
    cliPrintf("Y:%02d M:%02d D:%02d\n",
              rtc_date.year,
              rtc_date.month,
              rtc_date.day);
    ret = true;
  }


  if (ret == false)
  {
    cliPrintf("rtc info\n");
    cliPrintf("rtc get info\n");
    cliPrintf("rtc set time [h] [m] [s]\n");
    cliPrintf("rtc set date [y] [m] [d]\n");
  }
}
#endif

#endif
