# ARM Interrupt Preemption and Interrupt Storm Scenarios in Linux Kernel
# Document Outline
## Sources
- Web research on ARM interrupt preemption and PREEMPT_RT
- Web research on Linux kernel interrupt storm scenarios
- ARM Cortex-M exception handling documentation
- Linux kernel documentation on preemption and locking
## Core Thesis
A comprehensive technical reference covering interrupt preemption mechanisms in the ARM Linux kernel and all scenarios in which interrupt storms can occur, including mitigation strategies.

# Part 1: Interrupt Preemption in the ARM Kernel
## 1.1 Overview of Kernel Preemption
In the Linux kernel running on ARM/ARM64 architectures, preemption refers to the ability of the kernel to interrupt (preempt) a currently running task to allow a higher-priority task to execute.
## 1.2 Levels of Kernel Preemption
- No Forced Preemption (Vanilla Linux) — standard throughput-oriented kernel
- Voluntary Preemption — designated preemption points inserted in long-running kernel code
- Kernel Preemption (CONFIG_PREEMPT) — low-latency desktop; kernel code can be preempted at almost any point
- Fully Preempted (PREEMPT_RT patch) — interrupt disabling is removed, priority inheritance is enforced; provides real-time guarantees
## 1.3 PREEMPT_RT on ARM
As of September 2024, PREEMPT_RT was fully merged into mainline Linux on supported architectures including ARM64 (as well as x86, x86_64, and RISC-V). This means full real-time preemption is now available natively on ARM64 without needing out-of-tree patches.
For an architecture to support PREEMPT_RT, kernel preemption must be supported and CONFIG_ARCH_NO_PREEMPT must remain unselected.
## 1.4 How Interrupt Preemption Works on ARM Cortex-M (Bare Metal / RTOS)
On ARM Cortex-M processors (used in microcontrollers), the NVIC (Nested Vectored Interrupt Controller) manages interrupt preemption:
- An interrupt with higher priority (numerically lower value) can preempt a currently executing ISR of lower priority
- Preemption priority determines whether an ISR can be interrupted by another
- Sub-priority determines the order of execution when multiple interrupts of the same preemption priority are pending simultaneously
- Features like tail-chaining and late-arriving preemption optimize interrupt handling
## 1.5 ARM Exception Model Overview
An exception is defined in the ARM specification as "a condition that changes the normal flow of control in a program." The terms "interrupt" and "exception" are often used interchangeably; however, in ARM documentation, "interrupt" is used to describe a type of "exception."
### Exception Types in ARM Cortex-M:
- Reset
- NMI (Non-Maskable Interrupt)
- HardFault
- MemManage
- BusFault
- UsageFault
- SVCall
- PendSV
- SysTick
- External Interrupts (IRQ0 - IRQn)
## 1.6 NVIC Configuration and Priority Registers
The NVIC provides:
- Programmable priority levels for each interrupt
- Priority grouping (preemption priority vs sub-priority split)
- Interrupt enable/disable control
- Pending interrupt management
- Active interrupt status
## 1.7 Advanced Exception Topics
### Exception Entry and Exit
When an exception occurs, the processor automatically saves context (R0-R3, R12, LR, PC, xPSR) to the stack and loads the exception handler address.
### Tail-Chaining
When one exception finishes and another is pending, the processor skips the unstacking/restacking sequence, reducing latency.
### Late-Arriving Preemption
If a higher-priority exception arrives during the stacking phase of a lower-priority exception, the processor switches to the higher-priority handler without completing the original entry sequence.
### Lazy State Preservation
For floating-point context, the processor reserves stack space but defers actual FP register saving until the handler uses FP instructions.
## 1.8 Key Considerations for Preemptible Kernels
- With CONFIG_PREEMPT enabled, a preemptible kernel introduces SMP-like locking issues — concurrency and reentrancy must be handled carefully
- Interrupt preemption can cause deadlock if a higher-priority interrupt waits on a resource locked by a lower-priority one
- The kernel uses preempt_disable() / preempt_enable() and spinlocks to protect critical sections
- Per-CPU variables must be accessed with preemption disabled
- Sleeping in atomic context (with preemption disabled or in interrupt context) is a bug
## 1.9 Preemption Control APIs in Linux Kernel
preempt_disable()       // Increment preempt count, disable preemption
preempt_enable()        // Decrement preempt count, re-enable preemption
preempt_count()         // Get current preempt count
in_interrupt()          // Check if in interrupt context
in_atomic()            // Check if in atomic context (preemption disabled)
local_irq_disable()    // Disable local interrupts
local_irq_enable()     // Enable local interrupts
local_irq_save(flags)  // Save and disable local interrupts
local_irq_restore(flags) // Restore interrupt state
## 1.10 Locking in Preemptible Kernels
| Lock Type | Preemption | Interrupts | Sleep Allowed |
| spin_lock() | Disabled | Enabled | No |
| spin_lock_irq() | Disabled | Disabled | No |
| spin_lock_irqsave() | Disabled | Disabled (saved) | No |
| mutex_lock() | Enabled | Enabled | Yes |
| rw_lock() | Disabled | Enabled | No |
| semaphore | Enabled | Enabled | Yes |


# Part 2: Interrupt Storm Scenarios in Linux Kernel
## 2.1 Definition
An interrupt storm (also called an interrupt flood or IRQ storm) is a condition where a hardware interrupt is asserted at such a high rate that the CPU spends most or all of its time servicing interrupts, leaving no cycles for normal processing. This results in a livelock condition where the system appears frozen.
## 2.2 Kernel-Level Interrupt Storm Scenarios
### Scenario 1: Shared IRQ Line — Faulty Driver Not Clearing Interrupt
When multiple devices share the same IRQ line (common on PCI), the OS queries each registered driver to check if the interrupt originated from its hardware. A faulty driver may always claim "yes" (returning IRQ_HANDLED) without actually servicing the interrupt, preventing the original device from getting its interrupt cleared. This causes continuous re-assertion of the interrupt signal.
Root Cause: Driver bug — incorrect IRQ_HANDLED return without servicing hardware.
Kernel Detection: The note_interrupt() function tracks unhandled interrupts. After 100,000 unhandled interrupts, the kernel disables the IRQ line and logs "nobody cared."
### Scenario 2: Level-Triggered Interrupt Not Acknowledged/Cleared
If a device asserts a level-triggered interrupt and the kernel driver fails to instruct the hardware to de-assert it (e.g., doesn't read a status register or write an ACK), the interrupt stays asserted permanently, flooding the CPU.
Root Cause: Driver fails to clear the interrupt source in hardware.
Symptoms: Specific IRQ count in /proc/interrupts increases rapidly; system becomes unresponsive.
### Scenario 3: IRQ Handler Registered Before Hardware Reset
When a driver registers its IRQ handler before resetting the hardware to a known state during probe, the device may already be asserting interrupts, causing a storm before the driver is ready to service them.
Root Cause: Incorrect driver initialization order.
Fix: Reset hardware to known state before calling request_irq() or devm_request_irq().
### Scenario 4: PCIe/PCI Controller Interrupt Storm
Unhandled or unmasked PCIe controller interrupt events can cause a storm. This includes:
- Interrupt group events not properly masked before registration
- PCIe error interrupts (AER) firing continuously for persistent errors
- MSI/MSI-X misconfiguration causing repeated interrupt delivery
Root Cause: PCIe subsystem misconfiguration or persistent hardware errors.
### Scenario 5: CMCI (Corrected Machine Check Interrupt) Storm
Intel hardware delivers corrected machine check interrupts when error rates exceed a programmable threshold. If the error is persistent (e.g., degrading memory DIMM), CPUs receive a constant influx of CMCI interrupts.
Root Cause: Persistent correctable hardware errors (typically memory).
Kernel Mitigation: The Linux MCE subsystem detects CMCI storms and switches from interrupt mode to polling mode. Logs show "CMCI storm detected" and "CMCI storm subsided."
### Scenario 6: Network/NIC Interrupt Storm (High Packet Rate)
Under heavy network load, the NIC generates interrupts faster than the CPU can process packets. Without interrupt coalescing or NAPI (New API) polling, this causes an interrupt storm that starves the CPU from doing useful work.
Root Cause: High packet rate exceeding CPU processing capacity.
Scenarios that trigger this:
- DDoS attacks flooding the network interface
- Broadcast storms on the network
- Misconfigured network causing packet loops
- High-throughput legitimate traffic without proper tuning
Kernel Mitigation: NAPI (New API) — the network stack switches from interrupt-driven to polling mode under high load.
### Scenario 7: SMBus/I2C Interrupt Storm
Misconfigured or faulty bus controllers like the i801 SMBus can generate an unusually high number of interrupts, causing system lockups.
Root Cause: SMBus controller hardware fault or firmware misconfiguration.
Common Affected Hardware: Intel i801 SMBus controller on various motherboards.
Fix: Disable SMBus interrupt mode or update BIOS/firmware.
### Scenario 8: PCIe/PCI ISA Compatibility Mode Mismatch
PCI cards configured to operate in ISA compatibility mode cannot properly interact with ISA interrupt routing, causing interrupts to never be cleared by the OS.
Root Cause: Legacy compatibility mode conflicts with modern interrupt routing.
Historical Context: This was a known issue in older FreeBSD versions and early Linux kernels.
### Scenario 9: Firmware/BIOS Misconfiguration
Incorrect BIOS/UEFI/coreboot settings cause spurious interrupts on specific IRQ lines (e.g., irq10, irq16, irq17).
Root Cause: ACPI table errors, incorrect interrupt routing tables, or BIOS bugs.
Common Scenarios:
- ACPI interrupt routing table (DSDT) errors
- Incorrect APIC mode settings
- Legacy PIC mode conflicts with APIC
- Incorrect IRQ polarity or trigger mode in ACPI
### Scenario 10: Hardware Fault / Metastability
Electrically unstable signals due to faulty hardware, loose connections, or metastability in components can generate spurious interrupts.
Root Cause: Physical hardware degradation or manufacturing defects.
Scenarios:
- Degrading capacitors on interrupt lines
- Loose PCIe/PCI card connections
- EMI (Electromagnetic Interference) on interrupt traces
- Prototype or amateur-built hardware with timing violations
### Scenario 11: Memory/IO/Interrupt Resource Conflicts
Resource conflicts between devices (overlapping memory, I/O, or interrupt assignments) can cause both devices to trigger interrupts incorrectly.
Root Cause: BIOS or OS resource allocation errors.
### Scenario 12: GPIO/External Interrupt Storm
External interrupt lines (e.g., from a CPLD or GPIO expander) that are not properly debounced or whose status is not cleared in time can re-trigger continuously.
Root Cause: Missing debounce logic, incorrect edge/level trigger configuration, or driver not clearing GPIO interrupt status.
Common in: Embedded ARM platforms with external interrupt sources.
### Scenario 13: EDAC/Error Detection Interrupt Storm
Hardware error detection subsystems (L1/L2 cache EDAC, network cache EDAC, PCIe error monitors) can generate interrupt storms when persistent correctable errors exceed thresholds.
Root Cause: Persistent hardware errors triggering continuous error reporting interrupts.
### Scenario 14: Timer Interrupt Storm
Misconfigured or faulty timer hardware (HPET, LAPIC timer, ARM architected timer) generating interrupts at an unexpectedly high rate.
Root Cause: Timer misconfiguration, clocksource instability, or hardware fault.
### Scenario 15: USB Controller Interrupt Storm
USB host controllers (EHCI, xHCI) can generate interrupt storms due to:
- Faulty USB devices continuously signaling
- Port status change interrupts not being cleared
- USB hub generating continuous connect/disconnect events
Root Cause: Faulty USB device or controller driver bug.
### Scenario 16: Thermal/Power Management Interrupt Storm
Thermal sensors or power management controllers generating continuous interrupts when temperature thresholds are persistently exceeded.
Root Cause: Overheating hardware or misconfigured thermal thresholds.
### Scenario 17: Disk/Storage Controller Interrupt Storm
Storage controllers (AHCI, NVMe) generating excessive interrupts due to:
- Continuous error conditions on failing drives
- Misconfigured interrupt coalescing
- RAID controller with degraded array generating continuous alerts
Root Cause: Failing storage media or controller misconfiguration.
## 2.3 Impact on Userspace
### Livelock — System Appears Frozen
The kernel spends all CPU time servicing interrupts, leaving no cycles for scheduling userspace processes. The system appears completely unresponsive ("frozen") even though it's technically still running.
### Kernel Hang / Watchdog Timeout (HWT)
Interrupt storms prevent the kernel scheduler from running, triggering hardware watchdog timeouts (HWT) that can result in device reboots.
### Kernel Panic from Asynchronous SError Interrupt
On ARM platforms, hardware faults generating continuous SError interrupts can lead to kernel panics and unexpected reboots.
### Application Latency Spikes
Even if the storm doesn't completely lock the system, userspace applications experience severe latency spikes as CPU time is consumed by interrupt processing.
### Packet Drops and Network Failures
Network interrupt storms cause packet drops, connection timeouts, and degraded throughput visible to userspace applications.
## 2.4 Linux Kernel Mitigation Mechanisms
### Spurious IRQ Detection (note_interrupt)
The kernel tracks unhandled interrupts per IRQ line. After 100,000 unhandled interrupts (configurable), the IRQ is disabled and a warning is logged.
// From kernel/irq/spurious.c
if (action_ret == IRQ_NONE) {
    desc->irqs_unhandled++;
    if (unlikely(desc->irqs_unhandled > 99900)) {
        __report_bad_irq(irq, desc, action_ret);
        desc->status |= IRQ_DISABLED;
        disable_irq_nosync(irq);
    }
}
### CMCI Storm Detection
The MCE subsystem monitors CMCI interrupt rates and switches from interrupt mode to polling mode when a storm is detected.
### NIC Interrupt Coalescing
Hardware rate-limits interrupts via programmable timers. The NIC batches multiple events into a single interrupt.
### NAPI (New API) Polling
The network stack switches from interrupt-driven to polling mode under high load:
- First packet triggers interrupt
- Driver disables further interrupts and schedules NAPI poll
- Kernel polls for packets in softirq context
- When no more packets, re-enables interrupts
### Dynamic IRQ-to-Poll Switching
The OS switches from interrupt mode to polling when thresholds are exceeded, then switches back when the storm subsides.
### IRQ Thread Migration
The kernel can migrate interrupt handling to different CPUs to distribute the load.
### Interrupt Rate Limiting (Hardware)
Modern hardware implements programmable interrupt throttling registers (e.g., Intel NIC ITR - Interrupt Throttle Rate).
## 2.5 Diagnostic Commands and Tools
# Watch interrupt counts in real-time
watch -n1 cat /proc/interrupts

# Check for disabled IRQs (storm detection triggered)
dmesg | grep "nobody cared"
dmesg | grep "irq.*disabled"
dmesg | grep "CMCI storm"

# Check spurious interrupt count
cat /proc/spurious

# Monitor interrupt rate per second
while true; do cat /proc/interrupts; sleep 1; done | awk '/pattern/'

# Check IRQ affinity
cat /proc/irq/<IRQ_NUMBER>/smp_affinity

# View interrupt statistics
cat /proc/stat | grep intr

# Check softirq statistics
cat /proc/softirqs

# Trace interrupt handlers
echo 1 > /sys/kernel/debug/tracing/events/irq/irq_handler_entry/enable
cat /sys/kernel/debug/tracing/trace_pipe

# perf tool for interrupt analysis
perf stat -e irq:irq_handler_entry -a sleep 10
perf top -e irq:irq_handler_entry
## 2.6 Prevention Best Practices
- Always reset hardware before registering IRQ handlers in driver probe functions
- Use threaded IRQs (request_threaded_irq) for non-critical interrupts to reduce hard IRQ time
- Implement proper interrupt coalescing in hardware and driver configuration
- Use NAPI for network drivers to handle high packet rates
- Validate IRQ_HANDLED returns — only return IRQ_HANDLED if your device actually generated the interrupt
- Clear interrupt sources in hardware before returning from the handler
- Use devm_request_irq() to ensure proper cleanup on driver removal
- Configure appropriate interrupt priority grouping on ARM Cortex-M platforms
- Implement debouncing for GPIO/external interrupt your source documents. Monitor /proc/interrupts regularly for abnormal interrupt rates
- Keep firmware/BIOS updated to fix ACPI and interrupt routing bugs
- Use interrupt rate limiting where hardware supports it

# Part 3: Summary and Quick Reference
## Interrupt Storm Root Causes Summary
| Category | Scenarios | Common Fix |
| Driver Bugs | Shared IRQ not cleared, early IRQ registration, incorrect IRQ_HANDLED | Fix driver code |
| Hardware Faults | Metastability, degrading components, loose connections | Replace hardware |
| Firmware/BIOS | ACPI errors, incorrect routing, legacy conflicts | Update firmware |
| High Load | Network floods, DDoS, broadcast storms | NAPI, coalescing, rate limiting |
| Persistent Errors | CMCI, EDAC, failing storage | Replace faulty component, polling mode |
| Configuration | ISA/PCI conflicts, resource overlaps, thermal thresholds | Reconfigure system |

## Key Kernel Config Options
CONFIG_PREEMPT=y              # Enable kernel preemption
CONFIG_PREEMPT_RT=y           # Full real-time preemption (mainline since 6.12)
CONFIG_IRQ_FORCED_THREADING=y # Force threaded IRQ handlers
CONFIG_DEBUG_SHIRQ=y          # Debug shared IRQ handlers
CONFIG_LOCKUP_DETECTOR=y      # Detect soft/hard lockups
CONFIG_DETECT_HUNG_TASK=y     # Detect hung tasks
