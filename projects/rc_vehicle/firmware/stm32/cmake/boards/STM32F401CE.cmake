# STM32F401CE (Cortex-M4, 512K Flash, 96K RAM) — Nucleo-F401RE и др.
set(OPENCM3_TARGET stm32f4)
set(LD_SCRIPT_DIR f4)
set(LD_SCRIPT stm32f401xe.ld)
set(MCU_DEFINE STM32F4)
set(MCU_CFLAGS -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16)
