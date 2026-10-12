#include "diag.h"

#include "cpu.h"
#include "regs.h"

#define CRASH_MAGIC 0xDEADC0DEUL

/* Секция .noinit не обнуляется при старте, поэтому отчёт переживает
 * программный сброс. После подачи питания там мусор — его отсекает magic. */
__attribute__((section(".noinit"))) static crash_report_t s_crash;

reset_cause_t diag_reset_cause(void)
{
    const uint32_t csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF; /* сбросить флаги, чтобы следующий запуск видел свежие */

    /* Порядок важен: любой сброс выставляет и PINRSTF (внутренний сброс идёт
     * через вывод NRST), а включение питания — ещё и BORRSTF. */
    if (csr & RCC_CSR_IWDGRSTF) return RESET_IWDG;
    if (csr & RCC_CSR_WWDGRSTF) return RESET_WWDG;
    if (csr & RCC_CSR_LPWRRSTF) return RESET_LOW_POWER;
    if (csr & RCC_CSR_SFTRSTF)  return RESET_SOFTWARE;
    if (csr & RCC_CSR_PORRSTF)  return RESET_POWER_ON;
    if (csr & RCC_CSR_BORRSTF)  return RESET_BROWN_OUT;
    if (csr & RCC_CSR_PINRSTF)  return RESET_PIN;
    return RESET_UNKNOWN;
}

bool diag_take_crash(crash_report_t *out)
{
    if (s_crash.magic != CRASH_MAGIC) {
        return false;
    }
    *out = s_crash;
    s_crash.magic = 0;
    return true;
}

/*
 * При входе в исключение ядро само кладёт в стек 8 слов:
 *   r0, r1, r2, r3, r12, lr, pc, xpsr.
 * Бит 2 в EXC_RETURN (значение LR в обработчике) говорит, какой стек
 * использовался: 0 — MSP, 1 — PSP. Обработчик голый (naked), чтобы
 * компилятор не трогал стек до того, как мы возьмём нужный указатель.
 */
__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4      \n"
        "ite eq          \n"
        "mrseq r0, msp   \n"
        "mrsne r0, psp   \n"
        "b hardfault_c   \n");
}

__attribute__((used, noreturn)) void hardfault_c(const uint32_t *frame)
{
    s_crash.pc = frame[6];
    s_crash.lr = frame[5];
    s_crash.psr = frame[7];
    s_crash.cfsr = SCB->CFSR;
    s_crash.hfsr = SCB->HFSR;
    s_crash.mmfar = SCB->MMFAR;
    s_crash.bfar = SCB->BFAR;
    s_crash.magic = CRASH_MAGIC;

    /* Если подключён отладчик — остановиться прямо здесь. */
    if (CORE_DHCSR & CORE_DHCSR_C_DEBUGEN) {
        __asm volatile("bkpt #0");
    }
    cpu_reset();
}

__attribute__((noreturn)) void diag_trigger_fault(void)
{
    /* Переход по адресу с нулевым младшим битом означает попытку перейти
     * в режим ARM, которого у Cortex-M нет: UsageFault INVSTATE,
     * который без включённого UsageFault перерастает в HardFault. */
    void (*volatile bad)(void) = (void (*)(void))0x08000000UL;
    bad();
    for (;;) {
    }
}

/* ------------------------------------------------------------- IWDG */

#define IWDG_KEY_START  0xCCCCUL
#define IWDG_KEY_ACCESS 0x5555UL
#define IWDG_KEY_RELOAD 0xAAAAUL

void watchdog_init(void)
{
    /* На точке останова отладчика IWDG не должен сбрасывать плату. */
    DBGMCU_APB1_FZ |= DBGMCU_APB1_FZ_IWDG_STOP;

    /* LSI ~32 кГц / 64 = 500 Гц; 1000 отсчётов ~ 2 с.
     * Частота LSI гуляет от 17 до 47 кГц, поэтому реальный таймаут
     * от ~1,4 до ~3,8 с — кормить надо заметно чаще. */
    IWDG->KR = IWDG_KEY_START;
    IWDG->KR = IWDG_KEY_ACCESS;
    IWDG->PR = 4;          /* делитель /64 */
    IWDG->RLR = 1000 - 1;
    while (IWDG->SR != 0U) {
        /* ждём, пока значения перейдут в домен LSI (PVU, RVU) */
    }
    IWDG->KR = IWDG_KEY_RELOAD;
}

void watchdog_feed(void)
{
    IWDG->KR = IWDG_KEY_RELOAD;
}
