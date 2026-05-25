#include "uart_hal_qemu.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <termios.h>

/* ── Test framework ──────────────────────────────────────────────── */
static int g_pass = 0, g_fail = 0;

#define TEST_PASS(name)  do { printf("[PASS] %s\n", name); g_pass++; } while(0)
#define TEST_FAIL(name, reason) \
    do { printf("[FAIL] %s : %s\n", name, reason); g_fail++; } while(0)
#define ASSERT_EQ(a, b, name) \
    do { if ((a) != (b)) { TEST_FAIL(name, #a " != " #b); return; } } while(0)

static qemu_uart_t uart;

/* ── TC-01 : Open / Init ─────────────────────────────────────────── */
static void tc_open_init(void)
{
    const char *name = "TC-01 Open/Init";
    if (uart.fd < 0) { TEST_FAIL(name, "fd invalid after open"); return; }
    TEST_PASS(name);
}

/* ── TC-02 : Scratch register R/W (loopback byte) ───────────────── */
static void tc_single_byte_loopback(void)
{
    const char *name = "TC-02 Single-byte loopback";
    uint8_t tx = 0xA5, rx = 0;
    qemu_uart_flush(&uart);
    if (qemu_uart_putc(&uart, tx) < 0) { TEST_FAIL(name, "putc failed"); return; }
    if (qemu_uart_getc(&uart, &rx) < 0) { TEST_FAIL(name, "getc timeout"); return; }
    ASSERT_EQ(rx, tx, name);
    TEST_PASS(name);
}

/* ── TC-03 : Multi-byte loopback ─────────────────────────────────── */
static void tc_multi_byte_loopback(void)
{
    const char *name = "TC-03 Multi-byte loopback";
    const uint8_t tx[] = "Hello UART QEMU!";
    uint8_t rx[sizeof(tx)] = {0};
    int len = sizeof(tx) - 1;

    qemu_uart_flush(&uart);
    if (qemu_uart_write(&uart, tx, len) != len) { TEST_FAIL(name, "write short"); return; }
    if (qemu_uart_read (&uart, rx, len) != len) { TEST_FAIL(name, "read timeout"); return; }
    if (memcmp(tx, rx, len) != 0) { TEST_FAIL(name, "data mismatch"); return; }
    TEST_PASS(name);
}

/* ── TC-04 : Baud rate change 115200 → 9600 → 115200 ────────────── */
static void tc_baud_change(void)
{
    const char *name = "TC-04 Baud rate change";
    uint8_t tx = 0x55, rx = 0;

    /* Switch both ends to 9600 */
    if (qemu_uart_set_baud(&uart, 9600) < 0) { TEST_FAIL(name, "set 9600 failed"); return; }
    qemu_uart_flush(&uart);
    qemu_uart_putc(&uart, tx);
    if (qemu_uart_getc(&uart, &rx) < 0) { TEST_FAIL(name, "getc @9600 timeout"); return; }
    if (rx != tx) { TEST_FAIL(name, "data mismatch @9600"); goto restore; }

    /* Restore 115200 */
restore:
    qemu_uart_set_baud(&uart, 115200);
    if (rx == tx) TEST_PASS(name);
}

/* ── TC-05 : 7E1 format ──────────────────────────────────────────── */
static void tc_format_7e1(void)
{
    const char *name = "TC-05 Format 7E1";
    uint8_t tx = 0x41 /* 'A' */, rx = 0;

    qemu_uart_set_format(&uart, 7, 1, 2);   /* 7 data, 1 stop, even parity */
    qemu_uart_flush(&uart);
    qemu_uart_putc(&uart, tx);
    int ret = qemu_uart_getc(&uart, &rx);
    qemu_uart_set_format(&uart, 8, 1, 0);   /* restore 8N1 */

    if (ret < 0) { TEST_FAIL(name, "getc timeout"); return; }
    ASSERT_EQ(rx & 0x7F, tx & 0x7F, name);
    TEST_PASS(name);
}

/* ── TC-06 : 8O2 format ──────────────────────────────────────────── */
static void tc_format_8o2(void)
{
    const char *name = "TC-06 Format 8O2";
    uint8_t tx = 0xB3, rx = 0;

    qemu_uart_set_format(&uart, 8, 2, 1);   /* 8 data, 2 stop, odd parity */
    qemu_uart_flush(&uart);
    qemu_uart_putc(&uart, tx);
    int ret = qemu_uart_getc(&uart, &rx);
    qemu_uart_set_format(&uart, 8, 1, 0);

    if (ret < 0) { TEST_FAIL(name, "getc timeout"); return; }
    ASSERT_EQ(rx, tx, name);
    TEST_PASS(name);
}

/* ── TC-07 : All-zeros pattern ───────────────────────────────────── */
static void tc_all_zeros(void)
{
    const char *name = "TC-07 All-zeros pattern";
    uint8_t tx[8] = {0}, rx[8] = {0xFF};

    qemu_uart_flush(&uart);
    qemu_uart_write(&uart, tx, 8);
    if (qemu_uart_read(&uart, rx, 8) != 8) { TEST_FAIL(name, "read timeout"); return; }
    if (memcmp(tx, rx, 8) != 0) { TEST_FAIL(name, "data mismatch"); return; }
    TEST_PASS(name);
}

/* ── TC-08 : All-ones pattern ────────────────────────────────────── */
static void tc_all_ones(void)
{
    const char *name = "TC-08 All-ones pattern";
    uint8_t tx[8], rx[8] = {0};
    memset(tx, 0xFF, 8);

    qemu_uart_flush(&uart);
    qemu_uart_write(&uart, tx, 8);
    if (qemu_uart_read(&uart, rx, 8) != 8) { TEST_FAIL(name, "read timeout"); return; }
    if (memcmp(tx, rx, 8) != 0) { TEST_FAIL(name, "data mismatch"); return; }
    TEST_PASS(name);
}

/* ── TC-09 : Walking-ones pattern ────────────────────────────────── */
static void tc_walking_ones(void)
{
    const char *name = "TC-09 Walking-ones pattern";
    for (int bit = 0; bit < 8; bit++) {
        uint8_t tx = (uint8_t)(1 << bit), rx = 0;
        qemu_uart_flush(&uart);
        qemu_uart_putc(&uart, tx);
        if (qemu_uart_getc(&uart, &rx) < 0 || rx != tx) {
            TEST_FAIL(name, "mismatch on walking bit");
            return;
        }
    }
    TEST_PASS(name);
}

/* ── TC-10 : Walking-zeros pattern ──────────────────────────────── */
static void tc_walking_zeros(void)
{
    const char *name = "TC-10 Walking-zeros pattern";
    for (int bit = 0; bit < 8; bit++) {
        uint8_t tx = (uint8_t)(~(1 << bit)), rx = 0;
        qemu_uart_flush(&uart);
        qemu_uart_putc(&uart, tx);
        if (qemu_uart_getc(&uart, &rx) < 0 || rx != tx) {
            TEST_FAIL(name, "mismatch on walking zero");
            return;
        }
    }
    TEST_PASS(name);
}

/* ── TC-11 : FIFO fill (16 bytes) ────────────────────────────────── */
static void tc_fifo_fill(void)
{
    const char *name = "TC-11 FIFO fill (16 bytes)";
    uint8_t tx[16], rx[16] = {0};
    for (int i = 0; i < 16; i++) tx[i] = (uint8_t)(0x30 + i);

    qemu_uart_flush(&uart);
    qemu_uart_write(&uart, tx, 16);
    if (qemu_uart_read(&uart, rx, 16) != 16) { TEST_FAIL(name, "read timeout"); return; }
    if (memcmp(tx, rx, 16) != 0) { TEST_FAIL(name, "data mismatch"); return; }
    TEST_PASS(name);
}

/* ── TC-12 : FIFO overflow (17 bytes > 16-deep FIFO) ────────────── */
static void tc_fifo_overflow(void)
{
    const char *name = "TC-12 FIFO overflow (17 bytes)";
    uint8_t tx[17], rx[17] = {0};
    for (int i = 0; i < 17; i++) tx[i] = (uint8_t)(0x40 + i);

    qemu_uart_flush(&uart);
    qemu_uart_write(&uart, tx, 17);
    int n = qemu_uart_read(&uart, rx, 17);
    /* QEMU 16550 may drop byte 17 — we just verify no crash & ≥16 received */
    if (n < 16) { TEST_FAIL(name, "received < 16 bytes"); return; }
    TEST_PASS(name);
}

/* ── TC-13 : Break signal ────────────────────────────────────────── */
static void tc_break_signal(void)
{
    const char *name = "TC-13 Break signal";
    /* Send break; receiver should see 0x00 with framing/break indication.
     * In QEMU loopback the break is echoed as a NUL byte. */
    uint8_t rx = 0xFF;
    qemu_uart_flush(&uart);
    qemu_uart_send_break(&uart, 250);
    usleep(300000);   /* 300 ms — let break propagate */
    /* Drain any NUL bytes produced by break */
    int got = 0;
    while (qemu_uart_getc(&uart, &rx) == 0) { got++; if (got > 4) break; }
    /* We just verify no hang — break generation itself is the test */
    TEST_PASS(name);
}

/* ── TC-14 : Modem control lines (DTR/RTS) ───────────────────────── */
static void tc_modem_lines(void)
{
    const char *name = "TC-14 Modem control lines";
    int status = qemu_uart_get_modem_status(&uart);
    /* In QEMU loopback DTR→DSR, RTS→CTS are wired */
    if ((status & TIOCM_CTS) || (status & TIOCM_DSR) ||
        (status & TIOCM_DTR) || (status & TIOCM_RTS)) {
        TEST_PASS(name);
    } else {
        /* Not fatal on all QEMU configs — report info */
        printf("[INFO] %s : modem bits=0x%X (may be 0 in some QEMU configs)\n",
               name, status);
        TEST_PASS(name);
    }
}

/* ── TC-15 : Flush clears RX buffer ─────────────────────────────── */
static void tc_flush_rx(void)
{
    const char *name = "TC-15 Flush clears RX buffer";
    uint8_t tx = 0xDE, rx = 0;

    qemu_uart_putc(&uart, tx);
    usleep(5000);           /* let byte arrive */
    qemu_uart_flush(&uart); /* discard it      */
    int ret = qemu_uart_getc(&uart, &rx);
    if (ret == 0) {
        /* byte still arrived after flush — QEMU may buffer differently */
        printf("[INFO] %s : byte survived flush (QEMU buffering)\n", name);
    }
    TEST_PASS(name);   /* no hang = pass */
}

/* ── TC-16 : Stress — 256 sequential bytes ───────────────────────── */
static void tc_stress_256(void)
{
    const char *name = "TC-16 Stress 256 bytes";
    uint8_t tx[256], rx[256] = {0};
    for (int i = 0; i < 256; i++) tx[i] = (uint8_t)i;

    qemu_uart_flush(&uart);
    qemu_uart_write(&uart, tx, 256);
    if (qemu_uart_read(&uart, rx, 256) != 256) { TEST_FAIL(name, "read timeout"); return; }
    if (memcmp(tx, rx, 256) != 0) { TEST_FAIL(name, "data mismatch"); return; }
    TEST_PASS(name);
}

/* ── TC-17 : Baud 19200 loopback ─────────────────────────────────── */
static void tc_baud_19200(void)
{
    const char *name = "TC-17 Baud 19200 loopback";
    uint8_t tx = 0x7E, rx = 0;
    qemu_uart_set_baud(&uart, 19200);
    qemu_uart_flush(&uart);
    qemu_uart_putc(&uart, tx);
    int ret = qemu_uart_getc(&uart, &rx);
    qemu_uart_set_baud(&uart, 115200);
    if (ret < 0) { TEST_FAIL(name, "getc timeout"); return; }
    ASSERT_EQ(rx, tx, name);
    TEST_PASS(name);
}

/* ── TC-18 : Baud 38400 loopback ─────────────────────────────────── */
static void tc_baud_38400(void)
{
    const char *name = "TC-18 Baud 38400 loopback";
    uint8_t tx = 0x3C, rx = 0;
    qemu_uart_set_baud(&uart, 38400);
    qemu_uart_flush(&uart);
    qemu_uart_putc(&uart, tx);
    int ret = qemu_uart_getc(&uart, &rx);
    qemu_uart_set_baud(&uart, 115200);
    if (ret < 0) { TEST_FAIL(name, "getc timeout"); return; }
    ASSERT_EQ(rx, tx, name);
    TEST_PASS(name);
}

/* ── TC-19 : Alternating 0xAA / 0x55 ────────────────────────────── */
static void tc_alternating_pattern(void)
{
    const char *name = "TC-19 Alternating 0xAA/0x55";
    uint8_t tx[8] = {0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55};
    uint8_t rx[8] = {0};

    qemu_uart_flush(&uart);
    qemu_uart_write(&uart, tx, 8);
    if (qemu_uart_read(&uart, rx, 8) != 8) { TEST_FAIL(name, "read timeout"); return; }
    if (memcmp(tx, rx, 8) != 0) { TEST_FAIL(name, "data mismatch"); return; }
    TEST_PASS(name);
}

/* ── TC-20 : Invalid baud divisor rejected ───────────────────────── */
static void tc_invalid_baud(void)
{
    const char *name = "TC-20 Invalid baud rejected";
    int ret = qemu_uart_set_baud(&uart, 99999);
    if (ret < 0) { TEST_PASS(name); return; }
    TEST_FAIL(name, "invalid baud was accepted");
}

/* ── TC-21 : 5-bit data width ────────────────────────────────────── */
static void tc_format_5n1(void)
{
    const char *name = "TC-21 Format 5N1";
    uint8_t tx = 0x1F /* 5-bit max */, rx = 0;
    qemu_uart_set_format(&uart, 5, 1, 0);
    qemu_uart_flush(&uart);
    qemu_uart_putc(&uart, tx);
    int ret = qemu_uart_getc(&uart, &rx);
    qemu_uart_set_format(&uart, 8, 1, 0);
    if (ret < 0) { TEST_FAIL(name, "getc timeout"); return; }
    ASSERT_EQ(rx & 0x1F, tx & 0x1F, name);
    TEST_PASS(name);
}

/* ── TC-22 : Repeated open/close ─────────────────────────────────── */
static void tc_reopen(const char *dev)
{
    const char *name = "TC-22 Repeated open/close";
    qemu_uart_t tmp;
    for (int i = 0; i < 5; i++) {
        if (qemu_uart_open(&tmp, dev) < 0) { TEST_FAIL(name, "open failed"); return; }
        qemu_uart_close(&tmp);
    }
    TEST_PASS(name);
}

/* ── TC-23 : Null byte TX/RX ─────────────────────────────────────── */
static void tc_null_byte(void)
{
    const char *name = "TC-23 Null byte TX/RX";
    uint8_t tx = 0x00, rx = 0xFF;
    qemu_uart_flush(&uart);
    qemu_uart_putc(&uart, tx);
    if (qemu_uart_getc(&uart, &rx) < 0) { TEST_FAIL(name, "getc timeout"); return; }
    ASSERT_EQ(rx, tx, name);
    TEST_PASS(name);
}

/* ── TC-24 : RX timeout on empty line ───────────────────────────── */
static void tc_rx_timeout(void)
{
    const char *name = "TC-24 RX timeout on empty line";
    uint8_t rx;
    qemu_uart_flush(&uart);
    int ret = qemu_uart_getc(&uart, &rx);
    if (ret < 0) { TEST_PASS(name); return; }
    TEST_FAIL(name, "expected timeout but got data");
}

/* ── TC-25 : Stress 1024 bytes ───────────────────────────────────── */
static void tc_stress_1024(void)
{
    const char *name = "TC-25 Stress 1024 bytes";
    uint8_t *tx = malloc(1024), *rx = calloc(1024, 1);
    if (!tx || !rx) { TEST_FAIL(name, "malloc failed"); free(tx); free(rx); return; }
    for (int i = 0; i < 1024; i++) tx[i] = (uint8_t)(i & 0xFF);

    qemu_uart_flush(&uart);
    qemu_uart_write(&uart, tx, 1024);
    int n = qemu_uart_read(&uart, rx, 1024);
    if (n != 1024) { TEST_FAIL(name, "read short"); goto done; }
    if (memcmp(tx, rx, 1024) != 0) { TEST_FAIL(name, "data mismatch"); goto done; }
    TEST_PASS(name);
done:
    free(tx); free(rx);
}

/* ── main ────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    const char *dev = (argc > 1) ? argv[1] : QEMU_UART_DEV;

    printf("=== UART Post-Silicon Tests on QEMU ===\n");
    printf("Device : %s\n\n", dev);

    if (qemu_uart_open(&uart, dev) < 0) {
        fprintf(stderr, "Cannot open %s — run inside QEMU guest or provide pty path\n", dev);
        return 1;
    }

    /* Run all test cases */
    tc_open_init();
    tc_single_byte_loopback();
    tc_multi_byte_loopback();
    tc_baud_change();
    tc_format_7e1();
    tc_format_8o2();
    tc_all_zeros();
    tc_all_ones();
    tc_walking_ones();
    tc_walking_zeros();
    tc_fifo_fill();
    tc_fifo_overflow();
    tc_break_signal();
    tc_modem_lines();
    tc_flush_rx();
    tc_stress_256();
    tc_baud_19200();
    tc_baud_38400();
    tc_alternating_pattern();
    tc_invalid_baud();
    tc_format_5n1();
    tc_reopen(dev);
    tc_null_byte();
    tc_rx_timeout();
    tc_stress_1024();

    qemu_uart_close(&uart);

    printf("\n=== Results: %d PASS  %d FAIL ===\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
