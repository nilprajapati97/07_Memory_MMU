Linux Kernel Synchronization
Part 2 of 2

IPC Mechanisms • Memory-Mapped Files • Shared Memory Architecture
ARM Cache Coherency • Page Table Internals • Kernel Data Structures

## Document Overview
This document provides a comprehensive technical reference for Linux IPC mechanisms and shared memory architecture, targeting embedded systems engineers working with ARM platforms. It covers:
- All 8 IPC Mechanisms — Pipes, Message Queues, Shared Memory, Semaphores, Sockets, Signals, Memory-Mapped Files, D-Bus with comparison table
- Memory-Mapped Files & Shared Memory Internals — kernel and user space management, page cache, POSIX and System V APIs
- Shared Memory Architecture Diagrams — step-by-step memory assignment, ARM64 page table walk, kernel data structures, VMA layouts
- Cache Coherency — ARM ACE/CHI protocol, MESI states, multi-core coherency for shared memory

Platform context: ARM (Cortex-A / Cortex-M), Linux kernel drivers, bootloaders, Qualcomm SoC environments.
# 1. Inter-Process Communication (IPC) Mechanisms
IPC mechanisms enable processes to communicate and synchronize with each other. Linux provides a rich set of IPC primitives, each with different trade-offs in speed, complexity, and use case.
## 1.1 Pipes
### Anonymous Pipes
- Unidirectional communication between parent and child processes
- Created with pipe() system call
- Data flows in one direction (half-duplex)
- Exists only while processes are running

### Named Pipes (FIFOs)
- Bidirectional communication between unrelated processes
- Created with mkfifo() or mknod()
- Persists in filesystem until explicitly deleted
- Multiple readers/writers possible

▶ Use cases: Simple parent-child communication, shell pipelines
## 1.2 Message Queues
### Characteristics
- Structured message passing with priority levels
- Messages stored in kernel until retrieved
- Processes do not need to run simultaneously
- POSIX (mq_*) and System V (msgget, msgsnd, msgrcv) variants

Advantages: Asynchronous, structured data, priority support
Disadvantages: Size limits, kernel memory overhead
▶ Use cases: Task queuing, event notification systems
## 1.3 Shared Memory
### Characteristics
- Fastest IPC mechanism (no kernel copying)
- Multiple processes map same memory region
- Requires explicit synchronization (semaphores/mutexes)
- POSIX (shm_open, mmap) and System V (shmget, shmat) variants

Advantages: Highest performance, large data transfer
Disadvantages: Requires manual synchronization, complex
▶ Use cases: High-performance data sharing, multimedia applications, database systems
## 1.4 Semaphores (as IPC)
### Characteristics
- Synchronization primitive (not data transfer)
- Controls access to shared resources
- POSIX (named/unnamed) and System V variants
- Counting or binary types

▶ Use cases: Resource locking, producer-consumer synchronization
## 1.5 Sockets
### Types
- Unix Domain Sockets: Local IPC on same machine
- Network Sockets: TCP/UDP for networked communication

### Characteristics
- Bidirectional, connection-oriented or connectionless
- Works across network boundaries
- Stream (SOCK_STREAM) or datagram (SOCK_DGRAM) modes

Advantages: Flexible, network-capable, well-established APIs
Disadvantages: Higher overhead than other local IPC
▶ Use cases: Client-server applications, network services, Docker containers
## 1.6 Signals
### Characteristics
- Asynchronous notification mechanism
- Limited data transfer (signal number only)
- Standard signals (SIGTERM, SIGKILL) and real-time signals
- Handled via signal handlers

Advantages: Simple, immediate notification
Disadvantages: No data payload, can be lost, limited number
▶ Use cases: Process control, error notification, timer events
## 1.7 Memory-Mapped Files
### Characteristics
- File mapped into process address space
- Changes reflected in file and vice versa
- Can be shared between processes
- Created with mmap()

Advantages: Efficient file I/O, shared between processes
Disadvantages: Requires file backing, synchronization needed
▶ Use cases: Large file processing, shared configuration, databases
## 1.8 D-Bus (Linux Desktop)
### Characteristics
- High-level message bus system
- System bus (system services) and session bus (user applications)
- Supports method calls, signals, and properties
- XML-based interface definitions

▶ Use cases: Desktop application communication, system service coordination
## 1.9 IPC Mechanism Comparison Table

| IPC Mechanism | Speed | Data Size | Synchronization | Persistence | Complexity |
| Pipes | Fast | Limited | Implicit | No | Low |
| Named Pipes (FIFOs) | Fast | Limited | Implicit | Yes (filesystem) | Low |
| Message Queues | Medium | Medium | Built-in | Yes (kernel) | Medium |
| Shared Memory | Fastest | Large | Manual required | Yes (kernel) | High |
| Semaphores | N/A | N/A (sync only) | Purpose-built | Yes (kernel) | Medium |
| Sockets | Slower | Unlimited | Manual | No | Medium-High |
| Signals | Fast | None (number only) | Async | No | Low |
| Memory-Mapped Files | Fast | Large | Manual required | Yes (file) | Medium |
| D-Bus | Slow | Medium | Built-in | No | High |


## 1.10 IPC Selection Criteria
Choose based on your requirements:
- Pipes/FIFOs — Simple, one-way or sequential data flow
- Message Queues — Structured messages, asynchronous, priority needed
- Shared Memory — Maximum performance, large data, willing to handle synchronization
- Sockets — Network capability needed, client-server model, flexibility
- Signals — Simple notifications, process control
- Semaphores — Synchronization only, resource counting
- Memory-Mapped Files — File-based sharing, persistent data
- D-Bus — Desktop app communication, introspectable APIs
## 1.11 ARM / Embedded Considerations
- Shared memory is preferred for high-throughput data (e.g., video frames, sensor data)
- Message queues work well in RTOS environments
- Signals are lightweight for interrupt-like notifications
- Sockets enable communication with network processors
- Cache coherency must be considered with shared memory on multi-core ARM systems
- Memory barriers must be used appropriately for ARM weak memory ordering
# 2. Memory-Mapped Files & Shared Memory
This section explains how memory is allocated and managed in both kernel and user space for memory-mapped files and shared memory IPC mechanisms — with particular relevance to ARM platform development.
## 2.1 Memory-Mapped Files
### Kernel Space Management — Page Cache Integration
- When you call mmap() on a file, the kernel does NOT immediately allocate physical memory
- Instead, it creates Virtual Memory Area (VMA) structures in the process's address space
- The file's pages are managed through the page cache (also called buffer cache)
- Physical pages are allocated on-demand via page faults

### Memory Allocation Flow
User calls mmap()
  ↓
Kernel creates VMA structure (virtual mapping)
  ↓
No physical memory allocated yet (lazy allocation)
  ↓
User accesses mapped address
  ↓
Page fault occurs
  ↓
Kernel checks if page exists in page cache
  ↓
If not: Read from disk → Allocate physical page → Add to page cache
  ↓
Map physical page to process page table
  ↓
Update TLB entry
  ↓
User access completes

### Key Kernel Structures
- struct vm_area_struct — Represents mapped region in process
- struct address_space — Links file to its cached pages
- struct page — Represents physical page frame
- Page tables (PGD → PUD → PMD → PTE on ARM64)

### User Space Virtual Address Space (ARM64)
High addresses
┌─────────────────┐
│  Kernel Space   │  (Not accessible from user mode)
├─────────────────┤  0xFFFF_FFFF_FFFF_FFFF (ARM64)
│     Stack       │
├─────────────────┤
│       ↓         │
│                 │
│       ↑         │
├─────────────────┤
│  Memory-mapped  │  ← mmap() region (file-backed)
│     Files       │
├─────────────────┤
│      Heap       │
├─────────────────┤
│      BSS        │
├─────────────────┤
│      Data       │
├─────────────────┤
│      Text       │
└─────────────────┘  0x0000_0000_0000_0000
Low addresses

### Example: mmap() — Lazy Physical Memory Allocation
int fd = open("data.bin", O_RDWR);
size_t size = 4096;

// User space call
void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, 0);

// At this point:
// - Virtual address assigned: addr
// - Physical memory: NOT YET ALLOCATED
// - Page table entry: Marked as "not present"

// First access triggers page fault
*(int *)addr = 42;  // Page fault -> kernel loads page -> maps it

// Subsequent accesses: Direct memory access (no kernel involvement)
int value = *(int *)addr;  // Fast, uses TLB

### Physical Memory Allocation — ARM Steps
Steps taken by the kernel on a page fault:
- 1. Page fault handler (do_page_fault() in kernel)
- 2. Checks VMA to determine it's a file-backed mapping
- 3. Calls filemap_fault() to handle file page fault
- 4. Allocates physical page from buddy allocator
- 5. Reads data from disk into page (if not in cache)
- 6. Updates process page table (PTE entry)
- 7. Invalidates/updates TLB entry
- 8. Returns to user space

### Memory Sharing Key Points
- Multiple processes mapping same file share the same physical pages
- Kernel maintains reference count on each page (struct page._refcount)
- Changes visible to all processes (if MAP_SHARED)
- Copy-on-write (COW) used for MAP_PRIVATE — pages duplicated on first write
## 2.2 Shared Memory Segments
### POSIX Shared Memory — shm_open + mmap
Kernel actions when creating a POSIX shared memory object:
// User creates shared memory object
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
ftruncate(fd, 4096);  // Set size

// Kernel actions:
// 1. Creates entry in tmpfs (RAM-based filesystem)
// 2. Allocates inode structure
// 3. No physical pages allocated yet (lazy)

Memory allocation details:
- Shared memory objects backed by tmpfs (temporary filesystem in RAM)
- Physical pages allocated on first access (lazy — same as mmap)
- Pages remain in memory (not swapped to disk by default)
- Multiple processes map the same physical pages

### POSIX Shared Memory Kernel Structures Diagram
Process A                    Kernel                    Process B
┌──────────┐              ┌──────────┐              ┌──────────┐
│ Virtual  │              │ Physical │              │ Virtual  │
│ Address  │──────────────│  Pages   │──────────────│ Address  │
│ 0x7f...  │   PTE maps   │ 0x8000.. │   PTE maps   │ 0x7e...  │
└──────────┘              └──────────┘              └──────────┘
     │                          │                          │
     └──────────────────────────┴──────────────────────────┘
              Same physical memory, different virtual addresses

### System V Shared Memory — shmget, shmat
Creating and attaching a System V shared memory segment:
// Create shared memory segment
int shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0666);

// Kernel actions:
// 1. Allocates 'struct shmid_kernel' structure
// 2. Allocates physical pages IMMEDIATELY (not lazy)
// 3. Adds to system-wide shared memory table

// Attach to process
void *addr = shmat(shmid, NULL, 0);

// Kernel actions on shmat():
// 1. Creates VMA in process address space
// 2. Maps existing physical pages to process page tables
// 3. Updates TLB
// 4. Increments attachment count (shm_nattch)

### Key Differences: POSIX vs System V Shared Memory
- System V: Physical memory allocated immediately at creation
- System V: Uses System V IPC namespace (not filesystem)
- System V: Persists until explicitly removed via shmctl(IPC_RMID)
- System V: Identified by integer key, not file descriptor
- POSIX: Identified by name string (/myshm), visible as file in /dev/shm
## 2.3 Memory Layout Comparison Diagrams
### Memory-Mapped File
Disk File <-> Page Cache <-> Process Virtual Memory
   │              │                    │
   │              └────────────────────┴─── Multiple processes
   │                                         can map same pages
   └─── Persistent storage

### POSIX Shared Memory
tmpfs (RAM) <-> Physical Pages <-> Process Virtual Memory
     │              │                    │
     │              └────────────────────┴─── Multiple processes
     │                                         share same pages
     └─── No disk backing (pure RAM)

### System V Shared Memory
Kernel IPC Table -> Physical Pages <-> Process Virtual Memory
                         │                    │
                         └────────────────────┴─── Multiple processes
                                                    attach to segment
## 2.4 Performance Characteristics Comparison

| Aspect | Memory-Mapped Files | POSIX Shared Memory | System V Shared Memory |
| Allocation | Lazy (on-demand) | Lazy (on-demand) | Immediate |
| Backing Store | Disk file | tmpfs (RAM) | Kernel shmfs |
| Persistence | Survives reboot | Lost on reboot | Lost on reboot |
| Overhead | Disk I/O possible | Pure RAM | Pure RAM |
| Identifier | File path | Name string (/myshm) | Integer key |
| Preferred For | File-based sharing | High-speed IPC, new code | Legacy / RTOS code |


## 2.5 ARM-Specific Considerations
### Cache Coherency on Multi-Core ARM
- Each core has its own L1 cache (private)
- Shared memory updates may not be visible immediately to other cores
- Requires explicit cache management and memory barriers

### ARM Cache Barrier Solutions
// ARM cache and memory barrier operations
__sync_synchronize();                       // GCC full memory barrier
asm volatile("dmb sy" ::: "memory");  // Data Memory Barrier (all accesses)
asm volatile("dsb sy" ::: "memory");  // Data Synchronization Barrier
asm volatile("isb"    ::: "memory");  // Instruction Synchronization Barrier

### Page Table Walks on ARM64
The 48-bit virtual address is split into four 9-bit indices and a 12-bit page offset:
Virtual Address (48-bit)
┌────────┬────────┬────────┬────────┬────────┐
│  PGD   │  PUD   │  PMD   │  PTE   │ Offset │
│ [47:39]│ [38:30]│ [29:21]│ [20:12]│ [11:0] │
└────────┴────────┴────────┴────────┴────────┘
    │        │        │        │        │
    ↓        ↓        ↓        ↓        ↓
  Level 0  Level 1  Level 2  Level 3  Physical
   (PGD)    (PUD)    (PMD)    (PTE)    Page

### TLB Management
- TLB flush required when unmapping pages
- ARM uses ASID (Address Space ID) to avoid full TLB flush on context switch
- TLBI instructions used for TLB invalidation
## 2.6 Practical Example: POSIX Shared Memory with Semaphore Synchronization
### Process A — Writer
// Process A (Writer)
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
ftruncate(fd, sizeof(int));
int *shared = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd, 0);

sem_t *sem = sem_open("/mysem", O_CREAT, 0666, 0);

*shared = 42;               // Write data
__sync_synchronize();       // Memory barrier (ARM)
sem_post(sem);              // Signal Process B

### Process B — Reader
// Process B (Reader)
int fd = shm_open("/myshm", O_RDWR, 0666);
int *shared = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                   MAP_SHARED, fd, 0);

sem_t *sem = sem_open("/mysem", 0);

sem_wait(sem);              // Wait for signal from A
__sync_synchronize();       // Memory barrier (ARM)
int value = *shared;        // Read data (value == 42)

### ARM Embedded Use Cases
- Framebuffer sharing between bootloader and kernel
- Device tree blob passing via shared memory
- Ramdisk/initramfs as memory-mapped regions
- DMA buffers shared between kernel drivers and user space
- Multimedia pipelines (camera HAL → video encoder on Qualcomm SoC)
# 3. Shared Memory Architecture: Kernel & User Space Diagrams
This section provides detailed step-by-step diagrams showing exactly how shared memory is assigned and used between kernel space and user space on ARM platforms.
## 3.1 High-Level Overview — Physical Memory Shared Across Processes
At the highest level, two processes share the same physical memory pages but access them through different virtual addresses:

┌════════════════════════════════════════════════════════════════┐
│                        PHYSICAL MEMORY (RAM)                    │
│                                                                 │
│  ┌─────────────────────────────────────────────────────┐  │
│  │   Shared Physical Pages: 0x4000_0000 — 0x4000_0FFF        │  │
│  │                                                           │  │
│  │   ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐                    │  │
│  │   │Page 0│ │Page 1│ │Page 2│ │Page 3│  ... (4KB each)    │  │
│  │   └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘                    │  │
│  └──────┼────────┼────────┼────────┼─────────────────┘  │
│         │        │        │        │                        │
│    ┌────┼────────┼────────┼────────┼────┐            │
│    │    │   MAPPED TO    │        │    │            │
│    ▼    ▼        ▼        ▼        ▼    ▼            │
│                                                                 │
│  ┌─────────────────────┐    ┌─────────────────────┐   │
│  │   Process A (User)   │    │   Process B (User)   │   │
│  │   VA: 0x7F00_0000    │    │   VA: 0x7E00_0000    │   │
│  └─────────────────────┘    └─────────────────────┘   │
└════════════════════════════════════════════════════════════════┘

## 3.2 Step-by-Step Memory Assignment
### Step 1: Process A calls shmget() or shm_open()
═══════════════════════════════════════════════════════════════════════
  Process A (User Space)          |          Kernel Space
  -----------------------         |          ---------------
                                  |
  shmget(key, 4096, ...) -------->| 1. Allocate shmid_kernel struct
                                  | 2. Allocate physical pages
                                  |    from buddy allocator
                                  | 3. Create entry in IPC table
                                  | 4. Return shmid to user
                                  |
                                  |   +------------------------+
                                  |   | struct shmid_kernel    |
                                  |   | +-----------------+    |
                                  |   | | shm_perm        |    |
                                  |   | | shm_segsz = 4096|    |
                                  |   | | shm_nattch = 0  |    |
                                  |   | | shm_file ----+  |    |
                                  |   | +-----------+  |  |    |
                                  |   +---------------+--+----+
                                  |                  |
                                  |                  v
                                  |   +------------------------+
                                  |   | tmpfs / shmfs inode    |
                                  |   | Physical Pages:        |
                                  |   | Page 0: 0x4000_0000    |
                                  |   | Page 1: 0x4000_1000    |
                                  |   +------------------------+
═══════════════════════════════════════════════════════════════════════

### Step 2: Process A calls shmat() to Attach
═══════════════════════════════════════════════════════════════════════
  Process A (User Space)          |          Kernel Space
  -----------------------         |          ---------------
                                  |
  shmat(shmid, NULL, 0) --------->| 1. Find free VMA in Process A
                                  | 2. Create vm_area_struct
                                  | 3. Set up page table entries
                                  | 4. Map VA -> Physical pages
                                  | 5. Increment shm_nattch
                                  |
  Returns: 0x7F00_0000            |
  (virtual address)               |
                                  |
  Process A Page Table:           |
  +------------------------------------------+
  | VA 0x7F00_0000 -> PA 0x4000_0000 (Page 0)|
  | VA 0x7F00_1000 -> PA 0x4000_1000 (Page 1)|
  +------------------------------------------+
═══════════════════════════════════════════════════════════════════════

### Step 3: Process B Attaches to the SAME Segment
═══════════════════════════════════════════════════════════════════════
  Process B (User Space)          |          Kernel Space
  -----------------------         |          ---------------
                                  |
  shmget(same_key, ...)           |   (Returns existing shmid)
  shmat(shmid, NULL, 0) --------->| 1. Find free VMA in Process B
                                  | 2. Create vm_area_struct
                                  | 3. Map to SAME physical pages
                                  | 4. Increment shm_nattch = 2
                                  |
  Returns: 0x7E00_0000            |
  (DIFFERENT virtual address)     |
                                  |
  Process B Page Table:           |
  +---------------------------------------------+
  | VA 0x7E00_0000 -> PA 0x4000_0000 (Page 0)   | <- SAME physical page!
  | VA 0x7E00_1000 -> PA 0x4000_1000 (Page 1)   | <- SAME physical page!
  +---------------------------------------------+
═══════════════════════════════════════════════════════════════════════

## 3.3 Complete System Architecture Diagram
The complete picture shows user space processes, kernel page tables, and physical memory all at once:

┌────────────────────────────────────────────────────────────────────┐
│                         USER SPACE                                  │
│                                                                    │
│  ┌──────────────────────┐         ┌──────────────────────┐       │
│  │      Process A       │         │      Process B       │       │
│  │                      │         │                      │       │
│  │  Virtual Memory:    │         │  Virtual Memory:    │       │
│  │  ┌──────────────┐  │         │  ┌──────────────┐  │       │
│  │  │ Stack          │  │         │  │ Stack          │  │       │
│  │  │ ┌──────────┐ │  │         │  │ ┌──────────┐ │  │       │
│  │  │ │ SHM Region│ │  │         │  │ │ SHM Region│ │  │       │
│  │  │ │ 0x7F000000│<┼──────────┼─┤►│ 0x7E000000│ │  │       │
│  │  │ │ (4KB)     │ │  │         │  │ │ (4KB)     │ │  │       │
│  │  │ └──────────┘ │  │         │  │ └──────────┘ │  │       │
│  │  │ Heap           │  │         │  │ Heap           │  │       │
│  │  │ Data / BSS     │  │         │  │ Data / BSS     │  │       │
│  │  │ Text           │  │         │  │ Text           │  │       │
│  │  └──────────────┘  │         │  └──────────────┘  │       │
│  └──────────┬───────────┘         └──────────┬───────────┘       │
│             │                              │                    │
├═════════════╪══════════════════════════════╪════════════════════┤
│             │         KERNEL SPACE          │                    │
│             ▼                              ▼                    │
│  ┌────────────────────┐          ┌────────────────────┐          │
│  │ Process A Page Table │          │ Process B Page Table │          │
│  │                      │          │                      │          │
│  │ PTE: 0x7F000000      │          │ PTE: 0x7E000000      │          │
│  │   -> PA 0x40000000   │          │   -> PA 0x40000000   │          │
│  │ Flags: RW|User|Valid  │          │ Flags: RW|User|Valid  │          │
│  └─────────┬──────────┘          └─────────┬──────────┘          │
│            │                                │                     │
│            └─────────────┬──────────────┘                     │
│                           ▼                                     │
│  ┌═════════════════════════════════════════════════════┐     │
│  │              PHYSICAL MEMORY (RAM)                      │     │
│  │    PA: 0x4000_0000                                      │     │
│  │   ┌──────────┬──────────┬──────────┬──────────┐      │     │
│  │   │ Byte 0   │ Byte 1   │ ...      │ Byte 4095│      │     │
│  │   └──────────┴──────────┴──────────┴──────────┘      │     │
│  │                                                          │     │
│  │  Process A writes: *(int*)0x7F000000 = 42               │     │
│  │  Process B reads:  val = *(int*)0x7E000000              │     │
│  │  val == 42  ✔                                          │     │
│  └═════════════════════════════════════════════════════┘     │
└────────────────────────────────────────────────────────────────────┘

## 3.4 ARM64 Page Table Walk for Shared Memory Access
When Process A accesses shared memory at VA 0x7F00_0000, the MMU performs a 4-level page table walk on ARM64:

Virtual Address: 0x0000_007F_0000_0000 (48-bit)

┌────────┬────────┬────────┬────────┬──────────────┐
│  L0/PGD │  L1/PUD │  L2/PMD │  L3/PTE │  Page Offset │
│ [47:39] │ [38:30] │ [29:21] │ [20:12] │    [11:0]    │
│   9 bits│   9 bits│   9 bits│   9 bits│   12 bits    │
└────┬────┴────┬────┴────┬────┴────┬────┴──────┬───────┘
     │         │         │         │           │
     ▼         ▼         ▼         ▼           ▼
  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐    ┌──────────┐
  │ PGD  │→│ PUD  │→│ PMD  │→│ PTE  │───►│ Physical │
  │Table │ │Table │ │Table │ │Entry │    │  Page    │
  └──────┘ └──────┘ └──────┘ └──────┘    │0x4000000│
                                           └──────────┘
  TTBR0_EL1                              Each table = 512 entries x 8B = 4KB

### PTE Entry Layout (ARM64) for Shared Memory
┌─────────────────────────────────────────────────────────┐
│ [63]    UXN = 1 (EL0 cannot execute)                  │
│ [62]    PXN = 1 (EL1 cannot execute)                  │
│ [47:12] Physical Page Address = 0x4000_0 (PA>>12)     │
│ [11]    nG = 0 (Global — visible to all ASIDs)        │
│ [10]    AF = 1 (Access Flag — page was accessed)      │
│ [9:8]   SH = 11 (Inner Shareable for SMP)             │
│ [7:6]   AP = 01 (Read/Write at EL0, EL1)              │
│ [4:2]   AttrIndx = Normal Memory (Write-Back Cache)   │
│ [1]     1 = Page descriptor (not block)               │
│ [0]     Valid = 1 (page is present)                   │
└─────────────────────────────────────────────────────────┘

Key flags for shared memory:
  SH=11 (Inner Shareable) -> cache coherent across all CPUs in cluster
  AP=01 -> both user (EL0) and kernel (EL1) can read/write
  nG=0  -> page is global, valid for all ASIDs

## 3.5 Kernel Internal Data Structures
The kernel maintains a chain of structures linking IPC namespace entries down to individual physical pages:

                    IPC Namespace
                   ┌──────────────────────┐
                   │ struct ipc_namespace  │
                   │  └─ struct ipc_ids    │
                   │      shm_ids[]        │
                   │      ┌────┐           │
                   │      │ [0]│──────────────────┐
                   │      │ [1]│───┐              │
                   │      │ ...│   │              │
                   │      └────┘   │              │
                   └───────────────┼─────────────┼──┘
                                   │              │
                                   ▼              ▼
                   ┌────────────────────┐  ┌───────────────────┐
                   │struct shmid_kernel  │  │struct shmid_kernel │
                   │ shm_perm           │  │ (another segment)  │
                   │ shm_segsz          │  └───────────────────┘
                   │ shm_nattch = 2     │
                   │ shm_file ──────────┼────┐
                   └────────────────────┘    │
                                               ▼
                              ┌─────────────────────────┐
                              │  struct file (shmfs)     │
                              │  f_mapping ──────────────┐
                              └─────────────────────────┘   │
                                                                ▼
                               ┌────────────────────────────┐
                               │  struct address_space       │
                               │  (Page Cache / Xarray)      │
                               │                             │
                               │  page_tree:                 │
                               │  +------+------+------+--+  │
                               │  | [0]  | [1]  | [2]  |..|  │
                               │  +--+---+--+---+--+---+--+  │
                               └──────┼──────┼──────┼────────┘
                                      ▼      ▼      ▼
                               ┌──────┐┌──────┐┌──────┐
                               │struct││struct││struct│
                               │ page ││ page ││ page │
                               │PA:   ││PA:   ││PA:   │
                               │0x4000││0x4001││0x4002│
                               │_0000 ││_0000 ││_0000 │
                               │count ││count ││count │
                               │ = 2  ││ = 2  ││ = 2  │
                               └──────┘└──────┘└──────┘
                                  ^ ^   (ref count = 2:
                                  | |    2 processes mapped this page)
                       Proc A PTE-/ \-Proc B PTE

## 3.6 Virtual Memory Areas (VMAs) per Process
Each process maintains a linked list of vm_area_struct entries in its mm_struct, describing all mapped regions:

Process A task_struct:
┌────────────────────────────────────────────────────┐
│  struct task_struct                               │
│  └─ struct mm_struct *mm                        │
│      └─ vm_area_struct *mmap (linked list)      │
│                                                  │
│  VMA List:                                        │
│  ┌──────────────────────────────────────────┐  │
│  │ VMA 1: [text]   0x0040_0000 - 0x0041_0000 │  │
│  │ VMA 2: [heap]   0x0060_0000 - 0x0070_0000 │  │
│  │ VMA 3: [shm]    0x7F00_0000 - 0x7F00_1000 │  │  <-- SHM
│  │    vm_start  = 0x7F00_0000                │  │
│  │    vm_end    = 0x7F00_1000                │  │
│  │    vm_flags  = VM_READ|VM_WRITE|VM_SHARED │  │
│  │    vm_file   = shmfs file pointer         │  │
│  │    vm_ops    = shm_vm_ops                 │  │
│  │ VMA 4: [stack]  0x7FFF_0000 - 0x8000_0000 │  │
│  └──────────────────────────────────────────┘  │
└────────────────────────────────────────────────────┘

Process B task_struct:
┌────────────────────────────────────────────────────┐
│  VMA List:                                        │
│  ┌──────────────────────────────────────────┐  │
│  │ VMA 1: [text]   0x0040_0000 - 0x0041_0000 │  │
│  │ VMA 2: [heap]   0x0060_0000 - 0x0080_0000 │  │
│  │ VMA 3: [shm]    0x7E00_0000 - 0x7E00_1000 │  │  <-- SHM
│  │    vm_start  = 0x7E00_0000 (DIFFERENT!)    │  │
│  │    vm_end    = 0x7E00_1000                │  │
│  │    vm_flags  = VM_READ|VM_WRITE|VM_SHARED │  │
│  │    vm_file   = SAME shmfs file pointer    │  │
│  │ VMA 4: [stack]  0x7FFF_0000 - 0x8000_0000 │  │
│  └──────────────────────────────────────────┘  │
└────────────────────────────────────────────────────┘

Key: Both vm_file pointers point to the SAME struct file (same shmfs inode)
     -> Both processes map to the same physical pages

## 3.7 Data Flow — Write by Process A, Read by Process B
Tracing the exact steps when Process A writes 0xDEADBEEF to the shared memory and Process B reads it:

Process A writes: *(int *)0x7F000000 = 0xDEADBEEF
═══════════════════════════════════════════════════

Step 1: CPU Core 0 (Process A) — Store Operation
┌──────────────────────────────────────────┐
│  Store: STR R0, [0x7F000000]                 │
│                                              │
│  1. Check TLB for VA 0x7F000000              │
│     TLB Hit  -> Get PA 0x40000000 directly   │
│     TLB Miss -> 4-level page table walk       │
│                                              │
│  2. VA 0x7F000000 -> PA 0x40000000           │
│                                              │
│  3. Write 0xDEADBEEF to L1 Cache             │
│     (Write-back policy on ARM Cortex-A)      │
│                                              │
│  4. Cache coherency protocol (MESI)          │
│     marks cache line state as "Modified"     │
└──────────────────────────────────────────┘

Step 2: CPU Core 1 (Process B) — Load Operation
┌──────────────────────────────────────────┐
│  Load: LDR R1, [0x7E000000]                  │
│                                              │
│  1. Check TLB for VA 0x7E000000              │
│     VA 0x7E000000 -> PA 0x40000000           │
│     (SAME physical address as Process A)     │
│                                              │
│  2. Check L1 Cache -> Miss                   │
│     (Core 1 cache does not have this line)   │
│                                              │
│  3. Cache coherency: Snoop Core 0 cache      │
│     Core 0 has "Modified" line               │
│     -> ACE/CHI protocol transfers data       │
│     -> Both caches enter "Shared" state      │
│                                              │
│  4. R1 = 0xDEADBEEF  ✔                     │
└──────────────────────────────────────────┘

NOTE: Memory barrier (DMB) required between write and sem_post
      to prevent CPU reordering on ARM weak-memory architecture

## 3.8 ARM Multi-Core Cache Coherency for Shared Memory
This diagram shows the ARM SoC cache hierarchy used to maintain coherency between cores accessing shared memory:

┌───────────────────────────────────────────────────────────┐
│                     ARM SoC (Qualcomm / etc.)              │
│                                                           │
│  ┌──────────────┐              ┌──────────────┐          │
│  │    Core 0     │              │    Core 1     │          │
│  │  (Process A)  │              │  (Process B)  │          │
│  │  ┌─────────┐  │              │  ┌─────────┐  │          │
│  │  │ L1 I$ │  │              │  │ L1 I$ │  │          │
│  │  └─────────┘  │              │  └─────────┘  │          │
│  │  ┌─────────┐  │              │  ┌─────────┐  │          │
│  │  │ L1 D$ │  │              │  │ L1 D$ │  │          │
│  │  │(Private)│ │              │  │(Private)│ │          │
│  │  │PA=0x4000│ │              │  │PA=0x4000│ │          │
│  │  │DEADBEEF │ │              │  │DEADBEEF │ │          │
│  │  │State: S │ │              │  │State: S │ │          │
│  │  └────┬────┘  │              │  └────┬────┘  │          │
│  └───────┼───────┘              └───────┼───────┘          │
│          │                              │                   │
│          │      ┌────────────────────┐   │                   │
│          └────┤  L2 Cache (Shared)  ├───┘                   │
│               │  (Unified / Cluster)   │                       │
│               │  ACE/CHI Protocol      │                       │
│               │  (Cache Coherent       │                       │
│               │   Interconnect)        │                       │
│               └──────────┬──────────┘                       │
│                          │                                  │
│               ┌──────────┴──────────┐                       │
│               │   CCI / CCN / CMN        │                       │
│               │  (Cache Coherent Bus)    │                       │
│               └──────────┬──────────┘                       │
│                          │                                  │
│               ┌──────────┴──────────┐                       │
│               │    DRAM Controller       │                       │
│               │    PA: 0x4000_0000        │                       │
│               │    Data: 0xDEADBEEF       │                       │
│               └─────────────────────┘                       │
└───────────────────────────────────────────────────────────┘

Cache States (MESI Protocol):
  M = Modified  (dirty, exclusive owner - must write-back on eviction)
  E = Exclusive (clean, single owner - can promote to M without broadcast)
  S = Shared    (clean, multiple owners - must invalidate to modify)
  I = Invalid   (not present in this cache)

## 3.9 Key Takeaways
### Summary: How Shared Memory Works Between Kernel and User Space
- Same physical page, different virtual addresses — Each process sees shared memory at a different VA but the same PA
- Kernel manages the mapping — Page tables in each process point to the same physical frame
- No data copying — Direct memory access is why shared memory is the fastest IPC mechanism
- Cache coherency is automatic on ARM — ACE/CHI protocol keeps caches consistent across cores
- Memory barriers still needed — For ordering guarantees (DMB, DSB on ARM)
- Reference counting — Kernel tracks how many processes map each page (struct page._refcount)
- VMA per process — Each process has its own vm_area_struct but both point to the same shmfs inode

## 3.10 Shared Memory API Summary

| Feature | POSIX (shm_open) | System V (shmget) | mmap (file-backed) |
| Create | shm_open() + ftruncate() | shmget() | open() + mmap() |
| Attach / Map | mmap() | shmat() | mmap() |
| Detach / Unmap | munmap() | shmdt() | munmap() |
| Delete | shm_unlink() | shmctl(IPC_RMID) | unlink() file |
| Identifier | Name string (/myshm) | Integer key | File path |
| Memory Alloc | Lazy (on first access) | Immediate at creation | Lazy (on first access) |
| Backing Store | tmpfs (/dev/shm) | Kernel shmfs | Disk file |
| Preferred For | New code, portable apps | Legacy / RTOS code | File-based sharing |


# 4. Document Summary
This document (Part 2 of 2) covered the following topics from the Linux IPC and Memory Systems conversation:

### Part 2 Coverage
- Section 1 — IPC Mechanisms: All 8 IPC types (Pipes, Named Pipes, Message Queues, Shared Memory, Semaphores, Sockets, Signals, Memory-Mapped Files, D-Bus) with comprehensive comparison table and selection criteria
- Section 2 — Memory-Mapped Files & Shared Memory: Kernel/user space memory management, page cache integration, VMA structures, POSIX and System V variants, ARM64 page table walk, TLB management, and practical code examples
- Section 3 — Shared Memory Architecture Diagrams: Step-by-step memory assignment (shmget/shmat/shmat for second process), complete system architecture, ARM64 page table PTE layout, kernel data structures (shmid_kernel, address_space, struct page), VMA per process, data flow tracing, and ARM multi-core cache coherency (ACE/CHI/MESI)

## Document Series

| Part | Title | Topics Covered |
| Part 1 | Concurrency & Synchronization Primitives | Spinlocks, Semaphores, Deadlock Prevention (7 strategies) |
| Part 2 | Linux IPC & Shared Memory | IPC Mechanisms, Memory-Mapped Files, Shared Memory Architecture |


# Appendix: Quick Reference
## A. IPC Mechanism Selection Guide

| Requirement | Best IPC Choice | Notes |
| Maximum throughput, large data | Shared Memory | Use POSIX shm_open + mmap; add barriers |
| Structured messages with priority | Message Queues | Use POSIX mq_open/mq_send/mq_receive |
| Simple parent-child communication | Anonymous Pipe | Use pipe(), single direction |
| Unrelated process communication | Named Pipe or Socket | FIFO or Unix domain socket |
| Network or cross-host communication | Network Socket | TCP (reliable) or UDP (datagram) |
| Process notification / signaling | Signal or Semaphore | Signal for async; semaphore for sync |
| Large file access / mapping | Memory-Mapped File | mmap() with MAP_SHARED or MAP_PRIVATE |
| Desktop service coordination | D-Bus | systemd, NetworkManager, BlueZ, PipeWire |


## B. ARM Memory Barrier Quick Reference

| Instruction / Function | Scope | Use When |
| DMB SY | Full system barrier | Shared memory write before sem_post() |
| DSB SY | Complete all pending accesses | Before system call, cache operations |
| ISB | Instruction stream flush | After self-modifying code, TTBR change |
| __sync_synchronize() | Compiler + hardware barrier | Portable C code with shared memory |
| smp_mb() (kernel) | SMP memory barrier | Linux kernel shared data between CPUs |
| atomic_store/load | Per-operation ordering | Lock-free programming (C11 atomics) |


Relevant to your Qualcomm IoT SoC experience: these mechanisms are used in DMA buffer sharing (dma-buf/ION framework), framebuffer access, multimedia pipelines, and camera HAL ↔ video encoder communication.
