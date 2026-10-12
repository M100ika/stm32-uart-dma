/*
 * startup.c — таблица векторов и Reset_Handler без ассемблерного startup-файла.
 *
 * После сброса ядро берёт из первых двух слов флеша начальный указатель
 * стека и адрес Reset_Handler. Reset_Handler готовит среду для C:
 * включает FPU, копирует .data из флеша в ОЗУ, обнуляет .bss и вызывает main.
 */
#include <stddef.h>
#include <stdint.h>
#include "regs.h"
#include "cpu.h"

/* Символы из линкер-скрипта: это адреса, а не переменные. Объявляем их
 * массивами неизвестной длины — так имя само является адресом. */
extern uint32_t _estack[];
extern uint32_t _sidata[], _sdata[], _edata[];
extern uint32_t _sbss[], _ebss[];

int main(void);

void Reset_Handler(void);
void Default_Handler(void);

/* Обработчики объявлены слабыми: драйвер переопределяет нужные. */
#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))
void NMI_Handler(void) WEAK_ALIAS;
void HardFault_Handler(void) WEAK_ALIAS;
void MemManage_Handler(void) WEAK_ALIAS;
void BusFault_Handler(void) WEAK_ALIAS;
void UsageFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void) WEAK_ALIAS;
void DebugMon_Handler(void) WEAK_ALIAS;
void PendSV_Handler(void) WEAK_ALIAS;
void SysTick_Handler(void) WEAK_ALIAS;
void DMA1_Stream5_IRQHandler(void) WEAK_ALIAS;
void DMA1_Stream6_IRQHandler(void) WEAK_ALIAS;
void USART2_IRQHandler(void) WEAK_ALIAS;
void EXTI15_10_IRQHandler(void) WEAK_ALIAS;

typedef void (*vector_t)(void);

/* 16 системных исключений + 97 внешних прерываний (столько у F446; у F401 их 85,
 * лишние записи для него просто не используются). */
#define VECTOR_COUNT (16 + 97)
#define IRQ(n) (16 + (n))

/*
 * Сначала диапазоном заполняем всё Default_Handler, затем переопределяем
 * используемые вектора. GCC разрешает повторную инициализацию элемента,
 * но предупреждает о ней (-Woverride-init) — здесь это намеренно.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
__attribute__((section(".isr_vector"), used))
const vector_t g_vector_table[VECTOR_COUNT] = {
    [0]  = (vector_t)(uintptr_t)_estack,
    [1]  = Reset_Handler,
    [2 ... VECTOR_COUNT - 1] = Default_Handler,
    [2]  = NMI_Handler,
    [3]  = HardFault_Handler,
    [4]  = MemManage_Handler,
    [5]  = BusFault_Handler,
    [6]  = UsageFault_Handler,
    [7 ... 10] = 0, /* зарезервировано */
    [11] = SVC_Handler,
    [12] = DebugMon_Handler,
    [13] = 0,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    [IRQ(IRQ_DMA1_STREAM5)] = DMA1_Stream5_IRQHandler,
    [IRQ(IRQ_DMA1_STREAM6)] = DMA1_Stream6_IRQHandler,
    [IRQ(IRQ_USART2)]       = USART2_IRQHandler,
    [IRQ(IRQ_EXTI15_10)]    = EXTI15_10_IRQHandler,
};
#pragma GCC diagnostic pop

/*
 * no-tree-loop-distribute-patterns: без этого GCC на -O2 заменяет циклы
 * копирования и обнуления вызовами memcpy/memset. Здесь это работало бы,
 * но startup-код не должен зависеть от библиотеки, которая сама может
 * опираться на ещё не инициализированные .data и .bss.
 */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void Reset_Handler(void)
{
    /* FPU: полный доступ к сопроцессорам CP10 и CP11. Сделать это нужно до
     * первой команды с плавающей точкой, иначе будет UsageFault (NOCP). */
    SCB_CPACR |= (0xFUL << 20);
    cpu_dsb();
    cpu_isb();

    /* Линкерные символы — отдельные «объекты» с точки зрения C, и сравнивать
     * указатели на разные объекты нельзя (неопределённое поведение).
     * Поэтому считаем размер через целочисленные адреса. */

    /* .data: начальные значения лежат во флеше по адресу _sidata. */
    size_t words = ((uintptr_t)_edata - (uintptr_t)_sdata) / sizeof(uint32_t);
    for (size_t i = 0; i < words; i++) {
        _sdata[i] = _sidata[i];
    }

    /* .bss: глобальные переменные без инициализатора обязаны быть нулями. */
    words = ((uintptr_t)_ebss - (uintptr_t)_sbss) / sizeof(uint32_t);
    for (size_t i = 0; i < words; i++) {
        _sbss[i] = 0;
    }

    (void)main();

    /* main не должна возвращаться; если вернулась — остаёмся здесь. */
    for (;;) {
    }
}

void Default_Handler(void)
{
    /* Сработало прерывание без обработчика. Номер активного исключения
     * виден в ICSR.VECTACTIVE — его удобно посмотреть отладчиком. */
    for (;;) {
    }
}
