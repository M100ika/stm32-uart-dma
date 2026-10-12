/*
 * uart_dma.h — драйвер USART2 с приёмом и передачей через DMA
 * и управлением направлением приёмопередатчика RS-485.
 *
 * Приём:  DMA1 Stream5 Channel4, циклический режим, 64-байтный буфер.
 *         Новые данные забираются по трём событиям: половина буфера (HT),
 *         конец буфера (TC) и пауза на линии (IDLE). Байты попадают
 *         в кольцевой буфер rx, откуда их читает основной цикл. Время
 *         прихода последнего байта запоминается для разбиения на кадры.
 * Передача: DMA1 Stream6 Channel4, обычный режим. uart_write кладёт данные
 *         в кольцевой буфер tx и при простое запускает DMA на непрерывный
 *         кусок; по окончании (TC) прерывание запускает следующий кусок.
 * RS-485: вывод DE (PA8) поднимается перед первым байтом и опускается,
 *         когда последний стоп-бит ушёл в линию (флаг TC у USART, а не у DMA).
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

/* Неблокирующее чтение принятых байт. */
size_t uart_read(void *data, size_t len);

/* Есть ли непрочитанные байты в буфере приёма. */
bool uart_rx_pending(void);

/* Время (timer_us_now) последнего принятого байта. */
uint32_t uart_last_rx_us(void);

/* Ждать, пока всё отправлено и линия RS-485 отпущена (перед сбросом). */
void uart_flush(void);

void uart_get_stats(uart_stats_t *out);

#endif /* UART_DMA_H */
