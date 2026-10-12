#include "board.h"

#include "cpu.h"
#include "systick.h"

#define LED_PIN 5U
#define BUTTON_PIN 13U
#define BUTTON_DEBOUNCE_MS 50U
#define BUTTON_IRQ_PRIO 7U

/* Поля по 2 бита на вывод (MODER, PUPDR, OSPEEDR) и по 4 бита (AFR).
 * Read-modify-write не атомарен: настраиваем выводы только при запуске,
 * до включения прерываний, которые могли бы трогать тот же порт. */
static void set_field2(volatile uint32_t *reg, unsigned pin, unsigned value)
{
    *reg = (*reg & ~(3UL << (pin * 2U))) | ((uint32_t)value << (pin * 2U));
}

void gpio_set_mode(gpio_t *port, unsigned pin, unsigned mode) { set_field2(&port->MODER, pin, mode); }
void gpio_set_pull(gpio_t *port, unsigned pin, unsigned pull) { set_field2(&port->PUPDR, pin, pull); }
void gpio_set_speed(gpio_t *port, unsigned pin, unsigned speed) { set_field2(&port->OSPEEDR, pin, speed); }

void gpio_set_af(gpio_t *port, unsigned pin, unsigned af)
{
    volatile uint32_t *reg = &port->AFR[pin >> 3];
    const unsigned shift = (pin & 7U) * 4U;
    *reg = (*reg & ~(0xFUL << shift)) | ((uint32_t)af << shift);
}

/* ------------------------------------------------------------------ LED */

void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR; /* после включения тактирования нужна пауза в пару тактов */
    gpio_set_mode(GPIOA, LED_PIN, GPIO_MODE_OUTPUT);
    led_set(false);
}

void led_set(bool on)
{
    /* BSRR атомарно ставит или сбрасывает бит без чтения ODR,
     * поэтому безопасен и из прерывания. */
    GPIOA->BSRR = on ? BIT(LED_PIN) : BIT(LED_PIN + 16U);
}

bool led_is_on(void)
{
    return (GPIOA->ODR & BIT(LED_PIN)) != 0U;
}

void led_toggle(void)
{
    led_set(!led_is_on());
}

/* --------------------------------------------------------------- кнопка */

static volatile uint32_t s_presses;
static uint32_t s_last_press_ms;

void button_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;

    gpio_set_mode(GPIOC, BUTTON_PIN, GPIO_MODE_INPUT); /* подтяжка стоит на плате */

    /* Линию EXTI13 подключаем к порту C: EXTICR4, биты [7:4], код порта C = 2. */
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~(0xFUL << 4)) | (2UL << 4);
    EXTI->FTSR |= BIT(BUTTON_PIN);  /* нажатие тянет вывод к земле: спадающий фронт */
    EXTI->RTSR &= ~BIT(BUTTON_PIN);
    EXTI->PR = BIT(BUTTON_PIN);     /* сбросить возможный старый флаг (запись 1) */
    EXTI->IMR |= BIT(BUTTON_PIN);

    nvic_set_priority(IRQ_EXTI15_10, BUTTON_IRQ_PRIO);
    nvic_enable(IRQ_EXTI15_10);
}

uint32_t button_take_presses(void)
{
    /* Прочитать и обнулить нужно атомарно, иначе нажатие между
     * этими двумя действиями потеряется. */
    uint32_t s = irq_save();
    uint32_t n = s_presses;
    s_presses = 0;
    irq_restore(s);
    return n;
}

void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR & BIT(BUTTON_PIN)) {
        EXTI->PR = BIT(BUTTON_PIN);
        /* Дребезг даёт серию фронтов за несколько миллисекунд;
         * засчитываем только первый в окне 50 мс. */
        const uint32_t now = millis();
        if (elapsed(now, s_last_press_ms, BUTTON_DEBOUNCE_MS)) {
            s_last_press_ms = now;
            s_presses++;
        }
    }
}
