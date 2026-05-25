| APPSBL DEBUGGING SCENARIOS |


Interview Preparation Guide
Document 2 of 3  —  Qualcomm UEFI ABL / LK Bootloader Debugging

APPSBL Stage  |  Debug Methodology  |  Root Cause Analysis  |  Cross-Questions
| Document CoverageBug 1 (DIFFICULT): Intermittent Boot Hang — RPMB Race Condition / Stale DTB OverlayBug 2 (INTERMEDIATE): Black Screen After Fastboot Flash — Board ID DTB MismatchCovers: Problem Statements | Failure Chains | Debug Steps with Code | Fixes | Cross-Questions |


# Bug Overview Comparison
| Attribute | Bug 1 — DIFFICULT | Bug 2 — INTERMEDIATE |
| Level | DIFFICULT | INTERMEDIATE |
| Platform | Qualcomm SDM845, UEFI ABL | Qualcomm SM6150, UEFI ABL |
| Symptom | Intermittent boot hang (~2% rate) after OTA | 100% black screen after fastboot flash of boot.img |
| Root Cause | RPMB-to-eMMC race + stale DTB overlay applied | Board ID mismatch: old dtbo with new boot.img |
| Debug Steps | 9 Steps | 7 Steps |
| Tools Used | JTAG (Trace32), UART, dtc, mkdtboimg, ABL instrumentation | ADB, UART, dtc, mkdtboimg, ABL debug logs |
| Why Difficult | Intermittent, multiple subsystems, no logs at crash, misleading symptoms | Logical error, works in full flash, ABL reports success silently |


| BUG 1 — DIFFICULT LEVEL |


Intermittent Boot Hang During Verified Boot on A/B Slot Switch After OTA
# Problem Statement
After an OTA (Over-The-Air) update on a Qualcomm SDM845-based device, approximately 1 in 50 boots would hang at the APPSBL (UEFI ABL) stage. The device behavior was:
- Shows the OEM splash screen normally
- Hangs indefinitely — no kernel boot, no UART output after a certain point
- A hard power cycle would sometimes recover it, sometimes not
- The issue ONLY occurred after an A/B slot switch (slot_a → slot_b) following OTA
- It NEVER reproduced in fastboot boot or when flashing directly

| Why This Is Extremely Difficult1. Intermittent: Only ~2% reproduction rate, tied to eMMC timing2. Misleading symptoms: Appears as APPSBL hang, but actual crash is in early kernel3. Multiple subsystems: AVB, RPMB, eMMC power management, DTB overlay, A/B slot mgmt4. No logs at crash point: Kernel crashes before UART/console is initialized5. Works in fastboot: Direct flashing does not trigger the A/B slot switch path6. Correct AVB behavior masks the issue: The rollback check passes, hiding RPMB corruption |


# Root Cause — Chain of Failure
The root cause was a race condition between RPMB (Replay Protected Memory Block) access and eMMC power state transition during verified boot's rollback index read, combined with a stale DTB overlay being applied from the wrong slot.

| Step 1 | OTA Update Writes Rollback Index | OTA updates slot_b and writes new rollback index to RPMB via the update agent. |


| Step 2 | ABL Switches to slot_b | On next boot, ABL switches active slot to slot_b by reading the boot control block from the misc partition. |


| Step 3 | AVB Begins Verification | ABL begins Android Verified Boot (AVB): reads vbmeta_b partition and needs to verify rollback index against stored value in RPMB (secure storage on eMMC). |


| Step 4 | RPMB Access Race Condition | ABL sends an RPMB read command to eMMC. However, the eMMC controller was still completing a power state transition (from sleep to active) triggered by the slot switch partition reads. The RPMB read returns stale/corrupted data (~2% of the time) — specifically, rollback index reads as 0 instead of the correct value (3). |


| Step 5 | AVB Rollback Check Passes Incorrectly | AVB compares image rollback index (3) against stored RPMB value (0 — corrupted read). Since image_index (3) > stored_index (0), AVB passes the check. But AVB then attempts to UPDATE the stored rollback index from 0 to 3 via RPMB write. |


| Step 6 | RPMB Write Triggers the Real Problem | The RPMB write succeeds, but the eMMC controller internal state is now inconsistent. The next eMMC read (loading dtbo_b partition — Device Tree Blob Overlay) returns data from the wrong LBA offset. ABL loads a corrupted or stale DTB overlay (partially from dtbo_a, partially garbage). |


| Step 7 | Corrupted DTB Overlay Applied | ABL applies the corrupted DTB overlay to the base DTB. The overlay contains incorrect memory node entries (wrong DDR region sizes or addresses). ABL does not validate DTB overlay integrity after application (no CRC check on merged DTB). |


| Step 8 | Silent Kernel Crash | ABL passes the corrupted merged DTB to the kernel. Kernel starts, parses DTB, gets wrong memory map. Kernel crashes during early memory initialization (before UART driver is up). Result: silent hang — no kernel logs, no crash dump, appears as APPSBL hang. |



# Debug Steps — Complete Walkthrough (9 Steps)
The following 9 debug steps trace the investigation from initial symptom to root cause confirmation.

| DEBUG STEP 1: Reproduce and Characterize |


### What I Did:
- Set up automated boot loop test (power cycle 1000 times after OTA)
- Connected UART console at 115200 baud
- Enabled maximum ABL debug logging (UEFI DEBUG_INFO level)
- Connected JTAG (Lauterbach Trace32) for post-mortem analysis

### UART Output During Hang (Observations):
| UART Log Output[ABL] Boot slot: _b[ABL] Loading vbmeta_b... OK[ABL] AVB: Verifying boot_b...[ABL] AVB: Rollback index check PASSED[ABL] Loading dtbo_b... OK[ABL] Applying DT overlay... OK[ABL] Loading boot_b... OK[ABL] Starting kernel at 0x80080000...<--- SILENCE - No kernel output ---> |


| Key Observation: ABL reports everything as "OK". The hang appears AFTER ABL hands off to kernel. This immediately tells us the crash is in early kernel, not in ABL itself. |


| DEBUG STEP 2: JTAG Post-Mortem Analysis |


### What I Did:
- Waited for hang to occur, then attached Lauterbach Trace32 JTAG debugger
- Dumped CPU registers and memory to analyze CPU state

### Trace32 Commands and Output:
| T32 JTAG Session; Connect to targetSYStem.CPU CortexA75SYStem.Mode Attach; Check where CPU is stuckRegister.view; Result: PC = 0xFFFFFFC0_08A12340 (kernel virtual address); Check exception levelPER.View "EL" SPR:(0x30,0x00,0x04,0x00,0x02) ; CurrentEL; Result: EL1 (kernel mode - confirms we are IN the kernel); Dump ESR_EL1 (Exception Syndrome Register)PER.View SPR:(0x30,0x00,0x05,0x02,0x00); Result: ESR = 0x96000044 -> Data Abort, Translation fault level 0; Check FAR_EL1 (Fault Address Register)PER.View SPR:(0x30,0x00,0x06,0x00,0x00); Result: FAR = 0x00000001_40000000 -> Accessing address in DDR range |


| JTAG Analysis: The kernel is crashing with a translation fault when accessing DDR address 0x140000000. This means the MMU page tables have no mapping for this address — but this address should be valid DDR! The crash is in kernel mode (EL1) at a kernel virtual address, confirming the kernel did start executing. |


| DEBUG STEP 3: Investigate Memory Map in DTB |


### What I Did:
- Dumped the DTB passed to the kernel (stored at known address 0x82000000) via T32
- Saved DTB binary and decompiled using dtc tool on host machine

| DTB Dump Commands; Dump DTB from DDR (T32)Data.SAVE.Binary "dtb_dump.dtb" 0x82000000--0x82100000# On host machine, decompile DTB:$ dtc -I dtb -O dts dtb_dump.dtb > dtb_dump.dts |


### Finding in the DTB Memory Node:
| DTB Memory Node (Bad Boot)memory { device_type = "memory"; reg = <0x00 0x80000000 0x00 0x40000000>, /* 1GB: 0x8000_0000 - 0xBFFF_FFFF */ <0x00 0xC0000000 0x00 0x40000000>; /* 1GB: 0xC000_0000 - 0xFFFF_FFFF */ /* MISSING: <0x01 0x00000000 0x00 0x80000000> 2GB: 0x1_0000_0000 - 0x1_7FFF_FFFF */}; |


| Critical Finding: The device has 4GB DDR, but the DTB only describes 2GB! The kernel was trying to access memory at 0x1_40000000 which is in the missing 2GB region — hence the translation fault. The DTB overlay was responsible for adding the third memory region for this board variant. |


| DEBUG STEP 4: Compare Good vs Bad DTB |


### What I Did:
- Extracted DTB from a successful boot (saved earlier during good boot run)
- Decompiled both good and bad DTBs and diffed the output

| DTB Comparison Commands# Extract DTB from a successful boot (saved earlier)$ dtc -I dtb -O dts good_dtb.dtb > good.dts$ dtc -I dtb -O dts bad_dtb.dtb > bad.dts$ diff good.dts bad.dts |


| diff Output< reg = <0x00 0x80000000 0x00 0x40000000>,< <0x00 0xC0000000 0x00 0x40000000>,< <0x01 0x00000000 0x00 0x80000000>; <- GOOD: has 3rd region---> reg = <0x00 0x80000000 0x00 0x40000000>,> <0x00 0xC0000000 0x00 0x40000000>; <- BAD: missing 3rd region |


| The bad DTB is missing the third memory region. This pointed directly to the DTB overlay being wrong — the overlay was supposed to add the third memory region for this specific board variant but failed to do so. |


| DEBUG STEP 5: Investigate DTB Overlay Source |


### What I Did:
- Dumped raw dtbo_b partition content from DDR (ABL loads it to a known buffer before applying)
- Parsed the DTBO image on host machine using mkdtboimg tool

| DTBO Dump Commands; In T32, dump the raw dtbo_b partition content from DDR; (ABL loads it to a known buffer before applying)Data.SAVE.Binary "dtbo_raw.bin" 0x9A000000--0x9A100000; On host, parse the DTBO image$ mkdtboimg dump dtbo_raw.bin |


| Finding: The DTBO content had incorrect magic bytes in the header for one of the overlay entries. The CRC of the loaded DTBO did not match the expected CRC from the signed image — confirming data corruption occurred during the eMMC read. |


| DEBUG STEP 6: Trace the eMMC Read Path |


### What I Did:
Added instrumentation to ABL's block I/O layer to log CRC of every read operation:

| ABL Block I/O Instrumentation (C Code)// In UFS/eMMC block read functionEFI_STATUS BlockIoRead(UINT32 Lba, UINT32 Blocks, VOID *Buffer) { EFI_STATUS Status; Status = UfsRead(Lba, Blocks, Buffer); // Debug: Verify read data CRC UINT32 Crc = CalculateCrc32(Buffer, Blocks * BLOCK_SIZE); DEBUG((EFI_D_ERROR, "[BLK] Read LBA=0x%x Blocks=%d CRC=0x%08x Status=%r\n", Lba, Blocks, Crc, Status)); return Status;} |


| Finding: On failing boots, the CRC of the dtbo_b read was DIFFERENT from successful boots, even though Status returned EFI_SUCCESS. The eMMC was returning wrong data silently — a ghost corruption where the driver reports success but delivers bad data. |


| DEBUG STEP 7: Correlate with RPMB Access Timing |


### What I Did:
Added timestamps to RPMB and partition read operations to detect any timing correlation:

| Timestamp InstrumentationDEBUG((EFI_D_ERROR, "[RPMB] Read rollback index: timestamp=%lu\n", GetTimerCountms()));DEBUG((EFI_D_ERROR, "[RPMB] Write rollback index: timestamp=%lu\n", GetTimerCountms()));DEBUG((EFI_D_ERROR, "[BLK] Read dtbo_b: timestamp=%lu\n", GetTimerCountms())); |


### Finding on Failing Boots:
| Failing Boot Timing Log[RPMB] Read rollback index: timestamp=1523[RPMB] Write rollback index: timestamp=1525 <- RPMB write happens[BLK] Read dtbo_b: timestamp=1526 <- dtbo read immediately after! |


| Successful Boot Timing Log[RPMB] Read rollback index: timestamp=1523[RPMB] Write rollback index: timestamp=1525[BLK] Read dtbo_b: timestamp=1533 <- 8ms gap on SUCCESSFUL boot |


| Key Insight: The dtbo_b read was happening within 1ms of the RPMB write on failing boots. On successful boots, there was a 5-10ms gap. The eMMC controller was not completing its partition switch from RPMB back to normal before the next read was issued. |


| DEBUG STEP 8: Root Cause Confirmation with Workaround |


### What I Did:
Added a deliberate 10ms delay after RPMB write before any subsequent eMMC reads:

| Workaround Code// In AVB rollback index update functionStatus = RpmbWriteRollbackIndex(SlotIndex, NewIndex);if (!EFI_ERROR(Status)) { // Workaround: Wait for eMMC controller to stabilize gBS->Stall(10000); // 10ms delay DEBUG((EFI_D_ERROR, "[AVB] Added post-RPMB stabilization delay\n"));} |


| Result: Ran 5000 boot cycles — ZERO failures. Root cause confirmed: the eMMC controller was not completing the RPMB-to-normal-partition transition before the next read was issued. |


| DEBUG STEP 9: Proper Fix Implementation |


### Fix 1: eMMC Driver — Ensure RPMB-to-Normal Partition Transition is Clean
| eMMC Driver Fix (C Code)EFI_STATUS RpmbOperationComplete(VOID) { // After RPMB operation, explicitly switch back to normal partition // and wait for controller ready Status = EmmcSwitchPartition(EMMC_PARTITION_USER); if (EFI_ERROR(Status)) return Status; // Poll device status until ready UINT32 Timeout = 100; // 100ms max while (Timeout--) { Status = EmmcSendStatus(&DeviceStatus); if (!EFI_ERROR(Status) && (DeviceStatus & EMMC_STATUS_READY)) { break; } gBS->Stall(1000); // 1ms } return Status;} |


### Fix 2: ABL — Add DTB Integrity Verification After Overlay Merge
| ABL DTB Validation Fix (C Code)// After applying DT overlayStatus = ApplyDtOverlay(BaseDtb, Overlay);if (!EFI_ERROR(Status)) { // Verify merged DTB integrity UINT32 MergedCrc = CalculateCrc32(BaseDtb, FdtTotalSize(BaseDtb)); if (!ValidateMemoryNodes(BaseDtb)) { DEBUG((EFI_D_ERROR, "[DTB] Memory node validation FAILED! Retrying...\n")); // Retry loading dtbo partition Status = ReloadAndApplyDtOverlay(BaseDtb, SlotSuffix); }} |


# Bug 1 Cross-Questions — Interview Preparation
These are the most likely cross-questions an interviewer will ask after you present this bug. Prepare concise, confident answers.

| Q: How did you narrow down that it was an eMMC issue and not flash corruption?A: I compared the raw flash content (read via JTAG memory-mapped flash access) with what ABL received in its buffer. The flash content was correct — the data was being corrupted during the read operation itself, not at rest in flash. This distinction between storage-at-rest corruption and read-path corruption was the key diagnostic insight. |


| Q: Why did AVB not catch the corrupted DTBO?A: AVB verified the DTBO BEFORE the eMMC issue occurred. The RPMB rollback index update happened AFTER AVB signature verification. The corrupted read happened on the SECOND read of DTBO (for overlay application), not the first read (for signature verification). ABL was reading the partition twice — once for AVB hash verification, once for actual use — and only the second read was affected. |


| Q: Could this happen on UFS devices too?A: UFS has a different architecture — RPMB is accessed through a separate well-defined LUN (Logical Unit Number), and the UFS controller handles partition switching more robustly due to the protocol design. However, a similar class of bug could occur if the UFS driver does not properly serialize RPMB and normal LUN accesses, particularly during back-to-back RPMB operations. |


| Q: How would you prevent this class of bugs in the future?A: Three approaches: 1) Always verify data integrity after reading critical structures (DTB, kernel) — do not just check return codes; 2) Add proper serialization and state machine handling in storage drivers for partition switches, using explicit ready polling; 3) Implement end-to-end CRC verification from flash to consumption point, not just at the AVB layer. Defense-in-depth is essential for critical boot path data. |


| Q: What tools did you use?A: Lauterbach Trace32 for JTAG debugging and post-mortem register/memory analysis to determine the kernel was crashing (not ABL hanging). UART for runtime logs during the boot loop test. Custom C instrumentation added to ABL source for CRC checking and timestamp logging. Standard Linux tools (dtc, mkdtboimg) for DTB/DTBO analysis on the host machine. |


| Q: How did you reproduce a 2% failure in a structured way?A: I set up an automated boot loop test using a relay-controlled power supply and a Python script that monitored UART output. When no kernel output was seen within 5 seconds of the ABL hand-off message, the script logged the failure and continued the test. Running 1000 cycles yielded approximately 20 failures, enough to attach JTAG on a subsequent failure. |



| Interview Presentation Framework for Bug 11. The symptom was: "Intermittent hang after OTA, ~2% repro rate, only on A/B slot switch"2. Initial investigation showed: "JTAG revealed kernel crash (EL1 translation fault), not ABL hang"3. The key insight was: "DTB had wrong memory map, traced to corrupted DTBO read after RPMB write"4. Root cause was: "eMMC RPMB-to-normal partition transition race condition"5. The fix was: "Proper partition switch handling + DTB integrity validation"6. Lessons learned: "Never trust EFI_SUCCESS without data verification; validate critical structures after construction" |


| BUG 2 — INTERMEDIATE LEVEL |


Device Boots to Black Screen After Fastboot Flash Due to Board ID Mismatch in DTB Overlay Selection
# Problem Statement
On a Qualcomm SM6150-based device, after flashing a new boot.img via fastboot, the device exhibited the following behavior:
- Shows the OEM splash screen normally
- ABL UART logs show "Starting kernel..." message
- Screen goes BLACK — no boot animation, no Android UI
- Device does NOT enter recovery or reboot automatically
- The SAME boot.img works perfectly when flashed via full factory image (all partitions together)
- Issue is 100% reproducible when flashing only boot.img via fastboot
- Previous boot.img (older build) works fine

| Why This Bug Is Moderately Difficult1. Misleading symptom: Black screen suggests display driver issue, but root cause is in bootloader DTB selection2. Partial success: Device actually boots to Android — it's just the display that's wrong3. Works in full flash: Full factory image includes matching dtbo.img, masking the issue4. ABL reports success: No errors in ABL logs — DTB selection "succeeded" with a fallback match |


# Root Cause Analysis
The root cause was a board ID mismatch between the new boot image's base DTB and the existing DTBO (Device Tree Blob Overlay) partition.

| What Changed | Details |
| Boot Image | New build updated board revision field from rev 2.0 to rev 2.1 to reflect a minor PCB change |
| DTBO Partition | Was NOT reflashed — developer only flashed boot.img via fastboot, old dtbo.img remained on device |
| DTB Matching | ABL looks for overlay with board_rev=0x21, finds none, falls back to best effort match with board_rev=0x20 |
| Wrong Selection | Fallback overlay configured DSI panel type B instead of correct DSI panel type A (different GPIO, timing, backlight) |
| Result | Kernel boots successfully but display driver initializes wrong panel — no video signal, backlight stays off |


# Debug Steps — Complete Walkthrough (7 Steps)
The following 7 debug steps trace the investigation from initial black screen symptom to confirmed root cause.

| DEBUG STEP 1: Verify the Device Is Actually Booting |


### What I Did:
- Connected USB cable and checked if device shows up via ADB
- If ADB works with a black screen, the device is booted to Android — display-only issue
- Checked kernel dmesg logs for display-related messages

| ADB and Kernel Log Check# Check if device shows up via ADB (even with black screen)$ adb devicesList of devices attachedXXXXXXXX device <- Device IS booting to Android!# Check kernel logs$ adb shell dmesg | head -50[ 0.000000] Booting Linux on physical CPU 0x0000000000 [0x517f803c][ 0.000000] Linux version 4.14.117-perf+ ...[ 2.341567] msm-dsi-display: Panel probe successful |


| Key Finding: The device IS fully booted — Android is running! This immediately shifts investigation from "boot hang" to "display initialization issue." The ADB check is always the first step for a black screen symptom. |


| DEBUG STEP 2: Investigate Display Subsystem via ADB |


### What I Did:
- Checked the display panel info exposed in sysfs
- Verified backlight state and tried forcing it on

| Display Subsystem Investigation# Check display panel detection$ adb shell cat /sys/class/graphics/fb0/msm_fb_panel_infopanel_name=dsi_sim_vid_panel <- WRONG! Should be "dsi_truly_1080p_vid"panel_type=8resolution=1080x2340# Check backlight$ adb shell cat /sys/class/backlight/panel0-backlight/brightness0 <- Backlight is OFF# Try forcing backlight on$ adb shell "echo 255 > /sys/class/backlight/panel0-backlight/brightness"# No change -- wrong backlight controller is being used |


| Key Finding: The kernel detected the WRONG panel (dsi_sim_vid_panel instead of dsi_truly_1080p_vid). Backlight is off because the wrong GPIO is configured. This means the device tree has wrong display configuration — pointing upstream to ABL's DTB overlay selection. |


| DEBUG STEP 3: Compare Device Trees (Good vs Bad Boot) |


### What I Did:
- Extracted the live DTB from both a good and bad boot via /sys/firmware/fdt
- Decompiled both and diffed the display node

| DTB Extraction and Comparison# Extract DTB from running device (bad boot)$ adb shell "cat /sys/firmware/fdt" > bad_fdt.dtb$ dtc -I dtb -O dts bad_fdt.dtb > bad.dts# Extract DTB from a known good boot (saved earlier)$ dtc -I dtb -O dts good_fdt.dtb > good.dts# Compare display nodes$ diff <(grep -A 30 "dsi-display" good.dts) <(grep -A 30 "dsi-display" bad.dts) |


| diff Output (Display Node)< qcom,dsi-display-primary {< qcom,dsi-panel = <&dsi_truly_1080p_vid>; <- GOOD: correct panel< qcom,mdss-dsi-bl-pmic-control-type = "bl_ctrl_wled";< qcom,platform-reset-gpio = <&tlmm 91 0>; <- GPIO 91---> qcom,dsi-display-primary {> qcom,dsi-panel = <&dsi_sim_vid_panel>; <- BAD: wrong panel> qcom,mdss-dsi-bl-pmic-control-type = "bl_ctrl_dcs";> qcom,platform-reset-gpio = <&tlmm 25 0>; <- GPIO 25 (wrong!) |


| Critical Finding: Panel node, backlight controller type, and reset GPIO are ALL wrong. GPIO 91 (correct reset pin) vs GPIO 25 (wrong pin); WLED backlight vs DCS backlight. This confirms the DTB overlay applied a completely different display configuration. |


| DEBUG STEP 4: Investigate DTB Overlay Selection Logic in ABL |


### What I Did:
Added debug logging to ABL's DTB selection code to print all candidate overlays and their match scores:

| ABL DTB Selection Debug Instrumentation// In DeviceTreeLib.c -- DTB matching functionVOID PrintDtbMatchInfo(DtInfo *DtEntry) { DEBUG((EFI_D_ERROR, "[DTB] Candidate: platform=0x%x hw_id=0x%x " "soc_rev=0x%x board_rev=0x%x pmic_rev=0x%x\n", DtEntry->PlatformId, DtEntry->HardwareId, DtEntry->SocRev, DtEntry->BoardRev, DtEntry->PmicRev));}// In the selection loopfor (i = 0; i < DtboCount; i++) { PrintDtbMatchInfo(&DtboEntries[i]); MatchScore = CalculateMatchScore(&DtboEntries[i], &BoardInfo); DEBUG((EFI_D_ERROR, "[DTB] Entry %d: MatchScore=%d\n", i, MatchScore));}DEBUG((EFI_D_ERROR, "[DTB] Selected entry: %d (score=%d)\n", BestIndex, BestScore)); |


### UART Output on Bad Boot:
| ABL DTB Selection Log[DTB] Board Info: platform=0x153 hw_id=0x0A soc_rev=0x10 board_rev=0x21 pmic_rev=0x20[DTB] Candidate 0: platform=0x153 hw_id=0x0A soc_rev=0x10 board_rev=0x20 pmic_rev=0x20[DTB] Entry 0: MatchScore=4 <- board_rev mismatch (0x20 vs 0x21), partial match[DTB] Candidate 1: platform=0x153 hw_id=0x0B soc_rev=0x10 board_rev=0x20 pmic_rev=0x20[DTB] Entry 1: MatchScore=3 <- hw_id AND board_rev mismatch[DTB] Candidate 2: platform=0x153 hw_id=0x0A soc_rev=0x10 board_rev=0x20 pmic_rev=0x30[DTB] Entry 2: MatchScore=3 <- board_rev AND pmic_rev mismatch[DTB] Selected entry: 0 (score=4) <- Best available, but NOT exact match! |


| Key Finding: No overlay has board_rev=0x21. The best match is entry 0 with board_rev=0x20. This is the overlay for the OLD board revision with a DIFFERENT display panel configuration. ABL silently selected the wrong overlay with no warning to the user. |


| DEBUG STEP 5: Verify the DTBO Partition Content |


### What I Did:
- Dumped the dtbo partition directly from the device via ADB
- Pulled the binary to host and parsed it with mkdtboimg

| DTBO Partition Analysis# Dump dtbo partition$ adb shell "dd if=/dev/block/bootdevice/by-name/dtbo of=/data/local/tmp/dtbo.img"$ adb pull /data/local/tmp/dtbo.img# Parse DTBO image structure$ mkdtboimg dump dtbo.img dt_table_header: dt_entry_count: 3 dt_entry [0]: <- This is what got selected! board_rev: 0x20 custom: 0x0A (display variant A -- truly panel, but OLD config) dt_entry [1]: board_rev: 0x20 custom: 0x0B (display variant B -- sim panel) dt_entry [2]: board_rev: 0x20 custom: 0x0A (display variant A -- different PMIC) |


| Key Finding: ALL DTBO entries have board_rev=0x20. The new boot.img expects board_rev=0x21. This confirms: the dtbo partition was not updated when boot.img was flashed. The version mismatch is the direct root cause. |


| DEBUG STEP 6: Root Cause Confirmation |


### What I Did:
Flashed the matching dtbo.img from the new build alongside boot.img and rebooted:

| Root Cause Confirmation# Flash the matching dtbo.img from the new build$ fastboot flash dtbo dtbo_new.img$ fastboot reboot |


| Result: Device booted with CORRECT display — full Android UI, correct backlight on GPIO 91, correct panel initialized. Root cause 100% confirmed: mismatched dtbo partition. |


| DEBUG STEP 7: Additional Verification of New DTBO |


| New DTBO Verification# Verify new DTBO has board_rev=0x21$ mkdtboimg dump dtbo_new.img dt_entry [0]: board_rev: 0x21 <- Now matches boot.img! custom: 0x0A dt_entry [1]: board_rev: 0x21 custom: 0x0B |


# Fix Implementation — Three-Pronged Approach
### Fix 1: ABL — Fail Loudly on DTB Mismatch Instead of Silent Fallback
| ABL Warning on DTB Mismatch// In DTB selection logicif (BestScore < EXACT_MATCH_SCORE) { DEBUG((EFI_D_ERROR, "[DTB] WARNING: No exact DTB overlay match found!\n")); DEBUG((EFI_D_ERROR, "[DTB] Expected board_rev=0x%x, best match=0x%x\n", BoardInfo.BoardRev, DtboEntries[BestIndex].BoardRev)); // Option A: Display warning on screen and continue (production) DisplayDtbMismatchWarning(); // Option B: Halt boot in engineering builds #ifdef ENGINEERING_BUILD DEBUG((EFI_D_ERROR, "[DTB] FATAL: DTB mismatch in eng build. Halting.\n")); CpuDeadLoop(); #endif} |


### Fix 2: Build System — Enforce DTBO Version Dependency
| Build System Version Check (Makefile)# In OTA/flash script, add version checkBOOT_DTB_VERSION := $(shell extract_board_rev boot.img)DTBO_VERSION := $(shell extract_board_rev dtbo.img)flash_boot: @if [ "$(BOOT_DTB_VERSION)" != "$(DTBO_VERSION)" ]; then \ echo "ERROR: boot.img board_rev != dtbo board_rev"; \ echo "Please flash dtbo.img as well: fastboot flash dtbo dtbo.img"; \ exit 1; \ fi fastboot flash boot boot.img |


### Fix 3: Improve DTB Match Scoring to Weight Critical Fields
| Improved DTB Match Scoring AlgorithmUINT32 CalculateMatchScore(DtInfo *Entry, BoardInfo *Board) { UINT32 Score = 0; if (Entry->PlatformId == Board->PlatformId) Score += 10; if (Entry->HardwareId == Board->HardwareId) Score += 10; if (Entry->SocRev == Board->SocRev) Score += 5; if (Entry->BoardRev == Board->BoardRev) Score += 8; // Higher weight if (Entry->PmicRev == Board->PmicRev) Score += 3; // CRITICAL: If board_rev does not match, penalize heavily // to prevent selecting overlay for wrong board variant if (Entry->BoardRev != Board->BoardRev) { Score -= 15; // Strong penalty for board rev mismatch DEBUG((EFI_D_WARN, "[DTB] Board rev mismatch penalty applied\n")); } return Score;} |


# Bug 2 Cross-Questions — Interview Preparation
The following cross-questions will likely come up when presenting this bug. Master these to demonstrate depth of knowledge.

| Q: How did you know to check ADB if the screen was black?A: Standard debugging practice — a black screen does not necessarily mean the device is not booting. I always check ADB connectivity first to determine if the issue is display-specific or a full boot failure. If the device appears on ADB, the problem is isolated to display or UI layer. This single check reduced the investigation space from the entire boot chain to just display initialization. |


| Q: What if ADB was not available?A: I would check UART logs to see if the kernel booted successfully. If UART shows kernel reaching init, the issue is in display initialization. If no UART output after ABL handoff, I would use JTAG to check the CPU state — if the CPU is executing kernel code (PC in kernel virtual address range, EL1), the device is booted and the problem is purely in display. |


| Q: Why did the developer not flash dtbo along with boot.img?A: This is a very common workflow issue. Developers often flash only the partition they changed — typically boot.img for kernel or bootloader changes. They do not realize that a board ID change in the base DTB requires a matching dtbo update. This is precisely why the build system fix is critical — it catches the version mismatch at flash time before the developer even tries to boot the device. |


| Q: Could this cause data corruption or security issues?A: In this case, no — it was only a display misconfiguration. However, in theory, a wrong DTB overlay could configure incorrect memory regions (like the Bug 1 scenario we discussed), wrong peripheral addresses that could cause memory-mapped I/O to wrong addresses, or disable security features like ARM TrustZone memory protection. That is why the ABL fix to warn on mismatch is critical for catching potentially dangerous misconfigurations, not just display issues. |


| Q: How is this different from a kernel display driver bug?A: The kernel display driver was working correctly — it initialized exactly the panel described in the DTB it received. The problem was upstream in the bootloader: ABL selected the wrong DTBO overlay, which gave the kernel a DTB describing the wrong panel. Fixing the kernel driver would not help at all. This distinction is important because it demonstrates understanding of the full boot chain and the DTB as the hardware description interface between bootloader and kernel. |


| Q: What is the difference between DTB and DTBO?A: DTB (Device Tree Blob) is the base device tree compiled alongside the kernel — it describes the SoC hardware that is common across all board variants (CPU, interconnects, standard peripherals). DTBO (Device Tree Blob Overlay) is a separate partition containing board-specific modifications — which display panel, which sensors, GPIO assignments, connector presence/absence for a specific board variant. ABL merges the base DTB with the correct DTBO to create the final device tree passed to the kernel. This separation allows one kernel image to support multiple board hardware variants. |


| Q: What happens during the DTB overlay merging process?A: ABL reads the base DTB (embedded in boot.img or a separate dtb partition), then reads the DTBO partition and iterates through its overlay entries to find the best matching entry for the current board (by matching platform_id, hardware_id, board_rev, etc.). The selected overlay is then applied to the base DTB using the FDT library's overlay application logic, which follows the DT overlay specification — adding nodes, modifying properties, and applying fragment nodes from the overlay to the base tree. The result is the merged DTB passed to the kernel. |



| Interview Presentation Framework for Bug 21. "The symptom was: After flashing a new boot.img via fastboot, the device showed the splash screen but went to black screen. No crash, no reboot."2. "Initial investigation showed: ADB worked — device was fully booted to Android. Issue was only with the display. Kernel had detected the wrong display panel."3. "The key insight was: DTB comparison revealed wrong panel node. This pointed to DTB overlay selection issue in ABL."4. "Root cause was: New boot.img had updated board revision ID (2.1 vs 2.0), dtbo partition was not reflashed. ABL fell back to wrong overlay."5. "Fix was three-pronged: ABL warns on mismatch, build system enforces version consistency, improved DTB match scoring." |


| QUICK REFERENCE SUMMARY |


# Tools and Techniques Reference
| Tool / Technique | Bug 1 Application | Bug 2 Application |
| JTAG (Trace32) | Post-mortem: read PC, ESR_EL1, FAR_EL1 to confirm kernel crash and fault address | Not needed — ADB available since device booted |
| UART Logs | Confirmed ABL reported OK on all steps; no output after kernel jump = early kernel crash | Captured DTB selection log with match scores to identify fallback selection |
| ADB Shell | Not available (device hung before Android) | First-line triage: confirmed device booted; checked panel info, backlight state |
| dtc (DT compiler) | Decompiled good vs bad DTB dumps from JTAG; revealed missing memory node | Decompiled /sys/firmware/fdt DTBs; found wrong display panel node |
| mkdtboimg | Parsed raw DTBO data dumped from DDR via JTAG; found corrupted CRC | Parsed on-device dtbo partition; confirmed all entries had board_rev=0x20 |
| ABL Instrumentation | Added CRC check on every block read; added timestamps on RPMB and eMMC operations | Added DTB selection debug logging to print match scores and board info |
| Boot Loop Test | 1000-cycle automated test to capture 2% failure; confirmed fix with 5000-cycle test | 100% reproducible — no automation needed; one flash cycle is enough |


# Debugging Methodology Comparison
| Phase | Bug 1 — DIFFICULT Approach | Bug 2 — INTERMEDIATE Approach |
| 1. Initial Triage | Automated 1000-cycle boot loop with UART monitoring to capture the 2% failure | ADB devices immediately to confirm device booted despite black screen |
| 2. Locate Crash | JTAG post-mortem: PC in kernel address space + ESR_EL1 = kernel crash confirmed | sysfs panel info: wrong panel name confirmed display-layer problem |
| 3. Isolate Component | DTB memory node diff: missing region traced to wrong DTBO overlay | DTB display node diff: wrong GPIO and panel traced to wrong DTBO overlay |
| 4. Find Root Cause | Timestamp correlation: RPMB write followed by eMMC read within 1ms = race condition | ABL match log: board_rev=0x21 expected, only 0x20 available = version mismatch |
| 5. Confirm Fix | 5000-cycle boot loop with delay patch: zero failures confirms root cause | Flash dtbo_new.img alongside boot.img: device displays correctly |


# Key Takeaways for Interviews
- Always check ADB first for black screen issues — a booted device narrows investigation scope dramatically
- Never trust EFI_SUCCESS return codes alone on storage operations — verify data integrity with CRC
- JTAG is essential for bugs that appear to be ABL hangs but are actually early kernel crashes
- DTB overlay mismatch is a common issue in multi-variant platforms; version tracking between boot.img and dtbo.img is critical
- Intermittent bugs require automation — a boot loop test with 1000+ cycles is necessary for low-probability failures
- Storage driver bugs (eMMC, UFS) can manifest as silent data corruption; always add defense-in-depth validation in ABL
- ABL should fail loudly on configuration mismatches rather than silently selecting a best-effort fallback

