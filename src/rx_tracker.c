#include "rx_tracker.h"

void rx_tracker_init(rx_tracker_t *t, uint16_t size)
{
    t->size = size;
    t->last_pos = 0;
}

unsigned rx_tracker_update(rx_tracker_t *t, uint16_t pos, rx_span_t out[2])
{
    if (pos >= t->size) {
        pos = 0;
    }

    unsigned n = 0;
    if (pos > t->last_pos) {
        /* Данные лежат одним куском: [last_pos, pos). */
        out[n++] = (rx_span_t){ t->last_pos, (uint16_t)(pos - t->last_pos) };
    } else if (pos < t->last_pos) {
        /* DMA перешёл через конец буфера: [last_pos, size) и [0, pos). */
        out[n++] = (rx_span_t){ t->last_pos, (uint16_t)(t->size - t->last_pos) };
        if (pos > 0U) {
            out[n++] = (rx_span_t){ 0, pos };
        }
    }
    /* pos == last_pos: новых данных нет. Ровно size байт за один вызов
     * отличить от нуля нельзя — поэтому драйвер вызывает update не реже
     * чем раз в полбуфера (прерывания HT и TC). */

    t->last_pos = pos;
    return n;
}
