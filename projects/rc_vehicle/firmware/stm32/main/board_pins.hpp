#pragma once

// Пины и номера периферии по семейству MCU (UART, TIM для PWM, RC-in, I2C).
// Настраивайте под вашу схему. Примеры для типовых плат.
// При сборке через CMake одно из STM32F1/F4/G4 задаётся в
// cmake/boards/<MCU>.cmake

#if !defined(STM32F1) && !defined(STM32F4) && !defined(STM32G4)
#define STM32F1 1 // Fallback для IDE/линтера (реальный MCU задаётся при make)
#endif

#if defined(STM32F1)
// Blue Pill STM32F103C8: USART2 (PA2/PA3), TIM2 CH1/CH2 (PA0/PA1), RC-in
// PA4/PA5, I2C1 PB6/PB7
#define UART_USART USART2
#define UART_GPIO_PORT GPIOA
#define UART_TX_PIN GPIO2
#define UART_RX_PIN GPIO3
#define PWM_TIM TIM2
#define PWM_THROTTLE_RCC RCC_GPIOA
#define PWM_THROTTLE_PORT GPIOA
#define PWM_THROTTLE_PIN GPIO0
#define PWM_STEERING_PORT GPIOA
#define PWM_STEERING_PIN GPIO1
#define RC_IN_THROTTLE_PORT GPIOA
#define RC_IN_THROTTLE_PIN GPIO4
#define RC_IN_STEERING_PORT GPIOA
#define RC_IN_STEERING_PIN GPIO5
#define I2C_PERIPH I2C1
#define I2C_SCL_PORT GPIOB
#define I2C_SCL_PIN GPIO6
#define I2C_SDA_PORT GPIOB
#define I2C_SDA_PIN GPIO7

#elif defined(STM32F4)
// Black Pill STM32F411: USART2 (PA2/PA3), TIM2 CH1/CH2 (PA0/PA1), RC-in
// PA4/PA5, I2C1 PB6/PB7
#define UART_USART USART2
#define UART_GPIO_PORT GPIOA
#define UART_TX_PIN GPIO2
#define UART_RX_PIN GPIO3
#define PWM_TIM TIM2
#define PWM_THROTTLE_PORT GPIOA
#define PWM_THROTTLE_PIN GPIO0
#define PWM_STEERING_PORT GPIOA
#define PWM_STEERING_PIN GPIO1
#define RC_IN_THROTTLE_PORT GPIOA
#define RC_IN_THROTTLE_PIN GPIO4
#define RC_IN_STEERING_PORT GPIOA
#define RC_IN_STEERING_PIN GPIO5
#define I2C_PERIPH I2C1
#define I2C_SCL_PORT GPIOB
#define I2C_SCL_PIN GPIO6
#define I2C_SDA_PORT GPIOB
#define I2C_SDA_PIN GPIO7

#elif defined(STM32G4)
// STM32G431CB: USART2 (PA2/PA3), TIM2, RC-in, I2C1 — настройте под вашу схему
#define UART_USART USART2
#define UART_GPIO_PORT GPIOA
#define UART_TX_PIN GPIO2
#define UART_RX_PIN GPIO3
#define PWM_TIM TIM2
#define PWM_THROTTLE_PORT GPIOA
#define PWM_THROTTLE_PIN GPIO0
#define PWM_STEERING_PORT GPIOA
#define PWM_STEERING_PIN GPIO1
#define RC_IN_THROTTLE_PORT GPIOA
#define RC_IN_THROTTLE_PIN GPIO4
#define RC_IN_STEERING_PORT GPIOA
#define RC_IN_STEERING_PIN GPIO5
#define I2C_PERIPH I2C1
#define I2C_SCL_PORT GPIOB
#define I2C_SCL_PIN GPIO6
#define I2C_SDA_PORT GPIOB
#define I2C_SDA_PIN GPIO7

#else
#error                                                                         \
    "Не задано семейство MCU (STM32F1, STM32F4, STM32G4). Проверьте конфиг платы в cmake/boards/"
#endif
