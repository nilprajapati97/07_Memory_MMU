# Q34: GPU Profiling and Tracing with ftrace/perf

**Section:** System Design | **Difficulty:** Hard | **Topics:** `TRACE_EVENT`, tracepoints, per-CPU ring buffer, `ktime_get`, `debugfs`, perf counters, GPU profiling

---

## Question

Implement GPU kernel launch tracing using `TRACE_EVENT` and per-CPU performance counters.

---

## Answer

```c
/* ─── Part 1: TRACE_EVENT definition (in header, e.g., gpu_trace.h) ──────*/

/* Define this file as the trace header */
#define TRACE_SYSTEM gpu_driver

#if !defined(_GPU_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _GPU_TRACE_H

#include <linux/tracepoint.h>
#include <linux/types.h>

/*
 * TRACE_EVENT(name, proto, args, struct, assign, print):
 * Registers a tracepoint that ftrace, perf, and BPF can attach to.
 */
TRACE_EVENT(gpu_kernel_launch,

    /* Prototype: what arguments this trace event takes */
    TP_PROTO(u64 ctx_id, u64 func_addr,
             u32 grid_x, u32 grid_y, u32 grid_z,
             u32 block_x, u32 block_y, u32 block_z),

    /* Args: how to refer to the arguments in ASSIGN/PRINT */
    TP_ARGS(ctx_id, func_addr, grid_x, grid_y, grid_z,
            block_x, block_y, block_z),

    /* Ring buffer struct: fields stored per event occurrence */
    TP_STRUCT__entry(
        __field(u64, ctx_id)
        __field(u64, func_addr)
        __field(u32, grid_x)
        __field(u32, grid_y)
        __field(u32, grid_z)
        __field(u32, block_x)
        __field(u32, block_y)
        __field(u32, block_z)
    ),

    /* Assign: how to fill the struct fields from the args */
    TP_fast_assign(
        __entry->ctx_id    = ctx_id;
        __entry->func_addr = func_addr;
        __entry->grid_x    = grid_x;
        __entry->grid_y    = grid_y;
        __entry->grid_z    = grid_z;
        __entry->block_x   = block_x;
        __entry->block_y   = block_y;
        __entry->block_z   = block_z;
    ),

    /* Print: human-readable format for trace output */
    TP_printk("ctx=%llu func=0x%llx grid=(%u,%u,%u) block=(%u,%u,%u)",
              __entry->ctx_id, __entry->func_addr,
              __entry->grid_x, __entry->grid_y, __entry->grid_z,
              __entry->block_x, __entry->block_y, __entry->block_z)
);

TRACE_EVENT(gpu_kernel_complete,
    TP_PROTO(u64 ctx_id, u64 func_addr, u64 exec_time_ns),
    TP_ARGS(ctx_id, func_addr, exec_time_ns),
    TP_STRUCT__entry(
        __field(u64, ctx_id)
        __field(u64, func_addr)
        __field(u64, exec_time_ns)
    ),
    TP_fast_assign(
        __entry->ctx_id       = ctx_id;
        __entry->func_addr    = func_addr;
        __entry->exec_time_ns = exec_time_ns;
    ),
    TP_printk("ctx=%llu func=0x%llx exec_time=%lluns",
              __entry->ctx_id, __entry->func_addr, __entry->exec_time_ns)
);

#endif /* _GPU_TRACE_H */

/* Include trace/define magic */
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE gpu_trace
#include <trace/define_trace.h>


/* ─── Part 2: Per-CPU performance counters ────────────────────────────────*/
#include <linux/percpu.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

struct gpu_perf_buf {
    u64  sm_active_cycles;    /* GPU SM active cycles             */
    u64  dram_read_bytes;     /* DRAM read bandwidth (bytes)       */
    u64  dram_write_bytes;    /* DRAM write bandwidth (bytes)      */
    u64  l2_hit_count;        /* L2 cache hits                    */
    u64  pcie_tx_bytes;       /* PCIe TX bytes (GPU→host)         */
    u64  pcie_rx_bytes;       /* PCIe RX bytes (host→GPU)         */
    u64  kernel_launches;     /* number of kernel launches        */
    u64  total_exec_ns;       /* cumulative execution time (ns)   */
} ____cacheline_aligned;

DEFINE_PER_CPU(struct gpu_perf_buf, gpu_perf_counters);

/* ─── Emit trace event and update per-CPU counters on launch ─────────────*/
void gpu_trace_kernel_launch(struct gpu_context *ctx, u64 func_addr,
                               u32 gx, u32 gy, u32 gz,
                               u32 bx, u32 by, u32 bz)
{
    /* Fire tracepoint — zero cost if no ftrace/perf consumer attached */
    trace_gpu_kernel_launch(ctx->ctx_id, func_addr,
                             gx, gy, gz, bx, by, bz);

    this_cpu_inc(gpu_perf_counters.kernel_launches);
}

/* ─── On GPU completion interrupt: update exec time ──────────────────────*/
void gpu_trace_kernel_complete(struct gpu_context *ctx, u64 func_addr,
                                 ktime_t launch_time)
{
    u64 exec_ns = ktime_to_ns(ktime_sub(ktime_get(), launch_time));

    trace_gpu_kernel_complete(ctx->ctx_id, func_addr, exec_ns);

    this_cpu_add(gpu_perf_counters.total_exec_ns, exec_ns);
}

/* ─── debugfs: expose perf counters to userspace ─────────────────────────*/
static int gpu_perf_show(struct seq_file *m, void *v)
{
    struct gpu_perf_buf total = {};
    int cpu;

    /* Aggregate per-CPU counters */
    for_each_possible_cpu(cpu) {
        const struct gpu_perf_buf *pb = &per_cpu(gpu_perf_counters, cpu);
        total.kernel_launches  += pb->kernel_launches;
        total.total_exec_ns    += pb->total_exec_ns;
        total.dram_read_bytes  += pb->dram_read_bytes;
        total.dram_write_bytes += pb->dram_write_bytes;
        total.pcie_tx_bytes    += pb->pcie_tx_bytes;
        total.pcie_rx_bytes    += pb->pcie_rx_bytes;
    }

    seq_printf(m, "kernel_launches:   %llu\n", total.kernel_launches);
    seq_printf(m, "total_exec_ns:     %llu\n", total.total_exec_ns);
    seq_printf(m, "avg_exec_ns:       %llu\n",
               total.kernel_launches ? total.total_exec_ns / total.kernel_launches : 0);
    seq_printf(m, "dram_read_MB:      %llu\n", total.dram_read_bytes >> 20);
    seq_printf(m, "dram_write_MB:     %llu\n", total.dram_write_bytes >> 20);
    seq_printf(m, "pcie_tx_MB:        %llu\n", total.pcie_tx_bytes >> 20);
    seq_printf(m, "pcie_rx_MB:        %llu\n", total.pcie_rx_bytes >> 20);

    return 0;
}

DEFINE_SHOW_ATTRIBUTE(gpu_perf);  /* creates gpu_perf_open with seq_file */

int gpu_debugfs_init(struct gpu_device *gpu, struct dentry *parent)
{
    debugfs_create_file("gpu_perf", 0444, parent, gpu, &gpu_perf_fops);
    return 0;
}
```

---

## Explanation

### Core Concept

Two complementary profiling mechanisms:

1. **`TRACE_EVENT`** — structured kernel tracepoints. Zero overhead when disabled (tracepoints compile to a conditional NOP). When enabled via `echo 1 > /sys/kernel/debug/tracing/events/gpu_driver/gpu_kernel_launch/enable`, every launch is recorded in the ftrace ring buffer with nanosecond timestamps.

2. **Per-CPU perf counters** — always-on lightweight statistics. No lock overhead. Aggregated for `nvidia-smi` equivalent display via debugfs.

### Key APIs / Macros Used

| API | Purpose |
|-----|---------|
| `TRACE_EVENT(name, proto, args, struct, assign, print)` | Define a kernel tracepoint |
| `trace_NAME(args...)` | Emit a tracepoint event (NOP if disabled) |
| `TP_PROTO` | Prototype of trace function |
| `TP_STRUCT__entry` | Fields stored in ring buffer per event |
| `TP_fast_assign` | Assignment block (runs in NMI-safe context) |
| `TP_printk` | Printf format for human-readable trace output |
| `DEFINE_PER_CPU(type, name)` | Per-CPU performance buffer |
| `this_cpu_inc/add` | Update per-CPU counter (no lock) |
| `ktime_get()` | Get current time (monotonic, nanosecond resolution) |
| `ktime_sub(a, b)` | Compute time difference |
| `seq_printf(m, fmt, ...)` | Output to debugfs seq_file |
| `DEFINE_SHOW_ATTRIBUTE(name)` | Define debugfs file_operations from show function |
| `debugfs_create_file(name, mode, parent, data, fops)` | Create debugfs entry |

### Trade-offs & Pitfalls

- **`TRACE_EVENT` overhead when enabled.** When a tracepoint is active, it writes to the ftrace ring buffer — involves a spinlock-protected per-CPU ring buffer write. Keep `TP_fast_assign` minimal and avoid sleeping/locks inside it.
- **`TP_printk` is lazy.** The format string is only formatted when the trace is read from `/sys/kernel/debug/tracing/trace` — not on the hot path. This is a key performance feature.

### NVIDIA / GPU Context

NVIDIA Nsight Systems uses Linux perf + `TRACE_EVENT` to collect:
- CUDA kernel launches (via GPU driver tracepoints)
- GPU memory operations (`cuMemcpy` tracepoints)
- NVTX markers (user-defined via `nvtxRangePush`)
- PCIe bandwidth counters from hardware performance counters

---

## Cross Questions & Answers

**CQ1: How does ftrace avoid overhead when no consumer is attached?**
> `TRACE_EVENT` compiles to a conditional branch: `if (unlikely(tracepoint_enabled))` — a single CPU flag check. When no consumer is attached, `tracepoint_enabled = 0` and the entire trace body is skipped. The CPU branch predictor predicts "not taken" since the flag is always 0 in normal operation — resulting in near-zero overhead (< 1 cycle in the non-tracing hot path). This is why NVIDIA ships tracepoints in production driver builds.

**CQ2: What is the difference between `ftrace` tracepoints and `perf_event` hardware counters?**
> `ftrace` tracepoints: software events inserted at specific code locations in the driver. They capture semantic information (context ID, function address, launch parameters). Low overhead, but limited to what the developer explicitly instruments. `perf_event` hardware counters: GPU hardware PMU (Performance Monitoring Unit) counters that count hardware events (L2 cache misses, SM utilization, DRAM bandwidth). They measure hardware behavior with zero software overhead. Nsight Compute uses HW PMU counters; Nsight Systems uses ftrace tracepoints + HW counters.

**CQ3: How would you use eBPF to trace GPU kernel launches without modifying the driver?**
> Attach a BPF program to the `gpu_kernel_launch` tracepoint: `bpftrace -e 'tracepoint:gpu_driver:gpu_kernel_launch { printf("ctx=%llu func=%llx\n", args->ctx_id, args->func_addr); }'`. eBPF can also be attached to `kprobe:gpu_ring_submit` (kernel function probe) without any driver modification. For post-processing: BPF maps (hash maps, histograms) aggregate data in kernel space, minimizing userspace overhead. `bpf_perf_event_output` streams events to perf ring buffers.

**CQ4: What is `debugfs` and is it safe to use in production kernels?**
> `debugfs` is a pseudo-filesystem mounted at `/sys/kernel/debug/`. It's designed for developer debug output — no strict ABI guarantees, may expose sensitive information. In production NVIDIA deployments: (1) debugfs is typically only accessible to root, (2) `nvidia-smi` uses NVML (user-space library via `/dev/nvidiactl` ioctl) rather than debugfs, (3) debugfs performance statistics are read-only snapshots — no security risk beyond information disclosure. For production monitoring, prefer the ioctl-based NVML API.

**CQ5: How would you implement sampling-based GPU profiling (e.g., collect PC samples)?**
> Install a hardware breakpoint on the GPU's PC (Program Counter) sampling register. Periodically (via hrtimer at 1kHz), read the current executing PC of each GPU SM from MMIO perf counters. Map PC → function name via the loaded CUDA kernel symbol table. Build a histogram of PC values to identify hot spots. This is how NVIDIA's `ncu --sampling-interval` works. NVIDIA's hardware provides a dedicated PC sampler register that captures the PC when the GPU's SM executes a sampling trigger instruction (`NV_PMU_PERFMON`).
