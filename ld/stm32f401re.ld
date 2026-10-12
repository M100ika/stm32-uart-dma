/* STM32F401RE: 512 КБ флеша, 96 КБ ОЗУ. */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 96K
}
INCLUDE sections.ld
