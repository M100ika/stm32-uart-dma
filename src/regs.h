/*
 * regs.h — минимальная карта регистров STM32F401/F446 и ядра Cortex-M4.
 *
 * Описаны только периферийные блоки, которые использует проект. Адреса и
 * смещения взяты из RM0368 (F401), RM0390 (F446) и PM0214 (ядро Cortex-M4).
 * Смещения ключевых регистров проверяются _Static_assert внизу файла,
 * поэтому ошибка в раскладке структуры не соберётся, а не всплывёт на железе.
 *
 * В рабочем проекте обычно берут CMSIS-заголовки от ST; здесь регистры
 * описаны вручную, чтобы было видно, что стоит за каждой строкой драйвера.
 */
#ifndef REGS_H
#define REGS_H

#include <stddef.h>
#include <stdint.h>

#define __IO volatile
#define BIT(n) (1UL << (n))

/* ---------------------------------------------------------------- RCC */
typedef struct {
    __IO uint32_t CR;          /* 0x00 */
    __IO uint32_t PLLCFGR;     /* 0x04 */
    __IO uint32_t CFGR;        /* 0x08 */
    __IO uint32_t CIR;         /* 0x0C */
    __IO uint32_t AHB1RSTR;    /* 0x10 */
    __IO uint32_t AHB2RSTR;    /* 0x14 */
    __IO uint32_t AHB3RSTR;    /* 0x18 */
    uint32_t      RESERVED0;   /* 0x1C */
    __IO uint32_t APB1RSTR;    /* 0x20 */
    __IO uint32_t APB2RSTR;    /* 0x24 */
    uint32_t      RESERVED1[2];/* 0x28 */
    __IO uint32_t AHB1ENR;     /* 0x30 */
    __IO uint32_t AHB2ENR;     /* 0x34 */
    __IO uint32_t AHB3ENR;     /* 0x38 */
    uint32_t      RESERVED2;   /* 0x3C */
    __IO uint32_t APB1ENR;     /* 0x40 */
    __IO uint32_t APB2ENR;     /* 0x44 */
    uint32_t      RESERVED3[2];/* 0x48 */
    __IO uint32_t AHB1LPENR;   /* 0x50 */
    __IO uint32_t AHB2LPENR;   /* 0x54 */
    __IO uint32_t AHB3LPENR;   /* 0x58 */
    uint32_t      RESERVED4;   /* 0x5C */
    __IO uint32_t APB1LPENR;   /* 0x60 */
    __IO uint32_t APB2LPENR;   /* 0x64 */
    uint32_t      RESERVED5[2];/* 0x68 */
    __IO uint32_t BDCR;        /* 0x70 */
    __IO uint32_t CSR;         /* 0x74 */
} rcc_t;
#define RCC ((rcc_t *)0x40023800UL)

#define RCC_CR_HSION        BIT(0)
#define RCC_CR_HSIRDY       BIT(1)
#define RCC_CR_PLLON        BIT(24)
#define RCC_CR_PLLRDY       BIT(25)

#define RCC_PLLCFGR_M_Pos   0U   /* 6 бит  */
#define RCC_PLLCFGR_N_Pos   6U   /* 9 бит  */
#define RCC_PLLCFGR_P_Pos   16U  /* 2 бита: 0=/2 1=/4 2=/6 3=/8 */
#define RCC_PLLCFGR_SRC     BIT(22) /* 0 = HSI, 1 = HSE */
#define RCC_PLLCFGR_Q_Pos   24U  /* 4 бита */
#define RCC_PLLCFGR_MASK    ((0x3FUL << 0) | (0x1FFUL << 6) | (0x3UL << 16) | BIT(22) | (0xFUL << 24))

#define RCC_CFGR_SW_Pos     0U
#define RCC_CFGR_SW_PLL     (2UL << 0)
#define RCC_CFGR_SWS_Msk    (3UL << 2)
#define RCC_CFGR_SWS_PLL    (2UL << 2)
#define RCC_CFGR_HPRE_Msk   (0xFUL << 4)
#define RCC_CFGR_PPRE1_Msk  (7UL << 10)
#define RCC_CFGR_PPRE1_DIV2 (4UL << 10)
#define RCC_CFGR_PPRE2_Msk  (7UL << 13)

#define RCC_AHB1ENR_GPIOAEN BIT(0)
#define RCC_AHB1ENR_GPIOCEN BIT(2)
#define RCC_AHB1ENR_DMA1EN  BIT(21)
#define RCC_APB1ENR_USART2EN BIT(17)
#define RCC_APB2ENR_SYSCFGEN BIT(14)

#define RCC_CSR_RMVF        BIT(24)
#define RCC_CSR_BORRSTF     BIT(25)
#define RCC_CSR_PINRSTF     BIT(26)
#define RCC_CSR_PORRSTF     BIT(27)
#define RCC_CSR_SFTRSTF     BIT(28)
#define RCC_CSR_IWDGRSTF    BIT(29)
#define RCC_CSR_WWDGRSTF    BIT(30)
#define RCC_CSR_LPWRRSTF    BIT(31)

/* -------------------------------------------------------------- FLASH */
typedef struct {
    __IO uint32_t ACR;         /* 0x00 */
} flash_t;
#define FLASH ((flash_t *)0x40023C00UL)

#define FLASH_ACR_LATENCY_Msk 0xFUL
#define FLASH_ACR_PRFTEN    BIT(8)
#define FLASH_ACR_ICEN      BIT(9)
#define FLASH_ACR_DCEN      BIT(10)

/* --------------------------------------------------------------- GPIO */
typedef struct {
    __IO uint32_t MODER;       /* 0x00 */
    __IO uint32_t OTYPER;      /* 0x04 */
    __IO uint32_t OSPEEDR;     /* 0x08 */
    __IO uint32_t PUPDR;       /* 0x0C */
    __IO uint32_t IDR;         /* 0x10 */
    __IO uint32_t ODR;         /* 0x14 */
    __IO uint32_t BSRR;        /* 0x18 */
    __IO uint32_t LCKR;        /* 0x1C */
    __IO uint32_t AFR[2];      /* 0x20, 0x24 */
} gpio_t;
#define GPIOA ((gpio_t *)0x40020000UL)
#define GPIOC ((gpio_t *)0x40020800UL)

#define GPIO_MODE_INPUT  0U
#define GPIO_MODE_OUTPUT 1U
#define GPIO_MODE_AF     2U
#define GPIO_PULL_UP     1U
#define GPIO_SPEED_HIGH  2U

/* ------------------------------------------------------------- USART */
typedef struct {
    __IO uint32_t SR;          /* 0x00 */
    __IO uint32_t DR;          /* 0x04 */
    __IO uint32_t BRR;         /* 0x08 */
    __IO uint32_t CR1;         /* 0x0C */
    __IO uint32_t CR2;         /* 0x10 */
    __IO uint32_t CR3;         /* 0x14 */
    __IO uint32_t GTPR;        /* 0x18 */
} usart_t;
#define USART2 ((usart_t *)0x40004400UL)

#define USART_SR_PE    BIT(0)
#define USART_SR_FE    BIT(1)
#define USART_SR_NF    BIT(2)
#define USART_SR_ORE   BIT(3)
#define USART_SR_IDLE  BIT(4)
#define USART_SR_RXNE  BIT(5)
#define USART_SR_TC    BIT(6)
#define USART_SR_TXE   BIT(7)
#define USART_CR1_RE     BIT(2)
#define USART_CR1_TE     BIT(3)
#define USART_CR1_IDLEIE BIT(4)
#define USART_CR1_UE     BIT(13)
#define USART_CR3_EIE    BIT(0)
#define USART_CR3_DMAR   BIT(6)
#define USART_CR3_DMAT   BIT(7)

/* ---------------------------------------------------------------- DMA */
typedef struct {
    __IO uint32_t CR;          /* +0x00 */
    __IO uint32_t NDTR;        /* +0x04 */
    __IO uint32_t PAR;         /* +0x08 */
    __IO uint32_t M0AR;        /* +0x0C */
    __IO uint32_t M1AR;        /* +0x10 */
    __IO uint32_t FCR;         /* +0x14 */
} dma_stream_t;

typedef struct {
    __IO uint32_t LISR;        /* 0x00 */
    __IO uint32_t HISR;        /* 0x04 */
    __IO uint32_t LIFCR;       /* 0x08 */
    __IO uint32_t HIFCR;       /* 0x0C */
    dma_stream_t  S[8];        /* 0x10 + 0x18 * n */
} dma_t;
#define DMA1 ((dma_t *)0x40026000UL)

#define DMA_SxCR_EN       BIT(0)
#define DMA_SxCR_TEIE     BIT(2)
#define DMA_SxCR_HTIE     BIT(3)
#define DMA_SxCR_TCIE     BIT(4)
#define DMA_SxCR_DIR_P2M  (0UL << 6)
#define DMA_SxCR_DIR_M2P  (1UL << 6)
#define DMA_SxCR_CIRC     BIT(8)
#define DMA_SxCR_MINC     BIT(10)
#define DMA_SxCR_PL_Pos   16U
#define DMA_SxCR_CHSEL_Pos 25U

/*
 * Флаги потоков 4..7 лежат в HISR/HIFCR. У каждого потока группа из шести
 * бит (FEIF, -, DMEIF, TEIF, HTIF, TCIF); смещения групп: 0, 6, 16, 22.
 */
#define DMA_FLAG_SHIFT_S5 6U
#define DMA_FLAG_SHIFT_S6 16U
#define DMA_FEIF   BIT(0)
#define DMA_DMEIF  BIT(2)
#define DMA_TEIF   BIT(3)
#define DMA_HTIF   BIT(4)
#define DMA_TCIF   BIT(5)
#define DMA_ALL_FLAGS (DMA_FEIF | DMA_DMEIF | DMA_TEIF | DMA_HTIF | DMA_TCIF)

/* ------------------------------------------------------- SYSCFG / EXTI */
typedef struct {
    __IO uint32_t MEMRMP;      /* 0x00 */
    __IO uint32_t PMC;         /* 0x04 */
    __IO uint32_t EXTICR[4];   /* 0x08..0x14 */
} syscfg_t;
#define SYSCFG ((syscfg_t *)0x40013800UL)

typedef struct {
    __IO uint32_t IMR;         /* 0x00 */
    __IO uint32_t EMR;         /* 0x04 */
    __IO uint32_t RTSR;        /* 0x08 */
    __IO uint32_t FTSR;        /* 0x0C */
    __IO uint32_t SWIER;       /* 0x10 */
    __IO uint32_t PR;          /* 0x14 */
} exti_t;
#define EXTI ((exti_t *)0x40013C00UL)

/* --------------------------------------------------------------- IWDG */
typedef struct {
    __IO uint32_t KR;          /* 0x00 */
    __IO uint32_t PR;          /* 0x04 */
    __IO uint32_t RLR;         /* 0x08 */
    __IO uint32_t SR;          /* 0x0C */
} iwdg_t;
#define IWDG ((iwdg_t *)0x40003000UL)

/* Отладочный блок: остановка IWDG, пока ядро стоит на точке останова. */
#define DBGMCU_APB1_FZ (*(__IO uint32_t *)0xE0042008UL)
#define DBGMCU_APB1_FZ_IWDG_STOP BIT(12)

/* ------------------------------------------------------ Cortex-M4 core */
typedef struct {
    __IO uint32_t CTRL;        /* 0x00 */
    __IO uint32_t LOAD;        /* 0x04 */
    __IO uint32_t VAL;         /* 0x08 */
    __IO uint32_t CALIB;       /* 0x0C */
} systick_t;
#define SYSTICK ((systick_t *)0xE000E010UL)
#define SYSTICK_CTRL_ENABLE    BIT(0)
#define SYSTICK_CTRL_TICKINT   BIT(1)
#define SYSTICK_CTRL_CLKSOURCE BIT(2)

typedef struct {
    __IO uint32_t CPUID;       /* 0x00 */
    __IO uint32_t ICSR;        /* 0x04 */
    __IO uint32_t VTOR;        /* 0x08 */
    __IO uint32_t AIRCR;       /* 0x0C */
    __IO uint32_t SCR;         /* 0x10 */
    __IO uint32_t CCR;         /* 0x14 */
    __IO uint8_t  SHPR[12];    /* 0x18..0x23, приоритеты системных исключений 4..15 */
    __IO uint32_t SHCSR;       /* 0x24 */
    __IO uint32_t CFSR;        /* 0x28 */
    __IO uint32_t HFSR;        /* 0x2C */
    __IO uint32_t DFSR;        /* 0x30 */
    __IO uint32_t MMFAR;       /* 0x34 */
    __IO uint32_t BFAR;        /* 0x38 */
} scb_t;
#define SCB ((scb_t *)0xE000ED00UL)
#define SCB_CPACR  (*(__IO uint32_t *)0xE000ED88UL)
#define CORE_DHCSR (*(__IO uint32_t *)0xE000EDF0UL)
#define CORE_DHCSR_C_DEBUGEN BIT(0)
#define SCB_AIRCR_SYSRESET   (0x05FAUL << 16 | BIT(2))

#define NVIC_ISER ((__IO uint32_t *)0xE000E100UL)
#define NVIC_IPR  ((__IO uint8_t *)0xE000E400UL)
#define NVIC_PRIO_BITS 4U /* у STM32F4 реализованы старшие 4 бита из 8 */

/* Номера прерываний (позиция в таблице векторов минус 16). */
enum {
    IRQ_EXTI15_10    = 40,
    IRQ_DMA1_STREAM5 = 16,
    IRQ_DMA1_STREAM6 = 17,
    IRQ_USART2       = 38,
};
#define SYSTICK_EXC_NUM 15

/* ------------------------------------------- проверка раскладки структур */
_Static_assert(offsetof(rcc_t, AHB1ENR) == 0x30, "RCC AHB1ENR");
_Static_assert(offsetof(rcc_t, APB1ENR) == 0x40, "RCC APB1ENR");
_Static_assert(offsetof(rcc_t, CSR) == 0x74, "RCC CSR");
_Static_assert(offsetof(gpio_t, AFR) == 0x20, "GPIO AFR");
_Static_assert(offsetof(usart_t, CR3) == 0x14, "USART CR3");
_Static_assert(offsetof(dma_t, S) == 0x10, "DMA S0");
_Static_assert(sizeof(dma_stream_t) == 0x18, "DMA stream stride");
_Static_assert(offsetof(syscfg_t, EXTICR) == 0x08, "SYSCFG EXTICR");
_Static_assert(offsetof(exti_t, PR) == 0x14, "EXTI PR");
_Static_assert(offsetof(scb_t, SHPR) == 0x18, "SCB SHPR");
_Static_assert(offsetof(scb_t, CFSR) == 0x28, "SCB CFSR");
_Static_assert(offsetof(scb_t, BFAR) == 0x38, "SCB BFAR");

#endif /* REGS_H */
