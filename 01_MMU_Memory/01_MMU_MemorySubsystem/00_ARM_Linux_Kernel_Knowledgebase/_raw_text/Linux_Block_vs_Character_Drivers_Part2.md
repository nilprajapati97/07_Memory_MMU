Linux Block vs Character Drivers
& RAM Disk Implementation
Part 2: Kernel Driver Architecture, Block I/O Subsystem & Complete Implementation
|  |

Linux Kernel Driver Development Series
Covers: Block vs Character Drivers | blk-mq Architecture | RAM Disk Implementation | DMA Considerations
# 1. Core Differences: Block vs Character Drivers
Understanding the difference between block and character drivers is fundamental for Linux kernel development. These two driver types serve fundamentally different purposes and operate at different layers of the kernel I/O stack.
## 1.1 Character Drivers
- undefinedstream of bytes (character by character)
- Access is sequential — data flows in order
- No buffering or caching by the kernel
- Direct, unbuffered I/O operations
- Examples: serial ports (UART), keyboards, mice, sensors, custom devices
## 1.2 Block Drivers
- Handle data in fixed-size blocks (typically 512 bytes, 1KB, 4KB)
- Support random access — can read/write any block
- Kernel provides buffer cache for performance
- Data organized in filesystem-friendly blocks
- Examples: hard drives, SSDs, SD cards, eMMC, USB storage
# 2. Technical Comparison
The table below provides a detailed technical comparison across all key aspects of both driver types:
| Aspect | Character Driver | Block Driver |
| Data Unit | Byte stream | Fixed-size blocks |
| Access Pattern | Sequential | Random access |
| Buffering | None (direct I/O) | Kernel buffer cache |
| File Operations | read(), write(), ioctl(), open(), close() | Request queue, bio structures |
| Device Files | /dev/ttyS0, /dev/input/event0 | /dev/sda, /dev/mmcblk0 |
| Complexity | Simpler to implement | More complex (request queues) |
| Filesystem Support | No | Yes (can mount filesystems) |
| Seeking | Limited or not supported | Full seek support |
| Registration | register_chrdev() | register_blkdev() + add_disk() |
| Main Structure | file_operations | block_device_operations + blk_mq_ops |
| Latency | Lower latency (direct path) | Higher (I/O scheduler overhead) |


# 3. Implementation Differences
## 3.1 Character Driver Structure
A character driver registers a set of file operations that map directly to system calls. The structure is straightforward — each user-space operation (read, write, ioctl) has a corresponding kernel function:
static struct file_operations char_fops = {
    .owner          = THIS_MODULE,
    .open           = char_open,
    .release        = char_release,
    .read           = char_read,
    .write          = char_write,
    .unlocked_ioctl = char_ioctl,
    .llseek         = char_llseek,
};

// Registration
register_chrdev(major, "mychar", &char_fops);
## 3.2 Block Driver Structure
Block drivers use a fundamentally different interface. Instead of direct file operations, they operate through a request queue where the kernel posts I/O requests that the driver must process:
static struct block_device_operations block_fops = {
    .owner   = THIS_MODULE,
    .open    = block_open,
    .release = block_release,
    .ioctl   = block_ioctl,
    .getgeo  = block_getgeo,
};

// Uses request queue
struct request_queue *queue;
queue = blk_init_queue(block_request, &lock);

// Registration
struct gendisk *disk;
disk = alloc_disk(minors);
# 4. Kernel I/O Flow Architecture
## 4.1 Character Driver Flow
Character drivers provide a direct, minimal path from user space to hardware with very few intermediate kernel layers:
| User Space↓ System Call ↓VFS (Virtual File System)↓ Direct path, minimal overhead ↓Character Driver → Hardware |


ℹ️  NOTE: The character driver flow bypasses the page cache entirely. This gives lower latency but means no kernel-level caching or I/O optimization.
## 4.2 Block Driver Flow
Block drivers go through multiple kernel layers that add overhead but provide significant performance optimizations through caching and I/O scheduling:
| User Space↓ System Call ↓VFS → Page Cache↓ Buffer/Cache Layer ↓Block Layer → I/O Scheduler↓ Request Queue / blk-mq ↓Block Driver → DMA Engine → Hardware |


ℹ️  NOTE: The block driver flow adds latency due to multiple layers but dramatically improves throughput through request merging, scheduling, and kernel caching.
# 5. When to Use Each Driver Type
## 5.1 Use Character Drivers When
- Data is naturally sequential (serial communication, UART)
- No need for filesystem support
- Direct hardware control is needed
- Low-latency requirements are critical
- Working with custom protocols: I2C, SPI devices
- Implementing custom mmap devices (as in our mmap driver)
- Sensor interfaces (ADC, temperature, accelerometer)
## 5.2 Use Block Drivers When
- Building a storage device (persistent data needed)
- Filesystem support is required (ext4, FAT32, etc.)
- Random access to data is needed
- Large data transfers are common
- Kernel caching would benefit performance
- Standard storage interface is required
- Building RAM disks, virtual storage, or NVM devices
## 5.3 Real-World Examples (ARM/Qualcomm IoT SoCs)
In Qualcomm IoT SoC development, both driver types appear at different layers of the system:
| Character Drivers | Block Drivers |
| UART drivers for console/debug | eMMC/UFS storage drivers |
| I2C sensor drivers | SD card drivers |
| SPI flash (when not using MTD) | NAND flash with MTD layer |
| GPIO drivers | RAM disk drivers |
| Custom hardware interfaces | Virtual block devices |


# 6. RAM Disk Block Driver — Complete Implementation
The following is a complete RAM disk block driver that demonstrates all key concepts of block drivers. This driver creates a block device backed by RAM that can be formatted with a filesystem and mounted like a real disk.
| Driver SpecificationsDevice Name: sramdisk | Capacity: 16 MB | Sector Size: 512 bytesI/O Interface: blk-mq (Multi-Queue Block Layer — modern Linux kernel)Memory: vmalloc() for large virtually-contiguous allocation |


## 6.1 Kernel Module: simple_ramdisk.c
/*
 * simple_ramdisk.c - A simple RAM disk block driver
 *
 * Creates a block device backed by RAM that can be
 * formatted with a filesystem and mounted.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/genhd.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

#define RAMDISK_NAME   "sramdisk"
#define RAMDISK_SIZE   (16 * 1024 * 1024)  // 16MB
#define SECTOR_SIZE    512
#define RAMDISK_MINORS 1

/* Device structure */
struct ramdisk_device {
    unsigned long         size;   /* Device size in bytes */
    u8                   *data;   /* The data array        */
    spinlock_t            lock;   /* For mutual exclusion  */
    struct request_queue *queue;  /* The device req queue  */
    struct gendisk       *gd;     /* The gendisk structure */
};

static struct ramdisk_device *ramdisk_dev = NULL;
static int ramdisk_major = 0;
### 6.1.1 Data Transfer Function
The core data transfer function performs bounds checking and then copies data between the RAM buffer and the caller's buffer:
static void ramdisk_transfer(struct ramdisk_device *dev,
                             sector_t sector,
                             unsigned long nsect,
                             char *buffer,
                             int write)
{
    unsigned long offset = sector * SECTOR_SIZE;
    unsigned long nbytes = nsect  * SECTOR_SIZE;

    /* Check for out of bounds access */
    if ((offset + nbytes) > dev->size) {
        pr_err("ramdisk: Beyond-end write (%ld %ld)\n",
               offset, nbytes);
        return;
    }

    if (write)
        memcpy(dev->data + offset, buffer, nbytes);  /* Write */
    else
        memcpy(buffer, dev->data + offset, nbytes);  /* Read  */
}
### 6.1.2 BIO Request Handler
The bio (block I/O) handler iterates over all segments in an incoming bio request using the kernel's bio_for_each_segment iterator:
static int ramdisk_handle_bio(struct ramdisk_device *dev,
                              struct bio *bio)
{
    struct bio_vec  bvec;
    struct bvec_iter iter;
    sector_t sector = bio->bi_iter.bi_sector;
    int      write  = bio_data_dir(bio);

    /* Iterate over all segments in the bio */
    bio_for_each_segment(bvec, bio, iter) {
        char         *buffer = kmap_atomic(bvec.bv_page);
        unsigned long offset = bvec.bv_offset;
        unsigned int  len    = bvec.bv_len;

        ramdisk_transfer(dev, sector, len / SECTOR_SIZE,
                         buffer + offset, write);

        kunmap_atomic(buffer);
        sector += len / SECTOR_SIZE;
    }
    return 0;
}
### 6.1.3 blk-mq Queue Handler
The blk-mq queue handler is the heart of the modern block driver. It is called by the block layer for each request, processes all associated bios, and signals completion:
static blk_status_t ramdisk_queue_rq(
                       struct blk_mq_hw_ctx *hctx,
                       const struct blk_mq_queue_data *bd)
{
    struct request       *req = bd->rq;
    struct ramdisk_device *dev = req->q->queuedata;
    struct bio           *bio;

    blk_mq_start_request(req);         /* Start the request */

    /* Process each bio in the request */
    __rq_for_each_bio(bio, req)
        ramdisk_handle_bio(dev, bio);

    blk_mq_end_request(req, BLK_STS_OK); /* Signal completion */
    return BLK_STS_OK;
}

/* blk-mq operations table */
static struct blk_mq_ops ramdisk_mq_ops = {
    .queue_rq = ramdisk_queue_rq,
};
### 6.1.4 Block Device Operations
Block device operations handle device lifecycle events. The getgeo function provides fake disk geometry required for tools that still expect CHS addressing:
static int ramdisk_open(struct block_device *bdev, fmode_t mode)
{
    pr_info("ramdisk: Device opened\n");
    return 0;
}

static void ramdisk_release(struct gendisk *gd, fmode_t mode)
{
    pr_info("ramdisk: Device closed\n");
}

static int ramdisk_getgeo(struct block_device *bdev,
                          struct hd_geometry *geo)
{
    /* Fake geometry: heads=4, sectors=16 */
    geo->heads     = 4;
    geo->sectors   = 16;
    geo->cylinders = (RAMDISK_SIZE / SECTOR_SIZE)
                     / (geo->heads * geo->sectors);
    geo->start     = 0;
    return 0;
}

static const struct block_device_operations ramdisk_fops = {
    .owner   = THIS_MODULE,
    .open    = ramdisk_open,
    .release = ramdisk_release,
    .getgeo  = ramdisk_getgeo,
};
### 6.1.5 Device Setup Function
The setup function allocates memory, initialises the blk-mq tag set, creates the request queue, and finally registers the gendisk with the kernel:
static int ramdisk_setup_device(struct ramdisk_device *dev)
{
    struct blk_mq_tag_set *tag_set;

    /* Allocate 16MB RAM for the disk */
    dev->size = RAMDISK_SIZE;
    dev->data = vmalloc(dev->size);
    if (!dev->data) {
        pr_err("ramdisk: Failed to allocate memory\n");
        return -ENOMEM;
    }
    memset(dev->data, 0, dev->size);
    spin_lock_init(&dev->lock);

    /* Allocate and configure blk-mq tag set */
    tag_set = kzalloc(sizeof(*tag_set), GFP_KERNEL);
    if (!tag_set) { vfree(dev->data); return -ENOMEM; }

    tag_set->ops           = &ramdisk_mq_ops;
    tag_set->nr_hw_queues  = 1;
    tag_set->queue_depth   = 128;
    tag_set->numa_node     = NUMA_NO_NODE;
    tag_set->cmd_size      = 0;
    tag_set->flags         = BLK_MQ_F_SHOULD_MERGE;
    tag_set->driver_data   = dev;

    if (blk_mq_alloc_tag_set(tag_set)) {
        pr_err("ramdisk: Failed to allocate tag set\n");
        kfree(tag_set); vfree(dev->data); return -ENOMEM;
    }

    /* Initialise the request queue */
    dev->queue = blk_mq_init_queue(tag_set);
    if (IS_ERR(dev->queue)) {
        blk_mq_free_tag_set(tag_set);
        kfree(tag_set); vfree(dev->data);
        return PTR_ERR(dev->queue);
    }
    dev->queue->queuedata = dev;

    /* Allocate and configure the gendisk */
    dev->gd = alloc_disk(RAMDISK_MINORS);
    if (!dev->gd) {
        blk_cleanup_queue(dev->queue);
        blk_mq_free_tag_set(tag_set);
        kfree(tag_set); vfree(dev->data); return -ENOMEM;
    }

    dev->gd->major        = ramdisk_major;
    dev->gd->first_minor  = 0;
    dev->gd->fops         = &ramdisk_fops;
    dev->gd->queue        = dev->queue;
    dev->gd->private_data = dev;
    snprintf(dev->gd->disk_name, 32, RAMDISK_NAME);
    set_capacity(dev->gd, RAMDISK_SIZE / SECTOR_SIZE);

    add_disk(dev->gd);  /* Make device visible to system */
    pr_info("ramdisk: Device registered, %d MB\n",
            RAMDISK_SIZE / (1024 * 1024));
    return 0;
}
### 6.1.6 Module Init and Exit
static int __init ramdisk_init(void)
{
    int ret;
    pr_info("ramdisk: Initializing\n");

    ramdisk_major = register_blkdev(0, RAMDISK_NAME);
    if (ramdisk_major < 0) {
        pr_err("ramdisk: Failed to register\n");
        return ramdisk_major;
    }
    pr_info("ramdisk: major = %d\n", ramdisk_major);

    ramdisk_dev = kzalloc(sizeof(struct ramdisk_device),
                          GFP_KERNEL);
    if (!ramdisk_dev) {
        unregister_blkdev(ramdisk_major, RAMDISK_NAME);
        return -ENOMEM;
    }

    ret = ramdisk_setup_device(ramdisk_dev);
    if (ret) {
        kfree(ramdisk_dev);
        unregister_blkdev(ramdisk_major, RAMDISK_NAME);
        return ret;
    }

    pr_info("ramdisk: Module loaded\n");
    return 0;
}

static void __exit ramdisk_exit(void)
{
    struct ramdisk_device *dev = ramdisk_dev;

    if (dev->gd)   { del_gendisk(dev->gd); put_disk(dev->gd); }
    if (dev->queue)  blk_cleanup_queue(dev->queue);
    if (dev->data)   vfree(dev->data);
    kfree(dev);
    unregister_blkdev(ramdisk_major, RAMDISK_NAME);
    pr_info("ramdisk: Module unloaded\n");
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sandeep");
MODULE_DESCRIPTION("Simple RAM Disk Block Driver");
MODULE_VERSION("1.0");
# 7. Build System — Makefile
The Makefile builds both the kernel module and the test script, and provides convenient targets for installation, testing and log inspection:
# Makefile for simple RAM disk block driver
obj-m += simple_ramdisk.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.order *.symvers

install:
	sudo insmod simple_ramdisk.ko
	@echo "Module loaded. Device at /dev/sramdisk"
	@sleep 1
	@ls -l /dev/sramdisk 2>/dev/null || echo "Waiting..."

uninstall:
	@if mount | grep -q sramdisk; then \
		echo "Unmounting..."; sudo umount /dev/sramdisk; \
	fi
	sudo rmmod simple_ramdisk

test: install
	@chmod +x test_ramdisk.sh
	@./test_ramdisk.sh

logs:
	dmesg | grep ramdisk | tail -20

info:
	@echo "=== RAM Disk Information ==="
	@if lsmod | grep -q simple_ramdisk; then \
		echo "Status: Loaded"; \
		lsblk /dev/sramdisk 2>/dev/null; \
	else echo "Status: Not loaded"; fi
# 8. Test Script — test_ramdisk.sh
The following bash script exercises the RAM disk driver by creating an ext4 filesystem, mounting, writing, reading, and running performance benchmarks:
#!/bin/bash
# Test script for RAM disk block driver
set -e

DEVICE="/dev/sramdisk"
MOUNT_POINT="/mnt/ramdisk"

echo "=== RAM Disk Block Driver Test ==="

# Check module is loaded
if ! lsmod | grep -q simple_ramdisk; then
    echo "Error: Module not loaded."
    echo "Load with: sudo insmod simple_ramdisk.ko"
    exit 1
fi

echo "* Device $DEVICE found"
lsblk $DEVICE

# Test 1: Create filesystem
echo "Test 1: Creating ext4 filesystem..."
sudo mkfs.ext4 -F $DEVICE
echo "* Filesystem created"

# Test 2: Mount the device
echo "Test 2: Mounting device..."
sudo mkdir -p $MOUNT_POINT
sudo mount $DEVICE $MOUNT_POINT
echo "* Mounted at $MOUNT_POINT"

# Test 3: Write test
echo "Test 3: Writing test files..."
sudo sh -c "echo 'Hello from RAM disk!' > $MOUNT_POINT/test.txt"
sudo sh -c "dd if=/dev/zero of=$MOUNT_POINT/bigfile \
            bs=1M count=5 2>/dev/null"
echo "* Files written"

# Test 4: Read & verify
echo "Test 4: Reading test files..."
sudo cat $MOUNT_POINT/test.txt
sudo ls -lh $MOUNT_POINT/

# Test 5: Performance — write
echo "Test 5: Write performance..."
sudo dd if=/dev/zero of=$MOUNT_POINT/perftest \
        bs=1M count=10 conv=fsync 2>&1 | grep -E "copied|MB/s"

# Test 6: Performance — read
echo "Test 6: Read performance..."
sudo dd if=$MOUNT_POINT/perftest of=/dev/null \
        bs=1M 2>&1 | grep -E "copied|MB/s"

# Cleanup
sudo umount $MOUNT_POINT
echo "* Device unmounted"
echo "=== All tests completed! ==="
echo "Kernel logs: dmesg | tail -30"
echo "Remove module: sudo rmmod simple_ramdisk"
# 9. Build, Load and Usage Instructions
## 9.1 Build the Module
# Build the kernel module
make

# Expected output files
# simple_ramdisk.ko   <-- kernel module binary
# Module.symvers, modules.order
## 9.2 Load and Verify
# Load module
sudo insmod simple_ramdisk.ko

# Verify device creation
ls -l /dev/sramdisk
lsblk /dev/sramdisk

# Check kernel log
dmesg | tail -10
# Expected: "ramdisk: Device registered with 16 MB capacity"
#           "ramdisk: Registered with major number XXX"
## 9.3 Format and Mount
# Create ext4 filesystem on the RAM disk
sudo mkfs.ext4 /dev/sramdisk

# Mount it
sudo mkdir -p /mnt/ramdisk
sudo mount /dev/sramdisk /mnt/ramdisk

# Use it like any disk
echo "Hello World" | sudo tee /mnt/ramdisk/test.txt
ls -l /mnt/ramdisk/
df -h /mnt/ramdisk
## 9.4 Performance Test
# Write performance
sudo dd if=/dev/zero of=/mnt/ramdisk/testfile bs=1M count=10

# Read performance
sudo dd if=/mnt/ramdisk/testfile of=/dev/null bs=1M
## 9.5 Clean Up
# Unmount and remove module
sudo umount /mnt/ramdisk
sudo rmmod simple_ramdisk

# Verify removal
lsmod | grep ramdisk   # Should show nothing
dmesg | tail -5        # Shows cleanup messages
# 10. Advanced Features & ARM SoC Considerations
## 10.1 Adding DMA Support
For real hardware block devices on ARM SoCs, replace vmalloc() with DMA-coherent allocation so the CPU and DMA engine see consistent data:
/* Use dma_alloc_coherent() instead of vmalloc() */
dev->data = dma_alloc_coherent(device, size,
                               &dma_handle,
                               GFP_KERNEL);
/* Free with: */
dma_free_coherent(device, size, dev->data, dma_handle);
## 10.2 Partition Support
To allow the device to be partitioned (like a real disk), increase the minor number count:
/* Increase RAMDISK_MINORS to allow partitions */
#define RAMDISK_MINORS 16
/* sramdisk  = whole disk */
/* sramdisk1 = partition 1, sramdisk2 = partition 2, etc. */
## 10.3 Multiple Device Instances
/* Create array of device instances */
#define NUM_DEVICES 4
static struct ramdisk_device *devices[NUM_DEVICES];

for (i = 0; i < NUM_DEVICES; i++) {
    devices[i] = kzalloc(sizeof(*devices[i]), GFP_KERNEL);
    ramdisk_setup_device(devices[i], i);
}
/* Creates /dev/sramdisk0, /dev/sramdisk1, etc. */
## 10.4 Persistent Storage (Save on Exit)
/* Save RAM disk contents to file on module exit */
struct file *fp = filp_open("/var/ramdisk.img",
                            O_WRONLY | O_CREAT, 0644);
if (!IS_ERR(fp)) {
    kernel_write(fp, dev->data, dev->size, 0);
    filp_close(fp, NULL);
}
## 10.5 TRIM/DISCARD Support
For SSD-backed or virtual storage devices, TRIM support allows the OS to notify the driver of freed blocks:
/* Add to blk_mq_ops */
static struct blk_mq_ops ramdisk_mq_ops = {
    .queue_rq   = ramdisk_queue_rq,
    .commit_rqs = ramdisk_commit_rqs,  /* TRIM support */
};

/* Enable discard in queue setup */
blk_queue_flag_set(QUEUE_FLAG_DISCARD, dev->queue);
blk_queue_discard_granularity(dev->queue, SECTOR_SIZE);
## 10.6 Qualcomm ARM SoC — Additional Considerations
For production block drivers on Qualcomm IoT SoCs (as used in embedded device development), the following kernel subsystems must be addressed:
- DMA Engine Integration: Use Linux DMA engine API (dma_request_channel, dmaengine_prep_slave_sg) for efficient scatter-gather transfers
- Cache Coherency: Explicitly flush/invalidate caches before DMA transfers on non-coherent ARM systems
- Interrupt Completion: Use completion_init() / wait_for_completion() for async transfer notification
- Runtime PM: Implement runtime_pm_get() / runtime_pm_put() to power-gate storage hardware when idle
- Error Handling: Implement retry logic with exponential backoff for transient I/O errors
- I/O Scheduling: Choose appropriate I/O scheduler (none/mq-deadline/bfq) in the device tree or via sysfs
- Power Management: Suspend/resume hooks for system-level sleep states (S2Idle, S2Ram)
# 11. Block Driver vs Character Driver — Implementation Summary
This table summarises the key implementation differences between the custom mmap character driver (Part 1) and the RAM disk block driver (Part 2):
| Aspect | Character Driver (mmap) | Block Driver (RAM Disk) |
| Data Access | Direct byte stream | Block-based (512B sectors) |
| File Ops | read(), write(), mmap() | Request queue handler |
| Registration | register_chrdev() | register_blkdev() + add_disk() |
| Main Structure | file_operations | block_device_operations + blk_mq_ops |
| Buffering | None | Kernel page cache |
| Use Case | Custom mmap / sensor device | Mountable filesystem storage |
| Memory Alloc | alloc_page() per page | vmalloc() (large virtual block) |
| Device Node | /dev/custom_mmap (char) | /dev/sramdisk (block) |
| I/O Interface | VFS direct path | blk-mq multi-queue |


# 12. Key Block Driver Concepts — Recap
## 12.1 Block Layer Architecture
- Uses modern blk-mq (multi-queue) interface instead of legacy single request queue
- Implements blk_mq_ops with a queue_rq handler for per-request processing
- Tag set provides per-hardware-queue request tracking and efficient concurrency
- Hardware queues (hw_ctx) map to physical I/O queues on the device
## 12.2 Request Processing Pipeline
- ramdisk_queue_rq(): Main entry point — called per request by the block layer
- Processes bio (block I/O) structures that describe scatter-gather memory maps
- Iterates through bio segments using bio_for_each_segment() macro
- Handles both READ (bio_data_dir == READ) and WRITE operations
- blk_mq_start_request() / blk_mq_end_request() bookend request processing
## 12.3 Memory Management
- Uses vmalloc() for large contiguous virtual memory (16MB)
- Direct memory copy (memcpy) for data transfer — no DMA in this example
- kmap_atomic() / kunmap_atomic() for safe access to bio page descriptors
- Zero-initialise all pages on allocation for data security (memset)
## 12.4 Gendisk and Partition Management
- gendisk structure represents the entire block device to the kernel
- set_capacity() specifies device size in 512-byte logical sectors
- add_disk() makes the device visible in /sys/block and /dev
- RAMDISK_MINORS controls the minor number space available for partitions

—  End of Part 2  —
Linux Kernel Driver Development Series | Part 2 of 2
