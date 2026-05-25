#ifndef UART_HAL_QEMU_H
#define UART_HAL_QEMU_H

#include <stdint.h>
#include <stdbool.h>
#include <termios.h>

/*
 * QEMU HAL — maps the uart_dev_t API onto a real tty fd (termios).
 * Inside a QEMU guest  : use /dev/ttyS0  (16550A emulated by QEMU)
 * On the host (pty pair): use /dev/pts/N  for loopback self-test
 */

#define QEMU_UART_DEV   "/dev/ttyS0"   /* override with -DQEMU_UART_DEV=... */
#define QEMU_TIMEOUT_MS  2000

typedef struct {
    int             fd;         /* open tty file descriptor           */
    struct termios  saved;      /* original termios (restored on close)*/
    speed_t         speed;      /* current Bxxx speed constant        */
} qemu_uart_t;

/* Lifecycle */
int  qemu_uart_open(qemu_uart_t *q, const char *dev);
void qemu_uart_close(qemu_uart_t *q);

/* Config */
int  qemu_uart_set_baud(qemu_uart_t *q, int baud);
int  qemu_uart_set_format(qemu_uart_t *q, int data_bits,
                           int stop_bits, int parity); /* parity: 0=N,1=O,2=E */
void qemu_uart_set_loopback(qemu_uart_t *q, bool en);  /* TIOCM_LOOP via ioctl */

/* TX / RX */
int  qemu_uart_putc(qemu_uart_t *q, uint8_t ch);
int  qemu_uart_getc(qemu_uart_t *q, uint8_t *ch);      /* returns -1 on timeout */
int  qemu_uart_write(qemu_uart_t *q, const uint8_t *buf, int len);
int  qemu_uart_read (qemu_uart_t *q, uint8_t *buf, int len);

/* Line status helpers */
int  qemu_uart_get_modem_status(qemu_uart_t *q);       /* TIOCMGET bits         */
void qemu_uart_flush(qemu_uart_t *q);
void qemu_uart_send_break(qemu_uart_t *q, int duration_ms);

#endif /* UART_HAL_QEMU_H */
