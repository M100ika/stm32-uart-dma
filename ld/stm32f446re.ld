/* STM32F446RE: 512 КБ флеша, 128 КБ ОЗУ (SRAM1 112 КБ + SRAM2 16 КБ подряд). */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}
INCLUDE sections.ld
