#include "clock.h"

#include "regs.h"

#define PLL_M 16UL
#define PLL_N 336UL
#define PLL_P_DIV4 1UL  /* кодировка поля P: 0=/2, 1=/4, 2=/6, 3=/8 */
#define PLL_Q 7UL

#define SYSCLK_PLL_HZ 84000000UL
/* Таблица задержек флеша при 2,7–3,6 В: 2 такта ожидания для 60 < HCLK <= 84 МГц. */
#define FLASH_LATENCY_84MHZ 2UL

/* Ожидание флага с ограничением: зависание при запуске хуже, чем работа на HSI. */
#define WAIT_LIMIT 100000UL

static uint32_t s_hclk_hz = HSI_HZ;
static uint32_t s_pclk1_hz = HSI_HZ;

static bool wait_set(volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
    for (uint32_t i = 0; i < WAIT_LIMIT; i++) {
        if ((*reg & mask) == value) {
            return true;
        }
    }
    return false;
}

bool clock_init(void)
{
    /* После сброса уже работает HSI, но убедимся, что он стабилен. */
    RCC->CR |= RCC_CR_HSION;
    if (!wait_set(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY)) {
        return false;
    }

    /* Задержку флеша поднимаем ДО повышения частоты: иначе на 84 МГц ядро
     * будет читать флеш быстрее, чем тот успевает отдавать данные.
     * Значение перечитываем — так требует RM для проверки записи. */
    FLASH->ACR = FLASH_LATENCY_84MHZ | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    if ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != FLASH_LATENCY_84MHZ) {
        return false;
    }

    /* Делители шин выставляем до переключения, чтобы APB1 ни на миг
     * не превысил 42 МГц. */
    uint32_t cfgr = RCC->CFGR;
    cfgr &= ~(RCC_CFGR_HPRE_Msk | RCC_CFGR_PPRE1_Msk | RCC_CFGR_PPRE2_Msk);
    cfgr |= RCC_CFGR_PPRE1_DIV2;
    RCC->CFGR = cfgr;
    s_pclk1_hz = HSI_HZ / 2U; /* пока SYSCLK = HSI, APB1 = 8 МГц */

    /* PLL настраивается только когда выключена. Меняем лишь свои поля:
     * у F446 в старших битах лежит PLLR, его не трогаем. */
    RCC->CR &= ~RCC_CR_PLLON;
    if (!wait_set(&RCC->CR, RCC_CR_PLLRDY, 0)) {
        return false;
    }
    uint32_t pll = RCC->PLLCFGR & ~RCC_PLLCFGR_MASK;
    pll |= (PLL_M << RCC_PLLCFGR_M_Pos) | (PLL_N << RCC_PLLCFGR_N_Pos) |
           (PLL_P_DIV4 << RCC_PLLCFGR_P_Pos) | (PLL_Q << RCC_PLLCFGR_Q_Pos);
    /* RCC_PLLCFGR_SRC = 0: источник HSI. */
    RCC->PLLCFGR = pll;

    RCC->CR |= RCC_CR_PLLON;
    if (!wait_set(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY)) {
        return false;
    }

    RCC->CFGR = (RCC->CFGR & ~3UL) | RCC_CFGR_SW_PLL;
    if (!wait_set(&RCC->CFGR, RCC_CFGR_SWS_Msk, RCC_CFGR_SWS_PLL)) {
        return false; /* SYSCLK остался на HSI, частоты выше уже верные */
    }

    s_hclk_hz = SYSCLK_PLL_HZ;
    s_pclk1_hz = SYSCLK_PLL_HZ / 2U;
    return true;
}

uint32_t clock_hclk_hz(void)
{
    return s_hclk_hz;
}

uint32_t clock_pclk1_hz(void)
{
    return s_pclk1_hz;
}
