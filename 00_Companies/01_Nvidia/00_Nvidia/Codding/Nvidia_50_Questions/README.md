# NVIDIA Linux Kernel Interview — 50 Questions & Answers
### Target: 9 Years Experience | GPU Driver / Linux Kernel Engineering

> Each question file contains: **Question → Answer (Code) → Explanation → 5 Cross Questions & Answers**

---

## Navigation Index

| # | Title | Section | File |
|---|-------|---------|------|
| Q01 | Spinlock to Protect Shared Counter (Multi-CPU) | Linux Kernel Internals | [Q01_Spinlock_Shared_Counter.md](Q01_Spinlock_Shared_Counter.md) |
| Q02 | Lock-Free SPSC Ring Buffer | Linux Kernel Internals | [Q02_SPSC_Ring_Buffer.md](Q02_SPSC_Ring_Buffer.md) |
| Q03 | kmalloc vs vmalloc vs get_free_pages | Linux Kernel Internals | [Q03_kmalloc_vmalloc_get_free_pages.md](Q03_kmalloc_vmalloc_get_free_pages.md) |
| Q04 | Kernel Linked List with Safe Deletion | Linux Kernel Internals | [Q04_Kernel_Linked_List.md](Q04_Kernel_Linked_List.md) |
| Q05 | RCU (Read-Copy-Update) for Read-Heavy Data | Linux Kernel Internals | [Q05_RCU_Read_Heavy.md](Q05_RCU_Read_Heavy.md) |
| Q06 | Work Queue for GPU Command Completions | Linux Kernel Internals | [Q06_Workqueue_GPU_Completions.md](Q06_Workqueue_GPU_Completions.md) |
| Q07 | mutex vs semaphore vs spinlock vs rwlock | Linux Kernel Internals | [Q07_Mutex_Semaphore_Spinlock_Rwlock.md](Q07_Mutex_Semaphore_Spinlock_Rwlock.md) |
| Q08 | Character Device Driver (read/write/ioctl) | Linux Kernel Internals | [Q08_Char_Device_Driver.md](Q08_Char_Device_Driver.md) |
| Q09 | Interrupt Top Half vs Bottom Half / Tasklet | Linux Kernel Internals | [Q09_Interrupt_Top_Bottom_Half.md](Q09_Interrupt_Top_Bottom_Half.md) |
| Q10 | Kernel MMIO with ioremap and Barriers | Linux Kernel Internals | [Q10_MMIO_ioremap_Barriers.md](Q10_MMIO_ioremap_Barriers.md) |
| Q11 | Buddy System + Page Cache with LRU Eviction | Memory Management | [Q11_Buddy_System_LRU_Cache.md](Q11_Buddy_System_LRU_Cache.md) |
| Q12 | Huge Pages / THP — Allocate 2MB Page | Memory Management | [Q12_Huge_Pages_THP.md](Q12_Huge_Pages_THP.md) |
| Q13 | Slab Cache for GPU Descriptor Objects | Memory Management | [Q13_Slab_Cache_GPU_Descriptor.md](Q13_Slab_Cache_GPU_Descriptor.md) |
| Q14 | CMA (Contiguous Memory Allocator) for GPU DMA | Memory Management | [Q14_CMA_GPU_DMA.md](Q14_CMA_GPU_DMA.md) |
| Q15 | IOMMU and GPU DMA Group Management | Memory Management | [Q15_IOMMU_GPU_DMA.md](Q15_IOMMU_GPU_DMA.md) |
| Q16 | Memory Pressure Shrinker for GPU Reclaim | Memory Management | [Q16_Memory_Pressure_Shrinker.md](Q16_Memory_Pressure_Shrinker.md) |
| Q17 | GFP Flags — KERNEL vs ATOMIC vs DMA | Memory Management | [Q17_GFP_Flags.md](Q17_GFP_Flags.md) |
| Q18 | Custom mmap for GPU Framebuffer | Memory Management | [Q18_mmap_GPU_Framebuffer.md](Q18_mmap_GPU_Framebuffer.md) |
| Q19 | Wait Queue for GPU Fence Completion | Concurrency & Synchronization | [Q19_Wait_Queue_GPU_Fence.md](Q19_Wait_Queue_GPU_Fence.md) |
| Q20 | Ticket Spinlock for FIFO Ordering | Concurrency & Synchronization | [Q20_Ticket_Spinlock.md](Q20_Ticket_Spinlock.md) |
| Q21 | Per-CPU Counter (Avoid False Sharing) | Concurrency & Synchronization | [Q21_Per_CPU_Counter.md](Q21_Per_CPU_Counter.md) |
| Q22 | Completion Variable for Module Init Sync | Concurrency & Synchronization | [Q22_Completion_Variable.md](Q22_Completion_Variable.md) |
| Q23 | Seqlock vs RCU — When to Use Each | Concurrency & Synchronization | [Q23_Seqlock_vs_RCU.md](Q23_Seqlock_vs_RCU.md) |
| Q24 | Kernel Deadlock Detection and Resolution | Concurrency & Synchronization | [Q24_Kernel_Deadlock_Detection.md](Q24_Kernel_Deadlock_Detection.md) |
| Q25 | Atomic State Machine for GPU Power States | Concurrency & Synchronization | [Q25_Atomic_State_Machine.md](Q25_Atomic_State_Machine.md) |
| Q26 | GPU Command Scheduler (Multi-Context, Multi-Priority) | System Design | [Q26_GPU_Command_Scheduler.md](Q26_GPU_Command_Scheduler.md) |
| Q27 | Zero-Copy DMA Transfer Engine (CPU↔GPU) | System Design | [Q27_Zero_Copy_DMA_Engine.md](Q27_Zero_Copy_DMA_Engine.md) |
| Q28 | GPU Virtual Memory Manager (GMMU) | System Design | [Q28_GPU_Virtual_Memory_Manager.md](Q28_GPU_Virtual_Memory_Manager.md) |
| Q29 | GPU Kernel-Mode Fault Handler (Page Fault Replay) | System Design | [Q29_GPU_Fault_Handler.md](Q29_GPU_Fault_Handler.md) |
| Q30 | Distributed GPU Job Scheduler Across Nodes | System Design | [Q30_Distributed_GPU_Job_Scheduler.md](Q30_Distributed_GPU_Job_Scheduler.md) |
| Q31 | NVIDIA GPU Kernel Driver Architecture (PCIe) | System Design | [Q31_NVIDIA_Driver_Architecture.md](Q31_NVIDIA_Driver_Architecture.md) |
| Q32 | GPU Memory Allocator (VRAM + System Memory) | System Design | [Q32_GPU_Memory_Allocator.md](Q32_GPU_Memory_Allocator.md) |
| Q33 | GPU Cluster Health Monitoring System | System Design | [Q33_GPU_Health_Monitoring.md](Q33_GPU_Health_Monitoring.md) |
| Q34 | GPU Profiling and Tracing Infrastructure | System Design | [Q34_GPU_Profiling_Tracing.md](Q34_GPU_Profiling_Tracing.md) |
| Q35 | PCIe Peer-to-Peer (P2P) DMA Between GPUs | System Design | [Q35_PCIe_P2P_DMA.md](Q35_PCIe_P2P_DMA.md) |
| Q36 | GPU SR-IOV Virtual Function Manager | System Design | [Q36_SR_IOV_VF_Manager.md](Q36_SR_IOV_VF_Manager.md) |
| Q37 | NUMA-Aware GPU Memory Allocation Policy | System Design | [Q37_NUMA_Aware_GPU_Alloc.md](Q37_NUMA_Aware_GPU_Alloc.md) |
| Q38 | Red-Black Tree for O(log n) VA Range Lookups | Performance & Algorithms | [Q38_Red_Black_Tree_VA.md](Q38_Red_Black_Tree_VA.md) |
| Q39 | Kernel Hash Table for GPU Context Lookups | Performance & Algorithms | [Q39_Hash_Table_GPU_Context.md](Q39_Hash_Table_GPU_Context.md) |
| Q40 | Lock-Free Stack Using Compare-and-Swap | Performance & Algorithms | [Q40_Lock_Free_Stack_CAS.md](Q40_Lock_Free_Stack_CAS.md) |
| Q41 | Debug Kernel NULL Pointer Dereference (Oops) | Debugging | [Q41_Kernel_Oops_Debug.md](Q41_Kernel_Oops_Debug.md) |
| Q42 | KASAN-Friendly Memory Debugging | Debugging | [Q42_KASAN_Memory_Debug.md](Q42_KASAN_Memory_Debug.md) |
| Q43 | ftrace-Based Latency Measurement | Debugging | [Q43_Ftrace_Latency.md](Q43_Ftrace_Latency.md) |
| Q44 | hrtimer GPU Watchdog Implementation | Performance & Algorithms | [Q44_Hrtimer_GPU_Watchdog.md](Q44_Hrtimer_GPU_Watchdog.md) |
| Q45 | LIS — Longest Increasing Subsequence O(n log n) | Performance & Algorithms | [Q45_LIS_O_nlogn.md](Q45_LIS_O_nlogn.md) |
| Q46 | Segment Tree for GPU Job Interval Queries | Performance & Algorithms | [Q46_Segment_Tree_GPU_Jobs.md](Q46_Segment_Tree_GPU_Jobs.md) |
| Q47 | Dijkstra for PCIe Topology Path Finding | Performance & Algorithms | [Q47_Dijkstra_PCIe_Topology.md](Q47_Dijkstra_PCIe_Topology.md) |
| Q48 | Trie for Kernel Module Parameter Namespace | Performance & Algorithms | [Q48_Trie_Module_Params.md](Q48_Trie_Module_Params.md) |
| Q49 | GPU Command Ring Buffer with Hardware Doorbell | Performance & Algorithms | [Q49_GPU_Command_Ring_Buffer.md](Q49_GPU_Command_Ring_Buffer.md) |
| Q50 | GPU Driver Bring-Up Sequence (PCIe Probe to First Command) | System Design | [Q50_GPU_Driver_Bringup.md](Q50_GPU_Driver_Bringup.md) |

---

## Sections Overview

| Section | Questions | Focus |
|---------|-----------|-------|
| Linux Kernel Internals | Q01–Q10 | Modules, locks, IRQs, drivers, MMIO |
| Memory Management | Q11–Q18 | Page allocator, slab, DMA, IOMMU, mmap |
| Concurrency & Synchronization | Q19–Q25 | Wait queues, atomics, deadlocks, state machines |
| System Design | Q26–Q37 | Schedulers, MMU, DMA engines, SR-IOV, NUMA |
| Performance & Algorithms | Q38–Q50 | RB-tree, hash, lock-free, LIS, Dijkstra, profiling |

---

## How to Study

1. Read the **Question** — attempt to answer mentally
2. Read the **Answer** code — understand every line
3. Read the **Explanation** — solidify concepts
4. Go through **Cross Questions** — simulate interviewer follow-ups
5. Revisit weekly until all 50 are fluent

---

*Generated: April 27, 2026 | Experience Level: 9 Years | Role: GPU Driver / Linux Kernel Engineer*
