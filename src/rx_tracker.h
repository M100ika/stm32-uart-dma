/*
 * rx_tracker.h — учёт новых байт в кольцевом DMA-буфере приёма.
 *
 * DMA в циклическом режиме пишет байты по кругу в буфер фиксированного
 * размера и уменьшает счётчик NDTR. Текущая позиция записи:
 *     pos = size - NDTR.
 * Трекер помнит, до какой позиции данные уже забраны, и по новой позиции
 * возвращает один или два непрерывных отрезка с новыми байтами.
 *
 * Логика не зависит от железа и проверяется unit-тестами на ПК.
 */
#ifndef RX_TRACKER_H
#define RX_TRACKER_H

#include <stdint.h>

typedef struct {
    uint16_t offset;
    uint16_t len;
} rx_span_t;

typedef struct {
    uint16_t size;      /* размер DMA-буфера */
    uint16_t last_pos;  /* до этой позиции данные уже обработаны */
} rx_tracker_t;

void rx_tracker_init(rx_tracker_t *t, uint16_t size);

/*
 * pos — текущая позиция записи DMA (size - NDTR). Значение pos == size
 * допустимо: так бывает, если NDTR прочитан как 0 в момент перезагрузки
 * циклического режима; оно трактуется как 0.
 * Возвращает число отрезков в out (0, 1 или 2).
 */
unsigned rx_tracker_update(rx_tracker_t *t, uint16_t pos, rx_span_t out[2]);

#endif /* RX_TRACKER_H */
