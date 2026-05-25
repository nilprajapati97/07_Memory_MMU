Part 1
Kernel Panic & Qualcomm Watchdog Implementation
ARM / ARM64 Embedded Systems — Deep Technical Reference

Qualcomm IoT SoC Platform Engineering


# SECTION 1: KERNEL PANIC
A kernel panic is a fatal, unrecoverable error detected by the OS kernel — the equivalent of a Windows BSOD but for Linux/Unix systems. When the kernel encounters a situation it cannot safely handle, it halts or reboots the system to prevent data corruption.

## 1.1  What a Kernel Panic Looks Like
Below is a complete console output from a classic Linux ARM32 kernel panic caused by a NULL pointer dereference:

| [ 142.318523] Unable to handle kernel NULL pointer dereference at virtual address 00000000 |
| [ 142.318541] pgd = c0004000 |
| [ 142.318548] [00000000] *pgd=00000000 |
| [ 142.318562] Internal error: Oops: 5 [#1] PREEMPT SMP ARM |
| [ 142.318570] Modules linked in: mydriver(O) |
| [ 142.318591] CPU: 0 PID: 1234 Comm: myprocess Tainted: G W O 5.10.0 #1 |
| [ 142.318598] Hardware name: Qualcomm MSM8996 (DT) |
| [ 142.318610] PC is at bad_function+0x14/0x3c [mydriver] |
| [ 142.318618] LR is at caller_function+0x28/0x50 [mydriver] |
| [ 142.318625] pc : [<bf001014>] lr : [<bf002028>] psr: 60000013 |
| [ 142.318632] sp : c9a3be90 ip : 00000000 fp : c9a3bea4 |
| [ 142.318639] r10: 00000000 r9 : c9a3c000 r8 : 00000000 |
| [ 142.318646] r7 : 00000001 r6 : c1234567 r5 : 00000000 r4 : c9a3bec0 |
| [ 142.318653] r3 : 00000000 r2 : 00000010 r1 : c1234567 r0 : 00000000 |
| [ 142.318661] Flags: nZCv IRQs on FIQs on Mode SVC_32 ISA ARM Segment user |
| [ 142.318668] Control: 10c5387d Table: 4b24006a DAC: 00000015 |
| [ 142.318675] Process myprocess (pid: 1234, stack limit = 0xc9a3c238) |
| [ 142.318682] Stack: (0xc9a3be90 to 0xc9a3c000) |
| [ 142.318690] be80: c9a3bea4 bf002028 c1234567 00000001 |
| [ 142.318706] Backtrace: |
| [ 142.318714] [<bf001000>] (bad_function) from [<bf002028>] (caller_function+0x28/0x50) |
| [ 142.318722] [<bf002000>] (caller_function) from [<c0123456>] (do_something+0x44/0x80) |
| [ 142.318730] [<c0123412>] (do_something) from [<c0234567>] (kernel_thread_fn+0x30/0x60) |
| [ 142.318738] [<c0234537>] (kernel_thread_fn) from [<c0012345>] (kthread+0x10c/0x130) |
| [ 142.318746] [<c0012239>] (kthread) from [<c000f000>] (ret_from_fork+0x14/0x24) |
| [ 142.318754] Code: e5903000 e3530000 0a000003 e5902004 (e5923000) |
| [ 142.318762] ---[ end trace a1b2c3d4e5f60001 ]--- |
| [ 142.318770] Kernel panic - not syncing: Fatal exception |
| [ 142.318778] CPU0: stopping |
| [ 142.318785] CPU1: stopping |
| [ 142.318792] CPU2: stopping |
| [ 142.318800] CPU3: stopping |
| [ 142.318808] Rebooting in 5 seconds.. |


## 1.2  Anatomy of a Kernel Panic — Field by Field
### Error Header
| Unable to handle kernel NULL pointer dereference at virtual address 00000000 |


| Field | Meaning |
| Type of fault | NULL pointer dereference, use-after-free, stack overflow, etc. |
| Virtual address | The faulting VA — 0x00000000 = classic NULL deref |


### Oops Code
| Internal error: Oops: 5 [#1] PREEMPT SMP ARM |


| Field | Meaning |
| Oops: 5 | FSR (Fault Status Register) value — 5 = translation fault, section |
| [#1] | Oops count (first occurrence) |
| PREEMPT | Kernel compiled with preemption enabled |
| SMP | Symmetric multiprocessing enabled |
| ARM | 32-bit ARM mode (not Thumb, not ARM64) |


Common FSR values (ARMv7):
| FSR Value | Fault Type |
| 0x05 | Translation fault (section) |
| 0x07 | Translation fault (page) |
| 0x0D | Permission fault (section) |
| 0x0F | Permission fault (page) |
| 0x08 | Precise external abort |


### CPU Register Dump
| PC is at bad_function+0x14/0x3c [mydriver] |
| LR is at caller_function+0x28/0x50 [mydriver] |


| Register | Role |
| PC | Program Counter — exactly where the crash happened |
| LR | Link Register — who called the crashing function |
| SP | Stack Pointer |
| r0–r3 | Function arguments / return value |
| r4–r11 | Callee-saved general purpose registers |
| r12 (ip) | Intra-procedure scratch register |


The +0x14/0x3c notation means: offset 0x14 bytes into a function of total size 0x3c bytes. This is critical for locating the exact faulting instruction using addr2line or GDB.

### Processor State (PSR)
| Flags: nZCv IRQs on FIQs on Mode SVC_32 ISA ARM Segment user |


| Field | Meaning |
| nZCv | Condition flags: N=0 (negative), Z=1 (zero), C=0 (carry), v=0 (overflow) |
| IRQs on | Interrupts were enabled at crash time |
| Mode SVC_32 | Supervisor mode — kernel context |
| ISA ARM | ARM instruction set (not Thumb) |
| Segment user | Accessing user memory segment |


### Call Stack / Backtrace
| [<bf001000>] (bad_function) from [<bf002028>] (caller_function+0x28/0x50) |
| [<bf002000>] (caller_function) from [<c0123456>] (do_something+0x44/0x80) |


- Read bottom-up for actual call order (innermost frame is at top)
- bf prefix = kernel module address space
- c0 prefix = core kernel address space
- Use addr2line or gdb to resolve addresses to source lines

### Code Dump
| Code: e5903000 e3530000 0a000003 e5902004 (e5923000) |


- Raw ARM opcodes around the faulting instruction
- The instruction in parentheses (e5923000) is the faulting instruction
- Decode: e5923000 = LDR r3, [r2, #0] — loading from address in r2 which was 0x00000000 → NULL deref!

## 1.3  ARM64 Kernel Panic Example
ARM64 panics differ from ARM32 in several key ways — they use the ESR (Exception Syndrome Register) instead of FSR, x0–x30 registers instead of r0–r15, and pstate instead of cpsr:

| [ 45.123456] Unable to handle kernel paging request at virtual address ffff800012345678 |
| [ 45.123470] Mem abort info: |
| [ 45.123475] ESR = 0x96000045 |
| [ 45.123480] EC = 0x25: DABT (current EL), IL = 32 bits |
| [ 45.123485] SET = 0, FnV = 0 |
| [ 45.123490] EA = 0, S1PTW = 0 |
| [ 45.123495] FSC = 0x05: level 1 translation fault |
| [ 45.123500] Data abort info: |
| [ 45.123505] ISV = 0, ISS = 0x00000045 |
| [ 45.123510] CM = 0, WnR = 1 |
| [ 45.123515] swapper pgtable: 4k pages, 48-bit VAs, pgdp=0000000081234000 |
| [ 45.123520] [ffff800012345678] pgd=0000000000000000 |
| [ 45.123525] Internal error: Oops: 96000045 [#1] PREEMPT SMP |
| [ 45.123530] CPU: 2 PID: 567 Comm: kworker/2:1 Tainted: G W 5.15.0 |
| [ 45.123535] pstate: 60400005 (nZCv daif +PAN -UAO -TCO -DIT -SSBS BTYPE=--) |
| [ 45.123540] pc : bad_write_fn+0x24/0x60 [mydriver] |
| [ 45.123545] lr : work_handler+0x48/0x90 [mydriver] |
| [ 45.123550] sp : ffff800012a3be90 |
| [ 45.123555] x29: ffff800012a3be90 x28: 0000000000000000 x27: ffff800011223344 |
| [ 45.123560] x26: 0000000000000001 x25: ffff800012345678 x24: 0000000000000010 |
| [ 45.123600] Call trace: |
| [ 45.123605] bad_write_fn+0x24/0x60 [mydriver] |
| [ 45.123610] work_handler+0x48/0x90 [mydriver] |
| [ 45.123615] process_one_work+0x1c8/0x3c0 |
| [ 45.123620] worker_thread+0x50/0x3c0 |
| [ 45.123625] kthread+0x118/0x128 |
| [ 45.123630] ret_from_fork+0x10/0x18 |
| [ 45.123635] Code: f9400260 b9400001 f9000001 f9000260 (f9000b41) |
| [ 45.123640] ---[ end trace 1a2b3c4d5e6f0001 ]--- |
| [ 45.123645] Kernel panic - not syncing: Oops: Fatal exception |


### ESR Decoding for ARM64
The ESR (Exception Syndrome Register) value 0x96000045 breaks down as:

| ESR Field | Value / Meaning |
| EC [31:26] | 0x25 = Data Abort (DABT) from current EL (EL1 = kernel) |
| IL [25] | 1 = 32-bit instruction length |
| FSC [5:0] | 0x05 = Level 1 translation fault (PGD entry not valid) |
| WnR [6] | 1 = Write fault (instruction was a store) |
| +PAN | Privileged Access Never — kernel cannot access user pages without uaccess |
| -UAO | User Access Override — not active |


## 1.4  Common Kernel Panic Causes & Their Signatures

| Panic Message / Signature | Root Cause |
| NULL pointer dereference at 00000000 | Dereferencing uninitialized or null pointer |
| kernel BUG at mm/slub.c:XXX | Heap corruption or double-free detected by allocator |
| stack-protector: Kernel stack is corrupted | Stack buffer overflow — canary check failed |
| Unable to handle kernel paging request | Bad pointer dereference, use-after-free, freed memory access |
| divide error: 0000 | Integer division by zero in kernel context |
| RCU stall detected | CPU stuck in non-preemptible section blocking RCU grace period |
| soft lockup - CPU#X stuck for 22s! | CPU spinning without yielding to scheduler |
| BUG: scheduling while atomic | Sleeping/scheduling inside spinlock or interrupt handler context |
| VFS: Unable to mount root fs | Boot failure — root filesystem not found or corrupt |
| Attempted to kill init! | PID 1 (init/systemd) crashed — unrecoverable userspace failure |


## 1.5  Debugging Workflow
### Using addr2line
| # Resolve PC address to source line |
| arm-linux-gnueabi-addr2line -e vmlinux -f bf001014 |
| # Output: |
| # bad_function |
| # /path/to/mydriver/driver.c:42 |


### Using GDB
| arm-linux-gnueabi-gdb vmlinux |
| (gdb) list *(bad_function+0x14) |
| (gdb) disassemble bad_function |


### Decoding the Faulting Instruction
| # Decode ARM opcode e5923000 |
| echo "e5923000" | xxd -r -p | arm-linux-gnueabi-objdump -D -b binary -m arm - |
| # => ldr r3, [r2] ; r2 was 0x0 => NULL deref (SIGSEGV) |


### With T32 / Lauterbach (Post-Mortem Analysis)
| ; Load crash dump |
| SYStem.CONFIG CORE 1. 1. |
| Data.LOAD.ELF vmlinux /NoCODE |
| ; Set PC to crash address |
| Register.Set PC 0xBF001014 |
| ; View backtrace |
| TASK.Stack |
| ; Inspect memory around SP |
| Data.dump 0xC9A3BE90 |


## 1.6  Panic vs. Oops vs. BUG — Comparison Table

| Type | Behavior | Recoverable? |
| Oops | Logs error + backtrace, may kill faulting process | Sometimes (if not in interrupt context) |
| BUG() / BUG_ON() | Explicit assertion failure — unconditional panic | No — always panics |
| WARN() / WARN_ON() | Logs warning + backtrace, continues execution | Yes — continues |
| Kernel Panic | System halts or reboots immediately | No |
| panic_on_oops=1 | Any Oops is escalated to a full panic | No |



# SECTION 2: QUALCOMM WATCHDOG IMPLEMENTATION
The Qualcomm watchdog is a hardware + software co-designed subsystem that monitors CPU liveness and triggers a system reset if the kernel or a CPU core stops making forward progress. It is a multi-level system involving hardware timers, interrupt controllers, TrustZone, and a dedicated kernel thread.

## 2.1  Architecture Overview
| Level | Component | What It Monitors |
| HW WDT | Always-on (AO) domain timer | Entire SoC — if not pet within timeout, hard reset |
| Bark | First-stage warning interrupt | CPU stuck — triggers NMI/FIQ handler in kernel |
| Bite | Second-stage fatal action | Hard reset / secure watchdog reset of entire SoC |
| Per-CPU WDT | msm_watchdog per-CPU pet via IPI | Each individual CPU core liveness check |


## 2.2  Hardware Registers
The watchdog is part of the APSS (Application Processor SubSystem) timer block, typically memory-mapped at:

| # MSM8996 / SDM845 / SM8150 etc. |
| APSS_WDT_BASE = 0x17980000 # (varies per SoC) |
|  |
| Registers: |
| WDT0_RST = APSS_WDT_BASE + 0x04 # Pet/reset the watchdog |
| WDT0_EN = APSS_WDT_BASE + 0x08 # Enable/disable |
| WDT0_BARK = APSS_WDT_BASE + 0x10 # Bark timeout (in timer ticks) |
| WDT0_BITE = APSS_WDT_BASE + 0x14 # Bite timeout (in timer ticks) |


| Register | Offset | Description |
| WDT0_RST | +0x04 | Write 1 to reset (pet) the watchdog timer — extends the timeout window |
| WDT0_EN | +0x08 | Write 1 to enable, 0 to disable (done during suspend) |
| WDT0_BARK | +0x10 | Bark timeout in timer ticks. Fires first as a warning (triggers NMI/FIQ) |
| WDT0_BITE | +0x14 | Bite timeout in timer ticks. Must be > BARK. Forces hard SoC reset. |


CRITICAL: Bark < Bite always — bark fires first as a warning, bite is the fatal reset that reboots the SoC.

## 2.3  Kernel Driver: msm_watchdog
### Source Location
| drivers/watchdog/msm_watchdog.c # Older MSM/Qualcomm kernels |
| drivers/soc/qcom/watchdog_v2.c # Newer Qualcomm watchdog v2 |


### Key Data Structures
| struct msm_watchdog_data { |
| unsigned int bark_time; /* bark timeout in ms */ |
| unsigned int bite_time; /* bite timeout in ms */ |
| unsigned int pet_time; /* how often to pet (ms) */ |
| bool do_ipi_ping; /* ping all CPUs via IPI */ |
| bool wdog_absent; /* WDT HW present? */ |
| void __iomem *base; /* MMIO base */ |
| struct device *dev; |
| struct task_struct *watchdog_task; /* kthread that pets WDT */ |
| spinlock_t freeze_lock; |
| bool freeze; /* suspend freeze */ |
| cpumask_t alive_mask; /* CPUs that responded to IPI */ |
| struct timer_list pet_timer; /* periodic pet timer */ |
| wait_queue_head_t pet_complete; |
| unsigned long timer_expired; |
| unsigned long last_pet; /* jiffies of last pet */ |
| atomic_t alive_count; |
| }; |


## 2.4  Watchdog Initialization Flow
| static int msm_watchdog_probe(struct platform_device *pdev) |
| { |
| struct msm_watchdog_data *wdog_dd; |
|  |
| /* 1. Allocate and parse DT */ |
| wdog_dd = devm_kzalloc(&pdev->dev, sizeof(*wdog_dd), GFP_KERNEL); |
| msm_wdog_dt_to_pdata(pdev, wdog_dd); // parse bark-time, bite-time from DT |
|  |
| /* 2. Map MMIO registers */ |
| wdog_dd->base = devm_ioremap_resource(&pdev->dev, res); |
|  |
| /* 3. Register bark IRQ (NMI/FIQ on newer SoCs) */ |
| wdog_dd->bark_irq = platform_get_irq(pdev, 0); |
| request_irq(wdog_dd->bark_irq, wdog_bark_handler, |
| IRQF_TRIGGER_RISING, "watchdog bark", wdog_dd); |
|  |
| /* 4. Program bark and bite timeouts */ |
| __raw_writel(timeout_to_ticks(wdog_dd->bark_time), wdog_dd->base + WDT0_BARK); |
| __raw_writel(timeout_to_ticks(wdog_dd->bite_time), wdog_dd->base + WDT0_BITE); |
|  |
| /* 5. Enable watchdog */ |
| __raw_writel(1, wdog_dd->base + WDT0_EN); |
| __raw_writel(1, wdog_dd->base + WDT0_RST); // initial pet |
|  |
| /* 6. Spawn watchdog kthread */ |
| wdog_dd->watchdog_task = kthread_run(watchdog_kthread, wdog_dd, "msm_watchdog"); |
|  |
| /* 7. Register with kernel watchdog framework */ |
| watchdog_register_device(&wdog_dd->wdd); |
| } |


## 2.5  Device Tree Binding
| /* arch/arm64/boot/dts/qcom/sdm845.dtsi */ |
| watchdog@17980000 { |
| compatible = "qcom,msm-watchdog"; |
| reg = <0x17980000 0x1000>; |
| reg-names = "wdt-base"; |
| interrupts = <GIC_SPI 3 IRQ_TYPE_EDGE_RISING>, /* bark */ |
| <GIC_SPI 4 IRQ_TYPE_EDGE_RISING>; /* bite (optional) */ |
| qcom,bark-time = <11000>; /* 11 seconds */ |
| qcom,pet-time = <9000>; /* pet every 9 seconds */ |
| qcom,ipi-ping; /* enable per-CPU IPI liveness check */ |
| qcom,wdog-absent-timeout = <0>; |
| }; |


## 2.6  The Pet Mechanism — watchdog_kthread
| static int watchdog_kthread(void *arg) |
| { |
| struct msm_watchdog_data *wdog_dd = arg; |
|  |
| while (!kthread_should_stop()) { |
| /* Wait for pet_time interval */ |
| wait_event_timeout(wdog_dd->pet_complete, |
| wdog_dd->timer_expired, |
| msecs_to_jiffies(wdog_dd->pet_time)); |
|  |
| if (wdog_dd->do_ipi_ping) |
| ping_other_cpus(wdog_dd); /* IPI all CPUs */ |
|  |
| /* Pet the hardware watchdog */ |
| __raw_writel(1, wdog_dd->base + WDT0_RST); |
| wdog_dd->last_pet = jiffies; |
| } |
| return 0; |
| } |


## 2.7  Per-CPU IPI Ping
| static void ping_other_cpus(struct msm_watchdog_data *wdog_dd) |
| { |
| int cpu; |
| cpumask_clear(&wdog_dd->alive_mask); |
| atomic_set(&wdog_dd->alive_count, 0); |
|  |
| /* Send IPI to all online CPUs */ |
| for_each_cpu(cpu, cpu_online_mask) { |
| smp_call_function_single(cpu, wdog_ping_cb, wdog_dd, 0); |
| } |
|  |
| /* Wait for all CPUs to respond */ |
| msleep(IPI_WAIT_TIME); |
|  |
| /* If any CPU did not respond => it is stuck */ |
| if (!cpumask_equal(&wdog_dd->alive_mask, cpu_online_mask)) { |
| /* Do NOT pet => let bark fire => dump state => bite resets */ |
| pr_err("Watchdog: CPU(s) failed IPI ping: %*pbl\n", |
| cpumask_pr_args(cpu_online_mask)); |
| } |
| } |
|  |
| static void wdog_ping_cb(void *info) |
| { |
| struct msm_watchdog_data *wdog_dd = info; |
| cpumask_set_cpu(smp_processor_id(), &wdog_dd->alive_mask); |
| } |


## 2.8  Bark Handler — First Warning
| static irqreturn_t wdog_bark_handler(int irq, void *dev_id) |
| { |
| struct msm_watchdog_data *wdog_dd = dev_id; |
|  |
| pr_err("Watchdog bark! last pet: %lu ms ago\n", |
| jiffies_to_msecs(jiffies - wdog_dd->last_pet)); |
|  |
| /* Dump all CPU register states */ |
| msm_trigger_wdog_dump(); |
|  |
| /* Trigger kernel panic — generates crash dump before bite */ |
| panic("Watchdog bark received!"); |
|  |
| return IRQ_HANDLED; |
| } |


KEY INSIGHT: The bark handler intentionally calls panic() to generate a ramdump/minidump BEFORE the hardware bite resets the SoC — giving you a post-mortem snapshot for debugging. Without this, the bite would cause a hard reset with no debug information.

## 2.9  Bark → Bite Timeline
| t=0s Watchdog enabled, pet timer starts |
| t=9s kthread pets WDT (RST register write) => timer resets |
| t=9s IPI ping sent to all CPUs |
| | CPU2 stops responding (hung in spinlock) |
| t=18s kthread tries to pet but SKIPS (CPU2 did not respond to IPI) |
| t=27s Bark fires (11s timeout from last successful pet at t=9s + margin) |
| => wdog_bark_handler() called |
| => panic() => ramdump captured |
| t=32s Bite fires (bite_time > bark_time) |
| => Hardware forces SoC reset |
| => Secure watchdog in TZ/AOSS resets entire system |


## 2.10  Secure Watchdog (TrustZone Layer)
On modern Qualcomm SoCs, there is also a TZ-controlled watchdog in the AOSS (Always-On SubSystem). This provides a second layer of protection that operates independently of Linux:

| AOSS_WDT — monitored by TZ/QSEE |
| | |
| If HLOS (Linux) watchdog driver fails to pet TZ watchdog via SCM call: |
| => TZ triggers a secure reset (bypasses Linux entirely) |


| /* Pet the TZ watchdog via Secure Channel Manager (SCM) */ |
| static void msm_pet_tz_watchdog(void) |
| { |
| struct scm_desc desc = {0}; |
| desc.args = 0; |
| desc.arginfo = SCM_ARGS(1); |
| scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT, TZ_WDOG_PET_CMD), &desc); |
| } |


## 2.11  Suspend/Resume Handling
| static int msm_watchdog_suspend(struct device *dev) |
| { |
| /* Disable WDT during suspend — no kthread running */ |
| __raw_writel(0, wdog_dd->base + WDT0_EN); |
| mb(); /* memory barrier — ensure write completes */ |
| wdog_dd->freeze = true; |
| return 0; |
| } |
|  |
| static int msm_watchdog_resume(struct device *dev) |
| { |
| /* Re-enable and pet immediately on resume */ |
| __raw_writel(1, wdog_dd->base + WDT0_EN); |
| __raw_writel(1, wdog_dd->base + WDT0_RST); |
| wdog_dd->freeze = false; |
| wake_up(&wdog_dd->pet_complete); |
| return 0; |
| } |


## 2.12  Watchdog Reset Reason Detection
After a watchdog bite reset, the bootloader (LK/UEFI ABL) reads the PMIC PON (Power-On) reason register to detect the reset cause:

| /* In LK / UEFI ABL */ |
| uint32_t pon_reason = pm8994_get_pon_reason(); |
|  |
| if (pon_reason & PON_REASON_WATCHDOG) { |
| /* Log watchdog reset, set boot reason cookie */ |
| smem_write_boot_reason(BOOT_REASON_WATCHDOG); |
| } |


In Linux, you can read watchdog information via:
| cat /sys/kernel/debug/qcom_dcc/watchdog_status |
| # or |
| cat /proc/last_kmsg # if pstore/ramoops configured |
| dmesg | grep -i "watchdog\|wdog\|bark\|bite" |


## 2.13  Common Watchdog Panic Signatures
| # Bark fired — CPU stuck |
| [12345.678] Watchdog bark! last pet: 11234 ms ago |
| [12345.679] Watchdog: CPU(s) failed IPI ping: 2 |
| [12345.680] Kernel panic - not syncing: Watchdog bark received! |
|  |
| # Soft lockup (related — scheduler watchdog) |
| [12345.678] watchdog: BUG: soft lockup - CPU#2 stuck for 22s! [kworker/2:1:1234] |
|  |
| # Hard lockup (NMI watchdog) |
| [12345.678] Watchdog detected hard LOCKUP on cpu 2 |
| [12345.679] NMI backtrace for cpu 2 |



# SECTION 3: WATCHDOG SUBSYSTEMS THAT REQUIRE PETTING
In the Qualcomm watchdog architecture, there are multiple subsystems that need to be "pet" (kept alive) at different levels. Each layer has its own petting mechanism, timeout, and failure consequence.

## 3.1  APSS Hardware Watchdog (WDT0)
- Who pets it: msm_watchdog kthread (Linux kernel)
- How: Write 1 to WDT0_RST register (MMIO write)
- Subsystem: APSS (Application Processor SubSystem) — main ARM/ARM64 cluster timer block
- Timeout: Typically 11 seconds (configurable via qcom,bark-time DT property)

| __raw_writel(1, wdog_dd->base + WDT0_RST); |


## 3.2  Per-CPU Cores (via IPI Ping)
- Who pets it: msm_watchdog kthread — sends IPI to each CPU
- How: smp_call_function_single() to all online CPUs
- Subsystem: Each individual CPU core in the APSS cluster
- CPU0, CPU1, CPU2, CPU3 — Silver/LITTLE cores (Cortex-A55)
- CPU4, CPU5, CPU6, CPU7 — Gold/BIG cores on big.LITTLE SoCs (Cortex-A75/A78)
- If any CPU fails to respond to IPI: kthread stops petting WDT0 => bark fires

| smp_call_function_single(cpu, wdog_ping_cb, wdog_dd, 0); |


## 3.3  TrustZone (TZ) / QSEE Secure Watchdog
- Who pets it: Linux kernel via SCM (Secure Channel Manager) call
- How: scm_call2() with TZ_WDOG_PET_CMD using ARM SMC instruction
- Subsystem: TrustZone (EL3) / QSEE — runs in secure world, independent of Linux
- Lives in the AOSS (Always-On SubSystem)
- If Linux is fully dead and cannot make SCM calls => TZ watchdog fires its own reset

| scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT, TZ_WDOG_PET_CMD), &desc); |
| /* This triggers the ARM SMC instruction: smc #0 */ |
| /* CPU transitions from EL1 (Linux) to EL3 (TZ) */ |
| /* TZ updates its watchdog timer and returns to EL1 */ |


## 3.4  AOSS (Always-On SubSystem) Watchdog
- Who pets it: TrustZone firmware (indirectly via HLOS SCM pet)
- Subsystem: AOSS — a dedicated low-power microcontroller (RPM/AOP) that stays alive even when APSS is in deep sleep
- Monitors the entire SoC health
- Can reset the SoC independently of APSS
- Receives pet indirectly: Linux pets TZ via SCM => TZ pets AOSS internally

The AOSS watchdog is the last line of defense. Even if the entire ARM cluster (APSS) is frozen, the AOSS microcontroller continues to run and will reset the SoC if not petted by TrustZone.

## 3.5  Linux Kernel Soft Lockup Watchdog (Scheduler WDT)
- Who pets it: Each CPU's dedicated watchdog kthread (one per CPU, highest priority)
- How: Resets a per-CPU hrtimer (high-resolution timer)
- Threshold: Fires after 20 seconds by default (CONFIG_BOOTPARAM_SOFTLOCKUP_PANIC_VALUE)
- Subsystem: Linux scheduler / kernel itself — not Qualcomm-specific but runs on top
- This is distinct from the Qualcomm HW watchdog — it monitors scheduler health

| /* In kernel/watchdog.c */ |
| watchdog_timer_fn() /* hrtimer callback — detects soft lockup */ |
|  |
| /* Called periodically — if CPU is in runnable state but not getting |
| scheduled for > softlockup_thresh seconds => soft lockup warning */ |


## 3.6  Linux Kernel Hard Lockup Watchdog (NMI WDT)
- Who pets it: Each CPU's watchdog kthread via NMI perf counter reset
- How: Uses hardware performance monitoring counters to generate periodic NMI
- Threshold: Fires after 2x softlockup_thresh (usually 40 seconds)
- This catches the case where a CPU is completely frozen — even interrupts are disabled
- On ARM64 Qualcomm: uses the arch timer or PMU overflow interrupt

| /* In kernel/watchdog.c */ |
| watchdog_overflow_callback() /* NMI perf event callback */ |
|  |
| /* If this callback stops being called on a CPU => that CPU is |
| completely frozen (even NMI/FIQ not delivered) => hard lockup */ |


## 3.7  Summary Table — All Watchdog Subsystems

| # Subsystem | Who Pets It | Mechanism | Reset Consequence |
| 1 APSS HW WDT (WDT0) | msm_watchdog kthread | WDT0_RST MMIO write | Bark IRQ => panic => Bite => SoC reset |
| 2 Per-CPU cores | msm_watchdog kthread | smp_call_function_single() | Stop petting WDT0 => bark fires => same as above |
| 3 TrustZone / QSEE | Linux kernel | scm_call2(TZ_WDOG_PET) | TZ-triggered secure SoC reset — no ramdump |
| 4 AOSS / AOP | TZ firmware (indirect) | Internal AOSS timer | Full SoC power reset — no Linux involvement at all |
| 5 Linux Soft Lockup WDT | Per-CPU watchdog kthread | hrtimer reset | panic("soft lockup") after 20s |
| 6 Linux Hard Lockup WDT | Per-CPU watchdog kthread | NMI perf counter reset | panic("hard LOCKUP") — CPU completely frozen |


## 3.8  Petting Dependency Chain
| Linux msm_watchdog kthread |
| | |
| +---> Pets APSS WDT0_RST (HW register) |
| | |__ Only if ALL CPUs responded to IPI ping |
| | |
| +---> IPI ping => CPU0, CPU1, CPU2 ... CPUn |
| | |__ Each CPU sets its bit in alive_mask |
| | |__ CPUs in deep LPM (cluster power collapse) may not respond! |
| | |
| +---> SCM call => TrustZone WDT pet |
| |__ TZ pets AOSS watchdog internally |
|  |
| Per-CPU watchdog kthread (kernel/watchdog.c) |
| +---> Resets hrtimer => prevents soft lockup detection |
| +---> Resets NMI perf counter => prevents hard lockup detection |


## 3.9  What Happens If Each Pet Fails

| Pet Missed | Consequence |
| WDT0_RST not written | Bark fires after bark_time => wdog_bark_handler => panic() => bite resets SoC |
| CPU IPI not responded | kthread stops petting WDT0 => same cascade: bark => panic => bite |
| SCM/TZ pet missed | TZ fires secure reset — NO ramdump captured, hard reset only |
| AOSS pet missed | AOSS resets entire SoC — NO Linux involvement, no crash dump |
| hrtimer not reset | soft lockup warning/panic triggered after 20s by Linux scheduler WDT |
| NMI counter not reset | hard lockup NMI panic — CPU fully frozen, IRQs disabled |



# SECTION 4: A DIFFICULT WATCHDOG BUG TO DEBUG
This section presents a real-world, interview-worthy watchdog bug that is genuinely difficult to debug — the kind that takes days or weeks and requires deep system knowledge across multiple layers: cpuidle, LPM, GIC clock gating, IPI delivery, and the watchdog pet logic.

## 4.1  Bug Description: "Watchdog Bark on Idle CPUs — But No CPU Was Actually Stuck"
### Scenario Setup
Platform: Qualcomm SDM845 (or SM8150), Android kernel 4.19, multi-cluster ARM64 (4x Cortex-A55 Silver + 4x Cortex-A75 Gold)

### Symptom
- System reboots randomly every few hours under LIGHT LOAD (near-idle conditions)
- last_kmsg / pstore shows:

| [43821.441203] Watchdog bark! last pet: 11432 ms ago |
| [43821.441210] Watchdog: CPU(s) failed IPI ping: 4,5,6,7 |
| [43821.441218] Kernel panic - not syncing: Watchdog bark received! |


- CPUs 4–7 (Gold/BIG cluster) failed IPI ping
- But crash dump shows NO spinlock held, NO IRQ storm, NO runaway thread on those CPUs
- Happens only on RETAIL builds — never reproducible on debug builds with UART enabled

## 4.2  Why It's Deceptively Hard to Debug

| Challenge | Why It's Hard |
| Heisenbugs on debug builds | UART/JTAG probing changes timing — the bug completely disappears |
| CPUs appear idle | No obvious stuck thread — registers show WFI (Wait For Interrupt) instruction |
| Intermittent | Happens every few hours, not reproducible on demand in lab |
| Multi-layer interaction | Involves cpuidle, clock gating, IPI routing, GIC, and watchdog kthread scheduling |
| No obvious kernel log | Happens silently — no WARN, no BUG, no prior error message before bark |


## 4.3  Root Cause
The real cause is a race condition between CPU cluster power collapse (deep idle state) and IPI delivery via the GIC (Generic Interrupt Controller).

### What Actually Happens (Race Condition Trace)
| t=0 msm_watchdog kthread runs on CPU0 (Silver cluster) |
| t=1 kthread calls smp_call_function_single(cpu4..7, wdog_ping_cb) |
| t=2 CPUs 4-7 are in cpuidle DEEP SLEEP (LPM - Low Power Mode) |
| => They have entered cluster power collapse |
| => GIC (Generic Interrupt Controller) has powered down for that cluster |
| t=3 IPI is sent via GIC SGI (Software Generated Interrupt) |
| => But GIC distributor for Gold cluster is CLOCK GATED |
| => IPI is LOST — never delivered to CPUs 4-7 |
| t=4 CPUs 4-7 never call wdog_ping_cb() |
| => alive_mask never gets bits 4-7 set |
| t=5 kthread sees CPUs 4-7 did not respond |
| => Stops petting WDT0_RST |
| t=6 Bark fires after 11s => panic => bite => reboot |


THE BUG: The msm_watchdog IPI ping does NOT account for CPUs in deep LPM (cluster power collapse) where the GIC is partially powered down. The IPI is silently dropped — no error, no log, no indication that delivery failed.

## 4.4  Full Debugging Steps
### Step 1: Analyze the Ramdump in CrashScope
| # Load crash dump |
| crash vmlinux vmcore |
|  |
| # Check what CPUs 4-7 were doing at crash time |
| crash> bt -a # backtrace all CPUs |
|  |
| # CPU4-7 all show: |
| # PID: 0 TASK: ffff800012345678 CPU: 4 COMMAND: "swapper/4" |
| # #0 [ffff800012a3be90] cpu_do_idle at ffff2000100abcd0 |
| # #1 [ffff800012a3bea0] cpuidle_enter_state at ffff200010234560 |
| # #2 [ffff800012a3beb0] cpuidle_enter at ffff200010234890 |


Finding from Step 1: CPUs 4–7 were in cpu_do_idle => WFI — not stuck, just sleeping deeply. This definitively rules out spinlock/deadlock as the root cause.

### Step 2: Check LPM (Low Power Mode) State
| # In ramdump — check cpuidle state at time of crash |
| crash> p cpuidle_devices # per-CPU cpuidle state |
|  |
| # Also check on live system for reproduction: |
| cat /sys/kernel/debug/lpm_stats/ # LPM statistics |
| cat /sys/kernel/debug/clk/summary # GIC clock state |


Finding from Step 2: CPUs 4–7 had entered C4 state (cluster power collapse) — the deepest idle state where the entire Gold cluster is power-gated including its GIC interface.

### Step 3: Correlate Watchdog Pet Timing with LPM Entry
Added temporary trace_printk instrumentation (non-intrusive, uses kernel ring buffer — does not affect timing like printk):

| /* In watchdog_v2.c */ |
| static void ping_other_cpus(struct msm_watchdog_data *wdog_dd) |
| { |
| for_each_cpu(cpu, cpu_online_mask) { |
| trace_printk("WDT: sending IPI to CPU%d, idle_state=%d\n", |
| cpu, cpuidle_get_last_residency(cpu)); |
| smp_call_function_single(cpu, wdog_ping_cb, wdog_dd, 0); |
| } |
| } |


Finding from ftrace: IPI was sent to CPU4 when it was in LPM state 4 (cluster collapse). The IPI was never acknowledged — confirmed by the alive_mask never setting bit 4.

### Step 4: Verify GIC Clock Gating via T32
Used T32 (Lauterbach JTAG) to read GIC distributor registers on a live system during deep idle:

| ; T32 script — read GIC GICD_CTLR while CPUs 4-7 in deep idle |
| Data.In 0x17A00000 ; GICD base for Gold cluster |
| ; => Returns 0xDEADDEAD (bus error) — GIC is clock gated! |
|  |
| ; When CPUs are running: |
| Data.In 0x17A00000 ; => Returns 0x00000003 (GIC enabled, all groups) |
|  |
| ; Confirmed: GIC distributor is INACCESSIBLE during cluster power collapse |


This confirmed the root cause: GIC distributor for the Gold cluster is clock-gated during cluster power collapse. Any IPI (SGI - Software Generated Interrupt) sent via the GIC to these CPUs is silently dropped.

### Step 5: Verify IPI Delivery Failure via GIC Pending Register
| /* Check GIC SGI pending register — should show IPI pending if it was lost */ |
| /* GICD_SPENDSGIR — SGI Set-Pending Register */ |
| uint32_t pending = readl(GICD_BASE + GICD_SPENDSGIR0); |
| /* If bit (cpu_id * 8 + irq_id) is set => IPI was sent but not cleared */ |
| /* => proves the CPU never serviced it (was in power collapse) */ |


## 4.5  The Fix
### Option A: Skip IPI for CPUs in Deep Idle (Correct Fix)
| static void ping_other_cpus(struct msm_watchdog_data *wdog_dd) |
| { |
| int cpu; |
| for_each_cpu(cpu, cpu_online_mask) { |
| /* Skip CPUs in cluster power collapse — they cannot receive IPI */ |
| /* An idle CPU is NOT a stuck CPU */ |
| if (msm_lpm_is_cpu_in_cluster_collapse(cpu)) { |
| /* Mark as alive — they are idle, not stuck */ |
| cpumask_set_cpu(cpu, &wdog_dd->alive_mask); |
| continue; |
| } |
| smp_call_function_single(cpu, wdog_ping_cb, wdog_dd, 0); |
| } |
| } |


### Option B: Wake CPU Before Sending IPI (Alternative)
| /* Force CPU out of deep idle before sending IPI */ |
| cpu_kick_from_idle(cpu); |
| udelay(100); /* Wait for CPU to wake from power collapse */ |
| smp_call_function_single(cpu, wdog_ping_cb, wdog_dd, 0); |


Drawback of Option B: Kicking CPUs out of deep idle just for a watchdog ping significantly increases power consumption — defeats the purpose of cpuidle.

### Option C: Use NMI/FIQ-Based Ping Instead of IPI (Most Robust)
Use the GIC's FIQ (Fast Interrupt Request) channel which bypasses cluster power gating. FIQ is routed through the AOSS and can wake even power-collapsed clusters.
- Qualcomm's newer watchdog implementations (watchdog_v2.c on SM8150+) use this approach
- FIQ is configured at GIC level to route through AOSS => AOSS wakes cluster => CPU processes FIQ
- This is the most robust solution but requires more GIC configuration

## 4.6  Interview Framing
Here is how to frame this debugging story in an interview (Staff Engineer / Principal Engineer level):

| Interview Script:"We had a watchdog reboot that only happened on retail builds under light load — never reproducible with UART or JTAG attached. The crash dump showed CPUs 4–7 failed the IPI liveness check, but they were not stuck — they were in deep idle. The bug was that the watchdog IPI was being silently dropped by the GIC when the Gold cluster was in power collapse. I confirmed this using T32 to read the GIC distributor registers during deep idle — it returned a bus error because the GIC was clock-gated. The fix was to skip the IPI ping for CPUs in cluster power collapse and treat them as alive, since an idle CPU is not a stuck CPU." |


### Key Technical Depth Points to Mention
- Why debug builds do not reproduce: UART keeps CPUs from entering deep LPM — UART requires clocks that prevent cluster collapse, fundamentally altering the timing window
- Why it is intermittent: Only triggers when ALL Gold CPUs happen to be in cluster collapse exactly when the watchdog IPI fires — a timing race window
- Why it is subtle: The IPI is not rejected — it is silently dropped. No error, no log, no indication from the hardware
- Cross-layer knowledge required: cpuidle => LPM => GIC clock gating => IPI delivery => watchdog pet logic — each layer is well-understood individually but the interaction is the bug
- Detection methodology: Ramdump analysis => cpuidle state inspection => ftrace instrumentation => T32 hardware register reading

## Summary: Qualcomm Watchdog Architecture Layers
| +---------------------------------------------+ |
| | Linux msm_watchdog kthread (pet every 9s) | |
| | + IPI ping to all online/active CPUs | |
| +---------------------------------------------+ |
| | HW WDT Bark (11s) => NMI/FIQ => panic() | |
| | HW WDT Bite (13s) => Hard SoC reset | |
| +---------------------------------------------+ |
| | TZ/AOSS Secure WDT => SCM pet required | |
| | (bypasses Linux if HLOS is fully dead) | |
| +---------------------------------------------+ |
| | PMIC PON WDT reset reason => LK/UEFI ABL | |
| +---------------------------------------------+ |


