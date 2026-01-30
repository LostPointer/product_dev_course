# Список LL-драйверов по семейству (только нужные для UART, PWM, SPI, GPIO, RCC)
if(MCU_DEFINE STREQUAL "STM32F1")
  set(CUBE_LL_SOURCES
    stm32f1xx_ll_utils.c
    stm32f1xx_ll_rcc.c
    stm32f1xx_ll_gpio.c
    stm32f1xx_ll_usart.c
    stm32f1xx_ll_tim.c
    stm32f1xx_ll_spi.c
  )
elseif(MCU_DEFINE STREQUAL "STM32F4")
  set(CUBE_LL_SOURCES
    stm32f4xx_ll_utils.c
    stm32f4xx_ll_rcc.c
    stm32f4xx_ll_gpio.c
    stm32f4xx_ll_usart.c
    stm32f4xx_ll_tim.c
    stm32f4xx_ll_spi.c
  )
elseif(MCU_DEFINE STREQUAL "STM32G4")
  set(CUBE_LL_SOURCES
    stm32g4xx_ll_utils.c
    stm32g4xx_ll_rcc.c
    stm32g4xx_ll_gpio.c
    stm32g4xx_ll_usart.c
    stm32g4xx_ll_tim.c
    stm32g4xx_ll_spi.c
  )
else()
  message(FATAL_ERROR "Неизвестное семейство MCU_DEFINE: ${MCU_DEFINE}")
endif()
