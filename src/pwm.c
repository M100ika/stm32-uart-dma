#include "pwm.h"

#include "board.h"
#include "regs.h"

#define PWM_PIN 5U
#define PWM_AF  1U   /* AF1 = TIM1/TIM2 */

void pwm_init(uint32_t tim_clk_hz)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;

    gpio_set_af(GPIOA, PWM_PIN, PWM_AF);
    gpio_set_mode(GPIOA, PWM_PIN, GPIO_MODE_AF);

    /* 1 МГц / 1000 отсчётов = 1 кГц. CCR1 = 0..1000: при CCR1 > ARR
     * выход в режиме PWM1 постоянно активен, то есть 1000 — это 100 %. */
    TIM2->CR1 = 0;
    TIM2->PSC = tim_clk_hz / 1000000U - 1U;
    TIM2->ARR = PWM_MAX - 1U;
    TIM2->CCR[0] = 0;
    /* PWM1: выход активен, пока CNT < CCR1. OC1PE — новое значение CCR1
     * применяется с начала следующего периода, без «рваных» импульсов. */
    TIM2->CCMR1 = TIM_CCMR1_OC1M_PWM1 | TIM_CCMR1_OC1PE;
    TIM2->CCER = TIM_CCER_CC1E;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

void pwm_set(uint16_t permille)
{
    if (permille > PWM_MAX) {
        permille = PWM_MAX;
    }
    TIM2->CCR[0] = permille;
}
