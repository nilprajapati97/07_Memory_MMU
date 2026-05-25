CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g
TARGET  = uart_tests

# ── QEMU settings ─────────────────────────────────────────────────
QEMU        = qemu-system-x86_64
KERNEL      = bzImage          # path to your guest kernel
ROOTFS      = rootfs.ext4      # path to your guest rootfs
QEMU_SERIAL = /dev/ttyS0       # serial device inside QEMU guest
QEMU_FLAGS  = -nographic \
              -serial mon:stdio \
              -append "console=ttyS0 root=/dev/sda rw" \
              -kernel $(KERNEL) \
              -hda $(ROOTFS) \
              -m 256M

# ── Sources ────────────────────────────────────────────────────────
SRCS = uart_hal_qemu.c uart_tests.c
OBJS = $(SRCS:.c=.o)

# ── Build ──────────────────────────────────────────────────────────
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Run inside QEMU guest (cross-compile then copy via scp/virtfs) ─
qemu-run: all
	@echo "Copy $(TARGET) into QEMU guest and run:"
	@echo "  scp $(TARGET) root@<guest-ip>:/tmp/"
	@echo "  ssh root@<guest-ip> /tmp/$(TARGET) $(QEMU_SERIAL)"

# ── Host loopback test using a pty pair (no real hardware needed) ──
# Opens a socat virtual null-modem, runs tests against one end.
pty-test: all
	@echo "Starting socat pty pair..."
	socat -d -d pty,raw,echo=0 pty,raw,echo=0 &
	@sleep 1
	@PTY=$$(ls /dev/pts/ | sort -n | tail -2 | head -1); \
	 echo "Using /dev/pts/$$PTY"; \
	 ./$(TARGET) /dev/pts/$$PTY; \
	 kill %1 2>/dev/null || true

# ── Launch QEMU (requires kernel + rootfs) ────────────────────────
qemu-launch:
	$(QEMU) $(QEMU_FLAGS)

# ── Clean ─────────────────────────────────────────────────────────
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all qemu-run pty-test qemu-launch clean
