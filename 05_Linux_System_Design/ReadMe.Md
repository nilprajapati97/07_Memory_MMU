<div align="center">

# 🐧 Linux Kernel Internals & System Design

### *A hands-on deep dive from silicon to scheduler — CPU simulation, kernel internals, and NVIDIA-style system design*

[![C](https://img.shields.io/badge/Language-C-blue?logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Python](https://img.shields.io/badge/Assembler-Python-yellow?logo=python&logoColor=white)](https://www.python.org/)
[![Linux](https://img.shields.io/badge/Platform-Linux-black?logo=linux&logoColor=white)](https://kernel.org/)
[![ARM](https://img.shields.io/badge/Target-ARM%20Cortex--A8-red?logo=arm&logoColor=white)](https://www.arm.com/)
[![QEMU](https://img.shields.io/badge/Emulator-QEMU-orange?logo=qemu&logoColor=white)](https://www.qemu.org/)

</div>

---

## 📋 Table of Contents

- [Overview](#-overview)
- [Repository Structure](#-repository-structure)
- [Module 1 — Single-Threaded CPU Simulator](#-module-1--single-threaded-cpu-simulator)
- [Module 2 — Linux Kernel Internals](#-module-2--linux-kernel-internals)
- [Module 3 — Memory & System Design Questions](#-module-3--memory--system-design-questions)
- [Module 4 — Scheduler Implementation](#-module-4--scheduler-implementation)
- [Module 5 — High-Frequency ISR Design Guide](#-module-5--high-frequency-isr-design-guide)
- [System Design — NVIDIA-Style Problems](#-system-design--nvidia-style-problems)
- [Interview Question Bank](#-interview-question-bank)
- [Quick Start](#-quick-start)

---

## 🎯 Overview

This repository is a structured exploration of **low-level systems engineering** — from building a CPU simulator from scratch in C, to understanding every major Linux kernel subsystem, to tackling system design problems that come up in NVIDIA, Google, and Meta infrastructure interviews.

The content is designed for engineers with **5–15 years of experience** who want to go beyond surface-level OS knowledge and build genuine hardware-aware intuition.

```
Five pillars of this repository:

  🖥️  CPU Simulator      →   🐧  Linux Kernel Deep Dive   →   🧮  Memory & System Design
  (build the hardware)        (understand the OS)              (25 NVIDIA-style problems)
  C · ARM · QEMU              18 chapters · 90+ files          5 categories · deep-dives

                          🗓️  Scheduler Implementation   →   ⚡  High-Frequency ISR Guide
                          (TCB, run-queue, ctx switch)        (5 patterns @ 100 kHz)
```

---

## 🗂️ Repository Structure

```
05_LKI_System_Design/
│
├── 01_SingleThreaded_CPU/          ← CPU simulator built from scratch
│   ├── Architecture/               ← 10 design documents (ISA → toolchain)
│   ├── Implementation/             ← C source, Python assembler, QEMU scripts
│   │   ├── src/                    ← cpu.c, alu.c, decoder.c, memory.c, main.c
│   │   ├── programs/               ← .asm source + assembled .bin programs
│   │   ├── tools/                  ← assembler.py (two-pass assembler)
│   │   └── qemu/                   ← full-system QEMU boot scripts + kernel images
│   └── Log_Analysis/               ← 10 real execution scenarios with analysis
│
├── 02_Understand_Linux_Kernel/     ← Deep dive into Linux Kernel (Robert Love)
│   ├── 00_Introduction/            ← History, architecture overview, monolithic vs microkernel
│   ├── 01_Getting_Started_With_The_Kernel/
│   ├── 02_Process_Management/
│   ├── 03_Process_Scheduling/
│   ├── 04_System_Calls/
│   ├── 05_Kernel_Data_Structures/
│   ├── 06_Interrupts_And_Interrupt_Handlers/
│   ├── 07_Bottom_Halves_And_Deferring_Work/
│   ├── 08_Intro_To_Kernel_Synchronization/
│   ├── 09_Kernel_Synchronization_Methods/
│   ├── 10_Timers_And_Time_Management/
│   ├── 11_Memory_Management/
│   ├── 12_Virtual_Filesystem/
│   ├── 13_Block_IO_Layer/
│   ├── 14_Process_Address_Space/
│   ├── 15_Page_Cache_And_Page_Writeback/
│   ├── 16_Devices_And_Modules/
│   ├── 17_Debugging/
│   └── INDEX/                      ← Master alphabetical index of all concepts
│
├── 03_Memory_Questions/            ← 25 NVIDIA-style system design problems + memory deep dives
│   ├── 01_Core_Kernel_OS_Design/
│   ├── 02_Concurrency_Synchronization/
│   ├── 03_Memory_Storage_Systems/
│   ├── 04_Device_Drivers_Hardware/
│   ├── 05_Performance_Optimization/
│   ├── System_Design                ← Full problem set (25 problems)
│   └── multithreading/
│
├── 04_Scheduler/                   ← Scheduler implementation & context-switching deep dive
│   ├── 01_Sample/                  ← Reference scheduler skeleton
│   ├── 02_Scheduler_Implementation/
│   └── 03_context_switchig_scheduler/
│
├── 05_High_Frequency_ISR_Design_Guide/  ← ISR design patterns for 100 kHz+ interrupts
│   ├── 00_High_Frequency_ISR_Design/
│   ├── 00_High_Frequency_ISR_Design_Guide.Md
│   ├── 01_LockFree_RingBuffer_SPSC/
│   ├── 02_DoubleBuffer_PingPong/
│   ├── 03_DMA_PingPong_HardwareOffload/
│   ├── 04_Linux_Kernel_BottomHalf/
│   └── 05_Atomics_MemoryOrdering_C11/
│
├── Questions.Md                    ← 120+ OS/kernel interview questions
└── ReadMe.Md                       ← This file
```

---

## 🖥️ Module 1 — Single-Threaded CPU Simulator

> **Goal:** Understand exactly how a CPU works by building one.

A **fully self-contained 32-bit RISC CPU simulator** written in C, with a custom ISA, Python assembler, and cross-compiled ARM support. No libraries. No shortcuts.

### Architecture Pipeline

```
┌──────────────┐     ┌───────────────┐     ┌──────────────────┐
│  .asm Source │────▶│ Python        │────▶│  .bin Binary     │
│ (fibonacci)  │     │ Assembler     │     │ (loaded at       │
└──────────────┘     │ (two-pass)    │     │  0x00010000)     │
                     └───────────────┘     └────────┬─────────┘
                                                    │
                              ┌─────────────────────▼──────────────────────┐
                              │              CPU Core (cpu.c)               │
                              │                                              │
                              │  ┌─────────┐  ┌─────────┐  ┌───────────┐  │
                              │  │  FETCH  │─▶│ DECODE  │─▶│  EXECUTE  │  │
                              │  │ mem[PC] │  │ decoder │  │   ALU /   │  │
                              │  └─────────┘  └─────────┘  │  handler  │  │
                              │                             └─────┬─────┘  │
                              │                             ┌─────▼─────┐  │
                              │                             │ WRITEBACK │  │
                              │                             │  Rd / PC  │  │
                              │                             └───────────┘  │
                              └──────────────────────────────────────────┘
```

### Key Components

| Component | File | Description |
|-----------|------|-------------|
| **CPU Core** | `src/cpu.c` | 4-stage fetch→decode→execute→writeback loop |
| **ALU** | `src/alu.c` | ADD, SUB, MUL, DIV, AND, OR, XOR, SHL, SHR + flags (Z, N, C, O) |
| **Decoder** | `src/decoder.c` | Splits 32-bit words into typed `Instruction` structs |
| **Memory** | `src/memory.c` | 1 MiB flat address space, little-endian, R/W accessors |
| **ISA** | `src/instructions.h` | Fixed 32-bit encoding: opcode[7:0] Rd[11:8] Rs1[15:12] Rs2[19:16] imm[31:20] |
| **Assembler** | `tools/assembler.py` | Two-pass Python assembler with label resolution |
| **QEMU Setup** | `qemu/run_qemu.sh` | Boots ARM binary under QEMU (vexpress-ca9 / BeagleBone Black) |

### Architecture Documents

| # | Document | What It Covers |
|---|----------|----------------|
| 00 | [Overview](./01_SingleThreaded_CPU/Architecture/00_Overview.md) | System diagram, design philosophy, data flow |
| 01 | [ISA](./01_SingleThreaded_CPU/Architecture/01_ISA.md) | Instruction encoding, opcode table, register aliases |
| 02 | [Memory Model](./01_SingleThreaded_CPU/Architecture/02_Memory_Model.md) | 1 MiB flat memory, address map |
| 03 | [ALU](./01_SingleThreaded_CPU/Architecture/03_ALU.md) | All arithmetic/logic operations, flag semantics |
| 04 | [Decoder](./01_SingleThreaded_CPU/Architecture/04_Decoder.md) | Bit extraction, sign extension, disassembler |
| 05 | [CPU Core](./01_SingleThreaded_CPU/Architecture/05_CPU_Core.md) | State machine, 4-stage cycle, `cpu_step` / `cpu_run` |
| 06 | [Assembler](./01_SingleThreaded_CPU/Architecture/06_Assembler.md) | Two-pass assembler, label resolution, syntax reference |
| 07 | [Toolchain & Build](./01_SingleThreaded_CPU/Architecture/07_Toolchain_Build.md) | Makefile, native vs ARM cross-compilation |
| 08 | [Fibonacci Walkthrough](./01_SingleThreaded_CPU/Architecture/08_Fibonacci_Walkthrough.md) | Step-by-step trace of Fibonacci execution |
| 09 | [Interview: CPU Design](./01_SingleThreaded_CPU/Architecture/09_Interview_CPU_Design.md) | CPU architecture interview Q&A |

### Log Analysis Scenarios

Ten real execution traces analyzed at the instruction level:

| # | Scenario | What It Demonstrates |
|---|----------|----------------------|
| 01 | [Normal Fibonacci Run](./01_SingleThreaded_CPU/Log_Analysis/01_Normal_Fibonacci_Run/) | Correct execution trace, register state |
| 02 | [Infinite Loop Detection](./01_SingleThreaded_CPU/Log_Analysis/02_Infinite_Loop_Detection/) | PC cycle detection, cycle limit enforcement |
| 03 | [Division By Zero](./01_SingleThreaded_CPU/Log_Analysis/03_Division_By_Zero/) | Fault handling, trap mechanism |
| 04 | [Stack Overflow](./01_SingleThreaded_CPU/Log_Analysis/04_Stack_Overflow/) | SP tracking, memory boundary violation |
| 05 | [Illegal Opcode Fault](./01_SingleThreaded_CPU/Log_Analysis/05_Illegal_Opcode_Fault/) | Undefined instruction exception |
| 06 | [Memory Out Of Bounds](./01_SingleThreaded_CPU/Log_Analysis/06_Memory_Out_Of_Bounds/) | Address space violation |
| 07 | [Function Call & Return](./01_SingleThreaded_CPU/Log_Analysis/07_Function_Call_Return/) | Call stack discipline, link register use |
| 08 | [Large Immediate Encoding](./01_SingleThreaded_CPU/Log_Analysis/08_Large_Immediate_Encoding/) | Sign extension, 12-bit immediate limits |
| 09 | [Off-By-One Loop Bug](./01_SingleThreaded_CPU/Log_Analysis/09_Off_By_One_Loop_Bug/) | Flag-based branch condition errors |
| 10 | [Flag Dependency Chain](./01_SingleThreaded_CPU/Log_Analysis/10_Flag_Dependency_Chain/) | Multi-instruction flag propagation |

---

## 🐧 Module 2 — Linux Kernel Internals

> **Goal:** Build a mental model of the entire Linux kernel — the same depth expected at NVIDIA/Google kernel/driver roles.

Based on **Robert Love's *Linux Kernel Development* (3rd Edition)** with 18 chapters across 90+ markdown files, each containing real kernel code snippets, data structure diagrams, and design rationale.

### Kernel Subsystem Map

```
                        ┌───────────────────────────────────┐
                        │         User Space                │
                        │  (processes, libraries, shells)   │
                        └──────────────┬────────────────────┘
                                       │  System Calls (Ch04)
                        ┌──────────────▼────────────────────┐
                        │         Linux Kernel              │
                        │                                   │
          ┌─────────────┤  Process Management (Ch02)        │
          │             │  ├── task_struct                  │
          │             │  ├── fork / clone / vfork         │
          │             │  └── threads                      │
          │             │                                   │
          │             │  Scheduling (Ch03)                │
          │             │  ├── CFS + Red-Black Tree         │
          │             │  ├── RT scheduling classes        │
          │             │  └── Load balancing               │
          │             │                                   │
     Sync │             │  Memory Management (Ch11, 14, 15) │
     (Ch  │             │  ├── Buddy allocator              │
     08,  │             │  ├── Slab/SLUB allocator          │
     09)  │             │  ├── Virtual memory areas (VMA)   │
          │             │  ├── Page tables + TLB            │
          │             │  └── Page cache + writeback       │
          │             │                                   │
          │             │  VFS Layer (Ch12)                 │
          │             │  ├── superblock / inode / dentry  │
          │             │  └── file_operations              │
          │             │                                   │
          └─────────────┤  Interrupts & Bottom Halves       │
                        │  (Ch06, 07)                       │
                        │  ├── IRQ handler registration     │
                        │  ├── Softirqs / Tasklets          │
                        │  └── Workqueues                   │
                        │                                   │
                        │  Devices & Modules (Ch16)         │
                        │  ├── kobject / kset / sysfs       │
                        │  ├── Platform devices             │
                        │  └── LKM loading/unloading        │
                        │                                   │
                        │  Debugging (Ch17)                 │
                        │  ├── printk / dynamic debug       │
                        │  ├── ftrace / perf / KGDB         │
                        │  └── KASAN / KCSAN               │
                        └───────────────────────────────────┘
```

### Chapter Index

| Chapter | Topic | Key Concepts |
|---------|-------|--------------|
| [00](./02_Understand_Linux_Kernel/00_Introduction/) | Introduction | Unix history, kernel architecture, monolithic vs microkernel |
| [01](./02_Understand_Linux_Kernel/01_Getting_Started_With_The_Kernel/) | Getting Started | Source tree layout, `make menuconfig`, building & installing |
| [02](./02_Understand_Linux_Kernel/02_Process_Management/) | Process Management | `task_struct`, `fork`/`clone`, COW, threads, zombie/orphan |
| [03](./02_Understand_Linux_Kernel/03_Process_Scheduling/) | Process Scheduling | CFS, `vruntime`, Red-Black tree, RT classes, load balancing |
| [04](./02_Understand_Linux_Kernel/04_System_Calls/) | System Calls | Syscall path, `syscall_table`, `copy_from_user`, adding syscalls |
| [05](./02_Understand_Linux_Kernel/05_Kernel_Data_Structures/) | Kernel Data Structures | `list_head`, `kfifo`, IDR maps, Red-Black trees, Radix trees |
| [06](./02_Understand_Linux_Kernel/06_Interrupts_And_Interrupt_Handlers/) | Interrupts | IRQ basics, ISR registration, IRQ stacks, interrupt control |
| [07](./02_Understand_Linux_Kernel/07_Bottom_Halves_And_Deferring_Work/) | Bottom Halves | Softirqs, tasklets, workqueues, choosing a mechanism |
| [08](./02_Understand_Linux_Kernel/08_Intro_To_Kernel_Synchronization/) | Synchronization Intro | Critical regions, race conditions, locking, deadlocks |
| [09](./02_Understand_Linux_Kernel/09_Kernel_Synchronization_Methods/) | Synchronization Methods | Atomics, spinlocks, R/W locks, semaphores, mutex, RCU, seqlocks |
| [10](./02_Understand_Linux_Kernel/10_Timers_And_Time_Management/) | Timers | Jiffies, `HZ`, kernel timers, hrtimers, clocksource/clockevents |
| [11](./02_Understand_Linux_Kernel/11_Memory_Management/) | Memory Management | Pages/zones, buddy allocator, SLUB, `kmalloc`/`vmalloc`, GFP flags |
| [12](./02_Understand_Linux_Kernel/12_Virtual_Filesystem/) | Virtual Filesystem | VFS overview, superblock, inode, dentry, file object, dcache |
| [13](./02_Understand_Linux_Kernel/13_Block_IO_Layer/) | Block I/O Layer | Block devices, `bio` structure, I/O schedulers (BFQ/Kyber), request queue |
| [14](./02_Understand_Linux_Kernel/14_Process_Address_Space/) | Process Address Space | `mm_struct`, VMAs, page tables, page faults, `mmap` |
| [15](./02_Understand_Linux_Kernel/15_Page_Cache_And_Page_Writeback/) | Page Cache | `address_space`, writeback, dirty ratio, `pdflush`/kworker |
| [16](./02_Understand_Linux_Kernel/16_Devices_And_Modules/) | Devices & Modules | Device model, kobject, sysfs, platform devices, LKM parameters |
| [17](./02_Understand_Linux_Kernel/17_Debugging/) | Debugging | `printk`, KGDB, KASAN/KCSAN, ftrace, perf, dynamic debug |

> 📖 **Master Index:** [`INDEX/Master_Index.md`](./02_Understand_Linux_Kernel/INDEX/Master_Index.md) — alphabetical lookup of every kernel concept, struct, and function across all chapters.

---

## 🧮 Module 3 — Memory & System Design Questions

> **Goal:** Apply kernel internals to open-ended design and memory problems the way they're posed in senior systems interviews.

The [`03_Memory_Questions/`](./03_Memory_Questions) module groups the **25 NVIDIA-style system design problems** by category, alongside dedicated memory and multithreading deep-dives.

| Folder | Focus |
|--------|-------|
| [`01_Core_Kernel_OS_Design/`](./03_Memory_Questions/01_Core_Kernel_OS_Design) | Scheduler, VM, interrupt subsystem, kernel modules, lock-free IO |
| [`02_Concurrency_Synchronization/`](./03_Memory_Questions/02_Concurrency_Synchronization) | Spinlock vs mutex vs RCU, multi-core scheduling, deadlock detection, slab |
| [`03_Memory_Storage_Systems/`](./03_Memory_Questions/03_Memory_Storage_Systems) | Page cache, zero-copy, FS design, block I/O scheduler, DMA transfer |
| [`04_Device_Drivers_Hardware/`](./03_Memory_Questions/04_Device_Drivers_Hardware) | GPU driver, PCIe framework, GPU power mgmt, network driver, tracing |
| [`05_Performance_Optimization/`](./03_Memory_Questions/05_Performance_Optimization) | eBPF-lite profiler, real-time low-latency, context-switch tuning, NUMA, IPC |
| [`System_Design`](./03_Memory_Questions/System_Design) | Full 25-problem set with categories and "what NVIDIA expects" |
| [`multithreading/`](./03_Memory_Questions/multithreading) | Threading patterns, primitives, and worked examples |

---

## 🗓️ Module 4 — Scheduler Implementation

> **Goal:** Move from theory (Ch03 of Module 2) to a working scheduler with real context switching.

The [`04_Scheduler/`](./04_Scheduler) module walks through a userspace scheduler — TCB layout, run/ready queues, and the mechanics of context switching — with ftrace-based analysis of multithreaded behaviour.

| Folder | What It Covers |
|--------|----------------|
| [`01_Sample/`](./04_Scheduler/01_Sample) | Minimal scheduler skeleton to anchor the design |
| [`02_Scheduler_Implementation/`](./04_Scheduler/02_Scheduler_Implementation) | Full implementation: TCB, ready queue, scheduling policy |
| [`03_context_switchig_scheduler/`](./04_Scheduler/03_context_switchig_scheduler) | Context-switch mechanics + ftrace logs and explanations |

---

## ⚡ Module 5 — High-Frequency ISR Design Guide

> **Goal:** Design ISRs that survive 100 kHz interrupt rates — only ~10 µs per interrupt.

The [`05_High_Frequency_ISR_Design_Guide/`](./05_High_Frequency_ISR_Design_Guide) module catalogs the five canonical patterns used in production embedded and kernel code to keep ISRs minimal and offload heavy work safely.

| # | Pattern | Why It Matters |
|---|---------|----------------|
| 00 | [Design Guide Overview](./05_High_Frequency_ISR_Design_Guide/00_High_Frequency_ISR_Design_Guide.Md) | The golden rule: ISR pushes, worker pulls |
| 01 | [Lock-Free Ring Buffer (SPSC)](./05_High_Frequency_ISR_Design_Guide/01_LockFree_RingBuffer_SPSC) | Single-producer / single-consumer queue between ISR and worker |
| 02 | [Double Buffer (Ping-Pong)](./05_High_Frequency_ISR_Design_Guide/02_DoubleBuffer_PingPong) | Two buffers swapped under a flag — no copy in ISR |
| 03 | [DMA Ping-Pong (Hardware Offload)](./05_High_Frequency_ISR_Design_Guide/03_DMA_PingPong_HardwareOffload) | Let the DMA engine do the producing |
| 04 | [Linux Kernel Bottom Halves](./05_High_Frequency_ISR_Design_Guide/04_Linux_Kernel_BottomHalf) | Softirqs, tasklets, workqueues, threaded IRQs |
| 05 | [Atomics & Memory Ordering (C11)](./05_High_Frequency_ISR_Design_Guide/05_Atomics_MemoryOrdering_C11) | `memory_order_*`, acquire/release, fences |

---

## 🎯 System Design — NVIDIA-Style Problems

> **Goal:** Practice designing real systems at the kernel/hardware boundary the way it is expected in senior systems interviews.

25 open-ended problems across 5 categories — each designed to probe trade-offs, cache-line awareness, hardware interaction, and failure modes.

### Problem Categories

| Category | Problems |
|----------|----------|
| 🧠 **Core Kernel & OS Design** | Process scheduler for GPU-heavy workloads · VM management with NUMA · interrupt subsystem · kernel module system · lock-free I/O queue |
| ⚡ **Concurrency & Synchronization** | Scalable locking (spinlock vs mutex vs RCU) · RCU from scratch · multi-core task scheduling · deadlock detection · thread-safe slab allocator |
| 💾 **Memory & Storage Systems** | Page cache · zero-copy transfer · high-performance filesystem · block I/O scheduler for SSD+GPU · DMA transfer system |
| 🖥️ **Device Drivers & Hardware** | GPU driver architecture · PCIe communication framework · GPU power management · high-throughput network driver · kernel tracing system |
| 🚀 **Performance & Optimization** | Kernel profiling (eBPF-lite) · real-time low-latency system · minimize context switch overhead · NUMA-aware allocator · high-throughput IPC |

### How to Approach a Design Problem

When asked to **design a GPU driver**, for example, a strong answer covers all layers:

```
User Space                    Kernel Space                 Hardware
───────────                   ────────────                 ────────
Application
    │  ioctl / mmap
    ▼
Driver API ──────────────▶ Command Queue ──────────────▶  GPU Registers
                           Memory Mapper                   DMA Engine
                           Interrupt Handler  ◀────────    IRQ Line
                           Fence / Sync                    VRAM
                           Error / Reset Path
```

> 📄 See [`03_Memory_Questions/System_Design`](./03_Memory_Questions/System_Design) for the full problem set.

---

## 📚 Interview Question Bank

> 120+ questions covering every subsystem — calibrated to **10-year experience** depth.

The [`Questions.Md`](./Questions.Md) file contains a comprehensive bank organized by topic:

| Topic Area | Question Count | Sample Question |
|------------|---------------|-----------------|
| **Core OS Fundamentals** | 10 | What happens from power-on to `main()` in a Linux system? |
| **Processes, Threads & Scheduling** | 10 | Explain Linux CFS scheduling. How does `vruntime` work? |
| **Synchronization & Concurrency** | 10 🔥 | What is the ABA problem in lock-free programming? |
| **Memory Management** | 15 🔥 | What is the slab allocator and why does it exist? |
| **File Systems & Storage** | 10 | What is the VFS layer and why is it important? |
| **IPC & Networking** | 10 | `select` vs `poll` vs `epoll` — when does each win? |
| **Linux Kernel / Driver** | 15 🔥 | What is RCU and where is it useful in the kernel? |
| **Architecture (x86 + ARM)** | 10 | Why does x86 code sometimes fail on ARM without barriers? |
| **Virtualization & Containers** | 6 | How do cgroups and namespaces isolate containers? |
| **Performance & Debugging** | 10 | System is 100% CPU but throughput dropped — how do you debug? |
| **Design Problems** | 6 | Design a bounded lock-free ring buffer. |

> 🎯 **NVIDIA Focus:** They probe *performance*, *parallelism*, *hardware-aware thinking*, and *real-world debugging* — not definitions.

---

## ⚡ Quick Start

### Build & Run the CPU Simulator (Native Linux)

```bash
cd 01_SingleThreaded_CPU/Implementation

# Build native binary
make

# Assemble the Fibonacci program
python3 tools/assembler.py programs/test_fibonacci.asm programs/test_fibonacci.bin

# Run the simulator
./cpu_sim programs/test_fibonacci.bin
```

### Build & Run Under QEMU (ARM)

```bash
cd 01_SingleThreaded_CPU/Implementation

# Cross-compile for ARM Cortex-A8
make arm

# Launch QEMU full-system emulation
cd qemu && ./run_qemu.sh
```

### Expected Output (Fibonacci trace)

```
[CPU] PC=0x00010000  FETCH  raw=0x...
[CPU] PC=0x00010004  ADD    R3, R1, R2
[CPU] PC=0x00010008  ...
...
[DONE] Cycles: 42  R0=55  (fib(10) = 55)
```

---

<div align="center">

**Built for engineers who want to understand systems all the way down.**

*CPU → Kernel → Design → Scheduler → ISR*

</div>
