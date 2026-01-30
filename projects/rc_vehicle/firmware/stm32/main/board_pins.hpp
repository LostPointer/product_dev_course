#pragma once

// Пины и периферия по семейству MCU (STM32Cube LL).
// Используются в uart_bridge, pwm_control, rc_input, imu (SPI).
// Порты: GPIOA, GPIOB (STM32Cube). Пины: номер 0..15 или маска LL_GPIO_PIN_x.

#if !defined(STM32F1) && !defined(STM32F4) && !defined(STM32G4)
#define STM32F1 1  // Fallback для IDE
#endif

#if defined(STM32F1)
// Blue Pill STM32F103C8: USART2 (PA2/PA3), TIM2 CH1/CH2 (PA0/PA1), RC-in PA4/PA5, IMU SPI2 PB12/PB13/PB14/PB15
#define UART_USART       USART2
#define UART_GPIO_PORT   GPIOA
#define UART_TX_PIN      2
#define UART_RX_PIN      3
#define PWM_TIM          TIM2
#define PWM_THROTTLE_PORT GPIOA
#define PWM_THROTTLE_PIN  0
#define PWM_STEERING_PORT GPIOA
#define PWM_STEERING_PIN  1
#define RC_IN_THROTTLE_PORT  GPIOA
#define RC_IN_THROTTLE_PIN   4
#define RC_IN_STEERING_PORT  GPIOA
#define RC_IN_STEERING_PIN   5
// IMU MPU-6050 по SPI2 (PB13 SCK, PB14 MISO, PB15 MOSI, PB12 NCS)
#define SPI_PERIPH       SPI2
#define SPI_SCK_PORT     GPIOB
#define SPI_SCK_PIN      13
#define SPI_MISO_PORT    GPIOB
#define SPI_MISO_PIN     14
#define SPI_MOSI_PORT    GPIOB
#define SPI_MOSI_PIN     15
#define SPI_NCS_PORT     GPIOB
#define SPI_NCS_PIN      12

#elif defined(STM32F4)
// Black Pill STM32F411: USART2 (PA2/PA3), TIM2 CH1/CH2 (PA0/PA1), RC-in PA4/PA5, IMU SPI2 PB12–PB15
#define UART_USART       USART2
#define UART_GPIO_PORT   GPIOA
#define UART_TX_PIN      2
#define UART_RX_PIN      3
#define PWM_TIM          TIM2
#define PWM_THROTTLE_PORT GPIOA
#define PWM_THROTTLE_PIN  0
#define PWM_STEERING_PORT GPIOA
#define PWM_STEERING_PIN  1
#define RC_IN_THROTTLE_PORT  GPIOA
#define RC_IN_THROTTLE_PIN   4
#define RC_IN_STEERING_PORT  GPIOA
#define RC_IN_STEERING_PIN   5
// IMU MPU-6050 по SPI2
#define SPI_PERIPH       SPI2
#define SPI_SCK_PORT     GPIOB
#define SPI_SCK_PIN      13
#define SPI_MISO_PORT    GPIOB
#define SPI_MISO_PIN     14
#define SPI_MOSI_PORT    GPIOB
#define SPI_MOSI_PIN     15
#define SPI_NCS_PORT     GPIOB
#define SPI_NCS_PIN      12

#elif defined(STM32G4)
// STM32G431CB: USART2 (PA2/PA3), TIM2, RC-in, IMU SPI2 — настройте под схему
#define UART_USART       USART2
#define UART_GPIO_PORT   GPIOA
#define UART_TX_PIN      2
#define UART_RX_PIN      3
#define PWM_TIM          TIM2
#define PWM_THROTTLE_PORT GPIOA
#define PWM_THROTTLE_PIN  0
#define PWM_STEERING_PORT GPIOA
#define PWM_STEERING_PIN  1
#define RC_IN_THROTTLE_PORT  GPIOA
#define RC_IN_THROTTLE_PIN   4
#define RC_IN_STEERING_PORT  GPIOA
#define RC_IN_STEERING_PIN   5
// IMU MPU-6050 по SPI2
#define SPI_PERIPH       SPI2
#define SPI_SCK_PORT     GPIOB
#define SPI_SCK_PIN      13
#define SPI_MISO_PORT    GPIOB
#define SPI_MISO_PIN     14
#define SPI_MOSI_PORT    GPIOB
#define SPI_MOSI_PIN     15
#define SPI_NCS_PORT     GPIOB
#define SPI_NCS_PIN      12

#else
#error "Не задано семейство MCU (STM32F1, STM32F4, STM32G4). Проверьте cmake/boards/"
#endif
