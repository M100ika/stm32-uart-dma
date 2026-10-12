/*
 * uart_dma.h — драйвер USART2 с приёмом и передачей через DMA.
 *
 * Приём:  DMA1 Stream5 Channel4, циклический режим, 64-байтный буфер.
 *         Новые данные забираются по трём событиям: половина буфера (HT),
 *         конец буфера (TC) и пауза на линии (IDLE). Байты попадают
 *         в кольцевой буфер rx, откуда их читает основной цикл.
 * Передача: DMA1 Stream6 Channel4, обычный режим. uart_write кладёт данные
 *         в кольцевой буфер tx и при простое запускает DMA на непрерывный
 *         кусок; по окончании (TC) прерывание запускает следующий кусок.
 *
 * Процессор не тратит время на каждый байт: одно прерывание на пачку.
 */
#ifndef UART_DMA_H
#define UART_DMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t rx_bytes;      /* принято в кольцевой буфер */
    uint32_t rx_dropped;    /* потеряно: основной цикл не успевал читать */
    uint32_t tx_bytes;      /* отправлено DMA */
    uint32_t tx_dropped;    /* не влезло в буфер передачи */
    uint32_t err_overrun;   /* ORE: байт пришёл, а предыдущий никто не забрал */
    uint32_t err_framing;   /* FE: нет стоп-бита — скорость или помеха */
    uint32_t err_noise;     /* NF: шум на линии */
    uint32_t err_dma;       /* ошибки шины DMA (TEIF) */
} uart_stats_t;

void uart_init(uint32_t pclk1_hz, uint32_t baud);

/* Неблокирующая запись: возвращает, сколько байт принято в буфер. */
size_t uart_write(const void *data, size_t len);

/* Блокирующая запись строки: ждёт места в буфере (спит в WFI).
 * Только из основного цикла, не из прерываний. */
void uart_puts(const char *s);

/* Неблокирующее чтение принятых байт. */
size_t uart_read(void *data, size_t len);

/* Дождаться, пока DMA и сдвиговый регистр отправят всё (перед сбросом). */
void uart_flush(void);

void uart_get_stats(uart_stats_t *out);

#endif /* UART_DMA_H */
