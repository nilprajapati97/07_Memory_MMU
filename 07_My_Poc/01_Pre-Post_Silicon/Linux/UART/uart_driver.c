#include "uart_driver.h"
#include <stdio.h>
#include <time.h>

/* ── MMIO helpers ────────────────────────────────────────────────── */
static inline void reg_write(uintptr_t base, uint32_t off, uint8_t val)
{
    *((volatile uint8_t *)(base + off)) = val;
}

static inline uint8_t reg_read(uintptr_t base, uint32_t off)
{
    return *((volatile uint8_t *)(base + off));
}

/* Returns elapsed ms since start */
static uint32_t elapsed_ms(struct timespec *start)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint32_t)((now.tv_sec  - start->tv_sec)  * 1000 +
                      (now.tv_nsec - start->tv_nsec) / 1000000);
}

/* ── Init ────────────────────────────────────────────────────────── */
int uart_init(uart_dev_t *dev, uintptr_t base, uint32_t clock_hz)
{
    dev->base     = base;
    dev->clock_hz = clock_hz;
    dev->lcr_val  = LCR_WLS_8;   /* 8N1 default */

    /* Disable all interrupts */
    reg_write(base, UART_IER, 0x00);

    /* Reset & disable FIFO */
    reg_write(base, UART_FCR, FCR_RX_RST | FCR_TX_RST);

    /* 8N1 */
    reg_write(base, UART_LCR, dev->lcr_val);

    /* Assert DTR, RTS */
    reg_write(base, UART_MCR, MCR_DTR | MCR_RTS | MCR_OUT2);

    return 0;
}

/* ── Baud rate ───────────────────────────────────────────────────── */
int uart_set_baud(uart_dev_t *dev, uint32_t divisor)
{
    if (divisor == 0) return -1;
    dev->baud = divisor;

    uint8_t lcr = reg_read(dev->base, UART_LCR);
    reg_write(dev->base, UART_LCR, lcr | LCR_DLAB);
    reg_write(dev->base, UART_DLL, (uint8_t)(divisor & 0xFF));
    reg_write(dev->base, UART_DLH, (uint8_t)((divisor >> 8) & 0xFF));
    reg_write(dev->base, UART_LCR, lcr & ~LCR_DLAB);
    return 0;
}

/* ── Line format ─────────────────────────────────────────────────── */
int uart_set_format(uart_dev_t *dev, uint8_t data_bits,
                    uint8_t stop_bits, uint8_t parity)
{
    uint8_t lcr = 0;

    switch (data_bits) {
        case 5: lcr |= LCR_WLS_5; break;
        case 6: lcr |= LCR_WLS_6; break;
        case 7: lcr |= LCR_WLS_7; break;
        case 8: lcr |= LCR_WLS_8; break;
        default: return -1;
    }
    if (stop_bits == 2) lcr |= LCR_STB;

    /* parity: 0=none, 1=odd, 2=even */
    if (parity == 1) { lcr |= LCR_PEN; }
    else if (parity == 2) { lcr |= LCR_PEN | LCR_EPS; }

    dev->lcr_val = lcr;
    reg_write(dev->base, UART_LCR, lcr);
    return 0;
}

/* ── Loopback ────────────────────────────────────────────────────── */
void uart_enable_loopback(uart_dev_t *dev, bool en)
{
    uint8_t mcr = reg_read(dev->base, UART_MCR);
    if (en) mcr |=  MCR_LOOP;
    else    mcr &= ~MCR_LOOP;
    reg_write(dev->base, UART_MCR, mcr);
}

/* ── FIFO ────────────────────────────────────────────────────────── */
void uart_fifo_enable(uart_dev_t *dev, bool en, uint8_t trig)
{
    if (en)
        reg_write(dev->base, UART_FCR,
                  FCR_FIFO_EN | FCR_RX_RST | FCR_TX_RST | trig);
    else
        reg_write(dev->base, UART_FCR, 0x00);
}

/* ── TX ──────────────────────────────────────────────────────────── */
int uart_putc(uart_dev_t *dev, uint8_t ch)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    while (!(reg_read(dev->base, UART_LSR) & LSR_THRE)) {
        if (elapsed_ms(&ts) > UART_TIMEOUT_MS) return -1;
    }
    reg_write(dev->base, UART_THR, ch);
    return 0;
}

int uart_puts(uart_dev_t *dev, const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        if (uart_putc(dev, buf[i]) < 0) return -(int)i;
    return (int)len;
}

/* ── RX ──────────────────────────────────────────────────────────── */
int uart_getc(uart_dev_t *dev, uint8_t *ch)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    while (!(reg_read(dev->base, UART_LSR) & LSR_DR)) {
        if (elapsed_ms(&ts) > UART_TIMEOUT_MS) return -1;
    }
    *ch = reg_read(dev->base, UART_RBR);
    return 0;
}

int uart_gets(uart_dev_t *dev, uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        if (uart_getc(dev, &buf[i]) < 0) return -(int)i;
    return (int)len;
}

/* ── Status helpers ──────────────────────────────────────────────── */
uint8_t uart_read_lsr(uart_dev_t *dev)
{
    return reg_read(dev->base, UART_LSR);
}

uint8_t uart_read_msr(uart_dev_t *dev)
{
    return reg_read(dev->base, UART_MSR);
}

uint8_t uart_scratch_rw(uart_dev_t *dev, uint8_t val)
{
    reg_write(dev->base, UART_SCR, val);
    return reg_read(dev->base, UART_SCR);
}

/* ── Break ───────────────────────────────────────────────────────── */
void uart_send_break(uart_dev_t *dev, bool en)
{
    uint8_t lcr = reg_read(dev->base, UART_LCR);
    if (en) lcr |=  LCR_BC;
    else    lcr &= ~LCR_BC;
    reg_write(dev->base, UART_LCR, lcr);
}

/* ── Interrupts ──────────────────────────────────────────────────── */
void uart_enable_irq(uart_dev_t *dev, uint8_t mask)
{
    reg_write(dev->base, UART_IER, mask);
}

uint8_t uart_read_iir(uart_dev_t *dev)
{
    return reg_read(dev->base, UART_IIR);
}
