# STM32G431CBTx (Cortex-M4, 128K Flash, 32K RAM) — из схемы rc_vehicle
set(OPENCM3_TARGET stm32g4)
set(LD_SCRIPT_DIR g4)
set(LD_SCRIPT stm32g431xb.ld)
set(MCU_DEFINE STM32G4)
set(MCU_CFLAGS -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16)
