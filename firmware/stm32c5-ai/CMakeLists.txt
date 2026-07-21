cmake_minimum_required(VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools")
include(arm-none-eabi-gcc)


# 프로젝트 이름은 최종 출력 파일 이름으로 사용된다.
#
set(PRJ_NAME stm32c5-fw)


project(${PRJ_NAME}
  LANGUAGES ASM C CXX
)


set(EXECUTABLE ${PRJ_NAME}.elf)


# USB 스택 선택: 0 = TinyUSB, 1 = ST USB Device Library
#   cmake -DHW_USB_STACK=1 .. 로 ST 스택 빌드 전환 가능
#
if(NOT DEFINED HW_USB_STACK)
  set(HW_USB_STACK 0)
endif()

if(HW_USB_STACK EQUAL 1)
  # ST USB Device Library (신형 HAL PCD) - usbd_conf.c 등은 usb/stm32/ 에 위치
  file(GLOB_RECURSE USB_SRC_FILES CONFIGURE_DEPENDS
    src/lib/ST/STM32_USB_Device_Library/Core/Src/*.c
    src/hw/driver/usb/stm32/*.c
  )
  set(USB_INC_DIRS
    src/hw/driver/usb
    src/hw/driver/usb/stm32
    src/hw/driver/usb/stm32/usb_cdc
    src/lib/ST/STM32_USB_Device_Library/Core/Inc
  )
else()
  # TinyUSB (device, CDC, STM32 fsdev DCD) - tusb_config.h 는 usb/tinyusb/ 에 위치
  set(USB_SRC_FILES
    src/lib/tinyusb/src/tusb.c
    src/lib/tinyusb/src/common/tusb_fifo.c
    src/lib/tinyusb/src/device/usbd.c
    src/lib/tinyusb/src/class/cdc/cdc_device.c
    src/lib/tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
    src/lib/tinyusb/src/portable/st/stm32_fsdev/fsdev_common.c
  )
  set(USB_INC_DIRS
    src/hw/driver/usb
    src/hw/driver/usb/tinyusb
    src/lib/tinyusb/src
  )
endif()



# 지정한 폴더에 있는 파일만 포함한다.
#
file(GLOB SRC_FILES CONFIGURE_DEPENDS
  src/*.cpp
  src/*.c
  src/ap/*.cpp
  src/ap/*.c
  src/bsp/*.c
  src/bsp/rtos/*.c
  src/bsp/device/*.c

  # FreeRTOS (CMSIS-RTOS v2, Cortex-M33 non-secure, heap_4) - 비재귀 glob
  src/lib/FreeRTOS/Source/*.c
  src/lib/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/*.c
  src/lib/FreeRTOS/Source/CMSIS_RTOS_V2/*.c
  src/lib/FreeRTOS/Source/portable/MemMang/heap_4.c

  # lwIP (STM32 hal2, IPv4 only) - 비재귀 glob
  src/lib/lwip/src/core/*.c
  src/lib/lwip/src/core/ipv4/*.c
  src/lib/lwip/src/api/*.c
  src/lib/lwip/src/netif/ethernet.c
  src/lib/lwip/src/apps/sntp/*.c
  src/lib/lwip/src/apps/lwiperf/*.c
  src/lib/lwip/ports/freertos/arch/*.c
  src/lib/lwip/interfaces/ethernet/*.c
  src/lib/lwip/port/*.c
)

# 하위폴더에 있는 파일까지 포함한다.
#
file(GLOB_RECURSE SRC_FILES_RECURSE CONFIGURE_DEPENDS
  src/ap/*.cpp
  src/ap/*.c
  src/bsp/*.c
  src/common/*.c
  src/hw/*.c
  # src/lib/ST/STM32H5xx_HAL_Driver/Src/*.c
  )


file(GLOB_RECURSE SRC_FILES_SPECIAL CONFIGURE_DEPENDS
  src/lib/ST/stm32c5xx_drivers/hal/*.c
  src/lib/ST/stm32c5xx_drivers/os_port/no_os/*.c
  )


# 특정 폴더를 빌드에서 제외한다.
#
set(EXCLUDE_PATHS
  # ST USB Device Library 백엔드 소스는 재귀 glob 에서 빼고, HW_USB_STACK==1 일 때만
  # 위 USB_SRC_FILES 로 명시 추가한다(TinyUSB 빌드에서 컴파일되지 않도록).
  src/hw/driver/usb/stm32
)

foreach(TMP_PATH ${SRC_FILES_RECURSE}) 
  foreach(EXCLUDE_PATH ${EXCLUDE_PATHS}) 
    string(FIND ${TMP_PATH} ${EXCLUDE_PATH} RESULT) 
    if(NOT ${RESULT} EQUAL -1) 
      list(REMOVE_ITEM SRC_FILES_RECURSE ${TMP_PATH}) 
    endif() 
  endforeach(EXCLUDE_PATH) 
endforeach(TMP_PATH)


add_executable(${EXECUTABLE}
  ${SRC_FILES}
  ${SRC_FILES_RECURSE}
  ${SRC_FILES_SPECIAL}
  ${USB_SRC_FILES}
)



target_include_directories(${EXECUTABLE} PRIVATE 
  src  
  src/ap
  src/ap/modules
  src/ap/modules/common
  src/bsp
  src/bsp/device
  src/bsp/rtos  
  src/common 
  src/common/core
  src/common/hw/include 
  src/hw
  src/hw/driver
  src/lib

  # USB (스택별 소스/헤더 경로: 위 HW_USB_STACK 분기에서 설정)
  ${USB_INC_DIRS}

  src/lib/ST/CMSIS/Include
  src/lib/ST/CMSIS/Device/ST/STM32C5xx/Include
  src/lib/ST/stm32c5xx_drivers/ll
  src/lib/ST/stm32c5xx_drivers/hal
  src/lib/ST/stm32c5xx_drivers/os_port/no_os

  # FreeRTOS
  src/lib/FreeRTOS/Source/include
  src/lib/FreeRTOS/Source/CMSIS_RTOS_V2
  src/lib/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure

  # lwIP
  src/lib/lwip/src/include
  src/lib/lwip/ports/common
  src/lib/lwip/ports/freertos
  src/lib/lwip/port
  src/lib/lwip/interfaces/ethernet
)

target_compile_definitions(${EXECUTABLE} PRIVATE
  -DSTM32C5A3xx
  -DUSE_HAL_DRIVER
  -DUSE_EXTERNAL_ENV
  -DUSE_FULL_ASSERT
  -DHW_USB_STACK=${HW_USB_STACK}
  )

target_compile_options(${EXECUTABLE} PRIVATE
  -mcpu=cortex-m33
  -mthumb
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard

  -fdata-sections
  -ffunction-sections
  
  -fstack-usage 
  -Wmissing-field-initializers
  -Wuninitialized 
  # -Wenum-conversion

  -Wall
  -g3
  -O0
  )

set_source_files_properties(
    ${SRC_FILES_SPECIAL}
    PROPERTIES
        COMPILE_OPTIONS "-O0"
)

target_link_options(${EXECUTABLE} PRIVATE
  -T../src/bsp/ldscript/STM32C5A3xx_FLASH.ld

  -mcpu=cortex-m33
  -mthumb
  -mfpu=fpv5-sp-d16
  -mfloat-abi=hard
  -specs=nano.specs

  -lc
  -lm
  # -lstdc++
  # -lsupc++
  -fno-rtti
  
  -Wl,-Map=${PRJ_NAME}.map,--cref
  -Wl,--gc-sections
  -Xlinker -print-memory-usage -Xlinker
  )

set (CMAKE_C_FLAGS "-Wenum-conversion")
set (CMAKE_CXX_FLAGS "-fno-rtti")

message(${CMAKE_CXX_COMPILER_VERSION})
if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11)
  string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--no-warn-rwx-segments")
endif()

add_custom_command(TARGET ${EXECUTABLE} 
  POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} ARGS -O binary ${EXECUTABLE} ${PROJECT_NAME}.bin
  COMMENT "Invoking: Make Bin -> ${KEYBOARD_PATH}"
  )  
  

