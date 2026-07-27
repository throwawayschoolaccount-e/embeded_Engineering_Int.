// regmap_f103.h
// Minimal register map for the STM32F103 peripherals this bootloader
// touches: RCC (clocks), GPIOA, USART2, the flash controller (FPEC), and
// SysTick. No CMSIS/HAL - direct struct-over-address access, same style
// as the blink test that already proved the toolchain + board work.
#pragma once
#include <cstdint>

#define __IO volatile

// ---- RCC ---------------------------------------------------------------
struct RCC_TypeDef {
    __IO uint32_t CR;
    __IO uint32_t CFGR;
    __IO uint32_t CIR;
    __IO uint32_t APB2RSTR;
    __IO uint32_t APB1RSTR;
    __IO uint32_t AHBENR;
    __IO uint32_t APB2ENR;
    __IO uint32_t APB1ENR;
    __IO uint32_t BDCR;
    __IO uint32_t CSR;
};
#define RCC (reinterpret_cast<RCC_TypeDef*>(0x40021000u))
constexpr uint32_t RCC_APB2ENR_IOPAEN   = (1u << 2);
constexpr uint32_t RCC_APB1ENR_USART2EN = (1u << 17);

// ---- GPIOA ---------------------------------------------------------------
struct GPIO_TypeDef {
    __IO uint32_t CRL;
    __IO uint32_t CRH;
    __IO uint32_t IDR;
    __IO uint32_t ODR;
    __IO uint32_t BSRR;
    __IO uint32_t BRR;
    __IO uint32_t LCKR;
};
#define GPIOA (reinterpret_cast<GPIO_TypeDef*>(0x40010800u))

// ---- USART2 ---------------------------------------------------------------
struct USART_TypeDef {
    __IO uint32_t SR;
    __IO uint32_t DR;
    __IO uint32_t BRR;
    __IO uint32_t CR1;
    __IO uint32_t CR2;
    __IO uint32_t CR3;
    __IO uint32_t GTPR;
};
#define USART2_REG (reinterpret_cast<USART_TypeDef*>(0x40004400u))
constexpr uint32_t USART_CR1_UE  = (1u << 13);
constexpr uint32_t USART_CR1_TE  = (1u << 3);
constexpr uint32_t USART_CR1_RE  = (1u << 2);
constexpr uint32_t USART_SR_TXE  = (1u << 7);
constexpr uint32_t USART_SR_RXNE = (1u << 5);

// ---- Flash controller (FPEC) ---------------------------------------------
struct FLASH_TypeDef {
    __IO uint32_t ACR;
    __IO uint32_t KEYR;
    __IO uint32_t OPTKEYR;
    __IO uint32_t SR;
    __IO uint32_t CR;
    __IO uint32_t AR;
    __IO uint32_t RESERVED;
    __IO uint32_t OBR;
    __IO uint32_t WRPR;
};
#define FLASH_R (reinterpret_cast<FLASH_TypeDef*>(0x40022000u))
constexpr uint32_t FLASH_CR_PG   = (1u << 0);
constexpr uint32_t FLASH_CR_PER  = (1u << 1);
constexpr uint32_t FLASH_CR_STRT = (1u << 6);
constexpr uint32_t FLASH_CR_LOCK = (1u << 7);
constexpr uint32_t FLASH_SR_BSY  = (1u << 0);
constexpr uint32_t FLASH_KEY1    = 0x45670123u;
constexpr uint32_t FLASH_KEY2    = 0xCDEF89ABu;

// ---- SysTick (Cortex-M3 core peripheral) ----------------------------------
struct SysTick_TypeDef {
    __IO uint32_t CTRL;
    __IO uint32_t LOAD;
    __IO uint32_t VAL;
    __IO uint32_t CALIB;
};
#define SYSTICK (reinterpret_cast<SysTick_TypeDef*>(0xE000E010u))
constexpr uint32_t SYSTICK_CTRL_ENABLE    = (1u << 0);
constexpr uint32_t SYSTICK_CTRL_TICKINT   = (1u << 1);
constexpr uint32_t SYSTICK_CTRL_CLKSOURCE = (1u << 2);

// ---- SCB (for vector table relocation on jump to application) ------------
#define SCB_VTOR (reinterpret_cast<volatile uint32_t*>(0xE000ED08u))
