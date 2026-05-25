# Q33: GPU Health Monitoring and Thermal Throttle

**Section:** System Design | **Difficulty:** Hard | **Topics:** `delayed_work`, health polling, ECC, thermal throttle, bad page retirement, `notifier_block`

---

## Question

Design a GPU health monitoring system with ECC error tracking, thermal throttling, and bad page retirement.

---

## Answer

```c
#include <linux/workqueue.h>
#include <linux/thermal.h>
#include <linux/bitops.h>
#include <linux/atomic.h>
#include <linux/slab.h>

#define GPU_HEALTH_POLL_MS    1000  /* poll every 1 second          */
#define GPU_TEMP_THROTTLE_C   85    /* throttle at 85°C             */
#define GPU_TEMP_CRITICAL_C   95    /* emergency shutdown at 95°C   */
#define GPU_ECC_RETIRE_THRESH 10    /* retire page after 10 errors  */

/* ─── ECC Error Counters ──────────────────────────────────────────────────*/
struct gpu_ecc_counters {
    atomic_long_t  single_bit_errors;  /* correctable (SECE) */
    atomic_long_t  double_bit_errors;  /* uncorrectable (DECE — fatal) */
    atomic_long_t  retired_pages;      /* pages removed from use */
    spinlock_t     bad_page_lock;
    struct list_head bad_pages;        /* list of retired page addresses */
};

struct bad_page_entry {
    u64              phys_addr;
    u32              error_count;
    struct list_head list;
};

/* ─── GPU Health State ────────────────────────────────────────────────────*/
struct gpu_health {
    struct delayed_work     poll_work;   /* periodic health check */
    struct workqueue_struct *health_wq;
    struct gpu_ecc_counters  ecc;
    atomic_t                 temp_c;     /* current temperature in °C */
    atomic_t                 throttle_pct; /* clock throttle % (0=none) */
    atomic_t                 health_flags; /* bitmask of active issues */
    struct gpu_device        *gpu;
};

/* Health flag bits */
#define GPU_HEALTH_THERMAL_THROTTLE  BIT(0)
#define GPU_HEALTH_ECC_DOUBLE_BIT    BIT(1)
#define GPU_HEALTH_PAGE_RETIRED      BIT(2)
#define GPU_HEALTH_CRITICAL_TEMP     BIT(3)
#define GPU_HEALTH_XBAR_ERROR        BIT(4)

/* ─── Read GPU temperature via MMIO ──────────────────────────────────────*/
static int gpu_read_temperature(struct gpu_device *gpu)
{
    u32 raw = readl(gpu->regs + NV_THERM_TEMPERATURE_REG);
    /* Convert hardware encoding to Celsius (device-specific formula) */
    return (raw & 0x1FF) / 2; /* simplified: divide by 2 */
}

/* ─── Read ECC error counters from GPU MMIO ──────────────────────────────*/
static void gpu_read_ecc(struct gpu_device *gpu,
                           struct gpu_ecc_counters *ecc,
                           u64 *new_addr)
{
    u32 sece = readl(gpu->regs + NV_ECC_SECE_COUNT_REG);
    u32 dece = readl(gpu->regs + NV_ECC_DECE_COUNT_REG);

    atomic_long_add(sece, &ecc->single_bit_errors);
    atomic_long_add(dece, &ecc->double_bit_errors);

    /* Read the physical address of the most recent ECC error */
    *new_addr = ((u64)readl(gpu->regs + NV_ECC_ADDR_HI_REG) << 32) |
                 readl(gpu->regs + NV_ECC_ADDR_LO_REG);

    /* Acknowledge (clear) counters */
    writel(0, gpu->regs + NV_ECC_SECE_COUNT_REG);
    writel(0, gpu->regs + NV_ECC_DECE_COUNT_REG);
}

/* ─── Retire a bad VRAM page ──────────────────────────────────────────────
 * Marks the page as unusable. The GPU allocator skips retired pages.
 * In production: persist to NVRAM/EEPROM so page stays retired across reboot.
 */
static void gpu_retire_page(struct gpu_health *health, u64 phys_addr)
{
    struct bad_page_entry *entry;
    struct bad_page_entry *existing;
    bool found = false;

    spin_lock(&health->ecc.bad_page_lock);

    /* Check if already retired */
    list_for_each_entry(existing, &health->ecc.bad_pages, list) {
        if (existing->phys_addr == phys_addr) {
            existing->error_count++;
            found = true;
            break;
        }
    }

    if (!found) {
        entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
        if (entry) {
            entry->phys_addr   = phys_addr;
            entry->error_count = 1;
            list_add(&entry->list, &health->ecc.bad_pages);
            atomic_long_inc(&health->ecc.retired_pages);
        }
    }

    spin_unlock(&health->ecc.bad_page_lock);

    /* Mark in gen_pool: remove this page from the VRAM allocator */
    gen_pool_free(health->gpu->vram_pool, phys_addr, GPU_PAGE_SIZE);

    atomic_or(GPU_HEALTH_PAGE_RETIRED, &health->health_flags);
    pr_warn("GPU: retired VRAM page at PA=0x%llx (total retired: %ld)\n",
            phys_addr, atomic_long_read(&health->ecc.retired_pages));
}

/* ─── Apply thermal throttle ──────────────────────────────────────────────*/
static void gpu_apply_thermal_throttle(struct gpu_health *health, int temp)
{
    int throttle_pct;

    if (temp >= GPU_TEMP_CRITICAL_C) {
        /* Emergency: reduce to minimum frequency (10%) */
        throttle_pct = 90;
        atomic_or(GPU_HEALTH_CRITICAL_TEMP, &health->health_flags);
        pr_crit("GPU: CRITICAL temperature %d°C! Emergency throttle.\n", temp);
        /* Trigger GPU emergency shutdown sequence */
        schedule_work(&health->gpu->emergency_shutdown_work);
    } else if (temp >= GPU_TEMP_THROTTLE_C) {
        /* Linear throttle: 0% at 85°C, 50% at 90°C */
        throttle_pct = (temp - GPU_TEMP_THROTTLE_C) * 10;
        throttle_pct = min(throttle_pct, 50);
        atomic_or(GPU_HEALTH_THERMAL_THROTTLE, &health->health_flags);
    } else {
        /* Temperature OK — remove throttle */
        throttle_pct = 0;
        atomic_andnot(GPU_HEALTH_THERMAL_THROTTLE, &health->health_flags);
    }

    if (throttle_pct != atomic_read(&health->throttle_pct)) {
        atomic_set(&health->throttle_pct, throttle_pct);
        /* Write new clock frequency to GPU MMIO */
        gpu_set_clock_throttle(health->gpu, throttle_pct);
    }
}

/* ─── Health poll function ────────────────────────────────────────────────
 * Runs every GPU_HEALTH_POLL_MS milliseconds via delayed_work.
 */
static void gpu_health_poll(struct work_struct *work)
{
    struct gpu_health *health =
        container_of(to_delayed_work(work), struct gpu_health, poll_work);
    struct gpu_device *gpu = health->gpu;
    int temp;
    u64 ecc_addr;

    /* ── 1. Read and process temperature ──────────────────────────────── */
    temp = gpu_read_temperature(gpu);
    atomic_set(&health->temp_c, temp);
    gpu_apply_thermal_throttle(health, temp);

    /* ── 2. Read and process ECC errors ───────────────────────────────── */
    gpu_read_ecc(gpu, &health->ecc, &ecc_addr);

    if (atomic_long_read(&health->ecc.double_bit_errors) > 0) {
        /* Double-bit = uncorrectable — page must be retired immediately */
        atomic_or(GPU_HEALTH_ECC_DOUBLE_BIT, &health->health_flags);
        gpu_retire_page(health, ecc_addr);
    }

    if (atomic_long_read(&health->ecc.single_bit_errors) > GPU_ECC_RETIRE_THRESH) {
        /* Too many single-bit errors on same page — proactively retire */
        gpu_retire_page(health, ecc_addr);
        atomic_long_set(&health->ecc.single_bit_errors, 0);
    }

    /* ── 3. Reschedule for next poll ───────────────────────────────────── */
    queue_delayed_work(health->health_wq, &health->poll_work,
                        msecs_to_jiffies(GPU_HEALTH_POLL_MS));
}

/* ─── Initialization ──────────────────────────────────────────────────────*/
int gpu_health_init(struct gpu_health *health, struct gpu_device *gpu)
{
    health->gpu      = gpu;
    health->health_wq = alloc_workqueue("gpu_health",
                                         WQ_UNBOUND | WQ_FREEZABLE, 1);
    if (!health->health_wq)
        return -ENOMEM;

    INIT_DELAYED_WORK(&health->poll_work, gpu_health_poll);
    spin_lock_init(&health->ecc.bad_page_lock);
    INIT_LIST_HEAD(&health->ecc.bad_pages);

    /* Start polling */
    queue_delayed_work(health->health_wq, &health->poll_work,
                        msecs_to_jiffies(GPU_HEALTH_POLL_MS));
    return 0;
}
```

---

## Explanation

### Core Concept

```
  ┌──────────────────────────────────────────────────┐
  │           GPU Health Monitor                     │
  │                                                  │
  │  delayed_work ─────── 1s ─────► health_poll_fn  │
  │                                       │          │
  │                    ┌──────────────────┤          │
  │                    ▼                  ▼          │
  │            Temperature Read    ECC Error Read    │
  │                    │                  │          │
  │                    ▼                  ▼          │
  │            Thermal Throttle   Retire Bad Page    │
  │            (clock reduction)  (gen_pool remove)  │
  │                    │                  │          │
  │                    ▼                  ▼          │
  │            Alert (health_flags)   nvidia-smi     │
  └──────────────────────────────────────────────────┘
```

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `INIT_DELAYED_WORK(work, fn)` | Initialize delayed work item |
| `queue_delayed_work(wq, work, delay)` | Schedule work after `delay` jiffies |
| `to_delayed_work(work)` | Get `delayed_work` from embedded `work_struct` |
| `alloc_workqueue("name", flags, max)` | Create workqueue for health monitoring |
| `WQ_FREEZABLE` | Pause work during system suspend |
| `WQ_UNBOUND` | Run on any CPU (not bound to specific core) |
| `atomic_or(bits, v)` | Set health flag bits atomically |
| `atomic_andnot(bits, v)` | Clear health flag bits atomically |
| `gen_pool_free(pool, addr, size)` | Remove bad page from allocator |
| `list_for_each_entry` | Scan bad page list |

### Trade-offs & Pitfalls

- **Polling latency.** 1-second polling may be too slow for critical temperature events. Use both: polling (for statistics) + interrupt (for threshold crossings). NVIDIA GPUs fire an interrupt when temperature crosses a threshold, eliminating the 1-second detection delay.
- **ECC address accuracy.** Some GPU generations report only the page-granularity PA, not the exact byte. Retired pages may be larger than the actual error location — wasted VRAM. Report to `nvidia-smi` for user-visible diagnostics.

### NVIDIA / GPU Context

- NVIDIA's NVML (management library) exposes health data via `nvmlDeviceGetEccErrors`, `nvmlDeviceGetTemperature`
- `nvidia-smi` displays retired page count: `nvidia-smi --query-retired-pages=address,type,count`
- In HPC: GPU ECC errors are reported to the job scheduler — jobs running on a GPU with many DBE errors may be rescheduled on a healthy GPU

---

## Cross Questions & Answers

**CQ1: What is the difference between SECE (single-bit error) and DECE (double-bit error) in GPU ECC?**
> SECE (Single-bit Error Correctable Error): one bit flipped in memory — ECC hardware corrects it automatically and the GPU continues running. The data is restored but the page is flagged. DECE (Double-bit Error — Uncorrectable): two bits flipped — ECC cannot correct it, data is corrupt. The GPU typically raises an interrupt and marks the page as bad. Any data on that page is lost. DCEs require immediate action (retire the page, potentially terminate the running GPU context).

**CQ2: How do you persist retired page information across reboots?**
> Retired pages are stored in GPU NVRAM (non-volatile memory on the GPU card, accessible via MMIO). On `pci_probe`, the driver reads the retired page list from NVRAM and excludes those physical addresses from the `gen_pool` VRAM allocator. On shutdown, newly retired pages are written to NVRAM. This persistence ensures retired pages are never reallocated even after a reboot. `nvidia-smi --query-retired-pages` reads this list. If too many pages are retired (> threshold%), NVIDIA marks the GPU as "RMA" (Return Merchandise Authorization).

**CQ3: What is GPU thermal throttling and how does it work at the hardware level?**
> Thermal throttling reduces the GPU clock frequency to lower power dissipation and temperature. The driver writes a new target frequency to the GPU's clock control register (PCLK_MULTIPLIER or equivalent). The GPU's PLLs reduce frequency within microseconds. Modern GPUs also have HW throttle: the GPU autonomously reduces frequency based on an internal thermal sensor, independent of the driver. The driver monitors and reports this HW throttle via the `throttleReasons` bitmask in NVML.

**CQ4: What is `WQ_FREEZABLE` and why should health monitoring workqueues use it?**
> `WQ_FREEZABLE` causes work items in the workqueue to be held during system suspend (freeze phase of PM). Without it, the health poll work would run while the GPU is suspended — reading from MMIO that may be powered down, causing system crashes. With `WQ_FREEZABLE`, `queue_delayed_work` calls are automatically suspended during `freeze_workqueues()` and resumed after `thaw_workqueues()`. The health monitor naturally resumes polling after system resume.

**CQ5: How would you implement an alert escalation system when GPU health degrades?**
> Three-tier escalation: (1) **Log** (`pr_warn`): log ECC errors and temperature to kernel ring buffer — no user impact. (2) **Throttle**: reduce GPU frequency — impacts performance but system keeps running. (3) **Kill**: send `SIGKILL` to processes using the GPU and trigger GPU reset — data loss, but prevents system panic. Implement with atomic health state: `GPU_ALERT` → `GPU_THROTTLE` → `GPU_FATAL`. Notify userspace via `send_sig(SIGBUS, task, 0)` for unrecoverable errors (matches Linux VM behavior for MCE on physical memory errors).
