Device Tree Concepts for Linux Kernel & Embedded Systems
Technical Reference for Interview Preparation
Prepared for: Sandeep Kumar
Topics: DTBO | Qualcomm ABL DTBO Selection | boot.img Header V4
|  |


# 1. DTBO in Device Tree
A DTBO (Device Tree Blob Overlay) is a compiled binary form of a Device Tree Overlay (DTO) — a mechanism that allows you to dynamically modify or extend the base Device Tree Blob (DTB) at runtime, without recompiling the entire device tree.
## 1.1 Key Concepts
Device Tree Blob (DTB) is the compiled binary of the main Device Tree Source (DTS) that describes the hardware platform. A DTBO is an overlay on top of this base DTB.
| Term | Description |
| DTB | Device Tree Blob — compiled binary of main hardware description (DTS) |
| DTS | Device Tree Source — human-readable hardware description file |
| DTBO | Device Tree Blob Overlay — compiled binary overlay that patches the base DTB |
| DTO | Device Tree Overlay — the source form of a DTBO overlay |
| dtc | Device Tree Compiler — tool to compile DTS/DTO to DTB/DTBO |


## 1.2 What DTBO Does
- Allows partial, modular hardware description — you can add, modify, or disable nodes/properties in the base DTB
- Enables runtime hardware configuration — useful for add-on boards, HATs, or optional peripherals (e.g., Raspberry Pi HATs, Qualcomm daughter boards)
- Supports dynamic loading — the bootloader or kernel can apply overlays at boot time or even at runtime via configfs

## 1.3 DTBO File Flow
.dts (overlay source)
    |  dtc (Device Tree Compiler)
.dtbo (Device Tree Blob Overlay)
    |  Applied by bootloader (U-Boot) or kernel
Final merged DTB (used by kernel)

## 1.4 Structure of a DTS Overlay (compiled to DTBO)
/dts-v1/;
/plugin/;

&i2c1 {
    status = "okay";
    my_sensor@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
    };
};
The /plugin/ directive marks it as an overlay. &i2c1 is a label reference to an existing node in the base DTB — the overlay patches it.
## 1.5 How DTBO is Applied
Via U-Boot:
load mmc 0:1 ${fdt_addr} base.dtb
load mmc 0:1 ${dtbo_addr} overlay.dtbo
fdt addr ${fdt_addr}
fdt resize 8192
fdt apply ${dtbo_addr}
booti ${kernel_addr} - ${fdt_addr}
Via Linux configfs (runtime):
mkdir /sys/kernel/config/device-tree/overlays/my_overlay
cat overlay.dtbo > /sys/kernel/config/device-tree/overlays/my_overlay/dtbo

## 1.6 Practical Use Cases
Given Qualcomm IoT SoC and i.MX8X experience, DTBOs are commonly used for:
- Board variants — same SoC, different peripheral configurations (e.g., with/without a camera module)
- Add-on hardware — enabling I2C/SPI/UART peripherals on expansion connectors
- Android — Qualcomm uses DTBOs extensively; the dtbo.img partition in Android contains overlays applied by the bootloader (ABL/UEFI) to the base DTB
- Yocto builds — recipes can package .dtbo files and configure U-Boot to apply them

## 1.7 DTBO in Android (Qualcomm Context)
In Android on Qualcomm platforms, there's a dedicated dtbo partition:
- The bootloader selects the correct DTBO based on board ID / hardware strapping
- ABL (Android Bootloader) merges the DTBO with the base DTB before handing off to the kernel
- This is defined in the DTBO table — a header structure listing multiple overlays

## 1.8 Compilation
# Compile overlay DTS to DTBO
dtc -@ -I dts -O dtb -o overlay.dtbo overlay.dts

# The -@ flag preserves symbols needed for overlay application
Note: The -@ flag is critical — it generates a __symbols__ node in the DTB/DTBO so the kernel can resolve label references across base DTB and overlays.

# 2. Qualcomm ABL DTBO Selection and Application
Qualcomm's ABL (Android Bootloader), built on UEFI, has a sophisticated mechanism for selecting and applying DTBOs. This section provides a deep dive covering the UEFI/LK architecture context.
## 2.1 The DTBO Partition Layout
The dtbo partition contains a DTBO table — a structured header followed by multiple DTBO entries:
+---------------------------+
|  dt_table_header          |  (magic, version, dt_entry_count, ...)
+---------------------------+
|  dt_entry [0]             |  (dt_type, dt_size, dt_offset, id, rev, ...)
|  dt_entry [1]             |
|  ...                      |
+---------------------------+
|  DTBO binary data [0]     |
|  DTBO binary data [1]     |
|  ...                      |
+---------------------------+
The dt_table_header structure (from AOSP libdtb):
struct dt_table_header {
    uint32_t magic;           // 0xd7b7ab1e
    uint32_t total_size;
    uint32_t header_size;
    uint32_t dt_entry_size;
    uint32_t dt_entry_count;
    uint32_t dt_entries_offset;
    uint32_t page_size;
    uint32_t version;
};

struct dt_table_entry {
    uint32_t dt_size;
    uint32_t dt_offset;
    uint32_t id;          // SoC ID
    uint32_t rev;         // Board revision
    uint32_t custom[4];   // Platform-specific (subtype, variant, etc.)
};

## 2.2 Hardware Identification — How ABL Knows Which DTBO to Pick
Qualcomm ABL reads hardware identity from multiple sources:
a) SMEM (Shared Memory)
SMEM_ID_VENDOR0  ->  SoC ID (e.g., 0x415 for SM8250)
SMEM_ID_VENDOR1  ->  Board ID / Platform subtype
SMEM_ID_VENDOR2  ->  DDR type, memory config
ABL calls BoardPlatformChipId(), BoardPlatformSubType(), BoardHwPlatformSubType() — these are UEFI protocol wrappers over SMEM reads.
b) PMIC / SPMI
Board revision and power rail configuration can influence DTBO selection.
c) Fuses (eFuse / QFPROM)
Security state, SKU variants, and feature enablement bits read via QFPROMProtocol.
d) Hardware Strapping (GPIO)
Some boards use GPIO strapping to indicate board variant — ABL reads these via EFI_TLMM_PROTOCOL.

## 2.3 DTBO Selection Algorithm in ABL
The selection logic (in QcomModulePkg/Library/BootLib/UpdateDeviceTree.c):
// Pseudocode of ABL DTBO matching
for each dt_entry in dtbo_table:
    if (entry.id == platform_id) &&
       (entry.rev == board_rev || entry.rev == 0) &&
       (entry.custom == platform_subtype || entry.custom == 0):
        candidate = entry
        // Best match: most specific (non-zero fields) wins

// Priority: exact match > wildcard (0 = "don't care")
Matching priority (most specific wins):
- Exact SoC ID + exact board rev + exact subtype
- Exact SoC ID + exact board rev + wildcard subtype
- Exact SoC ID + wildcard rev + exact subtype
- Exact SoC ID + wildcard rev + wildcard subtype

## 2.4 DTBO Application — Merging with Base DTB
After selecting the DTBO, ABL applies it to the base DTB loaded from the boot or vendor_boot partition:
Step 1: Load base DTB from boot.img (appended DTB or dtb partition)
Step 2: Load selected DTBO from dtbo partition
Step 3: Call fdt_overlay_apply()  ->  merges DTBO into base DTB in memory
Step 4: Pass merged DTB address to kernel via x0 (ARM64 boot protocol)
The key UEFI function chain:
LoadImageAndAuth()
  -> UpdateDeviceTree()
      -> GetDtboIdx()          // selects correct DTBO entry
      -> ApplyDtboToBase()     // calls fdt_overlay_apply()
  -> BootLinux()
      -> // passes merged DTB in x0
fdt_overlay_apply() internals:
// From libfdt
int fdt_overlay_apply(void *fdt, void *fdto) {
    // 1. Resolve phandles -- remap DTBO phandles to avoid collision with base DTB
    // 2. Apply __fixups__ -- patch label references (&i2c1, &spi0, etc.)
    // 3. Apply __local_fixups__ -- fix internal DTBO phandle references
    // 4. Merge nodes -- overlay nodes are merged into base DTB tree
    // 5. Apply __overlay__ fragment nodes
}

## 2.5 Multiple DTBO Support (Android 9+)
From Android 9 onwards, ABL can apply multiple DTBOs in sequence:
dtbo partition -> [DTBO_0 for SoC base] + [DTBO_1 for board variant] + [DTBO_2 for RF config]
Each overlay is applied on top of the previously merged result:
base_dtb + dtbo[0]  ->  merged_0
merged_0 + dtbo[1]  ->  merged_1
merged_1 + dtbo[2]  ->  final_dtb  ->  kernel

## 2.6 Verification — AVB (Android Verified Boot)
Before applying, ABL verifies the DTBO partition using AVB 2.0:
avb_slot_verify()  ->  checks vbmeta signature over dtbo partition
This ensures the DTBO hasn't been tampered with. If verification fails, ABL either:
- Halts boot (LOCKED device)
- Warns and continues (UNLOCKED device)

## 2.7 Debugging DTBO Selection in ABL
Useful UEFI debug logs (enable via UEFI_LOG_LEVEL):
[ABL] Platform ID: 0x415, Board Rev: 0x2, SubType: 0x1
[ABL] DTBO: Matched entry index 3
[ABL] DTBO: Applying overlay at 0x9F000000 (size: 0x4200)
[ABL] FDT: Overlay applied successfully
You can also dump the final merged DTB from memory and inspect with:
dtc -I dtb -O dts <dumped_file>

## 2.8 Relevant Source Locations (AOSP / CAF)
| Component | Path |
| DTBO table header | system/libdtb/include/libdtb.h |
| ABL DTBO logic | QcomModulePkg/Library/BootLib/UpdateDeviceTree.c |
| fdt overlay | lib/libfdt/fdt_overlay.c |
| AVB integration | QcomModulePkg/Application/LinuxLoader/LinuxLoader.c |


## 2.9 Key Takeaway for Staff Engineer Interview
ABL's DTBO selection is a hardware-identity-driven best-match algorithm — it reads SoC ID, board revision, and platform subtype from SMEM/PMIC/fuses, scores each DTBO entry by specificity (exact match > wildcard), selects the best match, verifies it via AVB, then uses fdt_overlay_apply() to merge it with the base DTB in memory before kernel handoff.

# 3. boot.img Header V4
The boot_img_hdr_v4 structure is defined in AOSP under system/tools/mkbootimg/include/bootimg/bootimg.h. This section provides the full breakdown of the V4 header, its layout, and Qualcomm ABL context.
## 3.1 Header Structure
struct boot_img_hdr_v4 {
    uint8_t  magic[BOOT_MAGIC_SIZE];   // "ANDROID!" (8 bytes)
    uint32_t kernel_size;              // size in bytes
    uint32_t kernel_addr;             // physical load address (deprecated)
    uint32_t ramdisk_size;            // size in bytes
    uint32_t ramdisk_addr;            // physical load address (deprecated)
    uint32_t second_size;             // size in bytes (secondary bootloader)
    uint32_t second_addr;             // physical load address (deprecated)
    uint32_t tags_addr;               // physical addr for kernel tags (deprecated)
    uint32_t page_size;               // flash page size (4096 typically)
    uint32_t header_version;          // = 4
    uint32_t os_version;              // OS version and security patch level
    uint8_t  name[BOOT_NAME_SIZE];    // product name (16 bytes, null-terminated)
    uint8_t  cmdline[BOOT_ARGS_SIZE]; // kernel command line (512 bytes)
    uint32_t id[8];                   // SHA-1 of kernel/ramdisk/second (32 bytes)
    uint8_t  extra_cmdline[BOOT_EXTRA_ARGS_SIZE]; // extra cmdline (1024 bytes)
    uint32_t recovery_dtbo_size;      // size of recovery DTBO/ACPIO image
    uint64_t recovery_dtbo_offset;    // offset in boot image (for recovery)
    uint32_t header_size;             // size of this header struct
    uint32_t dtb_size;                // size of DTB
    uint64_t dtb_addr;                // physical load address for DTB
    uint32_t signature_size;          // NEW in v4: size of boot image signature
};

## 3.2 What's New in V4 vs V3
| Field | V3 | V4 |
| signature_size | Not present | Added (GKI 2.0 signing) |
| vendor_ramdisk_table | In vendor_boot | In vendor_boot (extended) |
| bootconfig | In vendor_boot | In vendor_boot (extended) |


The key addition in V4 is signature_size — it reserves space at the end of the boot image for a boot image signature used by GKI (Generic Kernel Image) signing. This is part of Android 12+ GKI 2.0.

## 3.3 V4 Boot Image Layout (on flash / in memory)
+---------------------------+  offset 0
|  boot_img_hdr_v4          |  (padded to page_size)
+---------------------------+
|  kernel                   |  (padded to page_size)
+---------------------------+
|  ramdisk                  |  (padded to page_size)
+---------------------------+
|  boot signature           |  <-- NEW in V4 (signature_size bytes)
+---------------------------+
Note: V4 dropped the second stage bootloader section from the image layout (it was deprecated in V3 already).

## 3.4 Companion: vendor_boot_img_hdr_v4
V4 introduced a versioned vendor_boot header as well. The vendor_boot partition (introduced in V3) was extended in V4 with:
struct vendor_boot_img_hdr_v4 {
    // ... all v3 fields ...
    uint32_t vendor_ramdisk_table_size;    // total size of vendor ramdisk table
    uint32_t vendor_ramdisk_table_entry_num;
    uint32_t vendor_ramdisk_table_entry_size;
    uint32_t bootconfig_size;              // size of bootconfig section
};
This enables multiple vendor ramdisk fragments — each fragment has:
struct vendor_ramdisk_table_entry_v4 {
    uint32_t ramdisk_size;
    uint32_t ramdisk_offset;
    uint32_t ramdisk_type;    // VENDOR_RAMDISK_TYPE_NONE/PLATFORM/RECOVERY/DLKM
    uint8_t  ramdisk_name[VENDOR_RAMDISK_NAME_SIZE];  // 32 bytes
    uint32_t board_id[VENDOR_RAMDISK_TABLE_ENTRY_BOARD_ID_ITEM_COUNT]; // 16 entries
};

## 3.5 Vendor Boot V4 Layout
+----------------------------------+
|  vendor_boot_img_hdr_v4          |  (padded to page_size)
+----------------------------------+
|  vendor ramdisk section          |  (all fragments concatenated, padded)
+----------------------------------+
|  DTB                             |  (padded to page_size)
+----------------------------------+
|  vendor ramdisk table            |  (array of vendor_ramdisk_table_entry_v4)
+----------------------------------+
|  bootconfig                      |  (padded to page_size)
+----------------------------------+

## 3.6 Key Constants
#define BOOT_MAGIC_SIZE         8
#define BOOT_NAME_SIZE          16
#define BOOT_ARGS_SIZE          512
#define BOOT_EXTRA_ARGS_SIZE    1024

## 3.7 Relevant Source Locations (AOSP)
| File | Purpose |
| system/tools/mkbootimg/include/bootimg/bootimg.h | Header struct definitions |
| system/tools/mkbootimg/mkbootimg.py | Image creation tool |
| system/tools/mkbootimg/unpack_bootimg.py | Image unpacking/inspection |
| bootable/recovery/install/ | Recovery boot image handling |


## 3.8 Qualcomm ABL Context
In Qualcomm ABL (UEFI), the boot image header is parsed in:
QcomModulePkg/Library/BootLib/BootImage.c
  -> ParseBootImageHeader()   // reads magic, version, sizes
  -> LoadImageFromPartition() // uses header_version to branch V0/V1/V2/V3/V4
ABL checks header_version first and dispatches to the appropriate parsing path — so V4 adds the signature_size read and the boot signature verification step before kernel handoff.

## 3.9 Boot Image Version Comparison Table
| Feature | V0 | V1 | V2 | V3 | V4 |
| recovery_dtbo | - | Yes | Yes | Yes | Yes |
| DTB field | - | - | Yes | Yes | Yes |
| vendor_boot | - | - | - | Yes | Yes |
| ramdisk table | - | - | - | - | Yes |
| signature_size | - | - | - | - | Yes |
| bootconfig | - | - | - | - | Yes |


# 4. Quick Reference & Interview Cheat Sheet
## 4.1 Key Commands
| Command | Purpose |
| dtc -@ -I dts -O dtb -o overlay.dtbo overlay.dts | Compile DTS overlay to DTBO with symbols |
| dtc -I dtb -O dts -o out.dts in.dtb | Decompile DTB back to DTS for inspection |
| fdtdump device.dtb | Dump and inspect DTB contents |
| fdtget device.dtb /node property | Read a specific node property from DTB |
| fdtput -t s device.dtb /node prop val | Set a property value in DTB |
| python3 unpack_bootimg.py --boot_img boot.img | Unpack boot.img and inspect components |


## 4.2 Key Interview Concepts
- __symbols__ node: Generated by dtc -@ flag; maps node labels to full paths for overlay resolution
- __fixups__ node: In DTBO; lists all label references that need to be patched against base DTB symbols
- __local_fixups__ node: In DTBO; lists phandle references internal to the DTBO itself
- phandle: Integer handle used to cross-reference nodes in DTS (e.g., clocks, interrupts, pinmux)
- fragment@N: In overlay DTS, wraps target node with __overlay__ child containing the changes
- GKI (Generic Kernel Image): Android 12+ kernel image that is hardware-agnostic; vendor extensions go in vendor_boot
- DLKM (Dynamic Loadable Kernel Module): Vendor kernel modules loaded from vendor_ramdisk at boot via ramdisk_type = DLKM
- AVB 2.0 (Android Verified Boot): Chain-of-trust from bootloader through kernel; verifies each partition with vbmeta signatures

## 4.3 Qualcomm SMEM IDs Cheat Sheet
| SMEM ID | Content | ABL Usage |
| SMEM_ID_VENDOR0 | SoC ID (e.g., 0x415 = SM8250) | BoardPlatformChipId() |
| SMEM_ID_VENDOR1 | Board ID / Platform subtype | BoardPlatformSubType() |
| SMEM_ID_VENDOR2 | DDR type / memory config | BoardHwPlatformSubType() |


Study Tip: Focus on the end-to-end flow: DTS source -> dtc compilation -> DTBO -> ABL selection (SMEM identity matching) -> fdt_overlay_apply() -> kernel handoff via x0. Being able to trace this entire path verbally is what differentiates a Staff Engineer answer from a mid-level one.
