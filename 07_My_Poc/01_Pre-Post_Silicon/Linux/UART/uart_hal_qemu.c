#include "uart_hal_qemu.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>

/* ── open ────────────────────────────────────────────────────────── */
int qemu_uart_open(qemu_uart_t *q, const char *dev)
{
    q->fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (q->fd < 0) { perror("uart open"); return -1; }

    tcgetattr(q->fd, &q->saved);

    struct termios t = q->saved;
    cfmakeraw(&t);
    t.c_cflag |= CLOCAL | CREAD;
    t.c_cc[VMIN]  = 0;
    t.c_cc[VTIME] = 0;
    cfsetispeed(&t, B115200);
    cfsetospeed(&t, B115200);
    q->speed = B115200;

    if (tcsetattr(q->fd, TCSANOW, &t) < 0) { perror("tcsetattr"); return -1; }
    tcflush(q->fd, TCIOFLUSH);
    return 0;
}

/* ── close ───────────────────────────────────────────────────────── */
void qemu_uart_close(qemu_uart_t *q)
{
    if (q->fd >= 0) {
        tcsetattr(q->fd, TCSANOW, &q->saved);
        close(q->fd);
        q->fd = -1;
    }
}

/* ── baud ────────────────────────────────────────────────────────── */
int qemu_uart_set_baud(qemu_uart_t *q, int baud)
{
    speed_t sp;
    switch (baud) {
        case 9600:   sp = B9600;   break;
        case 19200:  sp = B19200;  break;
        case 38400:  sp = B38400;  break;
        case 57600:  sp = B57600;  break;
        case 115200: sp = B115200; break;
        default: fprintf(stderr, "unsupported baud %d\n", baud); return -1;
    }
    struct termios t;
    tcgetattr(q->fd, &t);
    cfsetispeed(&t, sp);
    cfsetospeed(&t, sp);
    q->speed = sp;
    return tcsetattr(q->fd, TCSANOW, &t);
}

/* ── format ──────────────────────────────────────────────────────── */
int qemu_uart_set_format(qemu_uart_t *q, int data_bits,
                          int stop_bits, int parity)
{
    struct termios t;
    tcgetattr(q->fd, &t);

    t.c_cflag &= ~CSIZE;
    switch (data_bits) {
        case 5: t.c_cflag |= CS5; break;
        case 6: t.c_cflag |= CS6; break;
        case 7: t.c_cflag |= CS7; break;
        case 8: t.c_cflag |= CS8; break;
        default: return -1;
    }

    if (stop_bits == 2) t.c_cflag |=  CSTOPB;
    else                t.c_cflag &= ~CSTOPB;

    t.c_cflag &= ~(PARENB | PARODD);
    if (parity == 1) { t.c_cflag |= PARENB | PARODD; }   /* odd  */
    else if (parity == 2) { t.c_cflag |= PARENB; }        /* even */

    return tcsetattr(q->fd, TCSANOW, &t);
}

/* ── loopback ───────────────────────────────────────────────────── */
/* TIOCM_LOOP is non-standard; on QEMU 16550 loopback is done at the
 * register level (MCR bit 4). Here we use the tty local-echo as a
 * best-effort software loopback for host-side pty testing.          */
void qemu_uart_set_loopback(qemu_uart_t *q, bool en)
{
    struct termios t;
    tcgetattr(q->fd, &t);
    if (en) t.c_lflag |=  ECHO;
    else    t.c_lflag &= ~ECHO;
    tcsetattr(q->fd, TCSANOW, &t);
}

/* ── TX ──────────────────────────────────────────────────────────── */
int qemu_uart_putc(qemu_uart_t *q, uint8_t ch)
{
    return (write(q->fd, &ch, 1) == 1) ? 0 : -1;
}

int qemu_uart_write(qemu_uart_t *q, const uint8_t *buf, int len)
{
    int n = write(q->fd, buf, len);
    tcdrain(q->fd);   /* wait until all bytes are physically sent */
    return n;
}

/* ── RX (with timeout) ───────────────────────────────────────────── */
int qemu_uart_getc(qemu_uart_t *q, uint8_t *ch)
{
    fd_set rfds;
    struct timeval tv = { .tv_sec = QEMU_TIMEOUT_MS / 1000,
                          .tv_usec = (QEMU_TIMEOUT_MS % 1000) * 1000 };
    FD_ZERO(&rfds);
    FD_SET(q->fd, &rfds);

    int ret = select(q->fd + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0) return -1;   /* timeout or error */
    return (read(q->fd, ch, 1) == 1) ? 0 : -1;
}

int qemu_uart_read(qemu_uart_t *q, uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++)
        if (qemu_uart_getc(q, &buf[i]) < 0) return i;
    return len;
}

/* ── modem status ────────────────────────────────────────────────── */
int qemu_uart_get_modem_status(qemu_uart_t *q)
{
    int flags = 0;
    ioctl(q->fd, TIOCMGET, &flags);
    return flags;
}

/* ── flush ───────────────────────────────────────────────────────── */
void qemu_uart_flush(qemu_uart_t *q)
{
    tcflush(q->fd, TCIOFLUSH);
}

/* ── break ───────────────────────────────────────────────────────── */
void qemu_uart_send_break(qemu_uart_t *q, int duration_ms)
{
    tcsendbreak(q->fd, duration_ms / 250);   /* POSIX: 0 = ~250 ms */
}
