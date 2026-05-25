Part 3 - Qualcomm Device Boot Flow
A Comprehensive Technical Reference for ARM64 / Qualcomm SoC Platforms
Covers: PBL | XBL | TrustZone | AOP | ABL | Kernel Boot | SMP | EDL | Crash Dump | Security Fuses | IPC
|  |


# Section 1: Qualcomm Boot Flow Architecture Overview
The Qualcomm boot flow is one of the most sophisticated multi-stage boot architectures in the embedded world. It implements a complete chain of trust from immutable ROM code through to the Android/Linux userspace, with each stage cryptographically verifying the next.
## 1.1 Complete Boot Flow Architecture
Power On / Reset
      |
      v
+---------------------+
| Stage 0: PMIC       |  <- Power rail sequencing
| Power-On Reset      |     Crystal oscillator
+---------------------+
      |
      v
+---------------------+
| Stage 1: PBL        |  <- ROM, immutable, on-chip
| Primary Boot Loader |     Root of Trust (RoT)
+---------------------+
      | loads + verifies
      v
+---------------------+
| Stage 2: XBL        |  <- UEFI-based, in flash
| eXtensible Boot     |     Replaced legacy SBL
| Loader              |
|  +-- XBL Core       |
|  +-- XBL Loader     |
|  +-- XBL SEC        |
+---------------------+
      | loads + verifies
      v
+---------------------+
| Stage 3: TrustZone  |  <- EL3 Secure Monitor
| (QSEE)              |     Hardware key storage
+---------------------+
      |
      v
+---------------------+
| Stage 4: AOP        |  <- Always-On Processor
| Always-On Processor |     Power management
+---------------------+
      |
      v
+---------------------+
| Stage 5: ABL        |  <- Android Boot Loader
| ABOOT / UEFI ABL    |     LK or UEFI ABL
+---------------------+
      | loads + verifies
      v
+---------------------+
| Stage 6: Kernel     |  <- Linux Kernel
| head.S -> start_    |     DTB + Ramdisk
|           kernel()  |
+---------------------+
      |
      v
+---------------------+
| Stage 7: SMP +      |  <- Secondary CPUs
| Userspace           |     Android/Linux Init
+---------------------+
## 1.2 Stage Summary Table
| Stage | Name | Location | Exception Level | Key Responsibility |
| Stage 0 | Power-On Reset | PMIC / HW | N/A | Power rails, crystal osc, reset vector |
| Stage 1 | PBL | On-chip ROM | EL3 (Secure) | Root of Trust, XBL load+verify |
| Stage 2 | XBL | Flash (xbl_a/b) | EL3 (Secure) | DDR init, clocks, load all FW images |
| Stage 3 | TrustZone/QSEE | Flash (tz_a/b) | EL3 / S-EL1 | Secure monitor, key storage, DRM |
| Stage 4 | AOP | Flash (aop_a/b) | Cortex-M3 (AOP) | Always-on power/clock management |
| Stage 5 | ABL | Flash (abl_a/b) | EL1 (Normal) | Load kernel, AVB, A/B slot select |
| Stage 6 | Linux Kernel | Flash (boot_a/b) | EL1 (Normal) | head.S, MMU enable, start_kernel() |
| Stage 7 | SMP + Userspace | RAM | EL0-EL1 | Secondary CPUs, Android framework |

# Section 2: Stage 0 — Power-On Reset
## 2.1 Why It Exists
When power is applied or reset is asserted, the CPU has no valid state. All registers are undefined, memory is uninitialized, and there is no valid code to execute. The SoC must deterministically start execution from a known, trusted location. The Power-On Reset stage establishes the minimal electrical and clock conditions required for the first bootloader (PBL) to execute.
## 2.2 What Happens
- PMIC (Power Management IC) sequences power rails in a strict hardware-defined order
- Crystal oscillator stabilizes and provides the reference clock to the SoC
- PLLs are bypassed initially — CPU runs at the slow, safe XO frequency (~19.2 MHz)
- Reset vector is hardwired to the PBL ROM base address in silicon
- All CPUs except CPU0 are held in WFE (Wait For Event) spin loop
## 2.3 How It Works — Power Rail Sequence
PMIC (PM8998 / PM8150 / PM8350 etc.)
    |
    +-- VDD_MX  (memory subsystem voltage)  --> first
    +-- VDD_CX  (core logic voltage)         --> second
    +-- VDD_APC (application CPU voltage)    --> third
    +-- VDD_GPU, VDD_MSS ...                 --> subsystem rails
    |
    +-- PON (Power-On) sequence complete
        --> deassert RESET_N to SoC

SoC Reset Vector (hardwired in silicon):
    CPU0 PC = 0xFFFF0000  (or SoC-specific PBL ROM base)
    CPU1..N = held in WFE spin loop
    All EL3 (Secure World) execution
## 2.4 PMIC PON Reason Register Defines
After each boot, the PMIC PON (Power-On) reason register records WHY the system booted. This is read by the bootloader (LK/UEFI ABL) to determine the boot path and log the restart cause.
/* Qualcomm PMIC PON reasons - pm8998 / pm8150 / pm8350 */
#define PON_REASON_HARD_RESET     BIT(0)   /* Hard reset by software */
#define PON_REASON_SMPL           BIT(1)   /* Sudden Momentary Power Loss */
#define PON_REASON_RTC            BIT(2)   /* RTC alarm triggered boot */
#define PON_REASON_DC_CHG         BIT(3)   /* DC charger inserted */
#define PON_REASON_USB_CHG        BIT(4)   /* USB charger inserted */
#define PON_REASON_PON1           BIT(5)   /* Power-On pin 1 */
#define PON_REASON_WATCHDOG       BIT(6)   /* WDT bite triggered reset */
#define PON_REASON_KPDPWR         BIT(7)   /* Keypad power button */

/* POFF (Power-Off) reasons */
#define POFF_REASON_SOFT          BIT(0)   /* Software-initiated */
#define POFF_REASON_PS_HOLD       BIT(1)   /* PS_HOLD deasserted */
#define POFF_REASON_PMIC_WDT      BIT(2)   /* PMIC internal watchdog */
#define POFF_REASON_GP1           BIT(3)   /* General purpose */

/* Reading in LK/UEFI ABL */
uint32_t pon_reason = pm8994_get_pon_reason();
if (pon_reason & PON_REASON_WATCHDOG) {
    smem_write_boot_reason(BOOT_REASON_WATCHDOG);
}
# Section 3: Stage 1 — PBL (Primary Boot Loader)
## 3.1 Why It Exists — Root of Trust
The PBL is the most critical component of the entire boot chain. It is burned into on-chip ROM at manufacturing time and cannot be modified after fabrication. This immutability makes it the Root of Trust (RoT) — the first link in the cryptographic chain of trust that secures the entire system.
Since PBL is in ROM, there is no way for malicious software to replace or corrupt it. Any image that PBL verifies and executes inherits this trust. If PBL is compromised (e.g., hardware attack), the entire security model fails — which is why Qualcomm uses hardware crypto engines rather than software for PBL verification.
## 3.2 What PBL Does — 5 Steps
- Step 1: Minimal hardware initialization — configures IMEM (on-chip SRAM), minimal clocks
- Step 2: Determines boot device by reading BOOT_CONFIG fuses from QFPROM
- Step 3: Loads XBL from the selected boot device into IMEM
- Step 4: Verifies XBL signature using QFPROM-stored OEM public key hash (hardware crypto engine)
- Step 5: Transfers control to XBL, OR enters EDL mode if DLOAD cookie is set
## 3.3 Boot Device Selection — BOOT_CONFIG Fuses
PBL reads the BOOT_CONFIG fuse field from QFPROM to determine which storage device contains XBL:
| BOOT_CONFIG[3:0] | Boot Device | Notes |
| 0x0 | eMMC (SDC1) | Most common for phones/tablets |
| 0x1 | UFS | High-end devices, faster sequential I/O |
| 0x2 | SD Card (SDC2) | Development/prototype boards |
| 0x3 | USB (EDL mode) | Emergency download mode via USB |
| 0x4 | NAND Flash | Older/IoT platforms |
| 0xF | JTAG Debug | Engineering builds only — never in production |

## 3.4 Memory Layout During PBL
IMEM (Internal SRAM - ~256KB to 1MB depending on SoC):

+------------------+ 0x14680000
| PBL Code         | <- ROM code mapped here (read-only)
| (Read-Only ROM)  |
+------------------+ 0x14690000
| PBL Stack        | <- PBL uses this for its stack
| (~4KB)           |
+------------------+ 0x146A0000
| XBL Load Buffer  | <- XBL loaded here by PBL from flash
| (from eMMC/UFS)  |
+------------------+ 0x146B0000
| PBL Shared Data  | <- Boot cookies, DLOAD magic,
| (IMEM Cookies)   |    restart reason, minidump table ptr
+------------------+ 0x146BF000
| Minidump Table   | <- MSM minidump region table
| (MD_TABLE)       |    read by XBL for crash collection
+------------------+ 0x146C0000
## 3.5 Signature Verification — QFPROM-Based Code
PBL uses a dedicated hardware crypto engine (CE - Crypto Engine) to verify XBL's digital signature. The OEM public key hash is stored in one-time-programmable QFPROM fuses, making it immutable after manufacturing.
/* PBL signature verification sequence (conceptual - actual is HW-accelerated) */

/* Step 1: Read SHA256 hash of OEM public key from QFPROM fuses */
uint8_t oem_key_hash[32];
qfprom_read(QFPROM_OEM_PK_HASH_ROW0, &oem_key_hash[0]);
qfprom_read(QFPROM_OEM_PK_HASH_ROW1, &oem_key_hash[16]);

/* Step 2: Parse MBN header from loaded XBL image */
struct mbn_header *hdr = (struct mbn_header *)IMEM_XBL_BASE;

/* Step 3: Hash the public key in XBL's certificate chain */
uint8_t computed_key_hash[32];
hw_sha256(hdr->cert_chain_ptr, hdr->cert_chain_sz, computed_key_hash);

/* Step 4: Compare fuse hash vs computed hash */
if (memcmp(computed_key_hash, oem_key_hash, 32) != 0) {
    /* Key mismatch - image not from trusted OEM */
    pbl_error_handler(PBL_ERR_AUTH_FAIL);
    /* Does not return - system halts or enters EDL */
}

/* Step 5: RSA verify image signature using the verified public key */
/* Uses hardware CE (Crypto Engine) - not software RSA */
ret = hw_rsa_verify(
    hdr->sig_ptr,          /* 256 bytes RSA-2048 signature */
    hdr->sig_sz,           /* = 256 */
    xbl_image,             /* image data buffer */
    hdr->code_size,        /* image size excluding cert/sig */
    hdr->oem_public_key    /* RSA-2048 modulus from cert chain */
);

/* Step 6: Check anti-rollback version against QFPROM counter */
if (hdr->sw_version < qfprom_read_rollback_counter(IMAGE_XBL)) {
    pbl_error_handler(PBL_ERR_ROLLBACK);
}
## 3.6 MBN Header Format Struct
Every Qualcomm firmware image uses the MBN (Multi-Boot iNterface) header format. This header is prepended to each signed image and contains metadata, load addresses, and pointers to the cryptographic material.
/* MBN (Multi-Boot iNterface) header - used for XBL, TZ, ABL, modem, etc. */
struct mbn_header {
    uint32_t image_id;          /* Unique image identifier */
    uint32_t header_vsn_num;    /* Header version number */
    uint32_t image_src;         /* Byte offset of image in flash */
    uint8_t  *image_dest_ptr;   /* Physical load address in RAM */
    uint32_t image_size;        /* Total image size (code+sig+cert) */
    uint32_t code_size;         /* Actual code/data size only */
    uint8_t  *sig_ptr;          /* Pointer to RSA signature */
    uint32_t sig_sz;            /* Signature size = 256 (RSA-2048) */
    uint8_t  *cert_chain_ptr;   /* Pointer to certificate chain */
    uint32_t cert_chain_sz;     /* Certificate chain size */
    /* Extended fields in newer MBN versions: */
    uint32_t sw_version;        /* Anti-rollback version */
    uint32_t app_id;            /* TZ app ID (for TAs) */
    uint32_t metadata_size;     /* ELF metadata size */
    uint8_t  metadata[0];       /* Variable-length metadata */
};
## 3.7 DLOAD Mode Detection Code
Before loading XBL, PBL checks for a special "DLOAD magic cookie" in IMEM. If set, this indicates that either the previous boot failed, or software intentionally requested Emergency Download (EDL) mode for flashing.
/* PBL DLOAD mode detection - checks IMEM cookie before loading XBL */

#define SHARED_IMEM_BASE         0x146BF000
#define DLOAD_MAGIC_COOKIE_ADDR  (SHARED_IMEM_BASE + 0x10)
#define DLOAD_MAGIC_VALUE        0x10      /* Magic value = EDL mode */
#define CRASH_DUMP_COOKIE_ADDR   (SHARED_IMEM_BASE + 0x14)
#define CRASH_DUMP_MAGIC         0xC0DEDEAD

void pbl_check_dload_mode(void)
{
    uint32_t magic = *(volatile uint32_t *)DLOAD_MAGIC_COOKIE_ADDR;

    if (magic == DLOAD_MAGIC_VALUE) {
        /* EDL mode explicitly requested */
        /* Initialize USB and wait for Firehose programmer */
        pbl_enter_dload();
        /* Does not return */
    }

    /* Check for crash dump collection mode */
    uint32_t crash_magic = *(volatile uint32_t *)CRASH_DUMP_COOKIE_ADDR;
    if (crash_magic == CRASH_DUMP_MAGIC) {
        /* Previous boot crashed - collect ramdump before normal boot */
        pbl_enter_crash_dump_mode();
    }
}
# Section 4: Stage 2 — XBL (eXtensible Boot Loader)
XBL is Qualcomm's UEFI-based bootloader that replaced the older SBL (Secondary Boot Loader) starting from MSM8996/SDM845 era. It is the most complex and feature-rich stage of the boot process, responsible for initializing the majority of the hardware and loading all firmware images.
## 4.1 XBL Sub-stages
+---------------------------------------------------+
|               XBL (eXtensible Boot Loader)        |
|                                                   |
|  +---------------+  +---------------+  +-------+ |
|  | XBL Core      |  | XBL Loader    |  | XBL   | |
|  | (XBL_CORE)    |  | (XBL_LOADER)  |  | SEC   | |
|  |               |  | [formerly     |  |       | |
|  | UEFI DXE      |  |  SBL1]        |  | EL3   | |
|  | drivers       |  |               |  | only  | |
|  | HW abstraction|  | DDR init      |  |       | |
|  | Protocol      |  | Clock setup   |  | Sets  | |
|  | publishing    |  | Load all FW   |  | up TZ | |
|  | UEFI PI       |  | images        |  | XPPU/ | |
|  | framework     |  | Verify images |  | XMPU  | |
|  +---------------+  +---------------+  +-------+ |
+---------------------------------------------------+
## 4.2 DDR Initialization
### 4.2.1 Why DDR Training Is Required
PBL only uses IMEM (on-chip SRAM, ~1MB). To load the full kernel and firmware images (often hundreds of MB), DDR DRAM must be initialized and trained. DDR training compensates for signal timing variations caused by Process, Voltage, and Temperature (PVT) variations between individual chips and boards.
/* XBL DDR initialization sequence */
void ddr_init(void)
{
    /* Step 1: Configure DDR PHY (Physical Layer Interface) */
    /* Programs DQ/DQS/DM/CA/CLK pad settings */
    ddr_phy_init();

    /* Step 2: Run full DDR training algorithms */
    /* -- Write Leveling: compensate for CLK-to-DQS skew */
    /* -- Read DQS Centering: optimize read timing window */
    /* -- Write DQ Centering: optimize write timing window */
    /* -- CA Training (LPDDR4): command/address bus timing */
    /* -- Vref Training: optimize internal Vref voltage level */
    ddr_training_run();

    /* Step 3: Validate trained parameters */
    ddr_post_training_validation();

    /* Step 4: Store trained parameters for fast restore */
    /* Saves ~200-500ms on subsequent boots */
    ddr_save_training_data();

    /* Step 5: Enable DDR self-refresh for power management */
    ddr_enable_self_refresh();
    ddr_enable_power_collapse_support();
}

/* Training data persistence:
 * First boot:  Full DDR training (~200-500ms)
 *              Save to: /dev/block/bootdevice/ddrcs (partition)
 *              Also stored in SMEM for kernel use
 * Subsequent:  Load saved params -> Quick restore (~10ms)
 *              Run quick sanity check (2-3 read/write tests)
 *              Fall back to full training if sanity fails
 */
## 4.3 Clock and Voltage Setup
/* XBL clock tree initialization */
void clock_init(void)
{
    /* 1. Switch APSS CPU from XO (19.2 MHz) to PLL */
    /* Start with conservative frequency for stability */
    configure_apss_pll(SILVER_PLL_FREQ_600MHZ);

    /* 2. Configure bus clocks */
    configure_bimc_clock(200MHZ);    /* DDR bus (BIMC) */
    configure_snoc_clock(200MHZ);    /* System Network on Chip */
    configure_cnoc_clock(100MHZ);    /* Config Network on Chip */
    configure_ipa_clock(60MHZ);      /* IPA (Internet Packet Accelerator) */

    /* 3. Configure peripheral clocks */
    configure_uart_clock(19200000);  /* Debug UART */
    configure_spi_clock(25000000);   /* SPI for PMIC */

    /* 4. Initialize RPMh/AOP for runtime clock management */
    /* All future clock changes go through AOP via RPMh */
    rpm_init();
}

/* Voltage setup via PMIC SPMI interface */
void regulator_init(void)
{
    /* Set VDD_APC (CPU voltage) for boot frequency */
    pmic_spmi_write(VDD_APC_ADDR, VOLTAGE_800MV);

    /* Set VDD_MX (memory subsystem) for DDR */
    pmic_spmi_write(VDD_MX_ADDR, VOLTAGE_752MV);

    /* Set VDD_CX (core logic) */
    pmic_spmi_write(VDD_CX_ADDR, VOLTAGE_752MV);
}
## 4.4 Loading Firmware Images
XBL loads multiple firmware images from flash storage. Each image is verified using a certificate chain and anti-rollback check before execution. The order is critical — dependencies must be satisfied (e.g., AOP must start before voltage/clock changes are possible).
### 4.4.1 Complete Table of All Images Loaded by XBL
| Image / Partition | Flash Partition | Purpose | Load Order |
| xbl_sec | xbl_sec_a/b | TrustZone / EL3 secure monitor configuration | 1st |
| aop | aop_a/b | Always-On Processor (Cortex-M) firmware | 2nd |
| tz | tz_a/b | TrustZone OS (QSEE) - EL3 code | 3rd |
| hyp | hyp_a/b | Hypervisor (QHEE - Qualcomm Hypervisor) | 4th |
| devcfg | devcfg_a/b | Device configuration - XPPU/XMPU setup tables | 5th |
| keymaster | keymaster_a/b | Android Keymaster Trusted Application | 6th |
| cmnlib / cmnlib64 | cmnlib_a/b | Common library for TrustZone applications | 7th |
| rpm / aop_proc | rpm_a/b | RPM (Resource Power Manager) firmware | 8th |
| modem (mpss) | modem_a/b | Modem subsystem (MPSS) cellular firmware | 9th |
| adsp | dsp_a/b | Audio DSP (Hexagon) firmware | 10th |
| cdsp | dsp_a/b | Compute DSP (AI/ML acceleration) firmware | 11th |
| slpi | dsp_a/b | Sensor Low Power Island firmware | 12th |
| wlan / wpss | bluetooth_a/b | WLAN/Bluetooth (Atheros/QCA) firmware | 13th |
| abl | abl_a/b | Android Boot Loader (UEFI ABL) - loaded last | Last |

### 4.4.2 Loading Sequence Code
/* XBL image loading sequence - dependency ordered */
void xbl_load_images(void)
{
    int ret;

    /* 1. Load and start AOP first */
    /* AOP manages power/clocks - must be running before */
    /* any dynamic voltage/frequency scaling operations   */
    ret = load_and_auth_image("aop", AOP_LOAD_ADDR);
    ASSERT(ret == 0);
    boot_aop();    /* Release AOP from reset */

    /* 2. Load TZ/QSEE */
    /* TZ must be up before any secure operations can proceed */
    ret = load_and_auth_image("tz", TZ_LOAD_ADDR);
    ASSERT(ret == 0);

    /* 3. Load hypervisor */
    ret = load_and_auth_image("hyp", HYP_LOAD_ADDR);
    ASSERT(ret == 0);

    /* 4. Load devcfg - configures XPPU/XMPU tables */
    ret = load_and_auth_image("devcfg", DEVCFG_LOAD_ADDR);
    ASSERT(ret == 0);

    /* 5. Load RPM/AOP processor firmware */
    ret = load_and_auth_image("rpm", RPM_LOAD_ADDR);
    ASSERT(ret == 0);
    boot_rpm();    /* Release RPM subsystem from reset */

    /* 6. Load modem and DSP subsystems */
    /* These run in parallel after boot */
    ret = load_and_auth_image("modem", MPSS_LOAD_ADDR);
    ASSERT(ret == 0);
    ret = load_and_auth_image("adsp",  ADSP_LOAD_ADDR);
    ASSERT(ret == 0);
    ret = load_and_auth_image("cdsp",  CDSP_LOAD_ADDR);
    ASSERT(ret == 0);
    ret = load_and_auth_image("slpi",  SLPI_LOAD_ADDR);
    ASSERT(ret == 0);

    /* 7. Load Android Boot Loader (ABL) last */
    ret = load_and_auth_image("abl", ABL_LOAD_ADDR);
    ASSERT(ret == 0);

    /* Hand off to TZ which then drops to ABL */
    xbl_jump_to_tz_and_abl();
}
## 4.5 Secure Boot Verification in XBL
Each image loaded by XBL undergoes a multi-step cryptographic verification. This is more comprehensive than PBL's verification because it validates the full certificate chain (not just the key hash).
int load_and_auth_image(const char *partition, uint64_t load_addr)
{
    struct mbn_header hdr;
    uint8_t *image_buf;
    int ret;

    /* Step 1: Read MBN header from flash partition */
    ret = flash_read(partition, 0, &hdr, sizeof(hdr));
    if (ret) return -EIO;

    /* Step 2: Read full image into DDR */
    image_buf = (uint8_t *)load_addr;
    ret = flash_read(partition, sizeof(hdr),
                     image_buf, hdr.image_size);
    if (ret) return -EIO;

    /* Step 3: Verify certificate chain (3 levels) */
    /* Root CA -> OEM CA -> Attestation -> Signing cert */
    ret = secboot_verify_cert_chain(
              hdr.cert_chain_ptr,
              hdr.cert_chain_sz,
              &cert_chain_info);
    if (ret) return -EACCES;

    /* Step 4: Verify OEM cert against QFPROM hash */
    ret = secboot_verify_oem_cert(&cert_chain_info);
    if (ret) return -EACCES;

    /* Step 5: RSA verify image data signature */
    ret = secboot_verify_image(
              image_buf, hdr.code_size,
              hdr.sig_ptr, hdr.sig_sz,
              &cert_chain_info);
    if (ret) return -EACCES;

    /* Step 6: Anti-rollback version check */
    ret = secboot_check_rollback(
              hdr.sw_version,
              partition_to_rollback_fuse(partition));
    if (ret) return -EPERM;

    return 0;
}

/* Certificate chain structure:
 *
 * Qualcomm Root CA (burned into PBL ROM)
 *     +-- OEM Root Certificate (OEM generates)
 *         Hash stored in QFPROM OEM_PK_HASH fuses
 *             +-- OEM Attestation Certificate
 *                     +-- Image Signing Certificate
 *                             +-- Image Signature
 *                                 (RSA-2048 or RSA-4096)
 */
## 4.6 XPPU / XMPU Configuration — Memory Protection
XPPU (eXtensible Peripheral Protection Unit) and XMPU (eXtensible Memory Protection Unit) are Qualcomm-specific hardware units that enforce memory isolation between subsystems. They are programmed by XBL/devcfg and remain active throughout the device's lifetime.
/* Memory protection configuration - called during XBL devcfg loading */
void configure_memory_protection(void)
{
    /* Protect TZ memory region - ONLY EL3 (TrustZone) can access */
    xmpu_configure_region(
        TZ_BASE,   TZ_SIZE,
        XMPU_MASTER_TZ_ONLY,
        XMPU_PERM_RWX);

    /* Protect modem memory - ONLY MPSS (modem processor) can access */
    xmpu_configure_region(
        MPSS_BASE, MPSS_SIZE,
        XMPU_MASTER_MPSS_ONLY,
        XMPU_PERM_RWX);

    /* Protect ADSP memory - ONLY audio DSP can access */
    xmpu_configure_region(
        ADSP_BASE, ADSP_SIZE,
        XMPU_MASTER_ADSP_ONLY,
        XMPU_PERM_RWX);

    /* Protect CDSP memory - compute DSP only */
    xmpu_configure_region(
        CDSP_BASE, CDSP_SIZE,
        XMPU_MASTER_CDSP_ONLY,
        XMPU_PERM_RWX);

    /* HLOS (Linux) gets remaining DDR */
    /* Linux CANNOT access TZ/modem/DSP memory regions */
    xmpu_configure_region(
        HLOS_BASE, HLOS_SIZE,
        XMPU_MASTER_APPS,
        XMPU_PERM_RWX);

    /* Lock configuration - prevent runtime modifications */
    xmpu_lock_all_regions();
    xppu_lock_all_regions();
}
# Section 5: Stage 3 — TrustZone (QSEE) Initialization
## 5.1 Why TrustZone Exists
TrustZone provides a hardware-enforced separation between the "secure world" (EL3/S-EL1) and the "normal world" (Linux/Android). This separation is enforced by ARM's TrustZone technology in hardware — even if Linux (the normal world) is fully compromised, it cannot access secure world memory or resources.
TrustZone (running QSEE — Qualcomm Secure Execution Environment) handles the most sensitive operations on the device:
- Cryptographic key storage and operations (device unique keys, attestation keys)
- Secure storage via RPMB (Replay Protected Memory Block)
- DRM content protection (Widevine L1, PlayReady)
- Biometric data processing (fingerprint templates, face data)
- Secure watchdog — can reset the device even if Linux is frozen
- Android Keymaster and Gatekeeper implementations
## 5.2 Full TZ Initialization Code
/* TZ/QSEE initialization - runs at EL3 (secure world) */
void tz_init(void)
{
    /* 1. Configure SCR_EL3 (Secure Configuration Register) */
    /* NS bit = 0: we are in secure world */
    /* RW bit = 1: EL1/EL0 run in AArch64 */
    write_scr_el3(SCR_RW | SCR_SMD | SCR_HCE);

    /* 2. Set up VBAR_EL3 - exception vector table for EL3 */
    write_vbar_el3((uintptr_t)&tz_vector_table);

    /* 3. Configure GIC (Generic Interrupt Controller) */
    /* Route watchdog bark FIQ to EL3 - ensures TZ handles WDT */
    gic_configure_fiq_routing(WDT_BARK_IRQ, TARGET_EL3);

    /* 4. Initialize QSEE (Qualcomm Secure Execution Environment) */
    /* Sets up secure heap, IPC channels, TA loading machinery */
    qsee_init();

    /* 5. Load built-in Trusted Applications (TAs) */
    qsee_load_ta("keymaster");    /* Android Keymaster 4 */
    qsee_load_ta("gatekeeper");   /* Android Gatekeeper */
    qsee_load_ta("widevine");     /* Google Widevine DRM */
    qsee_load_ta("hdcp");         /* HDCP 2.x for display */
    qsee_load_ta("securedisplay");/* Trusted UI */

    /* 6. Configure secure watchdog */
    /* TZ watchdog is independent of Linux WDT */
    tz_wdog_configure(TZ_WDOG_PET_TIMEOUT_MS);

    /* 7. Drop to normal world */
    /* Hand off to hypervisor (EL2) which then starts ABL (EL1) */
    tz_jump_to_normal_world(HYP_ENTRY_ADDR);
    /* Does not return */
}
## 5.3 SCM Communication — How Linux Talks to TrustZone
Linux communicates with TrustZone via the SCM (Secure Channel Manager) driver. Under the hood, this uses the ARM SMC (Secure Monitor Call) instruction which triggers an exception to EL3 (TrustZone).
/* arch/arm64/kernel/smccc-call.S */
/* The SMC #0 instruction causes an exception to EL3 */
/* TrustZone exception handler processes the request */
/* and returns results in x0-x3 */

/* Example: Pet TZ watchdog via SCM (Secure Channel Manager) */
#include <linux/arm-smccc.h>

static void msm_pet_tz_watchdog(void)
{
    struct arm_smccc_res res;

    /* Call TrustZone watchdog pet service */
    arm_smccc_smc(
        SCM_SIP_FNID(SCM_SVC_BOOT, TZ_WDOG_PET_CMD),
        0, 0, 0,   /* args 1-3 */
        0, 0, 0,   /* args 4-6 */
        0,         /* arg 7 */
        &res       /* result: res.a0 = return code */
    );

    if (res.a0 != 0)
        pr_err("TZ WDT pet failed: %lx\n", res.a0);
}

/* Example: Get unique device key from TZ keymaster */
static int qcom_get_device_key(uint8_t *key_buf, size_t key_len)
{
    struct scm_desc desc = {0};
    desc.args[0] = (uint64_t)virt_to_phys(key_buf);
    desc.args[1] = key_len;
    desc.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);
    return scm_call2(SCM_SIP_FNID(SCM_SVC_CRYPTO,
                     TZ_GET_DEVICE_KEY), &desc);
}
# Section 6: Stage 4 — AOP (Always-On Processor)
## 6.1 Why AOP Exists
The AOP (Always-On Processor) is a small microcontroller (typically ARM Cortex-M3 or M55) embedded in the AOSS (Always-On SubSystem). It handles power management tasks that must continue even when the main APSS (ARM cluster running Linux) is in deep sleep or powered down.
The key responsibilities of AOP are:
- Managing power states — decides which rails to enable/disable based on requests
- Clock management — adjusts frequencies in response to RPMh requests from all subsystems
- Wake-up handling — monitors events (RTC, USB insert, charger) to wake the system
- SPMI control — communicates with PMIC via SPMI bus for voltage/current changes
- Running the AOSS-level watchdog — if APSS stops petting, AOP resets the SoC
- DDR self-refresh management — ensures DDR data is preserved during APSS sleep
## 6.2 How AOP Works — RPMh Communication Code
All subsystems (APSS, MPSS, ADSP, CDSP) communicate power and clock requests to AOP through RPMh (Resource Power Manager hardware). RPMh provides a hardware message passing interface that is efficient and doesn't require AOP to poll.
/* Linux driver: AOP/RPMh communication */
/* drivers/interconnect/qcom/rpmh.c */

/* Send a voltage level request to AOP */
/* AOP receives this and programs PMIC via SPMI */
int rpmh_set_voltage(struct device *dev, unsigned int addr,
                     unsigned int level)
{
    struct tcs_cmd cmd = {
        .addr = addr,        /* Resource address (CMD DB) */
        .data = level,       /* Requested voltage level */
    };

    /* ACTIVE_ONLY: only applies when system is running */
    return rpmh_write(dev, RPMH_ACTIVE_ONLY_STATE, &cmd, 1);
    /* AOP processes this request and programs PMIC voltage */
}

/* Example: Request SVS (Sub-Voltage Standard) for VDD_CX */
rpmh_set_voltage(dev, CMD_DB_ADDR_VDD_CX,
                 RPMH_REGULATOR_LEVEL_SVS);

/* Example: Request TURBO for VDD_APC before boosting CPU freq */
rpmh_set_voltage(dev, CMD_DB_ADDR_VDD_APC,
                 RPMH_REGULATOR_LEVEL_TURBO);

/* Aggregated requests:
 * Multiple clients (APSS, MPSS, ADSP) may request different
 * levels for the same rail. AOP takes the MAX and programs
 * PMIC accordingly. This is called "vote aggregation".
 *
 * Example: APSS wants SVS, MPSS wants NOMINAL
 * AOP programs PMIC for NOMINAL (higher wins)
 */
# Section 7: Stage 5 — ABL (Android Boot Loader)
ABL is the user-facing bootloader responsible for loading the Linux kernel and handing off control. There are two implementations used by Qualcomm across different platform generations: LK (Little Kernel) for older platforms and UEFI ABL for newer ones.
## 7.1 LK (Little Kernel) — Older Platforms (MSM8996 and earlier)
LK is a small, purpose-built RTOS-like kernel that Qualcomm used as ABOOT on their older platforms. It is open-source and available in the CodeAurora Forum repositories.
### 7.1.1 aboot_init()
/* app/aboot/aboot.c - LK ABOOT initialization */
void aboot_init(const struct app_descriptor *app)
{
    unsigned reboot_mode;
    bool boot_into_fastboot = false;
    bool boot_into_recovery = false;

    /* 1. Check hardware key combinations */
    if (keys_get_state(KEY_VOLUMEUP) &&
        keys_get_state(KEY_VOLUMEDOWN)) {
        boot_into_recovery = true;
    }

    /* 2. Read reboot reason from IMEM cookie */
    reboot_mode = check_reboot_mode();
    if (reboot_mode == FASTBOOT_MODE)
        boot_into_fastboot = true;
    else if (reboot_mode == RECOVERY_MODE)
        boot_into_recovery = true;

    /* 3. Read PMIC PON reason for charger-only boot */
    if (pm8994_get_pon_reason() & PON_REASON_USB_CHG)
        boot_into_charger = true;

    /* 4. Route to appropriate boot path */
    if (boot_into_fastboot) {
        fastboot_init(target_get_scratch_address(),
                      target_get_max_flash_size());
    } else if (boot_into_recovery) {
        load_boot_image_from_partition("recovery");
        boot_linux_from_mmc();
    } else {
        /* Normal boot - A/B slot selection */
        if (boot_control_get_active_slot() == 0)
            load_boot_image_from_partition("boot_a");
        else
            load_boot_image_from_partition("boot_b");
        boot_linux_from_mmc();
    }
}
### 7.1.2 boot_linux_from_mmc()
/* app/aboot/aboot.c - Linux boot from eMMC/UFS */
void boot_linux_from_mmc(void)
{
    struct boot_img_hdr *hdr;
    void (*entry)(unsigned, unsigned, unsigned) = 0;

    /* Step 1: Read boot.img header (first page) */
    hdr = (struct boot_img_hdr *)boot_header_addr;
    mmc_read(BOOT_PARTITION, hdr, BOOT_IMG_HDR_SIZE);

    /* Validate magic */
    if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
        dprintf(CRITICAL, "ERROR: Invalid boot magic!\n");
        return;
    }

    /* Step 2: Load kernel into DDR */
    mmc_read(BOOT_PARTITION + hdr->page_size,
             (void *)hdr->kernel_addr,
             hdr->kernel_size);

    /* Step 3: Load ramdisk */
    mmc_read(BOOT_PARTITION + kernel_offset,
             (void *)hdr->ramdisk_addr,
             hdr->ramdisk_size);

    /* Step 4: Load DTB */
    mmc_read(BOOT_PARTITION + dtb_offset,
             (void *)dtb_addr,
             hdr->dt_size);

    /* Step 5: Android Verified Boot (AVB) verification */
    if (avb_verify_boot_image(hdr) != AVB_SLOT_VERIFY_RESULT_OK) {
        dprintf(CRITICAL, "AVB verification FAILED!\n");
        /* In locked state: halt. In unlocked: warn and continue */
        if (device.is_unlocked) {
            dprintf(CRITICAL, "WARNING: Continuing on unlocked device\n");
        } else {
            halt(); /* Production locked device - abort boot */
        }
    }

    /* Step 6: Apply DTBO (Device Tree Overlays) */
    apply_dtbo_overlays(dtb_addr);

    /* Step 7: Update cmdline and memory map in DTB */
    update_device_tree((void *)hdr->tags_addr,
                       final_cmdline,
                       (void *)hdr->ramdisk_addr,
                       hdr->ramdisk_size);

    /* Step 8: CRITICAL - Disable MMU and caches */
    /* ARM64 kernel entry requirement: MMU must be OFF */
    arch_disable_cache(UCACHE);   /* Unified cache off */
    arch_disable_mmu();           /* MMU off */

    /* Step 9: Jump to kernel */
    /* ARM64: x0=DTB physical addr, x1=0, x2=0, x3=0 */
    entry = (void *)hdr->kernel_addr;
    entry(0, 0, (unsigned)hdr->tags_addr);
    /* Does not return */
}
## 7.2 UEFI ABL — Newer Platforms (SDM845+)
Starting with SDM845, Qualcomm replaced LK with a UEFI-based Android Boot Loader (ABL). This runs as a UEFI application within the XBL UEFI environment and uses standard UEFI protocols for hardware access.
### 7.2.1 LinuxLoaderEntry — Full Code
/* QcomModulePkg/Application/LinuxLoader/LinuxLoader.c */
EFI_STATUS EFIAPI LinuxLoaderEntry(
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable)
{
    BootParamlistPtr BootParamList = {0};
    EFI_STATUS Status;
    UINTN MapKey;

    /* 1. Initialize UEFI protocols and services */
    Status = gBS->HandleProtocol(
        gImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&BootParamList.ImageInfo);

    /* 2. Determine boot mode */
    /* Normal | Recovery | FastBoot | ChargerBoot */
    Status = GetBootMode(&BootParamList);

    /* 3. A/B slot selection */
    Status = GetSlotSuffix(&BootParamList.SlotSuffix);
    /* Reads BCB (Boot Control Block) from misc partition */

    /* 4. Load boot.img from correct slot */
    if (BootParamList.BootIntoRecovery) {
        Status = LoadImageFromPartition(
            &BootParamList, L"recovery");
    } else {
        Status = LoadImageFromPartition(
            &BootParamList, L"boot");
    }

    /* 5. Also load vendor_boot.img (Android 11+) */
    Status = LoadVendorBootImage(&BootParamList);

    /* 6. Android Verified Boot 2.0 (AVB) */
    Status = avb_slot_verify(
        AvbOps,
        requested_partitions,
        ab_suffix,
        AVB_SLOT_VERIFY_FLAGS_NONE,
        AVB_HASHTREE_ERROR_MODE_RESTART,
        &SlotData);

    /* 7. Build final kernel command line */
    Status = UpdateCmdLine(
        &BootParamList,
        FinalCmdLine,
        ROUNDUP(sizeof(FinalCmdLine), EFI_PAGE_SIZE));

    /* 8. Update device tree */
    /* Apply DTBO overlays, add chosen properties */
    Status = UpdateDeviceTree(
        BootParamList.DeviceTreeLoadAddr,
        FinalCmdLine,
        BootParamList.RamdiskLoadAddr,
        BootParamList.RamdiskSize);

    /* 9. Exit UEFI Boot Services - point of no return */
    Status = gBS->GetMemoryMap(&MemMapSize, MemMap,
                               &MapKey, &DescriptorSize,
                               &DescriptorVersion);
    Status = gBS->ExitBootServices(ImageHandle, MapKey);
    /* After this point, no UEFI services available */

    /* 10. Jump to kernel */
    /* x0 = DTB physical address (required by ARM64 boot protocol) */
    /* x1 = 0 (reserved) */
    /* x2 = 0 (reserved) */
    /* x3 = 0 (reserved) */
    typedef void (*LinuxKernel)(UINT32, UINT32, UINTN);
    LinuxKernel LinuxKernelEntry =
        (LinuxKernel)BootParamList.KernelLoadAddr;
    LinuxKernelEntry(0, 0,
        BootParamList.DeviceTreeLoadAddr);
    /* Does not return */

    return EFI_SUCCESS; /* Never reached */
}
## 7.3 boot.img Format — Android Image Structure
+--------------------------------+  <- offset 0x00
| boot_img_hdr (page_size bytes) |
|                                |
|  magic[8]:  "ANDROID!"         |
|  kernel_size, kernel_addr      |
|  ramdisk_size, ramdisk_addr    |
|  second_size, second_addr      |
|  tags_addr (DTB load addr)     |
|  page_size  = 4096             |
|  header_version = 2 or 3       |
|  os_version (Android version)  |
|  cmdline[512] / extra_cmdline  |
|  id[32] (SHA1 of contents)     |
+--------------------------------+  <- offset page_size
| Kernel Image                   |
| (Image.gz-dtb or plain Image)  |
| Padded to page_size boundary   |
+--------------------------------+  <- aligned to page_size
| Ramdisk Image (initrd.img)     |
| First-stage ramdisk (recovery  |
| or generic ramdisk for Android)|
| Padded to page_size boundary   |
+--------------------------------+  <- aligned to page_size
| Second Bootloader (optional)   |
| Usually unused on Qualcomm     |
+--------------------------------+  <- aligned to page_size
| Device Tree (DTB)              |
| Flattened device tree blob     |
| or DTBO overlays reference     |
+--------------------------------+
## 7.4 A/B Partition Scheme
Android 7.0+ introduced seamless OTA updates using an A/B (dual-copy) partition scheme. The device always has two copies of all bootable partitions. ABL reads the Boot Control Block (BCB) from the misc partition to determine which slot to boot.
/* Slot suffixes: "_a" or "_b" */
/* Partition pairs:           */
/*   boot_a  <-->  boot_b    */
/*   system_a <-> system_b   */
/*   vendor_a <-> vendor_b   */
/*   dtbo_a  <-->  dtbo_b    */

/* Boot Control Block - stored in misc partition */
struct bootloader_control {
    char      magic[4];          /* "BABC" (Boot AB Control) */
    uint32_t  version;           /* Structure version = 1 */
    uint8_t   nb_slot : 3;       /* Number of slots (= 2) */
    uint8_t   recovery_tries_remaining : 3;
    uint8_t   merge_status : 2;  /* Virtual A/B merge status */
    uint8_t   slot_suffix[4];    /* Current slot: "_a\0" or "_b\0" */
    uint8_t   reserved0[44];

    /* Per-slot metadata */
    struct slot_metadata {
        uint8_t priority : 4;         /* 0-15, higher = preferred */
        uint8_t tries_remaining : 3;  /* Boot attempts remaining */
        uint8_t successful_boot : 1;  /* 1 = booted successfully */
        uint8_t verity_corrupted : 1; /* 1 = dm-verity failed */
        uint8_t reserved : 7;
    } slot_info[2];

    uint8_t   reserved1[8];
    uint32_t  crc32_le;  /* CRC32 of the whole structure */
} __attribute__((packed));

/* ABL reads BCB and selects boot slot */
const char *select_boot_slot(void) {
    struct bootloader_control bc;
    read_misc_partition(&bc, sizeof(bc));
    /* Select highest priority slot with tries_remaining > 0 */
    if (bc.slot_info[0].priority > bc.slot_info[1].priority)
        return "_a";
    return "_b";
}
# Section 8: Stage 6 — Kernel Boot
## 8.1 Why ABL Disables MMU/Cache Before Jumping
The ARM64 Linux kernel has strict requirements for the CPU state at entry. These requirements exist because the kernel must set up its own memory management from scratch — it cannot tolerate any pre-existing MMU configuration from the bootloader.
/* ARM64 kernel entry requirements */
/* Documentation/arm64/booting.rst */
/*
 * Primary CPU entry state (enforced by kernel head.S):
 *
 * Registers:
 *   x0 = physical address of device tree blob (DTB)
 *   x1 = 0 (reserved - must be zero)
 *   x2 = 0 (reserved - must be zero)
 *   x3 = 0 (reserved - must be zero)
 *
 * CPU state requirements:
 *   - MMU must be OFF (SCTLR_EL1.M = 0)
 *   - Data cache must be OFF (SCTLR_EL1.C = 0)
 *   - Instruction cache may be ON or OFF
 *   - All interrupts must be masked (DAIF = 0xF)
 *   - CPU must be in EL2 (preferred) or EL1
 *   - AArch64 execution state
 *
 * Why MMU must be OFF:
 *   - Kernel will set up its own page tables
 *   - Any MMU mapping from bootloader would conflict
 *   - Kernel identity-maps itself during early init
 *
 * Why caches must be OFF:
 *   - Cache may contain stale bootloader data
 *   - Kernel needs coherent view of memory
 *   - Cache will be invalidated and re-enabled by kernel
 */
## 8.2 head.S Assembly Flow
The kernel's assembly entry point (arch/arm64/kernel/head.S) performs the minimal CPU and memory setup required to reach the first C function start_kernel(). This code runs with the MMU off and must be position-independent.
/* arch/arm64/kernel/head.S - kernel entry point (simplified) */

    /* ARM64 Image header - required format for bootloaders */
_head:
    b   primary_entry       /* Branch to actual entry code */
    .quad 0                 /* text_offset (= 0 for 4K aligned) */
    .quad _kernel_size      /* image_size: total kernel image size */
    .quad flags             /* flags: LE=0, page size, phys placement */
    .quad 0                 /* reserved */
    .quad 0                 /* reserved */
    .quad 0                 /* reserved */
    .ascii "ARM\x64"        /* magic: 0x644d5241 = "ARM64" */
    .long pe_header - _head /* PE/COFF offset (Windows compatibility) */

primary_entry:
    /* Save DTB address (x0) before any register clobbering */
    bl  preserve_boot_args

    /* Configure EL2 if we entered at EL2 */
    /* This sets up VHE (Virtualization Host Extensions) */
    bl  el2_setup

    /* Record which EL we booted at (for KASLR/EFI later) */
    bl  set_cpu_boot_mode_flag

    /* Set up initial page tables */
    /* Maps only the kernel image for now */
    bl  __create_page_tables

    /* Configure CPU: SCTLR, TCR, MAIR, TTBR registers */
    /* But do NOT enable MMU yet - done in __primary_switch */
    bl  __cpu_setup

    /* Enable MMU and branch to virtual address space */
    b   __primary_switch

__primary_switch:
    /* Atomically enable MMU + branch to virtual address */
    /* After this, we are running in virtual address space */
    adrp    x1, reserved_pg_dir
    adrp    x2, idmap_pg_dir
    bl      __enable_mmu    /* <- MMU ENABLED HERE */

    /* Clear BSS segment */
    bl  clear_bss_section

    /* Jump to C code - start_kernel() */
    b   start_kernel        /* <- ENTER C WORLD */
## 8.3 start_kernel() — C Entry Point
/* init/main.c - first C function called by kernel */
asmlinkage __visible void __init __no_sanitize_address start_kernel(void)
{
    char *command_line;

    /* Very early: set up stack protector canary */
    boot_init_stack_canary();

    /* Architecture-specific setup */
    /* Parses DTB, sets up memory map, CPU info */
    setup_arch(&command_line);

    /* Memory management initialization */
    mm_init();

    /* Scheduler initialization */
    sched_init();

    /* Interrupt controller (GIC v3) initialization */
    irq_init();

    /* Architecture timer (ARM Generic Timer) */
    time_init();

    /* Per-CPU areas setup */
    setup_per_cpu_areas();

    /* SoftIRQ and tasklet infrastructure */
    softirq_init();

    /* Early console (UART) - needed for dmesg */
    console_init();

    /* RCU (Read-Copy-Update) subsystem */
    rcu_init();

    /* Build CPU topology from DTB */
    arch_cpu_idle_init();

    /* Print kernel version and build info */
    pr_notice("%s", linux_banner);
    pr_notice("Command line: %s\n", boot_command_line);

    /* Start init process (PID 1) */
    /* This is the point of no return for early boot */
    rest_init();
}
## 8.4 rest_init() — Spawning Init and Idle
/* init/main.c - called from start_kernel() at the end */
static noinline void __ref rest_init(void)
{
    struct task_struct *tsk;
    int pid;

    /* Create kernel thread for kernel_init (PID 1) */
    /* This will eventually exec() /init or /sbin/init */
    pid = kernel_thread(kernel_init, NULL, CLONE_FS);
    rcu_read_lock();
    tsk = find_task_by_pid_ns(pid, &init_pid_ns);
    tsk->flags |= PF_NO_SETAFFINITY;
    rcu_read_unlock();

    /* Create kernel thread for kthreadd (PID 2) */
    /* kthreadd manages all future kernel threads */
    pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);

    /* Current thread (PID 0) becomes the idle thread */
    /* It runs cpu_startup_entry() = infinite WFI loop */
    init_idle_bootup_task(current);

    schedule_preempt_disabled();

    /* Start the CPU0 idle loop */
    cpu_startup_entry(CPUHP_ONLINE);
    /* Does not return */
}
# Section 9: Stage 7 — Secondary CPU Bring-Up (SMP)
Only CPU0 (the primary CPU) runs through the complete boot sequence. All other CPUs (CPU1 through CPUn) are held in a PSCI wait loop until the kernel is ready to bring them online. This section describes how secondary CPUs are brought up.
## 9.1 secondary_start_kernel() Code
/* arch/arm64/kernel/smp.c */
asmlinkage notrace void secondary_start_kernel(void)
{
    u64 mpidr = read_cpuid_mpidr() & MPIDR_HWID_BITMASK;
    unsigned int cpu;

    /* Find our CPU number from the MPIDR register */
    cpu = get_logical_index(mpidr);

    /* 1. Set up this CPU's page tables (same as primary) */
    cpu_setup();  /* SCTLR, TCR, MAIR setup */

    /* 2. Notify all subsystems that this CPU is starting */
    notify_cpu_starting(cpu);

    /* 3. Initialize this CPU's GIC CPU interface */
    gic_cpu_init();

    /* 4. Calibrate local timer (arch_timer) */
    calibrate_delay();

    /* 5. Initialize per-CPU scheduler data */
    smp_store_cpu_info(cpu);

    /* 6. Mark CPU as online - visible to rest of kernel */
    set_cpu_online(cpu, true);

    /* 7. Enable local interrupts */
    local_irq_enable();

    /* 8. Enter scheduler - this CPU is now fully functional */
    cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
    /* Does not return */
}
## 9.2 PSCI — Power State Coordination Interface
PSCI (Power State Coordination Interface) is the ARM standard for CPU power management in the secure world. CPU0 uses PSCI SMC calls to wake secondary CPUs from their initial WFI state.
/* arch/arm64/kernel/psci.c */
/* CPU0 wakes secondary CPUs via PSCI SMC calls to EL3/TZ */

int psci_cpu_on(unsigned long cpuid, unsigned long entry_point)
{
    struct arm_smccc_res res;

    /* PSCI_FN_CPU_ON = 0xC4000003 (SMCCC 1.1 standard) */
    arm_smccc_smc(
        PSCI_FN_CPU_ON,   /* Function ID */
        cpuid,            /* Target CPU MPIDR */
        entry_point,      /* Secondary CPU entry physical addr */
        0,                /* Context ID (unused) */
        0, 0, 0, 0,
        &res
    );

    return (int)res.a0;  /* 0 = success, negative = error */
}

/* PSCI function codes (ARM PSCI 1.0 spec) */
#define PSCI_FN_CPU_SUSPEND   0xC4000001  /* Suspend CPU */
#define PSCI_FN_CPU_OFF       0x84000002  /* Power off CPU */
#define PSCI_FN_CPU_ON        0xC4000003  /* Power on CPU */
#define PSCI_FN_AFFINITY_INFO 0xC4000004  /* Query CPU state */
#define PSCI_FN_MIGRATE       0xC4000005  /* Migrate TrustOS */
#define PSCI_FN_SYSTEM_OFF    0x84000008  /* Power off system */
#define PSCI_FN_SYSTEM_RESET  0x84000009  /* Reset system */

/* CPU bring-up flow in kernel (kernel/cpu.c) */
int cpu_up(unsigned int cpu)
{
    /* 1. Allocate CPU data structures */
    ret = _cpu_up(cpu, 0, CPUHP_ONLINE);
    /* 2. Calls arch_cpu_up() -> psci_cpu_on() */
    /* 3. Secondary CPU starts at secondary_startup() */
    /* 4. secondary_startup() -> secondary_start_kernel() */
    return ret;
}
# Section 10: Complete Timing Breakdown
The following table shows the approximate timing of each boot stage on a typical Qualcomm SDM845/SM8150 device. Times vary based on storage speed, DDR type, and whether DDR training data is cached.
| Time | Stage | Event | Details |
| t = 0 ms | Stage 0 | Power On | PMIC asserts power rails, crystal starts |
| t = 5 ms | Stage 0 | Reset deassert | PMIC completes sequencing, deasserts RESET_N |
| t = 10 ms | Stage 1 (PBL) | PBL starts | CPU0 begins executing ROM code at reset vector |
| t = 15 ms | Stage 1 (PBL) | Boot device select | Reads BOOT_CONFIG fuses from QFPROM |
| t = 30 ms | Stage 1 (PBL) | XBL loaded+verified | PBL loads XBL from eMMC/UFS into IMEM |
| t = 35 ms | Stage 2 (XBL) | XBL starts | UEFI PI framework initializes |
| t = 50 ms | Stage 2 (XBL) | DDR training starts | PHY init + training algorithms begin |
| t = 250 ms | Stage 2 (XBL) | DDR training done | FIRST BOOT: full training; subsequent: ~10ms |
| t = 260 ms | Stage 2 (XBL) | Clocks + voltage | PLL setup, BIMC/SNOC clocks configured |
| t = 280 ms | Stages 3+4 | TZ + AOP loaded | TrustZone and AOP firmware verified + started |
| t = 350 ms | Stage 2 (XBL) | All FW images loaded | Modem, ADSP, CDSP, SLPI, HYP loaded |
| t = 400 ms | Stage 2 (XBL) | XPPU/XMPU set | Memory protection units programmed and locked |
| t = 410 ms | Stage 5 (ABL) | ABL starts | UEFI ABL application enters LinuxLoaderEntry() |
| t = 430 ms | Stage 5 (ABL) | boot.img loaded | Kernel + ramdisk + DTB loaded from flash |
| t = 450 ms | Stage 5 (ABL) | AVB verification | Android Verified Boot 2.0 completes |
| t = 500 ms | Stage 6 (Kernel) | Kernel entry | head.S executes, page tables set up |
| t = 520 ms | Stage 6 (Kernel) | MMU enabled | Virtual address space active, start_kernel() reached |
| t = 600 ms | Stage 6 (Kernel) | start_kernel() | Scheduler, IRQ, timer, GIC initialized |
| t = 800 ms | Stage 7 (SMP) | Secondary CPUs up | PSCI CPU_ON for CPU1-7 |
| t = 1000 ms | Stage 6 (Kernel) | Device probing | GIC, UFS, PCIe, display drivers probe |
| t = 1500 ms | Userspace | init/systemd starts | PID 1 executes /init (Android init) |
| t = 2000 ms | Userspace | PIL subsys load | Modem/ADSP authenticated by TZ via PIL/PAS |
| t = 3000 ms | Userspace | Android FW starts | Zygote, system_server, SurfaceFlinger launch |
| t = 10000 ms | Userspace | Boot complete | Home screen visible, ACTION_BOOT_COMPLETED broadcast |

# Section 11: Partition Layout (eMMC/UFS)
Qualcomm Android devices use GPT (GUID Partition Table) on eMMC or UFS storage. All bootable partitions use the A/B scheme (suffix _a and _b). The following is a complete listing of partitions found on a typical Snapdragon 845/865/888 device.
## 11.1 Complete GPT Partition Table
| Partition Name | A/B? | Typical Size | Contents / Purpose |
| xbl_a / xbl_b | Yes | 3 MB | XBL (UEFI bootloader) |
| xbl_config_a/b | Yes | 64 KB | XBL hardware configuration |
| abl_a / abl_b | Yes | 1 MB | Android Boot Loader (UEFI ABL) |
| tz_a / tz_b | Yes | 4 MB | TrustZone (QSEE) firmware |
| hyp_a / hyp_b | Yes | 1 MB | Hypervisor (QHEE) firmware |
| aop_a / aop_b | Yes | 512 KB | Always-On Processor firmware |
| devcfg_a / _b | Yes | 64 KB | Device config (XPPU/XMPU tables) |
| rpm_a / rpm_b | Yes | 256 KB | RPM firmware (older SoCs) |
| keymaster_a / _b | Yes | 512 KB | Keymaster Trusted Application |
| cmnlib_a / _b | Yes | 512 KB | Common library for TZ apps (32-bit) |
| cmnlib64_a / _b | Yes | 512 KB | Common library for TZ apps (64-bit) |
| boot_a / boot_b | Yes | 100 MB | Linux kernel + ramdisk (boot.img) |
| vendor_boot_a/b | Yes | 100 MB | Vendor-specific ramdisk (Android 11+) |
| dtbo_a / dtbo_b | Yes | 8 MB | Device Tree Blob Overlay table |
| vbmeta_a / _b | Yes | 64 KB | AVB metadata / hash descriptors |
| system_a / _b | Yes | 3-6 GB | Android system partition (or super) |
| vendor_a / _b | Yes | 1 GB | Vendor HAL partition |
| modem_a / _b | Yes | 200 MB | Modem subsystem (MPSS) firmware |
| dsp_a / dsp_b | Yes | 32 MB | ADSP/CDSP/SLPI firmware images |
| bluetooth_a / _b | Yes | 1 MB | Bluetooth/WLAN firmware |
| persist | No | 32 MB | Persistent data (sensors cal, DRM certs) |
| misc | No | 4 MB | Boot Control Block (BCB) for A/B |
| metadata | No | 16 MB | f2fs metadata, dm-verity state |
| userdata | No | Remaining | User data (FDE/FBE encrypted) |
| frp | No | 512 KB | Factory Reset Protection data |
| logdump | No | 256 MB | Crash/ramdump storage partition |
| ddr | No | 4 MB | DDR training calibration data |
| cdt | No | 2 MB | Configuration Data Table (board ID) |
| sec | No | 64 KB | SEC partition (fuse blowing data) |
| splash | No | 8 MB | Boot splash screen image (BMP/PNG) |
| modemst1/2 | No | 2 MB each | Modem NV (Non-Volatile) storage |
| fsg | No | 2 MB | Modem golden copy (factory NV backup) |
| spunvm | No | 512 KB | Secure Processor Unit non-volatile memory |
| logfs | No | 4 MB | XBL/ABL log filesystem |
| limits | No | 1 MB | Thermal/power limits configuration |

# Section 12: Subsystem Firmware Loading
Qualcomm SoCs contain multiple independent processors running in parallel with the main APSS (ARM cluster). Each subsystem runs its own firmware and is isolated by hardware memory protection. The Linux kernel loads (or re-loads) subsystem firmware using the PIL (Peripheral Image Loader) framework.
## 12.1 SoC Die — Subsystem Overview
+------------------------------------------------------------------+
|                         SoC Die                                  |
|                                                                  |
|  +--------------+  +-------------+  +--------------+            |
|  | APSS         |  | MPSS        |  | ADSP         |            |
|  | ARM A55/A75  |  | Hexagon DSP |  | Hexagon DSP  |            |
|  | A78/X1 cores |  | (Cellular)  |  | (Audio)      |            |
|  | Runs Linux   |  | 5G/LTE stack|  | Audio/Voice  |            |
|  +--------------+  +-------------+  +--------------+            |
|        |                  |                  |                   |
|  +-----+------------------+------------------+------+           |
|  |              System NOC (Network on Chip)         |           |
|  +-----+------------------+------------------+------+           |
|        |                  |                  |                   |
|  +--------------+  +-------------+  +--------------+            |
|  | CDSP         |  | SLPI        |  | AOP          |            |
|  | Hexagon DSP  |  | Hexagon DSP |  | Cortex-M3    |            |
|  | AI/ML/CV     |  | Sensors     |  | Always-On    |            |
|  | Acceleration |  | Accel/Gyro  |  | Power Mgmt   |            |
|  +--------------+  +-------------+  +--------------+            |
|                                                                  |
|  +--------------+  +-------------+  +--------------+            |
|  | WPSS         |  | SPU          |  | NPU/HTP      |            |
|  | WLAN         |  | Security     |  | Neural Proc  |            |
|  | Bluetooth    |  | Processor    |  | Unit         |            |
|  | Firmware     |  | (Crypto)     |  | AI inferenc  |            |
|  +--------------+  +-------------+  +--------------+            |
|                                                                  |
|  +----------------------------------------------------------+   |
|  | Memory Subsystem: LPDDR4X/LPDDR5 via BIMC               |   |
|  +----------------------------------------------------------+   |
+------------------------------------------------------------------+
## 12.2 PIL (Peripheral Image Loader) Code
PIL (Peripheral Image Loader) is the Linux kernel framework for loading and authenticating subsystem firmware. It uses the PAS (Peripheral Authentication Service) SCM call to TrustZone to verify each firmware image.
/* drivers/remoteproc/qcom_q6v5_pas.c */
/* PAS = Peripheral Authentication Service (via TZ SCM) */

static int adsp_load(struct rproc *rproc, const struct firmware *fw)
{
    struct qcom_adsp *adsp = rproc->priv;
    int ret;

    /* Step 1: Allocate DDR memory for subsystem firmware */
    /* Memory region is isolated by XMPU - ADSP-only access */
    ret = qcom_mdt_load(
        adsp->dev,
        fw,                    /* ELF firmware image */
        rproc->firmware,       /* firmware filename */
        adsp->pas_id,          /* PAS ID = 1 for ADSP */
        adsp->mem_region,      /* DDR base address */
        adsp->mem_phys,
        adsp->mem_size,
        &adsp->mem_reloc);

    if (ret) {
        dev_err(adsp->dev, "MDT load failed: %d\n", ret);
        return ret;
    }

    /* Step 2: Authenticate firmware and release from reset */
    /* This SCM call to TrustZone:
     *   a) Verifies ELF signature using cert chain
     *   b) Configures XMPU for this subsystem's memory
     *   c) Releases subsystem processor from reset
     */
    ret = qcom_scm_pas_auth_and_reset(adsp->pas_id);
    if (ret) {
        dev_err(adsp->dev, "PAS auth+reset failed: %d\n", ret);
        return ret;
    }

    /* Step 3: Wait for subsystem ready signal */
    /* Subsystem sends IPC (GLINK) message when init complete */
    ret = wait_for_completion_timeout(
        &adsp->start_done,
        msecs_to_jiffies(5000));  /* 5 second timeout */

    return ret ? 0 : -ETIMEDOUT;
}
## 12.3 PAS IDs Table
| Subsystem | PAS ID | Processor | Function |
| ADSP | 0x01 | Hexagon Q6 | Audio processing, voice recognition |
| MODEM / MPSS | 0x04 | Hexagon Q6 | 5G/LTE/WCDMA cellular modem |
| WPSS (WLAN) | 0x06 | Xtensa/ARM | Wi-Fi and Bluetooth firmware |
| GPU | 0x09 | Adreno GPU | Graphics processing, compute shaders |
| SLPI | 0x0C | Hexagon Q6 | Sensor fusion (accel, gyro, mag) |
| CDSP | 0x12 | Hexagon Q6 | Compute/AI/CV acceleration |
| NPU / HTP | 0x17 | NPU core | Neural network inference engine |
| IPA | 0x18 | Dedicated HW | Internet Packet Accelerator (offload) |

# Section 13: Inter-Processor Communication (IPC)
With multiple independent processors running in parallel, a robust IPC (Inter-Processor Communication) infrastructure is essential. Qualcomm provides several layers of IPC from hardware mailboxes up to high-level QMI protocols.
## 13.1 IPC Architecture Diagram
+----------------------------------------------------------+
|                   IPC Architecture                        |
|                                                          |
|  APSS <-> MPSS   : QMI (Qualcomm MSM Interface)         |
|                    Over GLINK transport                   |
|                    Used for: modem control, data, calls   |
|                                                          |
|  APSS <-> ADSP   : APR (Audio Packet Router)             |
|                    AFE, ASM, ADM services                 |
|                    Used for: audio playback, capture      |
|                                                          |
|  APSS <-> CDSP   : FastRPC (DSP offload framework)       |
|                    Used for: AI inference, camera ISP     |
|                                                          |
|  APSS <-> SLPI   : SNS QMI (Sensors QMI service)         |
|                    Used for: sensor data streaming        |
|                                                          |
|  APSS <-> AOP    : RPMh (hardware mailbox)               |
|                    TCS (Trigger Command Sets)             |
|                    Used for: clk/voltage votes            |
|                                                          |
|  Underlying Transport Layers:                            |
|    GLINK    : Generic Link (shared memory + doorbell IRQ) |
|    SMP2P    : Shared Multi-Processor 2 Processor         |
|    SMEM     : Shared Memory (carved out of DDR)           |
|    Mailbox  : Hardware register-based doorbell            |
+----------------------------------------------------------+
## 13.2 SMEM (Shared Memory) Code and Defines
/* SMEM is a DDR region accessible by ALL subsystems */
/* Allocated during XBL and carved out of HLOS DDR   */

#define SMEM_BASE           0x86000000    /* Physical address */
#define SMEM_SIZE           0x200000      /* 2 MB */

/* Well-known SMEM item IDs */
#define SMEM_HW_SW_BUILD_ID           137  /* HW build info */
#define SMEM_BOOT_INFO_FOR_APPS       418  /* Boot parameters */
#define SMEM_CRASH_DUMP_MAGIC         577  /* Crash dump flag */
#define SMEM_POWER_ON_STATUS          633  /* PON reasons */
#define SMEM_IMAGE_VERSION_TABLE      469  /* SW version info */
#define SMEM_ID_VENDOR0               134  /* OEM private data */
#define SMEM_COREDUMP_APPDATA         575  /* Minidump metadata */

/* Linux API to access SMEM items */
void *smem_get_entry(unsigned id, size_t *size,
                     unsigned host, unsigned flags)
{
    /* Returns pointer to SMEM item, or NULL if not found */
    return __smem_get_entry_nonsecure(id, size, flags);
}

/* Example: Read boot info written by XBL */
struct smem_boot_info *info;
size_t size;
info = smem_get_entry(SMEM_BOOT_INFO_FOR_APPS, &size,
                      SMEM_HOST_ANY, SMEM_ANY_HOST_FLAG);
if (info) {
    pr_info("Platform: 0x%x, SoC ID: 0x%x\n",
            info->hw_platform, info->msm_id);
}
## 13.3 SMP2P (Shared Multi-Processor 2 Processor) Code and Defines
/* SMP2P provides 32 bidirectional bit-fields per subsystem pair */
/* Used for: error notification, subsystem restart, WDT bark */

/* SMP2P entry structure - one per direction per pair */
struct smp2p_entry {
    char     name[SMP2P_MAX_ENTRY_NAME];  /* "master-kernel" */
    uint32_t remote_data;   /* Bits written by remote processor */
    uint32_t local_data;    /* Bits written by local processor  */
};

/* Standard bit definitions for APSS <-> subsystem pairs */
#define SMP2P_ERR_FATAL_BIT     BIT(0)  /* Subsystem crashed */
#define SMP2P_WDOG_BITE_BIT     BIT(1)  /* WDT bite occurred */
#define SMP2P_CRASH_REASON_BIT  BIT(2)  /* Crash reason valid */
#define SMP2P_STOP_ACK_BIT      BIT(3)  /* Restart ACK */
#define SMP2P_READY_BIT         BIT(15) /* Subsystem init done */

/* Linux driver for SMP2P */
/* drivers/soc/qcom/smp2p.c */

/* Example: MPSS crashes -> sets ERR_FATAL -> APSS sees it */
static irqreturn_t smp2p_irq_handler(int irq, void *data)
{
    struct smp2p_entry *entry = data;
    uint32_t bits = entry->remote_data;

    if (bits & SMP2P_ERR_FATAL_BIT) {
        /* Notify subsystem restart (SSR) framework */
        qcom_ssr_notify_crash(entry->subsys_name);
    }
    return IRQ_HANDLED;
}
# Section 14: Subsystem Restart (SSR) Framework
The SSR (Subsystem Restart) framework allows a crashed subsystem to be restarted without rebooting the entire SoC. When the modem, ADSP, or any other subsystem crashes, Linux detects it (via SMP2P), collects a minidump, shuts down the subsystem, and restarts it.
## 14.1 qcom_ssr_notify_error() Code
/* drivers/remoteproc/qcom_common.c */

void qcom_ssr_notify_error(struct qcom_rproc_ssr *ssr,
                           bool crashed)
{
    struct rproc *rproc = ssr->rproc;

    /* Step 1: Log the crash */
    dev_err(&rproc->dev,
            "Fatal error on subsystem: %s (crashed=%d)\n",
            rproc->name, crashed);

    /* Step 2: Notify registered clients BEFORE shutdown */
    /* WiFi driver notified if WPSS crashes (it disconnects) */
    /* Audio driver notified if ADSP crashes (it re-routes)  */
    srcu_notifier_call_chain(&ssr->notifier_list,
                             SUBSYS_BEFORE_SHUTDOWN,
                             (void *)crashed);

    /* Step 3: Collect crash dump from subsystem memory */
    /* TZ may have already collected minidump regions */
    if (qcom_minidump_subsys_collect(rproc->name) < 0)
        dev_warn(&rproc->dev, "Minidump collection failed\n");

    /* Step 4: Stop the subsystem */
    /* Asserts subsystem reset line via SCM call to TZ */
    rproc_shutdown(rproc);

    /* Step 5: Wait for clients to complete shutdown */
    srcu_notifier_call_chain(&ssr->notifier_list,
                             SUBSYS_AFTER_SHUTDOWN, NULL);

    /* Step 6: Reload firmware from flash */
    /* PIL/PAS: load ELF -> TZ authenticates -> reset release */
    if (rproc_boot(rproc) == 0) {
        dev_info(&rproc->dev,
                 "Subsystem %s restarted successfully\n",
                 rproc->name);
    } else {
        dev_err(&rproc->dev,
                "Subsystem %s restart FAILED - escalating\n",
                rproc->name);
        /* Escalate to full SoC reset if subsystem cant restart */
        call_usermodehelper("/sbin/reboot", NULL, NULL,
                            UMH_NO_WAIT);
    }

    /* Step 7: Notify clients that subsystem is back up */
    srcu_notifier_call_chain(&ssr->notifier_list,
                             SUBSYS_AFTER_POWERUP, NULL);
}
# Section 15: EDL (Emergency Download) Mode
EDL (Emergency Download) mode is a special PBL-level mode that allows flashing the device over USB when all other boot stages have failed or been corrupted. It is the "last resort" recovery mechanism built into every Qualcomm SoC.
## 15.1 How to Enter EDL — 5 Methods
| # | Method | Details |
| 1 | DLOAD magic cookie | Software writes 0x10 to IMEM offset 0x10, then reboots. ABL can trigger this. |
| 2 | Hardware test point short | Device-specific PCB test point shorted to ground during power-on. Bypasses fuse check. |
| 3 | adb reboot edl | Works only if ADB/USB is accessible (Linux running). Writes cookie then reboots. |
| 4 | Key combo at power-on | Device-specific button combination during cold boot. Not standardized across devices. |
| 5 | Boot failure fallthrough | PBL automatically enters EDL if XBL fails authentication or BOOT_CONFIG = 0x3 (USB). |

## 15.2 Firehose Protocol XML Example
The Firehose protocol uses XML over USB to communicate between the host (PC) and the device in EDL mode. The Firehose programmer runs in IMEM and handles all flash operations.
<!-- Firehose XML protocol examples -->

<!-- 1. Configure storage target -->
<configure TargetName="MSM8998" verbose="1"
           AlwaysValidate="1"
           MaxPayloadSizeToTargetInBytes="1048576">
</configure>

<!-- 2. Erase a partition -->
<erase SECTOR_SIZE_IN_BYTES="4096"
       num_partition_sectors="131072"
       physical_partition_number="0"
       start_sector="2048"/>

<!-- 3. Flash a partition (program XML + binary transfer) -->
<program SECTOR_SIZE_IN_BYTES="4096"
         num_partition_sectors="131072"
         physical_partition_number="0"
         start_sector="2048"
         filename="boot.img"/>

<!-- 4. Read back a partition for verification -->
<read SECTOR_SIZE_IN_BYTES="4096"
      num_partition_sectors="2048"
      physical_partition_number="0"
      start_sector="0"
      filename="gpt_backup.bin"/>

<!-- 5. Write GPT partition table -->
<patch SECTOR_SIZE_IN_BYTES="4096"
       byte_offset="0"
       filename="partition.xml"
       physical_partition_number="0"
       size_in_bytes="4096"
       start_sector="0"
       value="0"/>

<!-- USB Device enumeration in EDL mode:
     VID: 05C6 (Qualcomm)   PID: 9008
     Tools: QFIL, edl.py, QDL (qdl),
            bkerler/edl (open-source) -->
## 15.3 PBL Firehose Flow Code
/* PBL EDL/Firehose flow (conceptual) */
void pbl_dload_mode(void)
{
    uint8_t programmer_buf[IMEM_PROG_SIZE];
    size_t  programmer_size;

    /* Step 1: Initialize USB in PBL (minimal USB driver) */
    /* Enumerates as VID:05C6 PID:9008 */
    pbl_usb_init();
    pbl_usb_enumerate();
    dprintf(INFO, "EDL: USB enumerated, waiting for programmer\n");

    /* Step 2: Receive programmer ELF from host via USB */
    /* Host (PC) sends: prog_emmc_firehose_8998_ddr.elf */
    /* or:              prog_ufs_firehose_sm8150_ddr.elf */
    pbl_usb_receive(programmer_buf, &programmer_size);

    /* Step 3: Authenticate programmer */
    /* Same cert chain verification as XBL */
    /* Programmer must be signed by Qualcomm */
    if (pbl_auth_image(programmer_buf) != 0) {
        dprintf(CRITICAL,
                "EDL: Programmer auth failed!\n");
        return; /* Stay in USB loop */
    }

    /* Step 4: Copy programmer to execution address */
    /* Programmer loads into IMEM */
    memcpy((void *)IMEM_PROGRAMMER_BASE,
           programmer_buf,
           programmer_size);

    /* Step 5: Jump to programmer */
    /* Programmer handles all Firehose XML commands */
    typedef void (*programmer_entry_t)(void);
    programmer_entry_t prog =
        (programmer_entry_t)IMEM_PROGRAMMER_BASE;
    prog();
    /* Does not return */
}
# Section 16: Crash Dump Collection Flow
When the system crashes (watchdog bark, kernel panic, subsystem crash), Qualcomm's crash dump infrastructure collects a memory snapshot for post-mortem debugging. There are two modes: full ramdump (all DDR) and minidump (registered regions only).
## 16.1 Ramdump Flow Diagram
  System crashes (panic, WDT bark, subsystem fatal)
      |
      v
  panic() / wdog_bark_handler()
      |
      +-- Write DLOAD magic cookie to IMEM offset 0x10
      |   *(uint32_t *)DLOAD_COOKIE_ADDR = DLOAD_MAGIC;
      |
      +-- Write CRASH_DUMP cookie to IMEM offset 0x14
      |
      +-- machine_restart() / watchdog bite
      |
      v
  System resets (warm reset)
      |
      v
  PBL reads DLOAD magic cookie
      |
      +-- DLOAD cookie SET --> Enter dump collection mode
      |
      v
  XBL reads crash cookies from IMEM
      |
      +-- Full ramdump mode:
      |       Save ALL DDR contents (2-12 GB)
      |       Written to: USB host / SD card / logdump partition
      |
      +-- Minidump mode (preferred in production):
              Save ONLY registered memory regions
              Much smaller: typically 50-200 MB
              Faster collection: ~30 seconds vs hours
              Written to: logdump partition or USB
## 16.2 Minidump Registration Code
/* drivers/soc/qcom/msm_minidump.c */

/* Register a memory region for minidump collection */
/* Called by drivers to register critical debug data */
int msm_minidump_add_region(const struct md_region *entry)
{
    struct md_region *mdr;
    int reg_cnt;

    /* Validate entry */
    if (!entry || !entry->virt_addr || !entry->size)
        return -EINVAL;

    if (entry->size > MAX_MD_REGION_SIZE)
        return -E2BIG;

    spin_lock(&mdt_lock);

    reg_cnt = minidump_table.num_regions;
    if (reg_cnt >= MAX_NUM_ENTRIES) {
        spin_unlock(&mdt_lock);
        return -ENOMEM;
    }

    /* Add to minidump table */
    mdr = &minidump_table.entry[reg_cnt];
    strlcpy(mdr->name, entry->name, sizeof(mdr->name));
    mdr->phys_addr  = entry->phys_addr;
    mdr->virt_addr  = entry->virt_addr;
    mdr->size       = entry->size;
    mdr->id         = reg_cnt;
    minidump_table.num_regions++;

    spin_unlock(&mdt_lock);
    return 0;
}

/* Example registrations by kernel subsystems */

/* Register kernel log buffer */
msm_minidump_add_region(&(struct md_region){
    .name      = "KLOGBUF",
    .phys_addr = virt_to_phys(log_buf),
    .virt_addr = (uintptr_t)log_buf,
    .size      = log_buf_len,
});

/* Register per-CPU task structs */
for_each_possible_cpu(cpu) {
    struct md_region r;
    snprintf(r.name, sizeof(r.name), "KSTACK%d", cpu);
    r.virt_addr = task_stack_page(idle_task(cpu));
    r.phys_addr = virt_to_phys((void *)r.virt_addr);
    r.size = THREAD_SIZE;
    msm_minidump_add_region(&r);
}

/* Register page tables */
msm_minidump_add_region(&(struct md_region){
    .name = "KPGTBL",
    .virt_addr = (uintptr_t)init_mm.pgd,
    .phys_addr = virt_to_phys(init_mm.pgd),
    .size = PGD_SIZE,
});
## 16.3 Minidump Table Struct
/* Minidump table - located at known IMEM address */
/* XBL reads this after reset to know what to collect */

#define MD_TABLE_IMEM_ADDR  0x146BF000   /* Fixed IMEM address */
#define MD_MAGIC            0x5143444D   /* "MDSQ" */
#define MAX_NUM_ENTRIES     201
#define MAX_MD_REGION_SIZE  SZ_64M

struct md_region {
    char     name[16];       /* Region name e.g. "KLOGBUF" */
    uint64_t phys_addr;      /* Physical address in DDR */
    uint64_t virt_addr;      /* Virtual address (for symbols) */
    uint64_t size;           /* Size in bytes */
    uint32_t id;             /* Auto-assigned region ID */
    uint32_t flags;          /* MD_REGION_VALID etc. */
};

struct md_table {
    uint32_t magic;              /* 0x5143444D = "MDSQ" */
    uint32_t version;            /* Table format version */
    uint32_t num_regions;        /* Number of registered regions */
    uint32_t enabled;            /* 1 = minidump enabled */
    struct md_region entry[MAX_NUM_ENTRIES];
};

/* Table is written to IMEM at boot */
/* XBL, TZ, Linux all share this table */
/* XBL reads it post-reset to collect minidump */
# Section 17: Security Fuses (QFPROM)
QFPROM (Qualcomm Fuse ROM) is a one-time-programmable (OTP) fuse array that stores security configuration burned during manufacturing. Once blown, fuse bits cannot be reset — making them the hardware root of the security model.
## 17.1 Complete Fuse Regions Table
| Fuse Name | Size (bits) | Purpose |
| OEM_PK_HASH | 256 bits | SHA256 hash of OEM root public key - core of chain of trust |
| SECURE_BOOT_EN | 1 bit | Enable secure boot enforcement (irreversible) |
| SHK (Sec HW Key) | 256 bits | Hardware-unique device encryption key (HWKM) |
| JTAG_DISABLE | 1 bit | Permanently disable JTAG debug access |
| RPMB_KEY_PROVISION | 1 bit | Flag indicating RPMB key has been provisioned |
| ANTI_ROLLBACK_1-5 | 32 bits each | Rollback version counters per image type (XBL, TZ, modem...) |
| OEM_CONFIG | 32 bits | OEM-specific configuration (features, debug policy) |
| SERIAL_NUM | 64 bits | Chip serial number (unique per device) |
| FEAT_CONFIG | 64 bits | Feature enable/disable (modem bands, crypto features) |
| DEBUG_DISABLE | 8 bits | Disable invasive/non-invasive debug per exception level |
| APPS_DBGEN | 1 bit | Enable APSS debug (JTAG/ETM) - usually disabled in production |
| MRC (Multi Root Cert) | 3 bits | Enable multiple OEM root certificates for MRC scheme |
| FOUNDRY_ID | 4 bits | Manufacturing foundry identifier (TSMC, Samsung, etc.) |
| ANTI_ROLLBACK_MODEM | 32 bits | Modem firmware anti-rollback version counter |

## 17.2 Fuse Blowing Process Code
Fuse blowing is a one-time, irreversible manufacturing step. It is performed in the factory during device provisioning. The QFPROM fuse controller applies a higher voltage pulse to permanently change the fuse cell state.
/* Fuse blowing sequence - performed ONCE during manufacturing */
/* After blowing, bits CANNOT be reset - permanent! */

/* QFPROM register offsets */
#define QFPROM_BASE              0x00784000  /* TLMM region */
#define QFPROM_OEM_PK_HASH_ROW0  0x00780218
#define QFPROM_OEM_PK_HASH_ROW1  0x00780220
#define QFPROM_SECURE_BOOT_EN    0x00780350
#define QFPROM_JTAG_DISABLE      0x00780370
#define QFPROM_ANTI_ROLLBACK_1   0x00780390

void blow_secure_boot_fuses(void)
{
    int ret;

    /* Step 1: Write OEM public key hash */
    /* hash[0:15] -> ROW0, hash[16:31] -> ROW1 */
    ret = qfprom_write_row(QFPROM_OEM_PK_HASH_ROW0,
                           pk_hash_low,   /* bits [127:0] */
                           pk_hash_low_redundant);
    ret |= qfprom_write_row(QFPROM_OEM_PK_HASH_ROW1,
                            pk_hash_high,  /* bits [255:128] */
                            pk_hash_high_redundant);

    /* Step 2: Enable secure boot enforcement */
    /* THIS IS THE POINT OF NO RETURN */
    /* After this, EVERY image must be signed by OEM key */
    ret |= qfprom_write_row(QFPROM_SECURE_BOOT_EN, 0x1, 0x1);

    /* Step 3: Set initial anti-rollback counters */
    ret |= qfprom_write_row(QFPROM_ANTI_ROLLBACK_1,
                            INITIAL_XBL_VERSION, 0);

    /* Step 4: Disable JTAG (PRODUCTION DEVICES ONLY!) */
    /* WARNING: Disabling JTAG cannot be undone */
    if (production_device) {
        ret |= qfprom_write_row(QFPROM_JTAG_DISABLE,
                                0x1, 0x1);
    }

    /* Verify fuses were correctly blown */
    verify_qfprom_fuses();
}
# Section 18: Exception Levels During Boot
ARM64 defines four Exception Levels (EL0-EL3) with increasing privilege. The Qualcomm boot flow transitions through multiple exception levels as control passes from the secure world to the normal world.
## 18.1 Exception Level Table for Each Boot Stage
| Boot Stage | Exception Level | World | Key Capabilities |
| PBL | EL3 | Secure | Full hw access, sets SCR_EL3, VBAR_EL3 |
| XBL / XBL_SEC | EL3 | Secure | DDR init, image auth, XPPU/XMPU setup |
| TrustZone (QSEE) | EL3 (resident) | Secure | Secure monitor, SMC handler, secure WDT |
| QSEE TAs | S-EL1 / S-EL0 | Secure | Keymaster, Widevine, Gatekeeper TAs |
| Hypervisor (QHEE) | EL2 | Normal | Stage-2 page tables, VM isolation (GVM) |
| ABL (UEFI ABL) | EL1 (via EL2 stub) | Normal | Loads kernel, AVB, no Stage-2 PT yet |
| Linux Kernel | EL1 | Normal | OS kernel, drivers, interrupt handlers |
| Linux Userspace | EL0 | Normal | Apps, Android framework, no kernel access |

## 18.2 EL Transitions Diagram
PBL (EL3, Secure)
    |
    | XBL loaded and verified
    v
XBL / XBL_SEC (EL3, Secure)
    |
    +-- TZ loaded -> remains resident as EL3 Secure Monitor
    |
    +-- HYP loaded -> will run at EL2 Normal world
    |
    | Switch to Normal World (set SCR_EL3.NS = 1)
    v
QHEE Hypervisor (EL2, Normal)
    |
    | Set up EL1 (ABL entry)
    v
ABL - UEFI ABL (EL1, Normal)
    |
    | ExitBootServices() -> MMU off -> jump to kernel
    v
Linux Kernel head.S (EL1, Normal, MMU OFF)
    |
    | __enable_mmu() -> virtual address space active
    v
Linux Kernel start_kernel() (EL1, Normal, MMU ON)
    |
    | TZ still at EL3 (always present as secure monitor)
    | Linux uses SMC to call TZ services (SCM driver)
    | PSCI: CPU_ON for secondary CPUs via EL3
    v
Linux Userspace (EL0, Normal)
    |
    | syscall instruction -> traps to EL1 (kernel)
    v
Kernel syscall handler (EL1, Normal)
# Section 19: Qualcomm-Specific Boot Concepts
## 19.1 CDT (Configuration Data Table) Structs
CDT (Configuration Data Table) solves the problem of supporting multiple board variants with the same silicon. A single SoC (e.g., SM8150) might be used in dozens of different board designs with different DDR vendors, panel types, or PMIC configurations. CDT tells XBL which hardware configuration to use without maintaining separate firmware builds.
/* CDT (Configuration Data Table) - stored in cdt partition */

struct cdt_header {
    uint32_t magic;          /* "CDT\0" = 0x00544443 */
    uint32_t version;        /* CDT format version */
    uint32_t num_entries;    /* Number of CDT records */
    uint32_t crc32;          /* CRC32 of all entries */
};

struct cdt_entry {
    uint32_t platform_id;       /* SoC ID: SM8150=394, SDM845=321 */
    uint32_t subtype_id;        /* Board subtype: MTP=0, QRD=1 */
    uint32_t platform_version;  /* HW revision */
    uint32_t ddr_id;            /* DDR vendor: Samsung=0, Hynix=1 */
    uint32_t boot_device;       /* eMMC=0, UFS=1, SD=2 */
    uint32_t display_id;        /* Panel type identifier */
    uint32_t pmic_model;        /* PMIC HW model number */
    uint8_t  foundry_id;        /* Chip foundry */
    uint8_t  chip_serial[8];    /* Unique chip serial number */
};

/* XBL reads CDT during init to select HW config */
void xbl_read_cdt(void)
{
    struct cdt_header hdr;
    struct cdt_entry entry;

    /* Read CDT from flash */
    flash_read("cdt", 0, &hdr, sizeof(hdr));
    if (hdr.magic != CDT_MAGIC)
        xbl_use_default_config();

    /* Find matching entry for this board */
    for (int i = 0; i < hdr.num_entries; i++) {
        flash_read("cdt", sizeof(hdr) + i*sizeof(entry),
                   &entry, sizeof(entry));
        if (entry.platform_id == get_platform_id() &&
            entry.subtype_id == get_board_subtype()) {
            xbl_configure_hw(&entry);
            return;
        }
    }
}
## 19.2 SMEM Boot Info Struct
XBL writes boot information into SMEM (Shared Memory) so that the Linux kernel and other subsystems can read hardware platform information without needing their own detection logic.
/* XBL writes this to SMEM_BOOT_INFO_FOR_APPS (ID 418) */
/* Linux reads it via smem_get_entry() during boot */
struct smem_boot_info {
    uint32_t format;            /* Structure format version */
    uint32_t hw_platform;       /* MTP=8, QRD=11, RCM=1 */
    uint32_t hw_platform_subtype;  /* Board subtype */
    uint32_t msm_id;            /* SoC ID: SDM845=321, SM8150=394 */
    uint32_t msm_id_v2;
    uint32_t pmic_model[4];     /* PMIC type per PM bus slot */
    uint32_t pmic_die_rev[4];   /* PMIC die revision */
    uint32_t foundry_id;        /* Silicon foundry */
    uint32_t chip_serial[2];    /* 64-bit unique serial number */
    uint32_t num_pmics;         /* Number of PMICs on SPMI bus */
    uint32_t platform_version;  /* Board hardware version */
    uint32_t accessory_chip;    /* Accessory chip info */
};

/* Linux reads platform ID at boot: */
/* arch/arm64/boot/dts/qcom/... reads smem_board_id */
## 19.3 Boot Cookie / Restart Reason Defines and Code
The restart reason mechanism allows software to communicate the intended boot mode to the bootloader across a reset. ABL writes a specific value to an IMEM address before triggering a reboot, and reads it back after the reset to determine the boot path.
/* Boot reason cookie - written to IMEM before reset */
/* Address is fixed and known to all boot stages */

#define RESTART_REASON_ADDR      (IMEM_BASE + 0x65C)
#define RESTART_REASON_BOOT_ADDR (IMEM_BASE + 0x660)

/* Boot reason values */
#define PON_RESTART_REASON_UNKNOWN        0x00
#define PON_RESTART_REASON_RECOVERY       0x01  /* adb reboot recovery */
#define PON_RESTART_REASON_BOOTLOADER     0x02  /* adb reboot bootloader */
#define PON_RESTART_REASON_RTC            0x03  /* RTC alarm boot */
#define PON_RESTART_REASON_DMVERITY_FAIL  0x04  /* dm-verity corrupt */
#define PON_RESTART_REASON_DMVERITY_ENF   0x05  /* dm-verity enforcing */
#define PON_RESTART_REASON_KEYS_CLEAR     0x06  /* Factory reset keys */
#define PON_RESTART_REASON_PANIC          0x40  /* Kernel panic */
#define PON_RESTART_REASON_WATCHDOG       0x41  /* WDT bite */
#define PON_RESTART_REASON_CHARGER        0x42  /* Charger boot */
#define PON_RESTART_REASON_THERMAL        0x43  /* Thermal shutdown */

/* Writing restart reason (in Linux kernel): */
#include <linux/notifier.h>

static int qcom_restart_handler(struct notifier_block *nb,
                                 unsigned long action, void *data)
{
    const char *cmd = (const char *)data;

    if (cmd) {
        if (!strcmp(cmd, "bootloader"))
            __raw_writel(PON_RESTART_REASON_BOOTLOADER,
                         restart_reason_addr);
        else if (!strcmp(cmd, "recovery"))
            __raw_writel(PON_RESTART_REASON_RECOVERY,
                         restart_reason_addr);
        else if (!strcmp(cmd, "panic"))
            __raw_writel(PON_RESTART_REASON_PANIC,
                         restart_reason_addr);
    }
    return NOTIFY_DONE;
}

/* ABL reads restart reason: */
uint32_t reason = readl(RESTART_REASON_ADDR);
switch (reason) {
    case PON_RESTART_REASON_BOOTLOADER:
        boot_into_fastboot = true;
        break;
    case PON_RESTART_REASON_RECOVERY:
        boot_into_recovery = true;
        break;
    case PON_RESTART_REASON_WATCHDOG:
        log_watchdog_reset();
        /* Fall through to normal boot */
        break;
    default:
        /* Normal boot */
        break;
}
# Summary: Complete Boot Flow Reference
The following table provides a quick reference for the complete Qualcomm boot flow, suitable for architecture discussions and technical interviews.
| Stage | Component | EL / World | Duration | Key Actions |
| 0 | PMIC + HW Reset | N/A | ~5ms | Power rail seq: MX->CX->APC, crystal osc, reset vector |
| 1 | PBL (ROM) | EL3 Secure | ~25ms | Boot config fuses, load XBL, cert chain verify, EDL check |
| 2 | XBL (UEFI) | EL3 Secure | ~365ms | DDR train, clocks, load TZ/AOP/HYP/modem/DSP, XPPU/XMPU |
| 3 | TrustZone/QSEE | EL3 Secure | ~30ms | SCR_EL3, GIC FIQ routing, QSEE init, load TAs, drop NS=1 |
| 4 | AOP (Cortex-M) | M-profile | ~10ms | Always-on power/clock mgmt, SPMI/PMIC control, AOSS WDT |
| 5 | ABL (UEFI/LK) | EL1 Normal | ~90ms | A/B select, boot.img load, AVB 2.0, ExitBootServices, MMU off |
| 6 | Linux Kernel | EL1 Normal | ~500ms | head.S: pg tables, MMU on; start_kernel(): sched, GIC, init |
| 7 | SMP + Userspace | EL0-EL1 | ~8500ms | PSCI CPU_ON, PIL load subsystems, Android framework, home |

Document End
