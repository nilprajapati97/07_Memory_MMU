Qualcomm Boot Flow
Power-On to Kernel Start
Document 1 of 3 — Interview Preparation Series
Bootloader Development | Qualcomm MSM/IoT Platforms | ARM Architecture
|  |


Coverage: Pages 1–20 | Stages: PBL → SBL/XBL → APPSBL → Kernel
- Complete Qualcomm Boot Flow Explanation with all stages
- PBL (Primary Bootloader) — Deep dive with 4 cross-questions
- SBL1/XBL (Secondary/eXtensible Bootloader) — DDR training, SMEM, 5 cross-questions
- APPSBL — LK / UEFI ABL — Verified Boot, Fastboot, A/B, 5 cross-questions
- Linux Kernel Boot — Entry to init process, 3 cross-questions
- Boot Time Breakdown Table across all stages
- Key Concepts summary for interview
- Complete ASCII Visual Boot Flow Diagram with Chain of Trust, Memory Map, and Decision Tree

Complete Qualcomm Boot Flow: Power-On to Kernel Start
This document provides a comprehensive explanation of the Qualcomm boot flow from power-on reset to kernel start. It covers every stage in depth with technical details, rationale, implementation specifics, and interview cross-questions that may arise during technical interviews for bootloader development positions.
Overview of Boot Stages
Qualcomm's boot architecture follows a chain of trust model with multiple stages:
| Stage | Name | Time | Memory Used | Key Responsibility |
| 1 | PBL (Primary Bootloader) | 50–100ms | Internal SRAM/IMEM (~256–512KB) | ROM code, Root of Trust, Load SBL/XBL |
| 2 | SBL1/XBL (Secondary / eXtensible Bootloader) | 300–600ms | IMEM → DDR (after training) | DDR training, HW setup, Load APPSBL |
| 3 | APPSBL — LK / UEFI ABL | 200–400ms | DDR (full access) | Display init, Verified Boot (AVB), Load kernel |
| 4 | Linux Kernel Boot | 500–1000ms | DDR (virtual addressing) | MMU setup, Parse DTB, Mount initramfs, Start /init |
| 5 | Android Init | 2–4s | DDR (fully mapped) | Mount filesystems, Start services, Launch Zygote/SystemServer |


Stage 1: Power-On Reset (POR) to PBL
  PBL (Primary Bootloader) — ROM Code  |  Time: ~50–100ms  |  Memory: Internal SRAM/IMEM
What Happens
When you press the power button or apply power:
- Hardware reset occurs; CPU comes out of reset state
- Program Counter (PC) is set to the boot ROM address (typically 0x00000000 or a fixed SoC address)
- CPU starts executing code from PBL (Primary Bootloader), which is hardcoded in ROM (immutable)
Why PBL Exists
- Root of Trust: PBL is the first software that runs and is immutable (burned into silicon)
- Minimal footprint: Runs without DDR using internal SRAM/IMEM only
- Security anchor: Contains the root public key hash burned in eFuses for secure boot
How PBL Works — Step-by-Step
Step 1: Hardware Initialization (Minimal)
- Configure CPU clocks to a safe operating frequency
- Initialize internal SRAM/IMEM (Instruction Memory) — typically 256KB–512KB
- Set up minimal exception vectors
- Configure boot media interface (eMMC, UFS, NAND, SD card)
Step 2: Determine Boot Source
- Read boot configuration pins/fuses to determine the boot device
- Common boot sources: eMMC, UFS, NAND, SD card, USB (emergency download mode)
- Check for Emergency Download Mode (EDL) — triggered if volume down + power is pressed
Step 3: Load SBL/XBL
- Read partition table from boot media (GPT — GUID Partition Table)
- Locate the SBL1/XBL partition (usually named "sbl1" or "xbl")
- Load SBL1/XBL image into IMEM (internal memory)
Step 4: Authenticate SBL/XBL (Secure Boot)
- Extract code signature from SBL1/XBL image header
- Retrieve OEM public key hash from eFuses
- Verify signature using RSA-2048/4096 or ECC cryptography
- If authentication fails: halt boot or enter EDL mode
- If authentication succeeds: continue to next step
Step 5: Jump to SBL/XBL
- Set up stack pointer in IMEM
- Jump to SBL1/XBL entry point
Cross-Questions for Stage 1: PBL
Q: "What if PBL code has a bug? Can it be updated?"
A: "No, PBL is ROM code and cannot be updated. However, Qualcomm designs PBL to be minimal and robust. If critical bugs are found, they are typically worked around in SBL/XBL. In extreme cases, new silicon revisions are required."
Q: "How does PBL know which boot device to use?"
A: "PBL reads boot configuration from eFuses or GPIO pins. The boot order is typically: eMMC/UFS → SD card → USB (EDL mode). Some chipsets allow boot device selection via hardware strapping pins."
Q: "What happens if the boot media is corrupted?"
A: "PBL will fail to load SBL. Depending on the chipset configuration, it may: 1) Try alternate boot sources, 2) Enter EDL (Emergency Download) mode for recovery via USB, or 3) Halt with no output."
Q: "Where is the root public key stored?"
A: "The SHA-256 hash of the OEM root public key is burned into OEM eFuses during manufacturing. The full public key is embedded in the signed image headers. This is why the eFuse-stored hash is immutable — it is the hardware root of trust."
Stage 2: SBL1/XBL (Secondary / eXtensible Bootloader)
  SBL/XBL — Secondary/eXtensible Bootloader  |  Time: ~300–600ms  |  Memory: IMEM → DDR (after training)
What Happens
SBL1 (older chipsets) or XBL (newer chipsets like SDM845+) takes over from PBL and performs the most hardware-intensive initialization.
Why SBL/XBL Exists
- DDR initialization: PBL runs from IMEM (limited size); SBL enables DDR for larger code execution
- Advanced hardware setup: Configure clocks, power rails, buses, and interrupt controllers
- Load next stage: Prepare and authenticate APPSBL (LK/UEFI ABL)
How SBL/XBL Works — Step-by-Step
Step 1: Early Hardware Initialization
- Configure system clocks (CPU, bus, peripheral clocks)
- Initialize PMIC (Power Management IC): Set voltage rails for CPU, DDR, peripherals
- Set up exception vectors and interrupt controller (GIC — Generic Interrupt Controller)
- Initialize UART for debug logs (if enabled in build)
Step 2: DDR Initialization (Critical — Longest Part)
DDR initialization is the most complex part of XBL and the primary contributor to boot time:
DDR Training Sub-Steps:
- Write Leveling: Align write data with clock (~50ms)
- Read Leveling: Align read data capture (~80ms)
- Vref Training: Optimize reference voltage for stable operation (~40ms)
- Memory Controller Config: Set frequency, timing parameters (CAS latency, RAS, etc.), memory map, interleaving
- Memory Verification: Basic read/write tests to verify DDR is functional
Step 3: Load Configuration Data
- Read CDT (Configuration Data Table) from boot media — contains board-specific settings: DDR type, PMIC configuration, GPIO mappings
- Read Device Tree Blob (DTB) or ACPI tables (for UEFI-based systems)
Step 4: Initialize Shared Memory (SMEM)
- Set up SMEM region in DDR (Shared Memory — used for inter-processor communication)
- SMEM is used by the Apps processor, modem, ADSP, and other subsystems
- Stores boot statistics, hardware info, and partition table
Step 5: Load Next Stage (SBL2/SBL3 or directly APPSBL)
On older chipsets: SBL1 → SBL2 (additional HW init) → SBL3 (loads/authenticates APPSBL)
On newer chipsets (XBL): XBL-Sec (security-focused init) → XBL-Core (main bootloader logic) → directly loads UEFI ABL
Step 6: Authenticate and Jump to APPSBL
- Load LK (Little Kernel) or UEFI ABL from boot media
- Authenticate using chain of trust (signature verification)
- Jump to APPSBL entry point
Cross-Questions for Stage 2: SBL/XBL
Q: "What is DDR training and why is it necessary?"
A: "DDR training calibrates the timing relationship between the memory controller and DDR chips. Due to PCB trace lengths, temperature, and voltage variations, optimal timing parameters vary per board. Training involves: write leveling (aligning write data with clock), read leveling (aligning read data capture), and Vref calibration (optimizing reference voltage). Without training, DDR access would be unreliable, causing data corruption."
Q: "How long does DDR training take and can you skip it?"
A: "Typically 100–500ms depending on DDR type (LPDDR4/5) and training algorithms — this is often the longest part of boot time. Optimizations include: saving training results to flash and reusing them (fast boot / training restore), which reduces boot time by 200–400ms. Full retraining is required after firmware updates, significant temperature changes, or if fast boot validation fails."
Q: "What is SMEM and why is it important?"
A: "SMEM (Shared Memory) is a reserved DDR region used for inter-processor communication on Qualcomm SoCs. The applications processor, modem, ADSP, and other subsystems use SMEM to share: boot statistics, hardware configuration, partition table, and runtime messages. It is initialized by SBL and persists across boot stages, enabling subsystem coordination."
Q: "What happens if DDR initialization fails?"
A: "Boot will halt. SBL typically outputs error codes via UART or LED patterns. Common causes: incorrect DDR configuration in CDT, hardware issues (bad DDR chips, power supply problems), or PCB design issues. Recovery requires JTAG debugging or EDL mode to reflash the correct configuration."
Q: "What is the difference between SBL and XBL?"
A: "SBL (Secondary Bootloader) is used on older Qualcomm chipsets, organized as SBL1 → SBL2 → SBL3. XBL (eXtensible Bootloader) is used on newer chipsets (SDM845+), organized as XBL-Sec → XBL-Core. XBL is based on UEFI EDK2 framework, providing better modularity, easier porting, and standardized interfaces. Both serve the same purpose but XBL is more maintainable and feature-rich."
Stage 3: APPSBL — LK (Little Kernel) / UEFI ABL
  APPSBL (Applications Bootloader)  |  LK or UEFI ABL  |  Time: ~200–400ms  |  Memory: DDR (full access)
What Happens
The applications bootloader (LK on older Android devices, UEFI ABL on newer ones) takes over from SBL/XBL. This is the most feature-rich stage visible to developers.
Why APPSBL Exists
- User interaction: Display splash screen, handle fastboot commands from host PC
- Partition management: Mount partitions, load kernel and device tree
- Advanced features: Verified boot, A/B partition switching, recovery mode selection
How LK/UEFI ABL Works — Step-by-Step
Step 1: Display Initialization
- Initialize display controller and panel hardware
- Show boot splash screen (OEM logo)
- Initialize framebuffer in DDR for rendering
Step 2: Fastboot Protocol Check
- Check if user pressed volume down (fastboot mode trigger)
If fastboot mode: Enter fastboot mode — listen for USB commands:
- fastboot flash — flash partitions
- fastboot boot — boot a custom kernel without flashing
- fastboot oem — OEM-specific commands
Step 3: Determine Boot Mode
- Check boot reason (normal boot, recovery, charger mode)
- Read misc partition for boot commands from Android
- Check for recovery mode request from previous OS session
Step 4: Load Device Tree Blob (DTB)
- Read DTB partition or extract DTB from boot image
- DTB contains hardware description: peripherals, memory map, GPIO assignments, interrupt numbers, clock configurations
- Pass DTB address to kernel for hardware discovery
Step 5: Verified Boot (AVB — Android Verified Boot)
- Read vbmeta partition (contains signatures and hash trees)
- Verify boot.img signature using the OEM public key
- Verify system and vendor partitions using dm-verity hash trees
Set boot state based on verification result:
- GREEN: Locked device, fully verified
- YELLOW: Unlocked with user-set key
- ORANGE: Custom OS (bootloader unlocked)
- RED: Verification failed — boot blocked or warned
- Display warning screen if device is unlocked
Step 6: Load Kernel and Ramdisk
- Read boot partition (contains kernel + ramdisk in Android boot image format)
- Parse boot image header to locate kernel and ramdisk offsets
- Load kernel (zImage or Image.gz) to DDR
- Load ramdisk (initramfs) to DDR; decompress if necessary
Step 7: Set Up Kernel Parameters
- Prepare command line arguments (bootargs), e.g.:
console=ttyMSM0,115200 androidboot.hardware=qcom androidboot.serialno=XXX
- Set up device tree or ATAGS (older ARM32) with memory map, boot reason, serial number
- Pass DTB address and memory map to kernel via device tree
Step 8: Jump to Kernel
- Disable MMU and caches (kernel will set up its own MMU configuration)
- Set CPU to appropriate exception level:
- ARM64: EL2 (hypervisor) or EL1 (kernel mode)
- ARM32: SVC mode
- Set registers: x0 = DTB address, x1/x2/x3 = 0 (reserved), PC = kernel entry point (_start)
- JUMP to kernel entry point!
Cross-Questions for Stage 3: APPSBL
Q: "What is the difference between LK and UEFI ABL?"
A: "LK (Little Kernel) is a lightweight bootloader used in older Qualcomm Android devices. UEFI ABL (Android Bootloader) is based on UEFI (Unified Extensible Firmware Interface) and used in newer chipsets (SDM845+). UEFI ABL provides better modularity, standardized interfaces, easier porting, and support for ACPI (instead of device tree). Both serve the same purpose but UEFI ABL is more feature-rich and maintainable."
Q: "Explain Android Verified Boot (AVB)."
A: "AVB ensures the integrity of the boot chain. It works by: 1) Signing boot.img, system.img, vendor.img with OEM private key; 2) Storing signatures in vbmeta partition; 3) ABL verifies signatures using public key (stored in device or vbmeta); 4) For large partitions (system, vendor), AVB uses dm-verity hash trees to verify blocks on-demand during runtime. If verification fails, device shows a warning or refuses to boot depending on configuration."
Q: "What is fastboot and when is it used?"
A: "Fastboot is a protocol for communicating with the bootloader over USB. It is used for: flashing partitions (system, boot, recovery), unlocking bootloader, booting custom kernels without flashing, and device recovery. You enter fastboot mode by holding volume down + power during boot. Common commands: fastboot flash boot boot.img, fastboot oem unlock, fastboot boot custom-kernel.img."
Q: "What are A/B partitions and how does ABL handle them?"
A: "A/B partitions (seamless updates) maintain two copies of boot-critical partitions (boot_a/boot_b, system_a/system_b). During OTA update, the inactive slot is updated while the device runs normally. After update, the bootloader switches to the new slot. If boot fails, it automatically rolls back to the previous slot. ABL reads the active slot from the misc partition or boot control block and loads the corresponding boot_a or boot_b partition."
Q: "How does the bootloader pass information to the kernel?"
A: "On ARM64, the bootloader passes: 1) Device Tree Blob (DTB) address in register x0; 2) Kernel command line embedded in DTB or passed separately; 3) Memory map in DTB; 4) Boot reason, serial number, and other info via command line parameters (androidboot.* parameters). The kernel parses DTB during early boot to discover hardware and configure drivers."
Stage 4: Linux Kernel Boot
  Linux Kernel Boot  |  Time: ~500–1000ms  |  Memory: DDR (initially physical → virtual addressing)
How Kernel Boot Works
Step 1: Kernel Entry Point
- Kernel starts at _start or stext (architecture-specific entry point)
- Running in physical address space (MMU is still disabled)
- CPU is in EL1 (kernel mode) on ARM64
Step 2: Early Setup
- Set up initial page tables (identity mapping for physical addresses)
- Enable MMU and switch to virtual addressing
- Set up exception vectors
- Initialize CPU-specific features (caches, TLB, FPU, hardware features)
Step 3: Decompress Kernel (if compressed)
- If kernel is compressed (zImage, Image.gz), self-extracting code runs first
- Decompress kernel image to final DDR location
- LZ4 compression is recommended for best decompression speed (~1.5 GB/s)
Step 4: Parse Device Tree
- Parse DTB (Device Tree Blob) passed by bootloader in register x0
- Discover hardware: memory size, peripherals, interrupt controllers, clock configs
- Initialize device tree framework for driver discovery
Step 5: Initialize Core Subsystems
- Memory management: Set up page allocator, slab/slub allocator
- Scheduler: Initialize task scheduler (CFS — Completely Fair Scheduler)
- Interrupt handling: Set up IRQ subsystem and GIC (Generic Interrupt Controller)
- Timers: Initialize system timers (arch timer, clockevent, clocksource)
Step 6: Mount Ramdisk (initramfs)
- Unpack ramdisk passed by bootloader (compressed cpio archive)
- Mount as root filesystem (temporary in-memory filesystem)
- Contains essential tools, init scripts, and SELinux policy
Step 7: Execute Init Process
- Run /init from ramdisk (on Android, this is the Android init process)
- Init process mounts real filesystems (system, vendor, data)
- Starts system services (servicemanager, surfaceflinger, zygote)
- Launches Android framework (Zygote → System Server → Home Launcher)
Cross-Questions for Stage 4: Linux Kernel
Q: "What is the difference between zImage and Image?"
A: "zImage is a compressed kernel image (gzip), while Image is uncompressed. zImage includes self-extracting code that decompresses the kernel during boot. zImage is smaller (saves flash space) but takes longer to boot due to decompression. Image boots faster but requires more storage. On modern devices with fast storage, Image.gz (gzip) or Image.lz4 (LZ4 — fastest decompression) is common."
Q: "What is initramfs and why is it needed?"
A: "Initramfs (initial RAM filesystem) is a temporary root filesystem loaded into RAM by the bootloader. It contains: essential storage and filesystem drivers, init scripts, and tools needed to mount the real root filesystem. On Android, initramfs contains the init binary, SELinux policies, and scripts to mount system/vendor/data partitions. It is needed because the kernel cannot directly mount the real root filesystem without first loading those drivers."
Q: "How does the kernel know where peripherals are located?"
A: "The kernel parses the Device Tree Blob (DTB) passed by the bootloader. DTB contains: memory-mapped register addresses for peripherals, interrupt numbers, clock configurations, GPIO assignments, and device-specific properties. The kernel's device tree framework parses this and instantiates drivers for each device node. On x86 systems, ACPI tables serve a similar purpose."
Complete Boot Time Breakdown
The following table provides a typical boot time breakdown for a modern Qualcomm Android device:
| Stage | Time | Key Activities | Optimization Potential |
| PBL | 50–100ms | ROM code, determine boot source, load & authenticate SBL/XBL | Low — ROM code, eFuse boot config helps |
| SBL/XBL | 300–600ms | DDR training (longest), PMIC setup, clock init, SMEM init, load APPSBL | High — DDR training restore saves 200–400ms |
| APPSBL (LK/ABL) | 200–400ms | Display init, verified boot (AVB), DTB overlay selection, load kernel+ramdisk | Medium — HW crypto, LZ4 kernel, parallel init |
| Kernel | 500–1000ms | Decompress, MMU/cache init, parse DTB, init subsystems, mount initramfs | Medium — LZ4 compression, quiet boot, deferred init |
| Android Init | 2–4s | Mount filesystems, start services, Zygote, System Server, Launcher | High — parallel services, F2FS, Zygote optimization |
| TOTAL | 3–6s | Complete cold boot to home screen | Target: <3s (automotive: <2s) |


Key Concepts to Emphasize in Interview
Chain of Trust
Each boot stage cryptographically authenticates the next stage before executing it. PBL (ROM) is the hardware root of trust; eFuse-stored key hash makes it immutable and trustworthy. The chain extends: PBL → verifies SBL/XBL → verifies APPSBL → verifies kernel via AVB.
Secure Boot & Root of Trust
The root of trust starts in eFuses (one-time programmable fuses burned during manufacturing). The SHA-256 hash of the OEM root public key is stored in eFuses. All subsequent stage verifications use RSA-2048/4096 or ECC cryptography. This makes the boot chain tamper-resistant.
DDR Training
Critical process that calibrates DDR interface timing (write leveling, read leveling, Vref training). Required because PCB trace lengths, temperature, and voltage create unique timing parameters per board. Results can be cached (fast boot) to reduce subsequent boot times by 200–400ms.
SMEM (Shared Memory)
Reserved DDR region (~512KB) initialized by SBL that persists across all boot stages. Provides inter-processor communication mechanism between Apps CPU, Modem, ADSP, and other subsystems. Stores: boot statistics, partition table, hardware info, subsystem status.
Verified Boot (AVB)
Android Verified Boot ensures integrity of the entire software stack. Uses vbmeta partition to store signatures and hash trees. dm-verity provides block-level on-demand verification for large partitions (system, vendor). Boot states (GREEN/YELLOW/ORANGE/RED) indicate trust level.
Device Tree (DTB/DTBO)
Hardware description language for ARM systems. Base DTB (in kernel/boot.img) describes SoC hardware. DTB Overlay (DTBO) partition contains board-specific modifications (display panel, sensors, GPIO assignments). ABL merges base DTB + correct DTBO and passes to kernel. Enables one kernel to support multiple board variants.
Fastboot Protocol
USB protocol for bootloader-level communication. Used for: flashing partitions, bootloader unlock, booting custom kernels without flashing. Essential for development workflow and device recovery. Entered by holding volume down + power during boot.
A/B Partition Scheme
Maintains two copies of boot-critical partitions (_a and _b slots) for seamless OTA updates. Update inactive slot while device runs normally. Automatic rollback if new slot fails to boot. ABL determines active slot from boot control block in misc partition.
Exception Levels (EL3→EL2→EL1)
ARM64 privilege levels: EL3 (Secure Monitor/TrustZone), EL2 (Hypervisor), EL1 (Kernel mode), EL0 (User mode). PBL starts at EL3, transitions happen during boot. Kernel runs at EL1 or EL2 (with virtualization).
UEFI Boot Services vs Runtime Services
UEFI ABL provides Boot Services (available only before ExitBootServices() is called) and Runtime Services (available to OS after boot). Kernel calls ExitBootServices() to take full ownership of the system. Runtime Services include: GetTime, SetVariable (for EFI variables), etc.
Visual Boot Flow Diagram
The following ASCII diagram represents the complete Qualcomm boot flow from Power-On Reset to Kernel Start, including the Chain of Trust, Memory Usage across stages, Boot Mode Decision Tree, and Architecture comparison between newer XBL-based and older SBL-based chipsets.
=================================================================================
              QUALCOMM BOOT FLOW DIAGRAM
              Power-On Reset -> Kernel Start
=================================================================================

   +---------------+
   |  POWER ON     |  User presses power button / Power applied
   |   RESET       |  CPU reset state, PC -> Boot ROM address
   +-------+-------+
           |
           v
+------------------------------------------------------------------------------+
|  STAGE 1: PBL (Primary Bootloader) -- ROM Code [~50-100ms]                  |
|  +------------------------------------------------------------------------+  |
|  |  Execution: From on-chip ROM (immutable, burned into silicon)          |  |
|  |  Memory:    Internal SRAM / IMEM (~256-512KB)                          |  |
|  |  Trust:     ROOT OF TRUST -- Contains OEM public key hash in eFuses    |  |
|  +------------------------------------------------------------------------+  |
|                                                                               |
|  +-----------------+   +------------------+   +------------------------+     |
|  | 1. Minimal HW   +-->| 2. Determine     +-->| 3. Load SBL/XBL       |     |
|  |    Init         |   |    Boot Source   |   |    from boot media    |     |
|  |  * CPU clocks   |   |  * Read boot     |   |  * Read GPT table     |     |
|  |  * Init IMEM    |   |    config fuses  |   |  * Locate partition   |     |
|  |  * Exception    |   |  * eMMC/UFS/     |   |  * Load to IMEM       |     |
|  |    vectors      |   |    NAND/SD/USB   |   |                       |     |
|  |  * Boot media   |   |  * Check EDL     |   |                       |     |
|  |    interface    |   |    mode request  |   |                       |     |
|  +-----------------+   +------------------+   +-----------+-----------+     |
|                                                            |                  |
|                   +--------------------+                  |                  |
|                   v                    |                  v                  |
|        +---------------------+         |                                     |
|        | 4. AUTHENTICATE     |         |                                     |
|        |    SBL/XBL          |         |                                     |
|        |  * Extract code sig |         |                                     |
|        |  * Verify against   +--FAIL-->| [HALT / EDL Mode]                   |
|        |    eFuse key hash   |         |                                     |
|        |  * RSA-2048/4096    |         |                                     |
|        |    or ECC           |         |                                     |
|        +--------+------------+         |                                     |
|                 | PASS                 |                                     |
|                 v                      |                                     |
|        +---------------------+         |                                     |
|        | 5. Jump to SBL/XBL  |         |                                     |
|        |  * Set stack pointer|         |                                     |
|        |  * Jump to entry pt |         |                                     |
|        +--------+------------+         |                                     |
+=================+===========================================================+
                  |
                  v
+==============================================================================+
|  STAGE 2: SBL/XBL (Secondary/eXtensible Bootloader) [~300-600ms]            |
|  +------------------------------------------------------------------------+  |
|  |  Execution: Initially from IMEM, then DDR after training               |  |
|  |  Purpose:   DDR init, advanced HW setup, load APPSBL                   |  |
|  |  Variants:  SBL1->SBL2->SBL3 (older) | XBL-Sec->XBL-Core (newer)     |  |
|  +------------------------------------------------------------------------+  |
|                                                                               |
|  +-----------------+   +--------------------------------------------+        |
|  | 1. Early HW     +-->| 2. DDR INITIALIZATION (Most Complex Part)  |        |
|  |    Init         |   |                                            |        |
|  |  * System clks  |   |  +--------------------------------------+  |        |
|  |  * PMIC config  |   |  |  DDR Training (~100-500ms)           |  |        |
|  |  * GIC (intr    |   |  |  * Write Leveling (align write/clk) |  |        |
|  |    controller)  |   |  |  * Read Leveling (align read capt)  |  |        |
|  |  * UART (debug) |   |  |  * Vref Training (optimize voltage) |  |        |
|  |                 |   |  |  * Memory Controller Config          |  |        |
|  |                 |   |  |  * Basic R/W Verification            |  |        |
|  |                 |   |  +--------------------------------------+  |        |
|  +-----------------+   +--------------------+-----------------------+        |
|                                             |                                 |
|                                             v                                 |
|  +-----------------+   +------------------+   +------------------------+     |
|  | 3. Load Config  +-->| 4. Init SMEM     +-->| 5. Load & Auth         |     |
|  |    Data         |   |    (Shared Mem)  |   |    APPSBL (LK/ABL)    |     |
|  |  * CDT (board   |   |  * Inter-proc    |   |  * Read from boot      |     |
|  |    config)      |   |    communication |   |    media               |     |
|  |  * DTB / ACPI   |   |  * Boot stats    |   |  * Authenticate        |     |
|  |  * DDR params   |   |  * HW info       |   |    (chain of trust)    |     |
|  |                 |   |  * Partition tbl  |   |  * Jump to APPSBL      |     |
|  +-----------------+   +------------------+   +-----------+------------+     |
+============================================+==============+==================+
                                                            |
                                                            v
+==============================================================================+
|  STAGE 3: APPSBL -- LK (Little Kernel) / UEFI ABL [~200-400ms]              |
|  +------------------------------------------------------------------------+  |
|  |  Execution: From DDR (full DDR access available)                       |  |
|  |  Purpose:   User interaction, verified boot, load kernel               |  |
|  |  LK: Older chipsets | UEFI ABL: Newer chipsets (SDM845+)               |  |
|  +------------------------------------------------------------------------+  |
|                                                                               |
|  +-----------------+   +------------------+   +------------------------+     |
|  | 1. Display Init +-->| 2. Check Boot    +-->| 3. Determine Boot      |     |
|  |  * Init display |   |    Mode Keys     |   |    Mode                |     |
|  |    controller   |   |                  |   |                        |     |
|  |  * Show splash  |   |  Vol Down held?  |   |  * Normal Boot         |     |
|  |    screen (logo)|   |    +--YES->+     |   |  * Recovery Mode       |     |
|  |  * Init frame-  |   |    v       |     |   |  * Charger Mode        |     |
|  |    buffer       |   | FASTBOOT   | NO  |   |  * Read misc partition  |     |
|  |                 |   | MODE       |     |   |                        |     |
|  |                 |   | (USB cmds) v     |   |                        |     |
|  +-----------------+   +------------------+   +-----------+------------+     |
|                                                            |                  |
|                 +------------------------------------------+                 |
|                 v                                                             |
|  +--------------------------------------------------------------------+       |
|  | 4. ANDROID VERIFIED BOOT (AVB)                                     |       |
|  |  * Read vbmeta partition (signatures + hash trees)                 |       |
|  |  * Verify boot.img signature using public key                      |       |
|  |  * Verify system/vendor via dm-verity hash trees                   |       |
|  |  * Set boot state: GREEN | YELLOW | ORANGE | RED                   |       |
|  |  * Display warning if device unlocked                              |       |
|  +-------------------------------+------------------------------------+       |
|                                  |                                            |
|                                  v                                            |
|  +-----------------+   +------------------+   +------------------------+     |
|  | 5. Load Kernel  +-->| 6. Setup Kernel  +-->| 7. JUMP TO KERNEL      |     |
|  |    & Ramdisk    |   |    Parameters    |   |                        |     |
|  |  * Parse boot   |   |  * Command line  |   |  * Disable MMU/Cache  |     |
|  |    image header |   |    (bootargs)    |   |  * Set CPU to EL2/EL1 |     |
|  |  * Load kernel  |   |  * DTB address   |   |  * x0 = DTB address   |     |
|  |    (zImage/gz)  |   |  * Memory map    |   |  * PC = kernel entry  |     |
|  |  * Load ramdisk |   |  * Boot reason   |   |    point (_start)     |     |
|  |    (initramfs)  |   |  * Serial number |   |  * JUMP!              |     |
|  +-----------------+   +------------------+   +-----------+------------+     |
+============================================+==============+==================+
                                                            |
                                                            v
+==============================================================================+
|  STAGE 4: LINUX KERNEL BOOT [~500-1000ms]                                   |
|  +------------------------------------------------------------------------+  |
|  |  Execution: From DDR, initially physical addressing -> virtual         |  |
|  |  Purpose:   Initialize OS, mount filesystems, start services           |  |
|  +------------------------------------------------------------------------+  |
|                                                                               |
|  +-----------------+   +------------------+   +------------------------+     |
|  | 1. Entry Point  +-->| 2. Early Setup   +-->| 3. Decompress Kernel   |     |
|  |  * _start/stext |   |  * Initial page  |   |    (if zImage/gz)      |     |
|  |  * Physical     |   |    tables        |   |  * Self-extracting     |     |
|  |    addr space   |   |  * Enable MMU    |   |    code runs           |     |
|  |  * CPU in EL1   |   |  * Exception     |   |  * Decompress to       |     |
|  |                 |   |    vectors       |   |    final DDR location  |     |
|  |                 |   |  * CPU features  |   |                        |     |
|  |                 |   |    (cache, TLB)  |   |                        |     |
|  +-----------------+   +------------------+   +-----------+------------+     |
|                                                            |                  |
|                 +------------------------------------------+                 |
|                 v                                                             |
|  +-----------------+   +------------------+   +------------------------+     |
|  | 4. Parse DTB    +-->| 5. Init Core     +-->| 6. Mount initramfs     |     |
|  |  * Discover HW  |   |    Subsystems    |   |  * Unpack ramdisk      |     |
|  |  * Memory size  |   |  * Memory mgmt   |   |  * Mount as temp       |     |
|  |  * Peripherals  |   |    (page/slab)   |   |    root filesystem     |     |
|  |  * IRQ config   |   |  * Scheduler     |   |  * Essential tools     |     |
|  |  * Clock config |   |  * IRQ subsystem |   |    & init scripts      |     |
|  |                 |   |  * System timers |   |                        |     |
|  +-----------------+   +------------------+   +-----------+------------+     |
|                                                            |                  |
|                 +------------------------------------------+                 |
|                 v                                                             |
|  +--------------------------------------------------+                         |
|  | 7. Execute /init (Android Init Process)          |                         |
|  |  * Mount real filesystems (system, vendor, data) |                         |
|  |  * Start system services                         |                         |
|  |  * Launch Zygote -> System Server                |                         |
|  |  * Boot animation displayed                      |                         |
|  +--------------------------+-----------------------+                         |
+=============================+==============================================+
                              |
                              v
             +---------------------+
             |   SYSTEM READY      |
             |   Home Screen       |
             |   (~3-6s total)     |
             +---------------------+

Chain of Trust (Secure Boot)
=========================================================================
                   CHAIN OF TRUST (SECURE BOOT)
=========================================================================

  +---------+  Authenticates  +---------+  Authenticates  +---------+  Verifies
  |   PBL   | --------------> |SBL/XBL  | --------------> | APPSBL  | ---------->
  |  (ROM)  |   RSA/ECC       |         |   RSA/ECC       |(LK/ABL) |  AVB
  |         |                 |         |                 |         |
  | Root of |                 | Loads & |                 | Loads & |  +--------+
  | Trust   |                 | inits   |                 | verifies|  | Kernel |
  | (eFuse) |                 | DDR     |                 | kernel  |  |+System |
  +---------+                 +---------+                 +---------+  +--------+

  eFuse Key Hash --> OEM Public Key --> Code Signatures --> dm-verity Hashes

=========================================================================

Memory Usage Across Stages
=========================================================================
                    MEMORY USAGE ACROSS STAGES
=========================================================================

  PBL          |########                        |  IMEM only (~256-512KB)
  SBL/XBL      |####################            |  IMEM -> DDR (after training)
  APPSBL       |        ######################  |  DDR (full access)
  Kernel       |          ####################  |  DDR (virtual addressing)
               +--------------------------------+
               0                             4GB+

  Legend: # = Active memory region  . = Available but not primary

=========================================================================

Boot Mode Decision Tree (APPSBL)
=========================================================================
                BOOT MODE DECISION TREE (APPSBL)
=========================================================================

                    +--------------+
                    | APPSBL Start |
                    +------+-------+
                           |
               +-----------v-----------+
               | Vol Down + Power?     |
               +-----------+-----------+
                  YES +----+---- NO
                      |         |
               +------v----+  +-v-----------------+
               | FASTBOOT  |  | Vol Up + Power?   |
               | MODE      |  +--------+----------+
               | (USB)     |   YES +---+--- NO
               +-----------+       |       |
                          +--------v-+  +--v--------------+
                          | RECOVERY |  | Check misc      |
                          | MODE     |  | partition       |
                          +----------+  +------+----------+
                                            +---+---+
                                            |       |
                                     +------v----+ +-v-----------+
                                     | CHARGER   | | NORMAL BOOT |
                                     | MODE      | |             |
                                     | (USB pwr) | | Load kernel |
                                     +-----------+ | & boot OS   |
                                                   +-------------+

=========================================================================

Newer vs Older Chipset Architecture
=========================================================================
        NEWER CHIPSETS: XBL ARCHITECTURE (SDM845+)
=========================================================================

  +---------+    +-----------+    +-----------+    +-----------+    +--------+
  |   PBL   |--->|  XBL-Sec  |--->| XBL-Core  |--->| UEFI ABL  |--->| Kernel |
  |  (ROM)  |    | (Security |    | (DDR init |    | (Android  |    |        |
  |         |    |  focused) |    |  HW setup)|    |  Boot     |    |        |
  |         |    |           |    |           |    |  Loader)  |    |        |
  +---------+    +-----------+    +-----------+    +-----------+    +--------+

  vs. OLDER CHIPSETS:

  +---------+    +-------+    +-------+    +-------+    +----+    +--------+
  |   PBL   |--->| SBL1  |--->| SBL2  |--->| SBL3  |--->| LK |--->| Kernel |
  |  (ROM)  |    |       |    |       |    |       |    |    |    |        |
  +---------+    +-------+    +-------+    +-------+    +----+    +--------+

=========================================================================

Document 1 of 3 | Qualcomm Boot Flow Series | Interview Preparation
