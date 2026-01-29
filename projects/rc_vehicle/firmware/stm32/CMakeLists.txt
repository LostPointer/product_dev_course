cmake_minimum_required(VERSION 3.13)
project(rc_vehicle_stm32 C CXX ASM)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# MCU по умолчанию; можно переопределить: cmake -DMCU=STM32F411CE ..
if(NOT DEFINED MCU)
  set(MCU STM32F103C8)
endif()

# Путь к libopencm3 (обязательно)
if(NOT DEFINED LIBOPENCM3_PATH)
  if(DEFINED ENV{LIBOPENCM3_PATH})
    set(LIBOPENCM3_PATH $ENV{LIBOPENCM3_PATH})
  else()
    message(FATAL_ERROR "LIBOPENCM3_PATH не задан. Укажите: -DLIBOPENCM3_PATH=/path/to/libopencm3 или export LIBOPENCM3_PATH=...")
  endif()
endif()

# Подключаем конфиг платы для выбранного MCU
set(BOARD_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/boards/${MCU}.cmake")
if(NOT EXISTS "${BOARD_CMAKE}")
  message(FATAL_ERROR "Неизвестный MCU: ${MCU}. Доступные: STM32F103C8, STM32F401CE, STM32F411CE, STM32G431CB")
endif()
include(${BOARD_CMAKE})

# Компилятор
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# Флаги C/C++
set(CMAKE_C_FLAGS "${MCU_CFLAGS} -fdata-sections -ffunction-sections -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${MCU_CFLAGS} -fdata-sections -ffunction-sections -Wall -Wextra -std=c++23")
set(LD_SCRIPT_PATH ${LIBOPENCM3_PATH}/lib/stm32/${LD_SCRIPT_DIR}/${LD_SCRIPT})
set(CMAKE_EXE_LINKER_FLAGS "${MCU_CFLAGS} -Wl,--gc-sections -T${LD_SCRIPT_PATH} -nostartfiles -Wl,--no-warn-flags")

# Include libopencm3
set(OPENCM3_INC ${LIBOPENCM3_PATH}/include)
set(OPENCM3_LIB ${LIBOPENCM3_PATH}/lib/libopencm3_${OPENCM3_TARGET}.a)

if(NOT EXISTS "${OPENCM3_LIB}")
  message(FATAL_ERROR "Библиотека не найдена: ${OPENCM3_LIB}. Соберите libopencm3: cd ${LIBOPENCM3_PATH} && make TARGETS=stm32/${OPENCM3_TARGET}")
endif()

set(RC_VEHICLE_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)

add_executable(rc_vehicle_stm32.elf
  main/main.cpp
  main/platform.cpp
  main/pwm_control.cpp
  main/rc_input.cpp
  main/imu.cpp
  main/failsafe.cpp
  main/uart_bridge.cpp
  ${RC_VEHICLE_COMMON_DIR}/protocol.cpp
  ${RC_VEHICLE_COMMON_DIR}/uart_bridge_base.cpp
)

target_include_directories(rc_vehicle_stm32.elf PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/main
  ${RC_VEHICLE_COMMON_DIR}
  ${OPENCM3_INC}
)

target_compile_definitions(rc_vehicle_stm32.elf PRIVATE
  ${MCU_DEFINE}
  MCU_${MCU}
)

target_link_libraries(rc_vehicle_stm32.elf ${OPENCM3_LIB})

# Генерация .bin и .hex для прошивки
add_custom_command(TARGET rc_vehicle_stm32.elf POST_BUILD
  COMMAND arm-none-eabi-objcopy -O binary rc_vehicle_stm32.elf rc_vehicle_stm32.bin
  COMMAND arm-none-eabi-objcopy -O ihex rc_vehicle_stm32.elf rc_vehicle_stm32.hex
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

message(STATUS "MCU: ${MCU} (${MCU_DEFINE}), libopencm3: ${LIBOPENCM3_PATH}")
