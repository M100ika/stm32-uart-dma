#include "uart_dma.h"

#include <string.h>

#include "board.h"
#include "cpu.h"
#include "regs.h"
#include "ringbuf.h"
#include "rx_tracker.h"

#define UART_TX_PIN 2U
#define UART_RX_PIN 3U
#define UART_AF 7U          /* AF7 = USART1..3 */
#define DMA_CHANNEL 4UL     /* USART2_RX — Stream5/Ch4, USART2_TX — Stream6/Ch4 */

#define RX_STREAM (&DMA1->S[5])
#define TX_STREAM (&DMA1->S[6])

/* Приоритеты: всё, что вызывает rx_process, должно иметь ОДИН приоритет —
 * тогда эти обработчики не вытесняют друг друга и rx_process не нужна
 * защита от повторного входа. */
#define RX_IRQ_PRIO 5U
#define TX_IRQ_PRIO 6U

#define RX_DMA_SIZE 64U     /* при 115200 бод полбуфера = 32 байта ~ 2,8 мс */
#define RX_RB_SIZE 512U
#define TX_RB_SIZE 1024U

static uint8_t s_rx_dma[RX_DMA_SIZE];
static uint8_t s_rx_mem[RX_RB_SIZE];
static uint8_t s_tx_mem[TX_RB_SIZE];
static ringbuf_t s_rx_rb;   /* писатель: прерывания приёма, читатель: основной цикл */
static ringbuf_t s_tx_rb;   /* писатель: основной цикл, читатель: DMA (прерывание TX) */
static rx_tracker_t s_rx_trk;

/* Длина куска, который сейчас передаёт DMA; 0 — передатчик свободен.
 * Меняется только при запрещённых прерываниях или в прерывании TX. */
static volatile uint32_t s_tx_chunk;

/* Каждый счётчик меняет только один уровень приоритета, поэтому инкременты
 * не теряются. Ошибки DMA приёма и передачи считаем раздельно: обработчики
 * имеют разный приоритет, и общий счётчик мог бы потерять инкремент при
 * вытеснении посреди read-modify-write. */
static volatile uart_stats_t s_stats;
static volatile uint32_t s_err_dma_rx;
static volatile uint32_t s_err_dma_tx;

/* ------------------------------------------------------------ приём */

static void rx_dma_start(void)
{
    dma_stream_t *s = RX_STREAM;
    s->CR &= ~DMA_SxCR_EN;
    while (s->CR & DMA_SxCR_EN) {
        /* поток выключается не мгновенно: ждём, пока EN реально станет 0 */
    }
    DMA1->HIFCR = DMA_ALL_FLAGS << DMA_FLAG_SHIFT_S5;

    s->PAR = (uint32_t)(uintptr_t)&USART2->DR;
    s->M0AR = (uint32_t)(uintptr_t)s_rx_dma;
    s->NDTR = RX_DMA_SIZE;
    s->FCR = 0;  /* прямой режим без FIFO: байт уходит в память сразу */
    s->CR = (DMA_CHANNEL << DMA_SxCR_CHSEL_Pos) | (2UL << DMA_SxCR_PL_Pos) |
            DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_DIR_P2M |
            DMA_SxCR_HTIE | DMA_SxCR_TCIE | DMA_SxCR_TEIE;
    rx_tracker_init(&s_rx_trk, RX_DMA_SIZE);
    s->CR |= DMA_SxCR_EN;
}

/* Забрать из DMA-буфера всё, что пришло с прошлого раза. */
static void rx_process(void)
{
    const uint16_t pos = (uint16_t)(RX_DMA_SIZE - RX_STREAM->NDTR);
    rx_span_t spans[2];
    const unsigned n = rx_tracker_update(&s_rx_trk, pos, spans);
    for (unsigned i = 0; i < n; i++) {
        const uint32_t put = rb_write(&s_rx_rb, &s_rx_dma[spans[i].offset], spans[i].len);
        s_stats.rx_bytes += put;
        s_stats.rx_dropped += spans[i].len - put;
    }
}

void DMA1_Stream5_IRQHandler(void)
{
    const uint32_t flags = (DMA1->HISR >> DMA_FLAG_SHIFT_S5) & DMA_ALL_FLAGS;
    DMA1->HIFCR = flags << DMA_FLAG_SHIFT_S5;

    if (flags & DMA_TEIF) {
        /* При ошибке шины DMA сам выключает поток — запускаем заново. */
        s_err_dma_rx++;
        rx_dma_start();
        return;
    }
    if (flags & (DMA_HTIF | DMA_TCIF)) {
        rx_process();
    }
}

void USART2_IRQHandler(void)
{
    const uint32_t sr = USART2->SR;

    if (sr & (USART_SR_IDLE | USART_SR_ORE | USART_SR_FE | USART_SR_NF)) {
        /* Эти флаги сбрасываются последовательностью «чтение SR, затем DR».
         * К моменту IDLE последний байт уже забран DMA, так что чтение DR
         * данные не крадёт. При ORE байт в DR и так потерян. */
        (void)USART2->DR;
        if (sr & USART_SR_ORE) s_stats.err_overrun++;
        if (sr & USART_SR_FE)  s_stats.err_framing++;
        if (sr & USART_SR_NF)  s_stats.err_noise++;
    }
    if (sr & USART_SR_IDLE) {
        /* Пауза после пакета: забираем хвост, не дожидаясь HT/TC. */
        rx_process();
    }
}

/* ---------------------------------------------------------- передача */

/* Запустить DMA на следующий непрерывный кусок, если передатчик свободен.
 * Вызывать при запрещённых прерываниях или из прерывания TX. */
static void tx_kick_locked(void)
{
    if (s_tx_chunk != 0U) {
        return;
    }
    const uint8_t *ptr;
    uint32_t n = rb_peek_contig(&s_tx_rb, &ptr);
    if (n == 0U) {
        return;
    }
    if (n > 0xFFFFU) {
        n = 0xFFFFU; /* NDTR 16-битный */
    }
    s_tx_chunk = n;

    dma_stream_t *s = TX_STREAM;
    DMA1->HIFCR = DMA_ALL_FLAGS << DMA_FLAG_SHIFT_S6;
    s->M0AR = (uint32_t)(uintptr_t)ptr;
    s->NDTR = n;
    /* TC в USART сбрасывается записью 0; запись 1 в остальные биты SR
     * ни на что не влияет. */
    USART2->SR = ~USART_SR_TC;
    s->CR |= DMA_SxCR_EN;
}

void DMA1_Stream6_IRQHandler(void)
{
    const uint32_t flags = (DMA1->HISR >> DMA_FLAG_SHIFT_S6) & DMA_ALL_FLAGS;
    DMA1->HIFCR = flags << DMA_FLAG_SHIFT_S6;

    if (flags & (DMA_TCIF | DMA_TEIF)) {
        if (flags & DMA_TEIF) {
            s_err_dma_tx++;
        } else {
            s_stats.tx_bytes += s_tx_chunk;
        }
        /* Кусок считаем обработанным и при ошибке, иначе застрянем на нём. */
        rb_skip(&s_tx_rb, s_tx_chunk);
        s_tx_chunk = 0;
        tx_kick_locked();
    }
}

size_t uart_write(const void *data, size_t len)
{
    const uint32_t put = rb_write(&s_tx_rb, data, (uint32_t)len);

    /* Проверка «свободен ли DMA» и его запуск должны быть одним целым:
     * иначе прерывание TX может запустить DMA между проверкой и запуском,
     * и один кусок уйдёт дважды. */
    const uint32_t primask = irq_save();
    tx_kick_locked();
    irq_restore(primask);

    /* tx_dropped меняет только основной цикл — защита не нужна. */
    s_stats.tx_dropped += (uint32_t)len - put;
    return put;
}

void uart_puts(const char *s)
{
    size_t len = strlen(s);
    while (len > 0U) {
        const size_t put = uart_write(s, len);
        s += put;
        len -= put;
        if (len > 0U) {
            cpu_wfi(); /* место освободит прерывание TC от DMA */
        }
    }
}

size_t uart_read(void *data, size_t len)
{
    return rb_read(&s_rx_rb, data, (uint32_t)len);
}

void uart_flush(void)
{
    while (s_tx_chunk != 0U || rb_count(&s_tx_rb) != 0U) {
    }
    /* DMA закончил, но последний байт ещё может быть в сдвиговом регистре. */
    while ((USART2->SR & USART_SR_TC) == 0U) {
    }
}

void uart_get_stats(uart_stats_t *out)
{
    /* Счётчики меняются в прерываниях: снимаем копию целиком атомарно. */
    const uint32_t primask = irq_save();
    out->rx_bytes = s_stats.rx_bytes;
    out->rx_dropped = s_stats.rx_dropped;
    out->tx_bytes = s_stats.tx_bytes;
    out->tx_dropped = s_stats.tx_dropped;
    out->err_overrun = s_stats.err_overrun;
    out->err_framing = s_stats.err_framing;
    out->err_noise = s_stats.err_noise;
    out->err_dma = s_err_dma_rx + s_err_dma_tx;
    irq_restore(primask);
}

/* ------------------------------------------------------------ инициализация */

void uart_init(uint32_t pclk1_hz, uint32_t baud)
{
    (void)rb_init(&s_rx_rb, s_rx_mem, RX_RB_SIZE);
    (void)rb_init(&s_tx_rb, s_tx_mem, TX_RB_SIZE);

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->APB1ENR;

    gpio_set_af(GPIOA, UART_TX_PIN, UART_AF);
    gpio_set_af(GPIOA, UART_RX_PIN, UART_AF);
    gpio_set_speed(GPIOA, UART_TX_PIN, GPIO_SPEED_HIGH);
    gpio_set_pull(GPIOA, UART_RX_PIN, GPIO_PULL_UP); /* линия в покое — «1» */
    gpio_set_mode(GPIOA, UART_TX_PIN, GPIO_MODE_AF);
    gpio_set_mode(GPIOA, UART_RX_PIN, GPIO_MODE_AF);

    /* При передискретизации 16 значение BRR равно pclk / baud:
     * старшие 12 бит — целая часть делителя, младшие 4 — дробная в 1/16.
     * Прибавка baud/2 даёт округление до ближайшего.
     * 42 МГц / 115200 = 364,6 -> 365 (0x16D), ошибка скорости 0,08 %. */
    USART2->CR1 = 0;
    USART2->BRR = (pclk1_hz + baud / 2U) / baud;
    USART2->CR2 = 0;                                   /* 1 стоп-бит */
    USART2->CR3 = USART_CR3_DMAR | USART_CR3_DMAT | USART_CR3_EIE;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE; /* 8N1 */

    /* Поток передачи настраиваем один раз, дальше меняются только адрес и длина. */
    dma_stream_t *tx = TX_STREAM;
    tx->CR = 0;
    while (tx->CR & DMA_SxCR_EN) {
    }
    tx->PAR = (uint32_t)(uintptr_t)&USART2->DR;
    tx->FCR = 0;
    tx->CR = (DMA_CHANNEL << DMA_SxCR_CHSEL_Pos) | (1UL << DMA_SxCR_PL_Pos) |
             DMA_SxCR_MINC | DMA_SxCR_DIR_M2P | DMA_SxCR_TCIE | DMA_SxCR_TEIE;

    rx_dma_start();

    nvic_set_priority(IRQ_DMA1_STREAM5, RX_IRQ_PRIO);
    nvic_set_priority(IRQ_USART2, RX_IRQ_PRIO);
    nvic_set_priority(IRQ_DMA1_STREAM6, TX_IRQ_PRIO);
    nvic_enable(IRQ_DMA1_STREAM5);
    nvic_enable(IRQ_DMA1_STREAM6);
    nvic_enable(IRQ_USART2);

    USART2->CR1 |= USART_CR1_UE; /* включаем последним, когда всё настроено */
}
