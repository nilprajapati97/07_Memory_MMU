#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/* ── Register offsets (16550-compatible) ─────────────────────────── */
#define UART_RBR    0x00   /* Receiver Buffer Register (R)           */
#define UART_THR    0x00   /* Transmitter Holding Register (W)       */
#define UART_DLL    0x00   /* Divisor Latch Low  (DLAB=1)            */
#define UART_DLH    0x04   /* Divisor Latch High (DLAB=1)            */
#define UART_IER    0x04   /* Interrupt Enable Register               */
#define UART_IIR    0x08   /* Interrupt Identification Register (R)  */
#define UART_FCR    0x08   /* FIFO Control Register (W)              */
#define UART_LCR    0x0C   /* Line Control Register                  */
#define UART_MCR    0x10   /* Modem Control Register                 */
#define UART_LSR    0x14   /* Line Status Register                   */
#define UART_MSR    0x18   /* Modem Status Register                  */
#define UART_SCR    0x1C   /* Scratch Register                       */

/* ── LSR bits ────────────────────────────────────────────────────── */
#define LSR_DR      (1 << 0)   /* Data Ready                         */
#define LSR_OE      (1 << 1)   /* Overrun Error                      */
#define LSR_PE      (1 << 2)   /* Parity Error                       */
#define LSR_FE      (1 << 3)   /* Framing Error                      */
#define LSR_BI      (1 << 4)   /* Break Interrupt                    */
#define LSR_THRE    (1 << 5)   /* TX Holding Register Empty          */
#define LSR_TEMT    (1 << 6)   /* Transmitter Empty                  */
#define LSR_RXFE    (1 << 7)   /* RX FIFO Error                      */

/* ── LCR bits ────────────────────────────────────────────────────── */
#define LCR_WLS_5   0x00
#define LCR_WLS_6   0x01
#define LCR_WLS_7   0x02
#define LCR_WLS_8   0x03
#define LCR_STB     (1 << 2)   /* 2 stop bits                        */
#define LCR_PEN     (1 << 3)   /* Parity Enable                      */
#define LCR_EPS     (1 << 4)   /* Even Parity Select                 */
#define LCR_SP      (1 << 5)   /* Stick Parity                       */
#define LCR_BC      (1 << 6)   /* Break Control                      */
#define LCR_DLAB    (1 << 7)   /* Divisor Latch Access Bit           */

/* ── MCR bits ────────────────────────────────────────────────────── */
#define MCR_DTR     (1 << 0)
#define MCR_RTS     (1 << 1)
#define MCR_OUT1    (1 << 2)
#define MCR_OUT2    (1 << 3)
#define MCR_LOOP    (1 << 4)   /* Internal loopback                  */

/* ── FCR bits ────────────────────────────────────────────────────── */
#define FCR_FIFO_EN     (1 << 0)
#define FCR_RX_RST      (1 << 1)
#define FCR_TX_RST      (1 << 2)
#define FCR_TRIG_1      0x00
#define FCR_TRIG_4      0x40
#define FCR_TRIG_8      0x80
#define FCR_TRIG_14     0xC0

/* ── IER bits ────────────────────────────────────────────────────── */
#define IER_ERBFI   (1 << 0)   /* Enable RX interrupt                */
#define IER_ETBEI   (1 << 1)   /* Enable TX empty interrupt          */
#define IER_ELSI    (1 << 2)   /* Enable line status interrupt       */
#define IER_EDSSI   (1 << 3)   /* Enable modem status interrupt      */

/* ── Baud rate divisors (assuming 1.8432 MHz clock) ─────────────── */
#define BAUD_9600    12
#define BAUD_19200    6
#define BAUD_38400    3
#define BAUD_115200   1

#define UART_TIMEOUT_MS  1000
#define UART_FIFO_DEPTH  16

typedef struct {
    uintptr_t   base;       /* MMIO base address                     */
    uint32_t    clock_hz;   /* Input clock frequency                 */
    uint32_t    baud;       /* Current baud divisor                  */
    uint8_t     lcr_val;    /* Cached LCR value                      */
} uart_dev_t;

/* ── API ─────────────────────────────────────────────────────────── */
int  uart_init(uart_dev_t *dev, uintptr_t base, uint32_t clock_hz);
int  uart_set_baud(uart_dev_t *dev, uint32_t divisor);
int  uart_set_format(uart_dev_t *dev, uint8_t data_bits,
                     uint8_t stop_bits, uint8_t parity);
void uart_enable_loopback(uart_dev_t *dev, bool en);
void uart_fifo_enable(uart_dev_t *dev, bool en, uint8_t trig);
int  uart_putc(uart_dev_t *dev, uint8_t ch);
int  uart_getc(uart_dev_t *dev, uint8_t *ch);
int  uart_puts(uart_dev_t *dev, const uint8_t *buf, uint32_t len);
int  uart_gets(uart_dev_t *dev, uint8_t *buf, uint32_t len);
uint8_t uart_read_lsr(uart_dev_t *dev);
uint8_t uart_read_msr(uart_dev_t *dev);
uint8_t uart_scratch_rw(uart_dev_t *dev, uint8_t val);
void uart_send_break(uart_dev_t *dev, bool en);
void uart_enable_irq(uart_dev_t *dev, uint8_t mask);
uint8_t uart_read_iir(uart_dev_t *dev);

#endif /* UART_DRIVER_H */
