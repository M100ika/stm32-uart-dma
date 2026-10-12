#include "timer_us.h"

#include "regs.h"

void timer_us_init(uint32_t tim_clk_hz)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    (void)RCC->APB1ENR;

    TIM5->CR1 = 0;
    TIM5->PSC = tim_clk_hz / 1000000U - 1U; /* 84 МГц / 84 = 1 МГц */
    TIM5->ARR = 0xFFFFFFFFU;
    /* PSC буферизован и вступает в силу на событии обновления;
     * генерируем его вручную, иначе первый период пройдёт без делителя. */
    TIM5->EGR = TIM_EGR_UG;
    TIM5->CR1 = TIM_CR1_CEN;
}

uint32_t timer_us_now(void)
{
    return TIM5->CNT;
}
