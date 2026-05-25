# Userspace Access to Peripheral Registers

Peripheral registers are normally only accessible from kernel space (since they live in the physical address space and require MMU mappings with special attributes). To let a **userspace driver** touch them, the kernel must expose that physical memory region into the process's virtual address space.

Here are the main mechanisms, from simplest to most production-grade.

---

## 1. The Core Idea: `mmap()` of Device Memory

A userspace driver does **not** issue raw load/store to a physical address. Instead:

1. Kernel knows the **physical address** of the peripheral (e.g. `0x3F200000` for GPIO on RPi).
2. Kernel creates a **virtual mapping** in the user process with:
   - Page table entries pointing to that physical frame
   - **Device memory attributes** (non-cacheable, non-bufferable, strongly-ordered on ARM)
3. Userspace gets a pointer; normal C dereference becomes an MMIO access.

```
   User process VA          MMU/Page tables          Physical address
   ┌────────────┐           ┌──────────────┐         ┌─────────────────┐
   │ regs = mmap│ ────────► │ PTE (Device, │ ──────► │ 0x3F200000      │
   │ regs[0]=1  │           │  nGnRnE, RW) │         │ GPIO controller │
   └────────────┘           └──────────────┘         └─────────────────┘
```

The CPU instruction is just `STR` / `LDR`, but the MMU attribute ensures it bypasses caches and reaches the device bus.

---

## 2. Common Mechanisms

### a) `/dev/mem` (quick & dirty, root only)
```c
int fd = open("/dev/mem", O_RDWR | O_SYNC);
volatile uint32_t *gpio = mmap(NULL, 4096,
                               PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, 0x3F200000);
gpio[7] = (1 << 21);   // write to GPIO register
```
- `O_SYNC` requests uncached mapping.
- Maps arbitrary physical memory → unsafe, usually disabled (`CONFIG_STRICT_DEVMEM`).

### b) **UIO** (Userspace I/O) — the proper way
The kernel ships a tiny stub driver; all logic lives in userspace.

Kernel side (`drivers/uio/`):
- Driver registers a `struct uio_info` describing memory regions and an IRQ.
- Kernel creates `/dev/uioX` and sysfs entries `/sys/class/uio/uioX/maps/mapN/`.

Userspace:
```c
int fd = open("/dev/uio0", O_RDWR);
void *regs = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

// Wait for hardware interrupt:
uint32_t cnt;
read(fd, &cnt, 4);            // blocks until IRQ fires
// ack/handle, then re-enable:
write(fd, &one, 4);
```
- Kernel handles only the *interrupt top-half* (mask + wake the waiter).
- DMA, register programming, protocol — all in userspace.

### c) **VFIO** (modern, IOMMU-backed)
- Used by DPDK, SPDK, QEMU/VFIO-PCI.
- Requires an **IOMMU** (SMMU on ARM, VT-d on x86).
- Groups devices, gives userspace safe DMA: device can only touch memory the IOMMU has mapped for it.
- Ioctls: `VFIO_GROUP_GET_DEVICE_FD`, `VFIO_DEVICE_GET_REGION_INFO`, then `mmap()` BARs.

### d) `sysfs` PCI resources
```
/sys/bus/pci/devices/0000:01:00.0/resource0
```
`mmap()` this file to get BAR0 in your process.

---

## 3. How the Mapping Actually Works (ARM64)

When the driver's `mmap` handler runs, it typically calls:

```c
static int my_mmap(struct file *f, struct vm_area_struct *vma)
{
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); // Device-nGnRnE
    return remap_pfn_range(vma,
                           vma->vm_start,
                           phys_addr >> PAGE_SHIFT,
                           vma->vm_end - vma->vm_start,
                           vma->vm_page_prot);
}
```

`remap_pfn_range()` installs PTEs with:
- **Memory type** = Device (via MAIR_EL1 index)
- `nG` (non-global), user-accessible (`AP[1]=1`)
- No cache, no speculation, no reordering across these accesses

You already have a stub for this in your workspace: see remap_pfn_range and mmap.

---

## 4. Interrupts in Userspace Drivers

You cannot run an ISR in userspace (interrupts are CPU-level kernel events). The pattern is:

| Step | Where | What happens |
|------|-------|--------------|
| 1 | Hardware | Asserts IRQ line |
| 2 | Kernel | Tiny ISR masks the IRQ, increments event count |
| 3 | Kernel | Wakes any waiter on the UIO/VFIO fd |
| 4 | Userspace | `read()` / `poll()` returns |
| 5 | Userspace driver | Reads status registers, services device |
| 6 | Userspace | `write()` 1 byte to re-enable IRQ in kernel |

---

## 5. DMA from Userspace

Registers are easy; **DMA buffers are the hard part** because the device needs a *bus address*, not a user VA.

- **Without IOMMU (UIO)**: must use `CMA` / `hugepages` / reserved memory, get the physical address, program the device manually. Fragile, no protection.
- **With IOMMU (VFIO)**: userspace calls `VFIO_IOMMU_MAP_DMA` with a user VA + size; kernel programs the SMMU so the device sees a chosen IOVA. Safe — device cannot scribble over arbitrary RAM.

---

## 6. Why Use a Userspace Driver at All?

| Pros | Cons |
|------|------|
| Develop/debug without kernel rebuilds | Higher latency than kernel ISR (context switches) |
| Crash isolation — bug doesn't panic kernel | Need IOMMU for safe DMA |
| Polling-mode possible (DPDK, SPDK get millions of pps) | Kernel still owns IRQ delivery |
| Language freedom (C++, Rust, Go) | Power management & sharing are awkward |
| Easy to license/ship out-of-tree | Root or specific capabilities required |

Typical use cases: **DPDK** (NICs), **SPDK** (NVMe), **FPGA control planes**, **industrial I/O**, prototyping new silicon.

---

## 7. Minimal End-to-End Example (UIO + GPIO toggle)

Device tree:
```dts
gpio_uio: gpio-uio@3f200000 {
    compatible = "generic-uio";
    reg = <0x3f200000 0x1000>;
    interrupts = <...>;
};
```

Userspace:
```c
int fd = open("/dev/uio0", O_RDWR);
size_t sz = 0x1000;
volatile uint32_t *r = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

r[GPFSEL2/4] |= (1 << 3);     // pin 21 as output
while (1) {
    r[GPSET0/4] = 1 << 21;
    usleep(500000);
    r[GPCLR0/4] = 1 << 21;
    usleep(500000);
}
```
No kernel driver code was written — only a DT node binding the region to `uio_pdrv_genirq`.

---

### TL;DR
A userspace driver accesses peripheral registers by asking the kernel (via `/dev/mem`, **UIO**, **VFIO**, or sysfs PCI resources) to `mmap()` the device's physical MMIO region into its address space with **Device memory attributes**. After that, reads/writes through the returned pointer become real bus transactions. Interrupts are delivered as readable events on a file descriptor, and safe DMA requires an **IOMMU** (VFIO).


# Why It's Still Called a "Userspace Driver"

Good question — the name can feel misleading because the kernel is clearly still involved. Here's the precise reasoning.

---

## The Definition of a "Driver"

A **device driver** = the software that **knows how a specific piece of hardware works** and **drives it** (programs its registers, implements its protocol, handles its events).

It is *not* defined by *where* it runs. It's defined by *what it does*.

---

## What Lives Where in a UIO/VFIO Setup

| Responsibility | Kernel side | Userspace side |
|---|---|---|
| Map MMIO region into address space | ✅ generic stub | — |
| Deliver IRQ as an event | ✅ generic stub | — |
| **Know register layout** | ❌ | ✅ |
| **Know init sequence** | ❌ | ✅ |
| **Implement device protocol** (e.g. NVMe queues, Ethernet RX/TX) | ❌ | ✅ |
| **Handle errors, state machine** | ❌ | ✅ |
| **Manage DMA descriptors** | ❌ | ✅ |
| **Expose API to applications** | ❌ | ✅ |

The kernel part is a **generic plumbing stub** (`uio_pdrv_genirq`, `vfio-pci`). It is the *same code* for a GPIO, a NIC, an FPGA, or an NVMe SSD. It has **zero device-specific knowledge**.

All the actual *driver logic* — the part that makes it "a driver for THIS chip" — lives in userspace.

---

## Analogy

Think of the kernel stub as a **power outlet and a doorbell wire**:
- The outlet (mmap) gives you electricity (access to registers).
- The doorbell (IRQ fd) tells you when something happened.

Neither the outlet nor the doorbell is "the appliance driver." The **appliance-specific logic** — knowing it's a microwave vs. a washing machine, what buttons to press, in what order — that's the driver. And in this model, it runs in userspace.

---

## Contrast with a Traditional Kernel Driver

**Kernel driver (e.g. `drivers/net/ethernet/intel/ixgbe/`)**
- Knows every register of the Intel 82599 NIC
- Implements ring buffers, RSS, offloads
- Runs in kernel context, ring 0
- Bug → kernel panic

**Userspace driver (e.g. DPDK `ixgbe` PMD)**
- Knows every register of the Intel 82599 NIC  ← *same knowledge!*
- Implements ring buffers, RSS, offloads      ← *same logic!*
- Runs in a user process, ring 3
- Bug → process crash, kernel survives

Same driver, different residence. That's why the qualifier is needed: **"userspace"** driver vs. **"kernel"** driver.

---

## Why the Kernel Can't Just Be Cut Out

The CPU's privilege model forces *some* kernel involvement:
- Only ring 0 can program the MMU → so kernel must set up the mapping.
- Only ring 0 receives interrupts → so kernel must forward them.
- Only ring 0 can program the IOMMU → so kernel must set up DMA windows.

But once those mappings exist, **every subsequent register access is a plain user-mode `LDR`/`STR`** that never enters the kernel. The hot path is 100% userspace. That's the meaningful sense in which the driver "runs in userspace."

---

## TL;DR

It's called a **userspace driver** because the **device-specific intelligence** — the part that actually qualifies as "a driver" — lives in a user process. The kernel only provides a generic, device-agnostic bridge (mapping + IRQ delivery) that any userspace driver can sit on top of.