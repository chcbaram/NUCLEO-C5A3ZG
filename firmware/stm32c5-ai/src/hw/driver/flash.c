#include "flash.h"


#ifdef _USE_HW_FLASH
#include "cli.h"
#include "stm32c5xx_ll_icache.h"


// STM32C5A3 내장 플래시
//   - 전체 1MB, 2 뱅크(뱅크당 512KB)가 0x08000000 부터 연속 배치
//   - 페이지(=소거 단위) 8KB, 프로그램 단위는 쿼드워드(16byte)
//   - FLASH_SIZE 는 런타임에 FLASHSIZE_BASE 를 읽어 결정되므로
//     아래 매크로들도 상수가 아닌 런타임 식이다.
//
#define FLASH_ADDR_BASE           FLASH_BASE
#define FLASH_ADDR_END            (FLASH_BASE + FLASH_SIZE)
#define FLASH_BANK_ADDR(bank)     (FLASH_BASE + ((bank) * FLASH_BANK_SIZE))
#define FLASH_MAX_BANK            FLASH_BANK_NB
#define FLASH_MAX_PAGE            (FLASH_SIZE / FLASH_PAGE_SIZE)
#define FLASH_PAGE_ADDR(page)     (FLASH_BASE + ((page) * FLASH_PAGE_SIZE))
#define FLASH_WRITE_SIZE          16
#define FLASH_TIMEOUT_MS          1000


#if CLI_USE(HW_FLASH)
static void cliFlash(cli_args_t *args);
#endif
static bool flashInPage(uint32_t page_num, uint32_t addr, uint32_t length);
static bool flashIsUserAddr(uint32_t addr, uint32_t length);
static void flashInvalidateCache(void);

static bool is_init = false;
static hal_flash_handle_t hflash;




bool flashInit(void)
{
  bool ret = true;


  if (HAL_FLASH_Init(&hflash, HAL_FLASH) != HAL_OK)
  {
    ret = false;
  }

  // 프로그램 단위는 쿼드워드(16byte) 고정.
  //   HAL 은 프로그램 시작 주소가 항상 16byte 정렬일 것을 요구하므로
  //   더 작은 단위 모드를 써도 이득이 없다.
  //
  if (ret == true)
  {
    if (HAL_FLASH_SetProgrammingMode(&hflash, HAL_FLASH_PROGRAM_QUADWORD) != HAL_OK)
    {
      ret = false;
    }
  }

  is_init = ret;
  logPrintf("[%s] flashInit()\n", ret ? "OK":"E_");

#if CLI_USE(HW_FLASH)
  cliAdd("flash", cliFlash);
#endif
  return ret;
}

bool flashIsUserAddr(uint32_t addr, uint32_t length)
{
  if (length == 0)
    return false;

  if (addr < FLASH_ADDR_BASE || addr >= FLASH_ADDR_END)
    return false;

  if ((addr + length) > FLASH_ADDR_END)
    return false;

  return true;
}

bool flashInPage(uint32_t page_num, uint32_t addr, uint32_t length)
{
  bool ret = false;

  uint32_t page_start;
  uint32_t page_end;
  uint32_t flash_start;
  uint32_t flash_end;


  page_start  = FLASH_PAGE_ADDR(page_num);
  page_end    = page_start + FLASH_PAGE_SIZE - 1;
  flash_start = addr;
  flash_end   = addr + length - 1;


  if (page_start >= flash_start && page_start <= flash_end)
  {
    ret = true;
  }

  if (page_end >= flash_start && page_end <= flash_end)
  {
    ret = true;
  }

  if (flash_start >= page_start && flash_start <= page_end)
  {
    ret = true;
  }

  if (flash_end >= page_start && flash_end <= page_end)
  {
    ret = true;
  }

  return ret;
}

// 플래시 내용이 바뀌면 ICACHE 에 남아있는 이전 데이터가 그대로 읽힐 수 있다.
// (ICACHE 는 코드 영역의 명령/리터럴 접근을 모두 캐싱한다)
//
void flashInvalidateCache(void)
{
  if (LL_ICACHE_IsEnabled(ICACHE) == 0U)
    return;

  LL_ICACHE_Invalidate(ICACHE);
  while (LL_ICACHE_IsActiveFlag_BUSY(ICACHE) != 0U)
  {
  }
  LL_ICACHE_ClearFlag_BSYEND(ICACHE);
}

bool flashErase(uint32_t addr, uint32_t length)
{
  bool ret = false;

  if (is_init != true)
    return false;

  if (flashIsUserAddr(addr, length) != true)
    return false;

  if (HAL_FLASH_ITF_Unlock(HAL_FLASH) != HAL_OK)
    return false;

  // 뱅크는 주소 공간에서 연속이므로 페이지 인덱스만으로 순회한다.
  // 다만 HAL_FLASH_EraseByAddr() 은 뱅크를 걸치는 요청을 허용하지 않아
  // 한번에 한 페이지씩 소거한다.
  //
  for (uint32_t i = 0; i < FLASH_MAX_PAGE; i++)
  {
    if (flashInPage(i, addr, length) != true)
      continue;

    if (HAL_FLASH_EraseByAddr(&hflash, FLASH_PAGE_ADDR(i), FLASH_PAGE_SIZE, FLASH_TIMEOUT_MS) != HAL_OK)
    {
      ret = false;
      break;
    }
    ret = true;
  }

  HAL_FLASH_ITF_Lock(HAL_FLASH);

  flashInvalidateCache();

  return ret;
}

bool flashWrite(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  bool     ret = true;
  uint32_t index;
  uint32_t write_length;
  uint32_t write_addr;
  uint32_t buf_[FLASH_WRITE_SIZE/4];
  uint8_t *buf = (uint8_t *)buf_;
  uint32_t offset;


  if (is_init != true)
    return false;

  if (flashIsUserAddr(addr, length) != true)
    return false;

  if (HAL_FLASH_ITF_Unlock(HAL_FLASH) != HAL_OK)
    return false;

  index  = 0;
  offset = addr % FLASH_WRITE_SIZE;

  // 시작 주소가 쿼드워드 경계가 아니면 앞쪽 조각을 먼저 처리한다.
  //
  if (offset != 0)
  {
    write_addr   = addr - offset;
    write_length = constrain(FLASH_WRITE_SIZE - offset, 0, length);

    memcpy(&buf[0], (void *)write_addr, FLASH_WRITE_SIZE);
    memcpy(&buf[offset], &p_data[0], write_length);

    if (HAL_FLASH_ProgramByAddr(&hflash, write_addr, buf_, FLASH_WRITE_SIZE, FLASH_TIMEOUT_MS) != HAL_OK)
    {
      ret = false;
    }

    index += write_length;
  }

  while (ret == true && index < length)
  {
    write_addr   = addr + index;
    write_length = constrain(length - index, 0, FLASH_WRITE_SIZE);

    if (write_length == FLASH_WRITE_SIZE)
    {
      memcpy(&buf[0], &p_data[index], write_length);
    }
    else
    {
      // 마지막 조각은 쿼드워드에 못 미치므로 남는 부분은 기존 값을 유지한다.
      //
      memcpy(&buf[0], (void *)write_addr, FLASH_WRITE_SIZE);
      memcpy(&buf[0], &p_data[index], write_length);
    }

    if (HAL_FLASH_ProgramByAddr(&hflash, write_addr, buf_, FLASH_WRITE_SIZE, FLASH_TIMEOUT_MS) != HAL_OK)
    {
      ret = false;
      break;
    }

    index += write_length;
  }

  HAL_FLASH_ITF_Lock(HAL_FLASH);

  flashInvalidateCache();

  return ret;
}

bool flashRead(uint32_t addr, uint8_t *p_data, uint32_t length)
{
  bool ret = true;
  uint8_t *p_byte = (uint8_t *)addr;


  for (uint32_t i = 0; i < length; i++)
  {
    p_data[i] = p_byte[i];
  }

  return ret;
}




#if CLI_USE(HW_FLASH)
void cliFlash(cli_args_t *args)
{
  bool ret = false;
  uint32_t i;
  uint32_t addr;
  uint32_t length;
  uint32_t pre_time;
  bool flash_ret;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("flash size    : %d KB\n", (int)(FLASH_SIZE/1024));
    cliPrintf("flash bank    : %d\n", (int)FLASH_MAX_BANK);
    cliPrintf("flash page    : %d KB, %d ea\n", (int)(FLASH_PAGE_SIZE/1024), (int)FLASH_MAX_PAGE);
    cliPrintf("flash write   : %d bytes\n", FLASH_WRITE_SIZE);
    for (i = 0; i < FLASH_MAX_BANK; i++)
    {
      cliPrintf("flash addr b%d : 0x%X\n", (int)i + 1, (int)FLASH_BANK_ADDR(i));
    }
    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "read"))
  {
    uint8_t data;

    addr   = (uint32_t)args->getData(1);
    length = (uint32_t)args->getData(2);

    for (i=0; i<length; i++)
    {
      flash_ret = flashRead(addr+i, &data, 1);

      if (flash_ret == true)
      {
        cliPrintf( "addr : 0x%X\t 0x%02X\n", addr+i, data);
      }
      else
      {
        cliPrintf( "addr : 0x%X\t Fail\n", addr+i);
      }
    }

    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "erase"))
  {
    addr   = (uint32_t)args->getData(1);
    length = (uint32_t)args->getData(2);

    pre_time = millis();
    flash_ret = flashErase(addr, length);

    cliPrintf( "addr : 0x%X\t len : %d %d ms\n", addr, length, (millis()-pre_time));
    if (flash_ret)
    {
      cliPrintf("OK\n");
    }
    else
    {
      cliPrintf("FAIL\n");
    }

    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "write"))
  {
    uint32_t data;

    addr = (uint32_t)args->getData(1);
    data = (uint32_t)args->getData(2);

    pre_time = millis();
    flash_ret = flashWrite(addr, (uint8_t *)&data, 4);

    cliPrintf( "addr : 0x%X\t 0x%X %dms\n", addr, data, millis()-pre_time);
    if (flash_ret)
    {
      cliPrintf("OK\n");
    }
    else
    {
      cliPrintf("FAIL\n");
    }

    ret = true;
  }

  if(args->argc == 3 && args->isStr(0, "check"))
  {
    uint32_t data = 0;
    uint32_t block = 4;


    addr    = (uint32_t)args->getData(1);
    length  = (uint32_t)args->getData(2);
    length -= (length % block);

    do
    {
      cliPrintf("flashErase()..");
      if (flashErase(addr, length) == false)
      {
        cliPrintf("Fail\n");
        break;
      }
      cliPrintf("OK\n");

      cliPrintf("flashWrite()..");
      for (i=0; i<length; i+=block)
      {
        data = i;
        if (flashWrite(addr + i, (uint8_t *)&data, block) == false)
        {
          cliPrintf("Fail %d\n", i);
          break;
        }
      }
      cliPrintf("OK\n");

      cliPrintf("flashRead() ..");
      for (i=0; i<length; i+=block)
      {
        data = 0;
        if (flashRead(addr + i, (uint8_t *)&data, block) == false)
        {
          cliPrintf("Fail %d\n", i);
          break;
        }
        if (data != i)
        {
          cliPrintf("Check Fail %d\n", i);
          break;
        }
      }
      cliPrintf("OK\n");


      cliPrintf("flashErase()..");
      if (flashErase(addr, length) == false)
      {
        cliPrintf("Fail\n");
        break;
      }
      cliPrintf("OK\n");
    } while (0);

    ret = true;
  }


  if (ret == false)
  {
    cliPrintf( "flash info\n");
    cliPrintf( "flash read  [addr] [length]\n");
    cliPrintf( "flash erase [addr] [length]\n");
    cliPrintf( "flash write [addr] [data]\n");
    cliPrintf( "flash check [addr] [length]\n");
  }
}
#endif

#endif
