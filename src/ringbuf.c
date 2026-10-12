#include "ringbuf.h"

#include <string.h>

/* Чтение «чужого» индекса — acquire, публикация своего — release. */
#define LOAD_ACQ(p)     __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define LOAD_RLX(p)     __atomic_load_n((p), __ATOMIC_RELAXED)
#define STORE_REL(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)

bool rb_init(ringbuf_t *rb, uint8_t *storage, uint32_t size)
{
    if (rb == NULL || storage == NULL || size < 2U || (size & (size - 1U)) != 0U) {
        return false;
    }
    rb->buf = storage;
    rb->mask = size - 1U;
    rb->head = 0;
    rb->tail = 0;
    return true;
}

uint32_t rb_size(const ringbuf_t *rb)
{
    return rb->mask + 1U;
}

uint32_t rb_count(const ringbuf_t *rb)
{
    /* Читаем tail раньше head: значение может только устареть «в меньшую
     * сторону» и никогда не превысит реальное количество данных. */
    uint32_t tail = LOAD_ACQ(&rb->tail);
    uint32_t head = LOAD_ACQ(&rb->head);
    return head - tail;
}

uint32_t rb_free(const ringbuf_t *rb)
{
    return rb_size(rb) - rb_count(rb);
}

uint32_t rb_write(ringbuf_t *rb, const void *data, uint32_t len)
{
    const uint32_t head = LOAD_RLX(&rb->head);       /* свой индекс */
    const uint32_t tail = LOAD_ACQ(&rb->tail);       /* индекс читателя */
    const uint32_t space = rb_size(rb) - (head - tail);
    if (len > space) {
        len = space;
    }
    if (len == 0U) {
        return 0;
    }

    /* Максимум два куска: до конца массива и с его начала. */
    const uint32_t pos = head & rb->mask;
    const uint32_t first = (len < rb_size(rb) - pos) ? len : rb_size(rb) - pos;
    const uint8_t *src = (const uint8_t *)data;
    memcpy(&rb->buf[pos], src, first);
    memcpy(&rb->buf[0], src + first, len - first);

    STORE_REL(&rb->head, head + len);                /* публикуем данные */
    return len;
}

uint32_t rb_read(ringbuf_t *rb, void *data, uint32_t len)
{
    const uint32_t tail = LOAD_RLX(&rb->tail);
    const uint32_t head = LOAD_ACQ(&rb->head);
    const uint32_t avail = head - tail;
    if (len > avail) {
        len = avail;
    }
    if (len == 0U) {
        return 0;
    }

    const uint32_t pos = tail & rb->mask;
    const uint32_t first = (len < rb_size(rb) - pos) ? len : rb_size(rb) - pos;
    uint8_t *dst = (uint8_t *)data;
    memcpy(dst, &rb->buf[pos], first);
    memcpy(dst + first, &rb->buf[0], len - first);

    STORE_REL(&rb->tail, tail + len);                /* освобождаем место */
    return len;
}

uint32_t rb_peek_contig(const ringbuf_t *rb, const uint8_t **ptr)
{
    const uint32_t tail = LOAD_RLX(&rb->tail);
    const uint32_t head = LOAD_ACQ(&rb->head);
    const uint32_t avail = head - tail;
    const uint32_t pos = tail & rb->mask;
    const uint32_t to_end = rb_size(rb) - pos;
    *ptr = &rb->buf[pos];
    return (avail < to_end) ? avail : to_end;
}

void rb_skip(ringbuf_t *rb, uint32_t n)
{
    const uint32_t tail = LOAD_RLX(&rb->tail);
    const uint32_t avail = LOAD_ACQ(&rb->head) - tail;
    if (n > avail) {
        n = avail;
    }
    STORE_REL(&rb->tail, tail + n);
}
