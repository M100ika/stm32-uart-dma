/*
 * Общая часть линкер-скрипта. MEMORY задаётся в файле конкретного МК.
 *
 * Флеш: таблица векторов, код, константы, начальные значения .data.
 * ОЗУ:  .data, .bss, .noinit, затем свободная область и стек сверху.
 */
ENTRY(Reset_Handler)

_estack     = ORIGIN(RAM) + LENGTH(RAM); /* стек растёт вниз от конца ОЗУ */
_min_stack  = 4K;                         /* проверяется при линковке ниже */

SECTIONS
{
    .isr_vector : ALIGN(4)
    {
        KEEP(*(.isr_vector))        /* обязана лежать по адресу 0x08000000 */
    } > FLASH

    .text : ALIGN(4)
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > FLASH

    _sidata = LOADADDR(.data);

    .data : ALIGN(4)
    {
        _sdata = .;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) : ALIGN(4)
    {
        _sbss = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /* Не обнуляется при старте: переживает программный сброс.
     * Здесь хранится отчёт о последнем HardFault. */
    .noinit (NOLOAD) : ALIGN(4)
    {
        *(.noinit .noinit.*)
        . = ALIGN(4);
    } > RAM

    /* Резерв под стек: если данные его съели, линковка упадёт. */
    ._stack_check (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + _min_stack;
    } > RAM

}

ASSERT(ADDR(.isr_vector) == ORIGIN(FLASH), "таблица векторов должна быть в начале флеша")
