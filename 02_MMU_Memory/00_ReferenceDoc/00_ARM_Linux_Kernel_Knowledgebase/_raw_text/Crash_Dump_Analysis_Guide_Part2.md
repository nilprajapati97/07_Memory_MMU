CRASH DUMP ANALYSIS GUIDE
Part 2 of 3
Qualcomm Ramdump & Minidump Deep Dive
| Document Type | Technical Reference Guide |
| Series | Crash Dump Analysis Guide — 3-Part Series |
| Version | 1.0 |
| Scope | Qualcomm SoC Ramdump, Minidump, DCC, Parsing, Subsystem Dumps |
| Platform | ARM64 / ARM32 — Qualcomm Snapdragon (SM8550, QCS405, etc.) |



# 1. Qualcomm Ramdump Architecture Overview
On Qualcomm SoCs, when a crash occurs (kernel panic, watchdog bite, or TZ-triggered secure watchdog), the system enters a controlled download mode. The process preserves the entire DDR memory state via a warm reset, allowing the bootloader to collect the dump and transfer it to a host PC via the Sahara protocol.
## 1.1 End-to-End Architecture Flow
+----------------------------------------------------------+
|                    NORMAL OPERATION                      |
|   Apps Processor (HLOS - Linux Kernel)                  |
+-------------------------+--------------------------------+
                          | Crash / Panic / Watchdog Bite
                          v
+----------------------------------------------------------+
|              CRASH DETECTION LAYER                       |
|  +----------+  +--------------+  +---------------------+|
|  | Kernel   |  | Secure WDT   |  | Non-Secure WDT      ||
|  | Panic    |  | (TZ/HYP)     |  | (Apps processor)    ||
|  +----+-----+  +------+-------+  +----------+----------+|
|       |               |                     |            |
|       v               v                     v            |
|  +----------------------------------------------------+  |
|  |     SCM Call / PSHOLD / IMEM Cookie Set            |  |
|  |  (Set restart reason + download mode cookie)       |  |
|  +------------------------+---------------------------+  |
+-----------+-----------------------------------------+----+
            | Warm Reset
            v
+----------------------------------------------------------+
|                   BOOTLOADER (SBL/XBL)                   |
|                                                          |
|  1. Reads IMEM magic cookie -> detects crash             |
|  2. Checks: download_mode enabled?                       |
|     +-- YES (Full Ramdump) -> Enter Sahara/Firehose mode |
|     +-- YES (Minidump)     -> Collect registered regions |
|     +-- NO                 -> Normal reboot              |
|  3. Transfers dump to host via USB (Sahara protocol)     |
|     OR writes to dedicated storage partition             |
+----------------------------------------------------------+

The PMIC keeps DDR powered in self-refresh mode during the warm reset so all memory contents survive to the next boot. This is the foundational mechanism that makes ramdump collection possible on Qualcomm platforms.

# 2. Full Ramdump vs. Minidump Comparison
Qualcomm provides two dump collection modes. The choice depends on the deployment context, available infrastructure, and time-to-collect constraints.
| Aspect | Full Ramdump | Minidump |
| Size | Entire DDR (2 GB–12 GB+) | Selective regions (50–200 MB typical) |
| Collection Time | Minutes (depends on DDR size + USB speed) | Seconds to ~1 minute |
| Content | All physical memory (kernel + user space) | Only pre-registered memory regions |
| Use Case | Complex crashes, memory corruption | Quick triage, field crash collection |
| Infrastructure | Sahara protocol, QPST/QFIL | Sahara protocol, minidump table |
| Analysis Tool | T32, crash utility, CrashScope, Ramparse | Ramparse, CrashScope, crash utility |
| Production Suitability | Typically disabled (too large/slow) | Enabled in production builds |
| DT/Kernel Config | download_mode enabled | CONFIG_QCOM_MINIDUMP + msm_minidump |



# 3. Full Ramdump Deep Dive
## 3.1 Entry into Download Mode
When the kernel panics, it invokes the panic notifier chain. The Qualcomm-specific handler sets the IMEM download mode cookie and triggers a warm reset via PSHOLD. The key function is shown below:
/* Kernel side: on panic */
static int panic_handler(struct notifier_block *nb,
                         unsigned long event, void *data)
{
    /* Set IMEM download mode cookie */
    __raw_writel(DOWNLOAD_MODE_MAGIC, restart_reason_addr);

    /* Optionally set specific restart reason */
    qcom_scm_set_download_mode(QCOM_DOWNLOAD_FULLDUMP);

    /* Trigger warm reset via PSHOLD/PON */
    machine_restart(NULL);
}
### IMEM Cookie Mechanism
IMEM (Internal Memory) is a small on-chip SRAM that is preserved across warm resets. It holds key metadata values that the bootloader reads on every boot to decide the reset reason:
+--------------------------------------+
| IMEM (Internal Memory) Layout        |
+--------------------------------------+
| Offset 0x0:  restart_reason          | <- Magic values
| Offset 0x4:  download_mode_cookie    | <- 0x12345678 = download mode
| Offset 0x8:  minidump_cookie         | <- 0x87654321 = minidump mode
| Offset 0xC:  pon_reason              |
| Offset 0x10: dload_type              | <- Full dump vs mini
| ...                                  |
| Offset 0x6C: boot_misc_info          |
+--------------------------------------+

## 3.2 Sahara Protocol (Dump Transfer)
The Sahara protocol is a Qualcomm-proprietary USB protocol used to transfer memory contents from a device in download mode to a host PC. The sequence is as follows:
Host (PC)                          Target (Device in download mode)
  |                                        |
  |<---- HELLO packet --------------------|  (Device announces Sahara mode)
  |                                        |
  |------ HELLO_RESPONSE ---------------->|  (Host acknowledges)
  |                                        |
  |<---- MEMORY_DEBUG packet -------------|  (Device sends memory table)
  |       [region_addr, region_size] x N  |
  |                                        |
  |------ MEMORY_READ request ----------->|  (Host requests specific region)
  |                                        |
  |<---- RAW DATA ------------------------|  (Device sends memory contents)
  |       ... (repeat for all regions) ...|
  |                                        |
  |------ RESET command ----------------- >|  (Host signals completion)
  |                                        |
The MEMORY_DEBUG packet contains a table of all physical memory regions the device wants to share. For a full ramdump, this includes all DDR banks. For minidump, this contains only the registered regions from the minidump table.

## 3.3 Ramdump File Structure
After collection, QPST/QFIL saves individual binary files, one per memory segment. The output directory typically contains:
ramdump_output/
+-- DDRCS0_0.BIN          # DDR Chip Select 0, rank 0
+-- DDRCS0_1.BIN          # DDR Chip Select 0, rank 1
+-- DDRCS1_0.BIN          # DDR Chip Select 1, rank 0
+-- DDRCS1_1.BIN          # DDR Chip Select 1, rank 1
+-- OCIMEM.BIN            # On-chip internal memory
+-- PIMEM.BIN             # Peripheral image memory
+-- CODERAM.BIN           # Code RAM
+-- DATARAM.BIN           # Data RAM
+-- MSGRAM.BIN            # Message RAM (IPC)
+-- load.cmm              # T32 script to load all segments
+-- DDRCS_info.txt        # Address ranges for each DDR file
+-- dump_info.txt         # Metadata about the dump

## 3.4 Memory Map Reconstruction
The physical memory layout of a typical Qualcomm SoC is divided into multiple regions. Each DDR bank is captured as a separate binary file. The mapping between files and physical addresses is critical for loading into T32 or crash utility:
| Physical Range | Region | BIN File | Notes |
| 0x0000_0000 – 0x0FFF_FFFF | Reserved (SBL, TZ) | (included in DDR) | Secure world memory |
| 0x1000_0000 – 0x1FFF_FFFF | Peripheral I/O | (not captured) | MMIO space |
| 0x4000_0000 – 0x7FFF_FFFF | DDR Bank 0 (1 GB) | DDRCS0_0.BIN | Kernel + user pages |
| 0x8000_0000 – 0xBFFF_FFFF | DDR Bank 1 (1 GB) | DDRCS0_1.BIN | Continuation |
| 0xC000_0000 – 0xFFFF_FFFF | DDR Bank 2 (1 GB) | DDRCS1_0.BIN | Continuation |
| 0x1_0000_0000 and up | DDR Bank 3+ (>4 GB SoCs) | Additional BIN files | For SoCs with >4 GB DDR |



# 4. Minidump Deep Dive
## 4.1 Architecture & Design
The Qualcomm Minidump framework is a lightweight, production-safe crash collection system. Instead of capturing all DDR, it captures only explicitly registered memory regions. Each kernel subsystem registers its critical state via the msm_minidump_add_region() API. At crash time, the bootloader reads the minidump table from SMEM/IMEM and transfers only the registered physical address ranges via Sahara.
+------------------------------------------------------------+
|                  KERNEL SUBSYSTEMS                         |
|                                                            |
|  +----------+ +----------+ +----------+ +--------------+  |
|  |Scheduler | | Workqueue| |  Timer   | | Per-CPU data |  |
|  |  stacks  | |  states  | |  lists   | |   areas      |  |
|  +----+-----+ +----+-----+ +----+-----+ +------+-------+  |
|       |            |            |               |          |
|       v            v            v               v          |
|  +----------------------------------------------------+    |
|  |         msm_minidump_add_region() API              |    |
|  |   Registers {name, virt_addr, phys_addr, size}     |    |
|  +------------------------+---------------------------+    |
|                           |                                |
|                           v                                |
|  +----------------------------------------------------+    |
|  |         MINIDUMP TABLE (in shared IMEM/SMEM)       |    |
|  |                                                     |    |
|  |  Entry[0]: {name:"KLOGBUF", phys:0x..., size:256K} |    |
|  |  Entry[1]: {name:"KTASK0",  phys:0x..., size:16K}  |    |
|  |  Entry[2]: {name:"KTASK1",  phys:0x..., size:16K}  |    |
|  |  Entry[3]: {name:"KCPUCTX", phys:0x..., size:4K}   |    |
|  |  ...                                               |    |
|  |  Entry[N]: {name:"KSTACKS", phys:0x..., size:...}  |    |
|  +------------------------+---------------------------+    |
|                           |                                |
+---------------------------+--------------------------------+
                            | On crash, bootloader reads this table
                            v
+------------------------------------------------------------+
|  BOOTLOADER (XBL): Iterates minidump table entries         |
|  -> Reads only the registered physical memory ranges       |
|  -> Transfers via Sahara to host                           |
+------------------------------------------------------------+

## 4.2 Minidump Registration API
The minidump registration API is defined in drivers/soc/qcom/msm_minidump.c. Any kernel driver or subsystem can register its critical memory regions to be included in a minidump collection:
/* Kernel: drivers/soc/qcom/msm_minidump.c */

struct md_region {
    char        name[MAX_NAME_LENGTH];  /* Region identifier */
    u64         phys_addr;              /* Physical address */
    u64         virt_addr;              /* Virtual address (for symbol resolution) */
    u64         size;                   /* Region size in bytes */
    u32         id;                     /* Unique region ID */
};

/* Register a region for minidump collection */
int msm_minidump_add_region(const struct md_region *entry);

/* Remove a region */
int msm_minidump_remove_region(const struct md_region *entry);

/* Update an existing region (e.g., stack pointer moved) */
int msm_minidump_update_region(int regno, const struct md_region *entry);

## 4.3 Default Registered Regions
The kernel minidump driver automatically registers a set of default regions at boot time. These provide the core information needed to analyze any crash:
| Region Name | Content | Size | Key Use |
| KLOGBUF | Kernel log buffer (dmesg) | 256 KB | First analysis step |
| KCRASHINFO | Crash metadata / panic reason | 4 KB | Crash type ID |
| KCPUCONTEXT_0..N | Per-CPU register context at crash | 4 KB each | Register analysis |
| KCURRENTTASK_0..N | Current task_struct per CPU | Varies | Task identification |
| KSTACK_0..N | Kernel stack per task | 16 KB each | Stack backtrace |
| KIRQSTACK_0..N | IRQ stacks per CPU | 16 KB each | IRQ context trace |
| KPANICDUMP | Panic dump area | 4 KB | Panic info |
| KRUNQUEUES | Scheduler run queues | Varies | Lockup diagnosis |
| KWQACTIVE | Active workqueues | Varies | Work item tracing |
| KPGTABLE | Page table (swapper_pg_dir) | Varies | MMU reconstruction |
| KTIMER | Timer list state | Varies | Timeout analysis |
| KRCU | RCU state | Varies | RCU stall |
| KMODULES | Loaded module list + addresses | Varies | Symbol resolution |
| KRTB | RTB (Register Trace Buffer) | Varies | Function trace |
| DCC_SRAM | DCC captured hardware registers | Varies | HW state at crash |


## 4.4 Custom Minidump Registration (Driver Example)
Any kernel driver can register its own critical state for minidump collection. This is especially useful for drivers managing hardware state that would otherwise be lost at crash time:
/* Example: A driver registering its critical state for minidump */
#include <soc/qcom/msm_minidump.h>

static char my_driver_state[PAGE_SIZE] __aligned(PAGE_SIZE);
static struct md_region md_entry;

static int my_driver_probe(struct platform_device *pdev)
{
    /* Register critical state buffer for minidump */
    scnprintf(md_entry.name, sizeof(md_entry.name), "MY_DRV_STATE");
    md_entry.phys_addr = virt_to_phys(my_driver_state);
    md_entry.virt_addr = (u64)my_driver_state;
    md_entry.size = PAGE_SIZE;

    if (msm_minidump_add_region(&md_entry))
        dev_err(&pdev->dev, "Failed to register minidump region\n");

    return 0;
}

## 4.5 Minidump Table Layout in SMEM/IMEM
The minidump global table (md_global_toc) resides in shared memory (SMEM) or IMEM. It contains a per-subsystem table of content (md_ss_toc), each holding the registered region list:
+----------------------------------------------------------+
| MINIDUMP GLOBAL TABLE (md_global_toc)                    |
+----------------------------------------------------------+
| magic           | 0x45444D49 ("IMDE")                    |
| version         | Table format version                   |
| num_subsystems  | Number of subsystems registered        |
+----------------------------------------------------------+
| SUBSYSTEM TABLE (md_ss_toc) for Apps Processor           |
| +-- status       : ENABLED / DISABLED                    |
| +-- num_regions  : Number of registered regions          |
| +-- encryption   : Encrypted dump flag                   |
| +-- ss_region_base_ptr: Pointer to region array          |
| +-- regions[]:                                           |
|     +-- [0] name: "KLOGBUF"                              |
|     |       addr: 0xFFFFFF80_12340000                    |
|     |       size: 0x40000                                |
|     |       valid: 1                                     |
|     +-- [1] name: "KCPUCTX0"                             |
|     |       addr: 0xFFFFFF80_ABCD0000                    |
|     |       size: 0x1000                                 |
|     |       valid: 1                                     |
|     +-- ...                                              |
+----------------------------------------------------------+
| SUBSYSTEM TABLE for Modem Processor                      |
| +-- regions[]: Modem-specific dumps (MBA, MPSS)          |
+----------------------------------------------------------+
| SUBSYSTEM TABLE for ADSP/CDSP/SLPI                      |
| +-- regions[]: DSP-specific dumps                        |
+----------------------------------------------------------+


# 5. DCC (Data Capture and Compare) Integration
DCC is a dedicated hardware debugging block on Qualcomm SoCs. It operates independently of the CPU and captures pre-programmed register values when triggered by a crash or watchdog event. Since DCC operates at the hardware level, it can capture critical register state even when the CPU is completely hung and unable to execute any software.
## 5.1 DCC Block Diagram
+----------------------------------------------+
| DCC Block (Hardware)                         |
|                                              |
|  Programmed at boot with register list:      |
|  +----------------------------------------+  |
|  | GCC_GPLL0_STATUS                       |  |
|  | GCC_APCS_CLOCK_CTL                     |  |
|  | TLMM_GPIO_IN_OUT_XX                    |  |
|  | RPM_STATUS_REGS                        |  |
|  | BIMC/LLCC NOC status registers         |  |
|  | PMIC PON STATUS                        |  |
|  | ...                                    |  |
|  +----------------------------------------+  |
|                                              |
|  On trigger (watchdog/crash):                |
|  -> Captures all programmed registers        |
|  -> Stores in DCC_SRAM                       |
|  -> Available in ramdump/minidump            |
+----------------------------------------------+

## 5.2 DCC Configuration via Device Tree
DCC is configured via Device Tree at boot time. The linked list of addresses to capture is specified as a DT property, allowing platform-specific register capture without kernel code changes:
/* Device Tree configuration */
qcom,dcc@<base> {
    compatible = "qcom,dcc-v2";
    reg = <base size>,          /* DCC register space */
          <sram_base sram_size>;  /* DCC SRAM */

    /* Linked list of addresses to capture */
    qcom,curr-link-list = <3>;
    qcom,link-list0-addr = <GCC_BASE+0x1000
                             GCC_BASE+0x1004
                             TLMM_BASE+0x100
                             ...>;
};

## 5.3 How DCC Captures Registers at Crash Time
DCC operates through three phases: programming, monitoring, and capture. Understanding this lifecycle is critical for effectively using DCC data in post-mortem analysis:
| Phase | Description |
| Programming | At boot, the kernel DCC driver programs the list of hardware register addresses into DCC hardware via sysfs or DT configuration. Each entry specifies an MMIO address to capture. |
| Monitoring | DCC continuously watches for a trigger signal. The trigger can be a watchdog bite, a manually asserted halt signal, or the SoC entering download mode. |
| Capture | On trigger, DCC autonomously reads all programmed register addresses and stores the values in DCC_SRAM (a dedicated on-chip SRAM). This happens at hardware speed, before any resets. |
| Recovery | After warm reset, DCC_SRAM contents are included in the OCIMEM.BIN (or a dedicated DCC.BIN) file during ramdump/minidump collection. |
| Analysis | The Qualcomm-provided dcc_parser.cmm T32 script, or ramparse.py, decodes the DCC_SRAM binary into human-readable register name = value output. |



# 6. Parsing & Analysis Workflow
## 6.1 Ramparse Commands and Output Files
Ramparse is Qualcomm's Python-based ramdump parser. It reconstructs kernel data structures from raw binary dumps and generates human-readable text files for each key kernel subsystem:
# Parse full ramdump
python ramparse.py \
    --vmlinux vmlinux \
    --ram-file DDRCS0_0.BIN@0x80000000 \
    --ram-file DDRCS0_1.BIN@0xC0000000 \
    --outdir ./parsed_output/ \
    --everything

# Parse minidump
python ramparse.py \
    --vmlinux vmlinux \
    --minidump-dir ./minidump_files/ \
    --outdir ./parsed_output/ \
    --everything
The --everything flag enables all available parsers. The output directory contains separate files for each kernel subsystem:
parsed_output/
+-- dmesg.txt               # Kernel log buffer (first thing to read)
+-- tasks.txt               # All tasks with state
+-- backtrace.txt           # Per-CPU and crashed task backtrace
+-- irq.txt                 # IRQ state at crash time
+-- workqueues.txt          # Workqueue state
+-- timers.txt              # Active timers
+-- modules.txt             # Loaded modules + load addresses
+-- memory_info.txt         # Memory statistics
+-- slabinfo.txt            # Slab allocator state
+-- page_tracking.txt       # Page owner info
+-- runqueue.txt            # Scheduler run queues per CPU
+-- softirq.txt             # Softirq state
+-- rtb.txt                 # Register trace buffer (last N function calls)
+-- dcc.txt                 # DCC captured hardware registers
+-- cpr.txt                 # CPR (voltage regulator) state
+-- schedstats.txt          # Scheduler statistics
+-- watchdog.txt            # Watchdog state and bite reason

## 6.2 Using crash Utility with Ramdump
The crash utility can be used with ramdump output when combined with a compatible ELF wrapper or vmcore file generated by Ramparse. This provides an interactive kernel debugging environment:
# Convert ramdump to a format crash can read
# (Ramparse generates a vmcore-compatible file)
crash vmlinux vmcore_from_ramdump

# Or use crash with raw ramdump (needs helper script)
crash vmlinux --machdep phys_base=0x80000000 ramdump.elf

# Essential crash commands for ramdump analysis:
crash> log                   # Kernel log buffer (dmesg)
crash> bt                    # Backtrace of panicking task
crash> bt -a                 # Backtrace of ALL CPUs
crash> ps                    # Process list at crash time
crash> task <address>        # Examine task_struct
crash> vm <pid>              # Virtual memory mappings
crash> kmem -s               # Slab allocator state
crash> rd <address> <count>  # Read raw memory
crash> dis <function>        # Disassemble function
crash> files <pid>           # Open file descriptors
crash> runq                  # Run queues per CPU
crash> irq -a                # IRQ state

## 6.3 T32 Loading Scripts (Full CMM Script)
The auto-generated load.cmm script in the ramdump output directory provides a complete T32 setup. Below is the full reference CMM script for loading a Qualcomm ramdump in Lauterbach TRACE32:
; T32 CMM script for Qualcomm ramdump loading
; (auto-generated as load.cmm in ramdump output)

SYSTEM.CPU CORTEXA73
SYSTEM.CONFIG.CORENUMBER 8
SYStem.Option MMUSPACES ON

; Load DDR segments at their physical addresses
Data.Load.Binary "DDRCS0_0.BIN" 0x80000000--0xBFFFFFFF /NoClear
Data.Load.Binary "DDRCS0_1.BIN" 0xC0000000--0xFFFFFFFF /NoClear
Data.Load.Binary "DDRCS1_0.BIN" 0x100000000--0x13FFFFFFF /NoClear

; Load OCIMEM (on-chip internal memory)
Data.Load.Binary "OCIMEM.BIN" 0x14680000--0x1469FFFF /NoClear

; Load kernel symbols (no code, preserve loaded data)
Data.Load.Elf "vmlinux" /NoCODE /NoClear /StripPART 4

; Reconstruct MMU from loaded page tables
MMU.FORMAT LINUXTABLES swapper_pg_dir
MMU.SCAN
MMU.ON

; Set CPU registers to crash-time state
Register.Set PC <crash_pc_from_dump>
Register.Set SP <crash_sp_from_dump>

; Useful T32 commands after loading:
List            ; Show source at crash point (PC)
Frame /Caller   ; Show full call stack
Data.dump       ; Hex dump of memory

; Load module symbols at their runtime load addresses
; (get addresses from modules.txt generated by ramparse)
Data.Load.Elf "my_module.ko" /NoCODE /NoClear /StripPART 4
               /RELOC 0xffffffc0<module_load_addr>


# 7. Subsystem Ramdumps
Qualcomm SoCs are heterogeneous multi-processor systems. Each peripheral processor (modem, DSPs, TZ) can crash independently. When a subsystem crashes, the Peripheral Image Loader (PIL) / Subsystem Restart (SSR) framework on the Apps processor collects the subsystem dump before restarting it.
## 7.1 Subsystem Ramdumps Table
| Subsystem | Dump Mechanism | Analysis Tool | Notes |
| Apps (HLOS) | Full / Mini dump | CrashScope, T32, crash utility | Main Linux kernel crash collection |
| Modem (MPSS) | SSR dump (PIL) | T32 + Modem symbols | Stored in /data/vendor/ssrdump/modem/ |
| ADSP | SSR dump (PIL) | T32 + Hexagon tools | Audio DSP; Hexagon architecture |
| CDSP | SSR dump (PIL) | T32 + Hexagon tools | Compute DSP (AI/ML workloads) |
| SLPI | SSR dump (PIL) | T32 + SLPI symbols | Sensor Low-Power Island |
| WCNSS / WLAN | SSR dump | WiFi FW debug tools | Connectivity subsystem |
| TZ (Secure) | Limited (TZBSP) | Special access only | Restricted; requires special tooling |
| RPM / AOP | RPM dump region | T32 with RPM symbols | Always-On Processor crash data |


## 7.2 SSR Dump Collection Code
When a peripheral processor crashes, the PIL framework on the Apps processor automatically collects and saves the subsystem dump. The relevant code is in drivers/remoteproc/qcom_common.c:
/* When a subsystem crashes, PIL collects its memory before restart */
/* Location: drivers/remoteproc/qcom_common.c */

static void qcom_minidump_cleanup(struct rproc *rproc)
{
    /* Iterate subsystem minidump table entries */
    /* Copy segments to /data/vendor/ssrdump/<subsys>/ */
    /* Files created: md_<REGION_NAME>.BIN */
}

/* SSR dump path: */
/* /data/vendor/ssrdump/modem/   <- Modem dumps */
/* /data/vendor/ssrdump/adsp/    <- ADSP dumps  */
/* /data/vendor/ssrdump/cdsp/    <- CDSP dumps  */
/* /data/vendor/ssrdump/slpi/    <- SLPI dumps  */


# 8. Advanced: Warm Reset Path & Crash Flow
The complete crash flow from kernel panic to dump collection involves five distinct stages. Understanding each stage is essential for diagnosing collection failures and interpreting dump metadata:
+---------------------------------------------------------------------+
| COMPLETE CRASH FLOW (Kernel Panic -> Dump Collection)               |
+---------------------------------------------------------------------+
|                                                                     |
| 1. CRASH EVENT                                                      |
|    +-- Kernel panic() called                                        |
|    +-- die() -> oops -> panic                                       |
|    +-- OR: Secure/Non-secure watchdog expires                       |
|                                                                     |
| 2. PANIC NOTIFIER CHAIN                                             |
|    +-- panic_notifier_list executes registered callbacks            |
|    +-- msm_restart_prepare():                                       |
|    |   +-- Flush caches (ensure DDR coherency)                      |
|    |   +-- Set IMEM download mode cookie                            |
|    |   +-- Set restart reason in PMIC PON registers                 |
|    |   +-- Disable caches + MMU (architecture-specific)            |
|    +-- Write CPU context (registers) to minidump region             |
|    +-- Trigger warm reset (PSHOLD deassert or SCM call)             |
|                                                                     |
| 3. HARDWARE RESET                                                   |
|    +-- PMIC PON detects PSHOLD change                               |
|    +-- Performs WARM RESET (DDR stays powered + in self-refresh)    |
|    +-- Boots XBL/SBL from start                                     |
|                                                                     |
| 4. BOOTLOADER (XBL/SBL)                                             |
|    +-- Initializes minimum hardware                                 |
|    +-- Reads IMEM cookie:                                           |
|    |   +-- Download mode magic found                                |
|    |   +-- Checks dload_type (full vs mini)                         |
|    |   +-- DDR still contains crash-time memory (warm reset)        |
|    +-- If MINIDUMP:                                                 |
|    |   +-- Reads minidump table from SMEM                           |
|    |   +-- Iterates registered regions                              |
|    |   +-- Transfers only those regions via Sahara                  |
|    +-- If FULL DUMP:                                                |
|    |   +-- Maps entire DDR range                                    |
|    |   +-- Transfers all DDR banks via Sahara                       |
|    +-- After transfer -> normal reboot or power down                |
|                                                                     |
| 5. HOST COLLECTION                                                  |
|    +-- QPST/QFIL/custom tool receives Sahara packets                |
|    +-- Writes BIN files to disk                                     |
|    +-- Generates metadata (addresses, sizes, timestamps)            |
|                                                                     |
+---------------------------------------------------------------------+


# 9. Key Kernel Configs & DT Properties
## 9.1 Kconfig Options
The following kernel configuration options control the Qualcomm crash collection infrastructure. These should be set appropriately for development vs. production builds:
# Kernel config options
CONFIG_QCOM_DLOAD_MODE=y          # Enable download mode support
CONFIG_QCOM_MINIDUMP=y            # Enable minidump framework
CONFIG_QCOM_WATCHDOG_V2=y         # Qualcomm watchdog driver
CONFIG_QCOM_WDOG_BITE_DETECT=y    # Watchdog bite detection
CONFIG_QCOM_DCC_V2=y              # DCC hardware block support
CONFIG_QCOM_RTB=y                 # Register trace buffer
CONFIG_MSM_BOOT_STATS=y           # Boot statistics
CONFIG_QCOM_SCM=y                 # Secure Channel Manager
CONFIG_QCOM_MEMORY_DUMP_V2=y      # Memory dump v2 framework

# Debug options (development builds only):
CONFIG_DEBUG_INFO=y               # Include debug symbols
CONFIG_FRAME_POINTER=y            # Frame pointers for backtrace
CONFIG_KALLSYMS=y                 # Symbol resolution in-kernel
CONFIG_KALLSYMS_ALL=y             # Include all symbols

## 9.2 Device Tree Nodes
The Device Tree configures the IMEM regions and download mode cookies. These DT properties are critical for mapping IMEM offsets to their functions:
/* Device Tree properties */
qcom,msm-imem@146bf000 {
    compatible = "qcom,msm-imem";
    reg = <0x146bf000 0x1000>;

    download_mode@0 {
        compatible = "qcom,msm-imem-download_mode";
        reg = <0x0 0x8>;
    };

    restart_reason@65c {
        compatible = "qcom,msm-imem-restart_reason";
        reg = <0x65c 0x4>;
    };

    minidump_table@0x724 {
        compatible = "qcom,msm-imem-minidump";
        reg = <0x724 0x4>;
    };
};


# 10. Debugging Tips & Tricks
## 10.1 Force Download Mode for Debugging
During development, you can force download mode to ensure every crash results in a dump collection. This is typically disabled in production:
# Enable download mode (persists across reboot)
adb shell "echo 1 > /sys/module/msm_poweroff/parameters/download_mode"

# Or via kernel command line at boot:
androidboot.download_mode=1

# Choose minidump vs full dump mode:
echo mini > /sys/kernel/dload/dload_mode    # Minidump only
echo full > /sys/kernel/dload/dload_mode    # Full ramdump

## 10.2 Trigger Manual Crash for Testing
For validating dump collection infrastructure, you can trigger a controlled crash on a development device:
# Method 1: Kernel panic via sysrq
echo c > /proc/sysrq-trigger

# Method 2: Watchdog bite (disable pet, wait for bite)
echo 0 > /sys/devices/platform/soc/<wdt>/pet_time

# Method 3: NULL pointer dereference via debug module
echo KERNEL_NULL > /sys/kernel/debug/trigger_crash

## 10.3 Verify Minidump Regions
Before a crash, you can verify that all expected minidump regions are registered correctly. This is essential during bring-up of new kernel features:
# Check registered regions via debugfs
cat /sys/kernel/debug/qcom_minidump/region_info

# Expected output format:
# Name          PhysAddr              VirtAddr              Size
# KLOGBUF       0x00000001A3400000   0xFFFFFF8012340000   262144
# KCPUCONTEXT0  0x00000001B5600000   0xFFFFFF80ABCD0000   4096
# KCPUCONTEXT1  0x00000001B5601000   0xFFFFFF80ABCD1000   4096
# ...

# Count registered regions:
cat /sys/kernel/debug/qcom_minidump/region_info | wc -l

## 10.4 Analyzing DCC in Post-Mortem
After loading a ramdump in T32, use the Qualcomm-provided DCC parser script to decode the DCC_SRAM contents into human-readable register values:
; In T32 after loading ramdump:
DO dcc_parser.cmm    ; Qualcomm-provided parser script

; Or manually read DCC SRAM contents:
Data.dump A:0x<DCC_SRAM_BASE>

; DCC SRAM binary format:
; [4-byte address][4-byte data][4-byte address][4-byte data]...
; Parser converts to: register_name = value format

; Example parsed output:
; GCC_GPLL0_STATUS      = 0x00000001   (PLL locked)
; TLMM_GPIO_IN_OUT_48   = 0x00000000   (GPIO low)
; BIMC_S_DDR0_STATUS    = 0x00000003   (DDR busy)


# 11. Common Crash Signatures
The following table summarizes the most common crash signatures seen in Qualcomm ramdump and minidump analysis, with guidance on the likely root cause and the first areas to investigate:
| Panic / Crash Signature | Likely Cause | Key Analysis Area |
| Kernel panic - not syncing: Watchdog bite! | Apps processor hung; WDT expired | Check IRQ-disabled paths, spinlock holders (crash> bt -a), DCC output for clocks/power |
| Unable to handle kernel paging request at virtual address XXXX | Invalid memory access; NULL ptr or use-after-free | Page table walk, check SMMU/IOMMU, decode faulting instruction, check callee-saved registers |
| Call trace: ... __schedule ... msm_watchdog_bark | Scheduling issue or soft lockup | Run bt -a on all CPUs, check preemption disabled paths, examine run queues |
| Internal error: synchronous external abort | Bus error (NOC/interconnect fault) | Check NOC error registers via DCC output, SMMU fault status, interconnect state |
| scm_call failed: func id XXXXX, ret: -X | TZ (Trustzone) call failed or rejected | Check TZ logs, SCM interface state, QSEECOM log, verify TZ version compatibility |
| Kernel panic: subsys <name> crashed and failed to restart | Peripheral processor (modem/ADSP) crash | Collect subsystem SSR dump from /data/vendor/ssrdump/, analyze with subsystem-specific tools |



— End of Part 2 | Continue to Part 3: Stack Dump Examples & Register Deep Dive —

Part 2 of 3 — Crash Dump Analysis Guide — Qualcomm Ramdump & Minidump Deep Dive
