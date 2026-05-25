Part 2 - U-Boot Verified Boot
Chain of Trust, FIT Images, RSA Signing, Rollback Protection & Platform Integration
This document provides a comprehensive technical reference for U-Boot Verified Boot — the cryptographic framework that ensures only trusted software executes on embedded Linux devices. Coverage includes FIT image format, key generation and signing workflows, internal verification flow, rollback protection, SPL verified boot, and platform-specific integration for Qualcomm and i.MX6.
Section 1: What Is U-Boot Verified Boot
U-Boot Verified Boot is a chain-of-trust mechanism that cryptographically validates each firmware/software component before executing it. The standard is defined by the FIT (Flattened Image Tree) format combined with RSA signature verification in U-Boot.
1.1 Purpose and What It Prevents
Verified Boot ensures that only cryptographically signed and trusted software runs on the device. It prevents:
- Malicious firmware injection — attackers cannot replace kernel, DTB, or ramdisk with malicious versions
- Rollback attacks — downgrading to older, vulnerable firmware versions is blocked by hardware monotonic counters
- Unauthorized kernel or rootfs modifications — any tampered component fails signature verification and boot is halted
- Supply chain attacks — even if storage is compromised, unsigned images will not boot
1.2 Chain of Trust — ROM to dm-verity
Each stage verifies the next stage using a public key embedded in the previous stage. The chain starts at the immutable ROM and extends to the root filesystem:
| ROM (immutable, on-chip) | verifies vSPL / TPL (Secondary Program Loader) | verifies vU-Boot proper (u-boot.bin) | verifies vFIT Image (kernel + DTB + initrd) | verifies vLinux Kernel | verifies (optionally) vdm-verity --> Root Filesystem |

The key principle is that each stage's verification public key is embedded ("baked in") to the previous stage's binary during the build process — making it tamper-evident. If any link in this chain is broken, the boot process halts.
Section 2: FIT Image (Flattened Image Tree)
FIT (Flattened Image Tree) is a single container image defined by a .its (Image Tree Source) file. It bundles all boot components (kernel, DTB, ramdisk) together with their cryptographic hashes and signatures. The format is based on the Device Tree Blob (DTB) format.
2.1 Complete .its File Example
The following .its file defines a complete FIT image with kernel, device tree blob, ramdisk, SHA256 hashes for each component, and an RSA-2048 + SHA256 signature over the configuration:
| /dts-v1/;/ { description = "Kernel + DTB + Initrd with signature"; #address-cells = <1>; images { kernel@1 { description = "Linux Kernel"; data = /incbin/("arch/arm64/boot/Image"); type = "kernel"; arch = "arm64"; os = "linux"; compression = "none"; load = <0x80080000>; entry = <0x80080000>; hash@1 { algo = "sha256"; }; }; fdt@1 { description = "Device Tree Blob"; data = /incbin/("arch/arm64/boot/dts/qcom/sdm845.dtb"); type = "flat_dt"; arch = "arm64"; compression = "none"; hash@1 { algo = "sha256"; }; }; ramdisk@1 { description = "Initial Ramdisk"; data = /incbin/("initrd.img"); type = "ramdisk"; arch = "arm64"; os = "linux"; compression = "gzip"; hash@1 { algo = "sha256"; }; }; }; configurations { default = "conf@1"; conf@1 { description = "Boot config"; kernel = "kernel@1"; fdt = "fdt@1"; ramdisk = "ramdisk@1"; signature@1 { algo = "sha256,rsa2048"; key-name-hint = "dev"; sign-images = "kernel", "fdt", "ramdisk"; }; }; };}; |

2.2 Full Explanation of Each Node
images{} — Component Declarations
The images node contains one or more sub-nodes, each describing a binary component to be loaded at boot:
- kernel@1 — The Linux kernel image. Specifies binary path (via /incbin/), target architecture, OS type, load and entry point addresses in physical memory.
- fdt@1 — The Device Tree Blob (DTB). Contains hardware description that the kernel uses to enumerate peripherals. Referenced by conf@1 configuration.
- ramdisk@1 — The initial ramdisk (initrd/initramfs). Used for early userspace before the real root filesystem is mounted. Can be compressed with gzip, lzma, or lz4.
hash@1 within each image — Per-Component Integrity
Each image node contains a hash sub-node specifying the hash algorithm. When mkimage builds the FIT image, it computes the hash of each component and stores it in this node. During verification, U-Boot recomputes and compares these hashes before the RSA signature check.
configurations{} — Boot Configurations
The configurations node allows multiple named boot configurations in a single FIT image. The default property specifies which configuration to boot. Each configuration references which kernel, fdt, and ramdisk images to use. The signature is applied at the configuration level, covering all referenced images together.
signature@1 — The Cryptographic Signature
The signature node within a configuration defines the signing algorithm and which images are covered:
- algo = "sha256,rsa2048" — SHA256 hash of all listed images, then RSA-2048 signature over that hash
- key-name-hint = "dev" — The key name to use when verifying. U-Boot looks for a key node named "key-dev" in its own device tree.
- sign-images — Comma-separated list of image properties to include in the signature hash. All three components (kernel, fdt, ramdisk) are signed together.
Section 3: Key Generation and Signing Flow
The verified boot signing workflow involves three steps: generating an RSA key pair, building and signing the FIT image, and rebuilding U-Boot with the public key embedded. This section covers all three steps with full commands and explanations.
3.1 Step 1: Generate RSA Key Pair
Generate a 2048-bit RSA private key and extract the corresponding public key certificate. The private key is used for signing (kept offline/in HSM); the public key certificate is embedded into U-Boot during build.
| # Create keys directorymkdir -p keys/# Generate 2048-bit RSA private key (F4 = 0x10001 public exponent)openssl genrsa -F4 -out keys/dev.key 2048# Extract public key certificate (X.509 self-signed)openssl req -batch -new -x509 -key keys/dev.key \ -out keys/dev.crt -days 3650 \ -subj "/CN=Verified Boot Dev Key/"# Verify the key pairopenssl rsa -in keys/dev.key -checkopenssl x509 -in keys/dev.crt -text -noout |

Important security considerations:
- The private key (dev.key) must be kept secure — never embed it in the device or store it in source control
- In production, use an HSM (Hardware Security Module) or dedicated signing server
- Use at least RSA-2048; prefer RSA-4096 for new designs
- The certificate validity period (3650 days = 10 years) should be set to the product lifetime
3.2 Step 2: Build FIT Image and Sign It
Use mkimage (from U-Boot tools) to create an unsigned FIT image from the .its source file, then sign it. The critical -K flag embeds the public key into U-Boot's device tree during this step:
| # Step 1: Create unsigned FIT image from .its sourcemkimage -f kernel.its kernel.itb# Step 2: Sign the FIT image AND embed public key into u-boot.dtb# -F = modify existing FIT image (add signatures)# -k = directory containing key files (dev.key, dev.crt)# -K = output DTB to embed public key into (U-Boot's own DTB)# -r = mark key as "required" (boot fails if signature missing)mkimage -F -k keys/ -K u-boot.dtb -r kernel.itb# Verify the signed FIT imagemkimage -l kernel.itb# Example output after signing:# FIT description: Kernel + DTB + Initrd with signature# Created: Mon Jan 1 00:00:00 2024# Image 0 (kernel@1)# Description: Linux Kernel# Type: Kernel Image# Compression: uncompressed# Hash algo: sha256# Hash value: 3d4a2e...# Configuration 0 (conf@1)# Sign algo: sha256,rsa2048:dev# Signature: OK |

What -K u-boot.dtb Does
The -K flag is the most critical part of the signing workflow. It instructs mkimage to:
- Extract the RSA public key from the certificate (keys/dev.crt)
- Precompute Montgomery multiplication parameters (n0-inverse, R-squared) for fast hardware-free RSA verification
- Write these parameters as a new node ("key-dev") into u-boot.dtb — U-Boot's own device tree blob
- Mark the key as "required = conf" meaning U-Boot will refuse to boot any FIT image that is not signed with this key
3.3 Step 3: Build U-Boot with Public Key Embedded
After mkimage modifies u-boot.dtb with the embedded public key, rebuild U-Boot so the updated DTB is compiled into the final U-Boot binary:
| # Set cross-compile toolchain for ARM64 targetexport CROSS_COMPILE=aarch64-linux-gnu-# Configure U-Boot for target boardmake <board>_defconfig# Enable verified boot options (or set in defconfig)make menuconfig# Ensure: CONFIG_FIT=y, CONFIG_FIT_SIGNATURE=y, CONFIG_RSA=y# CONFIG_REQUIRE_SIGNATURE=y, CONFIG_SHA256=y# The u-boot.dtb was already updated by mkimage -K# Now rebuild U-Boot — the DTB with embedded public key gets compiled inmake CROSS_COMPILE=aarch64-linux-gnu- u-boot.bin# The resulting u-boot.bin now contains the public key# and will ONLY boot FIT images signed with the corresponding private key# Verify the public key was embedded:fdtdump u-boot.dtb | grep -A 20 "signature" |

Section 4: U-Boot Verified Boot Internal Flow
Understanding the internal execution flow is essential for debugging verification failures and for customizing verification behavior. The flow starts at the bootm command and descends through several layers of verification.
4.1 bootm Flow Diagram
When the bootm command is invoked to boot a FIT image, the following call chain executes:
| bootm <fit_image_addr> | +-- fit_image_verify_required_sigs() | | | +-- Read signature node from FIT image DTB | +-- Locate public key in U-Boot's own DTB (u-boot.dtb) | | by key-name-hint (e.g., "dev" --> "key-dev" node) | +-- For each signed image (kernel, fdt, ramdisk): | | +-- Recompute SHA256 hash | | +-- Compare with stored hash node | +-- RSA verify: signature == RSA(SHA256(kernel+fdt+ramdisk)) | | | +-- [PASS] --> continue boot | +-- [FAIL] --> "Signature check failed" --> HALT | +-- fit_image_load() --> decompress + load kernel to RAM +-- fit_image_load() --> load DTB +-- fit_image_load() --> load ramdisk | +-- do_bootm_linux() --> jump to kernel entry point (x0 = DTB physical address on ARM64) |

4.2 fit_image_verify_required_sigs() Flow
The core verification function iterates over all keys in U-Boot's control FDT and verifies that required signatures are present and valid:
| /* cmd/bootm.c / common/image-fit.c */ int fit_image_verify_required_sigs(const void *fit, int image_noffset, const void *sig_blob){ int node; int count = 0; /* Iterate over all key nodes in U-Boot control DTB */ fdt_for_each_subnode(node, sig_blob, sig_node) { const char *required; required = fdt_getprop(sig_blob, node, "required", NULL); /* Only check keys marked as "required" */ if (!required || strcmp(required, "conf")) continue; /* Verify this signature requirement */ ret = fit_config_verify_sig(fit, noffset, sig_blob, node); if (ret) { printf("Signature check failed for key '%s'\n", fdt_get_name(sig_blob, node, NULL)); return ret; } count++; } /* If required keys exist but NONE verified = fail */ if (!count && CONFIG_IS_ENABLED(REQUIRE_SIGNATURE)) return -EPERM; return 0;} |

Section 5: U-Boot Source Code — Signature Verification
This section examines the core RSA verification code in U-Boot and shows exactly where the public key is stored and how it is structured in the device tree blob.
5.1 Core rsa_verify() Function
The primary verification entry point is rsa_verify() in lib/rsa/rsa-verify.c. It locates the public key, computes the hash of image regions, then performs RSA PKCS#1 v1.5 verification:
| /* lib/rsa/rsa-verify.c */ int rsa_verify(struct image_sign_info *info, const struct image_region region[], int region_count, uint8_t *sig, uint sig_len){ struct key_prop prop; int ret; /* 1. Find public key in U-Boot's control DTB (u-boot.dtb) */ /* key-name-hint in FIT maps to "key-<name>" node in U-Boot DTB */ ret = rsa_get_pub_key(info->fdt_blob, info->keyname, &prop); if (ret) { pr_err("RSA: public key '%s' not found\n", info->keyname); return ret; } /* 2. Compute SHA256 hash of all image regions */ /* regions = {kernel_data, fdt_data, ramdisk_data} */ ret = hash_calculate(info->checksum->name, region, region_count, info->fit_value); if (ret) { pr_err("RSA: hash calculation failed\n"); return ret; } /* 3. RSA PKCS#1 v1.5 signature verification */ /* Using Montgomery multiplication for efficiency */ ret = rsa_verify_key(&prop, sig, sig_len, info->fit_value, info->checksum); if (ret) { pr_err("RSA: signature verification failed: %d\n", ret); return ret; } return 0;} |

5.2 rsa_verify_key() — The Montgomery RSA Implementation
The actual modular exponentiation uses precomputed Montgomery parameters stored alongside the key in the DTB, avoiding the need for a floating-point or hardware divide unit:
| /* lib/rsa/rsa-mod-exp.c */ int rsa_verify_key(struct key_prop *prop, const uint8_t *sig, const uint32_t sig_len, const uint8_t *hash, const void *algo){ /* Montgomery arithmetic parameters */ uint32_t *result, *sig_tmp; /* m_RR = R^2 mod n (precomputed, stored in DTB) */ /* n0inv = -(1/n) mod 2^32 (precomputed, stored in DTB) */ /* Convert signature to Montgomery domain */ montgomery_mul(result, sig_tmp, prop->rr, 1, prop->modulus, prop->n0inv, prop->num_bits); /* Modular exponentiation: result = sig^e mod n */ /* e = 65537 (0x10001) for RSA-F4 */ for (i = 0; i < RSA_E_BITS; i++) { montgomery_mul(result, result, result, /* square */ result, prop->modulus, prop->n0inv, prop->num_bits); if (e & (1 << i)) montgomery_mul(result, result, sig_tmp, /* multiply */ result, prop->modulus, prop->n0inv, prop->num_bits); } /* PKCS#1 v1.5 padding verification */ return rsa_verify_padding(result, prop->num_bits/8, algo);} |

5.3 Where the Public Key Lives in U-Boot DTB
After running "mkimage -K u-boot.dtb", the public key is stored as a DTS node under /signature/. U-Boot reads this node at runtime to perform verification:
| /* u-boot.dtb after mkimage -K embeds the key */ /* View with: fdtdump u-boot.dtb */ / { signature { key-dev { required = "conf"; /* "conf" = required for configuration */ /* "image" = required per image */ algo = "sha256,rsa2048"; rsa,num-bits = <2048>; rsa,modulus = < 0xab12cd34 0xef567890 ... /* 512 bytes of RSA modulus */ ... /* 64 uint32 values = 256 bytes = 2048 bits */ >; rsa,exponent = <0x00 0x01 0x00 0x01>; /* 65537 = 0x10001 */ rsa,r-squared = < 0x12345678 ... /* R^2 mod n, precomputed by mkimage */ >; rsa,n0-inverse = <0x87654321>; /* -(1/n) mod 2^32 */ }; };}; |

Key observations about this structure:
- The modulus (n) and exponent (e) constitute the RSA public key — no private data is stored here
- rsa,r-squared and rsa,n0-inverse are Montgomery precomputed values — they speed up verification by 10-100x
- The node name "key-dev" corresponds to key-name-hint = "dev" in the FIT image signature node
- Multiple keys can coexist — each as a separate "key-<name>" node under /signature/
Section 6: CONFIG Options for Verified Boot
Verified boot behavior is controlled through U-Boot Kconfig options. The following table and descriptions cover all critical configuration options required for a fully functional and enforced verified boot implementation.
| CONFIG Option | Value | Description |
| CONFIG_FIT | y | Enable FIT (Flattened Image Tree) image support. Required for all FIT-based boot. Without this, only legacy uImage format is supported. |
| CONFIG_FIT_SIGNATURE | y | Enable RSA signature verification for FIT images. This is the core option that enables the verification engine. Depends on CONFIG_FIT, CONFIG_RSA, and CONFIG_SHA256. |
| CONFIG_FIT_VERBOSE | y | Print verbose output during FIT image loading and verification — hash values, key names, signature bytes. Essential for debugging. Disable in production to save code space. |
| CONFIG_RSA | y | Enable RSA cryptographic library in U-Boot. Provides the modular exponentiation and PKCS#1 v1.5 padding verification code. |
| CONFIG_RSA_SOFTWARE_EXP | y | Use software Montgomery arithmetic for RSA exponentiation. Alternative: CONFIG_RSA_HW_ACCEL for platforms with hardware crypto accelerators (Qualcomm CE, i.MX CAAM). |
| CONFIG_SHA256 | y | Enable SHA-256 hash algorithm. Required for the default sha256,rsa2048 signature algorithm. Can also enable CONFIG_SHA384, CONFIG_SHA512 for stronger hashing. |
| CONFIG_REQUIRE_SIGNATURE | y | CRITICAL SECURITY OPTION. Refuse to boot any FIT image that is not properly signed. Without this, U-Boot will verify signatures when present but happily boot unsigned images. Must be y in production. |
| CONFIG_FIT_ROLLBACK_PROTECT | y | Enable anti-rollback protection via rollback-index in FIT image configurations. Checks the image rollback-index against a hardware monotonic counter. Prevents downgrade to vulnerable versions. |
| CONFIG_FIT_ENABLE_SHA256_SUPPORT | y | Explicitly enable SHA256 in the FIT hash library (separate from the core SHA256 algorithm). Required for FIT image hash verification. |
| CONFIG_SECURE_BOOT | y | Platform-specific secure boot enforcement. On i.MX: triggers HAB verification. On Qualcomm: checks QFPROM fuses. This is separate from verified boot but complements it. |


Example defconfig snippet for a production device:
| # Required for verified bootCONFIG_FIT=yCONFIG_FIT_SIGNATURE=yCONFIG_FIT_VERBOSE=y# Cryptographic primitivesCONFIG_RSA=yCONFIG_RSA_SOFTWARE_EXP=yCONFIG_SHA256=yCONFIG_SHA384=y# SECURITY CRITICAL: Enforce signatureCONFIG_REQUIRE_SIGNATURE=y# Rollback protectionCONFIG_FIT_ROLLBACK_PROTECT=y# Platform secure boot (i.MX example)CONFIG_SECURE_BOOT=yCONFIG_IMX_HAB=y |

Section 7: Rollback Protection
Rollback protection prevents an attacker from downgrading firmware to an older, vulnerable version. It combines a version counter in the FIT image with a hardware monotonic counter that can only be incremented, never decremented.
7.1 FIT Image rollback-index
The rollback-index property is added to the configuration node in the .its file. This number must be monotonically increasing with each new firmware release:
| /* In FIT image .its file */ configurations { default = "conf@1"; conf@1 { description = "Boot config v5"; kernel = "kernel@1"; fdt = "fdt@1"; ramdisk = "ramdisk@1"; /* Anti-rollback version counter */ /* Must be >= value in hardware fuse */ rollback-index = <5>; signature@1 { algo = "sha256,rsa2048"; key-name-hint = "dev"; sign-images = "kernel", "fdt", "ramdisk"; }; };}; |

7.2 fit_check_rollback() Code
U-Boot's rollback check reads the rollback-index from the FIT image and compares it against the hardware counter. If the image version is older than the hardware counter, boot is rejected:
| /* common/image-fit.c */ int fit_check_rollback(const void *fit, int conf_noffset){ uint32_t fit_version, hw_version; int ret; /* Read version from FIT image configuration node */ fit_version = fdt_getprop_u32(fit, conf_noffset, "rollback-index"); if (fit_version == (uint32_t)-1) { /* No rollback-index in FIT = treat as version 0 */ fit_version = 0; } /* Read current rollback counter from hardware */ ret = read_rollback_counter_from_fuse(&hw_version); if (ret) { pr_err("Rollback: failed to read HW counter\n"); return ret; } pr_debug("Rollback: image=%u, fuse=%u\n", fit_version, hw_version); /* Reject if image version is older than fuse counter */ if (fit_version < hw_version) { pr_err("Rollback attack detected! image=%u, fuse=%u\n", fit_version, hw_version); return -EPERM; } /* Optionally: ratchet the counter forward */ /* (only do this after successful boot, in kernel) */ return 0;} |

7.3 Hardware Rollback Counter Storage
The rollback counter must be stored in hardware that cannot be decremented — one-time programmable (OTP) fuses or RPMB. Different platforms provide different mechanisms:
| Platform | Storage | Implementation |
| i.MX6/8 | OCOTP (On-Chip OTP) | NXP On-Chip OTP Controller. Fuse words dedicated to rollback counter. Each bit can be blown once (0->1). Counter = number of bits blown. Max counter = number of fuse bits. |
| Qualcomm | QFPROM | Qualcomm Fuse ROM. ANTI_ROLLBACK fuse rows per image type (XBL, TZ, HYP, ABL). Blown by TZ via SCM call during manufacturing or first boot after update. |
| Generic | RPMB (eMMC/UFS) | Replay Protected Memory Block in eMMC or UFS. Requires Keymaster TA (TrustZone) to protect. Supports arbitrary counter values. No limit but requires TZ to be functional. |
| Generic | TEE Secure Storage | OP-TEE or QSEE secure storage. Counter stored encrypted and integrity-protected in normal storage, with keys in TZ. Flexible but complex. |


Example: Reading/writing OCOTP rollback counter on i.MX6:
| /* drivers/misc/imx_ocotp.c */ #define OCOTP_ROLLBACK_FUSE_ADDR 0x460 /* Bank 1, Word 7 */ int read_rollback_counter_from_fuse(uint32_t *version){ uint32_t fuse_val; /* Read fuse word */ fuse_val = readl(OCOTP_BASE + OCOTP_ROLLBACK_FUSE_ADDR); /* Count number of set bits = rollback counter value */ *version = __builtin_popcount(fuse_val); return 0;}int bump_rollback_counter(uint32_t new_version){ uint32_t current, new_fuse; current = readl(OCOTP_BASE + OCOTP_ROLLBACK_FUSE_ADDR); /* Set new_version bits (cannot clear blown bits) */ new_fuse = current | ((1 << new_version) - 1); ocotp_write_fuse(OCOTP_ROLLBACK_FUSE_ADDR, new_fuse); return 0;} |

Section 8: SPL Verified Boot
For a complete chain of trust, SPL (Secondary Program Loader) must also verify U-Boot proper before executing it. SPL verified boot extends the chain from ROM all the way down to the kernel, with no unverified gap.
8.1 spl_fit_verify_signature() Code
The SPL has its own signature verification code, similar to U-Boot's but stripped down for the limited code space available in SRAM. SPL has its own embedded public key — distinct from the key used by U-Boot to verify the kernel FIT image:
| /* common/spl/spl_fit.c */ static int spl_fit_verify_signature(const void *fit, int images_noffset){ struct image_sign_info info; int ret; /* SPL has its own embedded public key (different from U-Boot) */ /* This key was embedded by: mkimage -F -k keys/ -K spl.dtb -r u-boot.itb */ info.fdt_blob = gd->fdt_blob; /* SPL's own DTB with embedded key */ info.keyname = "spl-key"; /* Must match key-name-hint in FIT */ info.required = true; /* Verify the FIT configuration signature */ ret = fit_config_verify(fit, &info); if (ret) { puts("SPL: U-Boot signature verification FAILED!\n"); /* Hang or enter recovery mode */ hang(); } puts("SPL: U-Boot signature OK\n"); return 0;} |

8.2 SPL Key Hierarchy Diagram
The SPL key hierarchy creates a full cryptographic chain from ROM through to the kernel. Each level verifies the next using a public key that was embedded during the build of the previous level:
| ROM public key (burned into SoC OTP fuses at manufacturing) | ROM verifies vSPL (Secondary Program Loader) | SPL contains: embedded public key for verifying U-Boot | SPL verifies vU-Boot proper (u-boot.itb = FIT image of U-Boot) | U-Boot contains: embedded public key for verifying kernel | U-Boot verifies vKernel FIT image (kernel.itb) | Contains: kernel + DTB + ramdisk + signatures vLinux Kernel | vdm-verity (block-level integrity of root filesystem) |

Key Separation Between Levels
It is considered best practice to use different key pairs for each level of the hierarchy:
- ROM-level key pair: Generated once, hash burned into SoC fuses at manufacturing. Extremely high security. If compromised, device is permanently compromised.
- SPL-level key pair (verifies U-Boot): Can be separate from ROM key. Allows U-Boot rollover without affecting ROM verification anchor.
- U-Boot-level key pair (verifies kernel): Can be updated independently of SPL/ROM keys. Allows independent kernel signing key rotation.
Section 9: Platform-Specific Integration
Verified boot integrates differently on different SoC platforms. The core FIT+RSA mechanism is portable, but the hardware root of trust and secure storage backends are platform-specific. This section covers Qualcomm and NXP i.MX6 — two platforms with extensive production deployment.
9.1 Qualcomm Integration (QFPROM)
Qualcomm devices use QFPROM (Qualcomm Fuse ROM) as the hardware root of trust. The secure boot enforcement is a separate layer (PBL + XBL) that runs before U-Boot, but Qualcomm also exposes fuse-based rollback counters that U-Boot can use:
| /* Qualcomm QFPROM-based secure boot check */ /* arch/arm/mach-snapdragon/qfprom.c */ #define QFPROM_CORR_AUTH_REGION 0x00780350#define QFPROM_SEC_BOOT_FUSE_ROW 0x00780380/* Check if Qualcomm secure boot fuse is blown */ bool qcom_is_secure_boot_enabled(void){ uint32_t val; /* QFPROM corrected region (with ECC correction applied) */ val = readl(QFPROM_CORR_AUTH_REGION); /* Bit 0 of AUTH_EN = SECURE_BOOT_EN fuse */ return (val & BIT(0)) != 0;}/* Read Qualcomm anti-rollback counter for a given image type */ int qcom_read_rollback_version(uint32_t image_id, uint32_t *version){ /* Different QFPROM rows for different images */ /* IMAGE_ID 0=XBL, 1=TZ, 2=HYP, 3=ABL, 4=HLOS */ uint32_t fuse_row = QFPROM_ANTI_ROLLBACK_BASE + (image_id * 4); uint32_t fuse_val = readl(fuse_row); /* Popcount gives the monotonic counter value */ *version = __builtin_popcount(fuse_val); return 0;}/* Pet the Qualcomm TZ watchdog while in U-Boot */ /* (TZ watchdog is independent of Linux watchdog) */ static void qcom_pet_tz_watchdog(void){ struct scm_desc desc = {0}; desc.args[0] = 0; desc.arginfo = SCM_ARGS(1); scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT, QCOM_TZ_WDOG_PET_CMD), &desc);} |

Qualcomm-specific verified boot notes:
- On Qualcomm IoT/embedded platforms using U-Boot (not XBL/ABL), U-Boot acts as the application bootloader after SPL
- QFPROM fuses are blown by TZ via authenticated SCM calls — they cannot be written directly from HLOS after secure boot is enabled
- The PBL+XBL chain already provides a hardware root of trust; U-Boot verified boot adds an additional software layer for kernel verification
- CONFIG_QCOM_SCM should be enabled in U-Boot to access QFPROM via the SCM interface
9.2 NXP i.MX6 Integration (HAB — High Assurance Boot)
The NXP i.MX6 uses HAB (High Assurance Boot), a ROM-resident cryptographic library that verifies software before execution. HAB uses the IVT (Image Vector Table) format and X.509 certificate chains:
| /* HAB (High Assurance Boot) integration in U-Boot */ /* arch/arm/mach-imx/hab.c */ /* HAB ROM Vector Table - function pointers in i.MX ROM */ typedef enum hab_status (*hab_rvt_authenticate_image_t) (uint8_t cid, ptrdiff_t ivt_offset, void **start, size_t *bytes, const void *callback);#define HAB_RVT_AUTHENTICATE_IMAGE (*(uint32_t *)0x00000094)int imx6_hab_authenticate_image(uint32_t ddr_start, uint32_t image_size){ hab_rvt_authenticate_image_t *hab_authenticate; enum hab_status hab_status; void *start = (void *)ddr_start; size_t bytes = image_size; /* Get HAB function pointer from ROM vector table */ hab_authenticate = (hab_rvt_authenticate_image_t) HAB_RVT_AUTHENTICATE_IMAGE; /* HAB_CID_UBOOT = 0x01 (customer code ID) */ hab_status = hab_authenticate(HAB_CID_UBOOT, ivt_offset, &start, &bytes, NULL); if (hab_status != HAB_SUCCESS) { printf("HAB: authentication failed status=0x%x\n", hab_status); /* If fuses are closed: hang */ if (imx_hab_is_enabled()) hang(); /* If fuses are open: warn and continue */ } return (hab_status == HAB_SUCCESS) ? 0 : -1;}/* Check if HAB (secure boot fuses) are blown */ bool imx_hab_is_enabled(void){ /* Read HAB_CFG field from OCOTP_CFG5 fuse */ uint32_t val = readl(OCOTP_BASE + OCOTP_CFG5); return ((val >> 25) & 0x3) == 0x3; /* HAB_CFG = 0b11 = closed */ } |

i.MX6 specific notes:
- HAB uses a 4-level certificate chain: Qualcomm CA -> OEM CA -> OEM Signing -> Image
- The CSF (Command Sequence File) describes the signing operations — similar to FIT .its for Qualcomm
- SRK (Super Root Key) hash is burned into OCOTP fuses — this is the hardware anchor
- In "open" configuration (fuses not blown), HAB errors are warnings only — useful for development
- In "closed" configuration (fuses blown), HAB errors halt boot — used in production
Section 10: Verified Boot vs. Secure Boot
The terms "Secure Boot" and "Verified Boot" are often used interchangeably but they refer to distinct mechanisms with different scopes and implementation approaches. Understanding the difference is critical for designing a complete security architecture.
| Feature | Secure Boot | Verified Boot (U-Boot) |
| Enforced by | Hardware (ROM + OTP fuses) | Software (U-Boot + FIT image verification) |
| Key storage | SoC fuses / OTP (burned at manufacturing) | U-Boot DTB (compiled into U-Boot binary) |
| Bypass possible? | No — once fuses are blown, bypassing requires physical SoC modification | Yes — if CONFIG_REQUIRE_SIGNATURE=n or U-Boot binary is replaced |
| Scope / coverage | SPL and U-Boot chain (boot loaders) | Kernel, DTB, and ramdisk (everything after U-Boot) |
| Standard | Platform-specific (HAB for i.MX, QSEE for Qualcomm, Secure Boot for RPi) | Open standard — FIT image format + RSA + SHA256, defined by U-Boot project |
| Rollback protection | Via hardware fuse counters (blown during firmware update) | Via rollback-index in FIT + hardware fuse or RPMB counter |
| Algorithm | RSA-2048/4096 + SHA256/SHA384, platform-specific certificate chains | RSA-2048/4096 + SHA256/SHA384 — configurable via .its algo field |
| Debug/dev mode | Open (fuses not blown): errors are warnings only | CONFIG_REQUIRE_SIGNATURE=n: verification runs but failures are non-fatal |
| Update model | New signing key requires fuse update (often impossible); use cert chain rotation | New signing key requires rebuilding U-Boot with new DTB (full U-Boot update) |
| Typical usage | Protecting the boot chain up to and including U-Boot itself from tampering | Protecting the runtime OS (kernel + drivers + init ramdisk) from tampering |


In a complete security architecture, both mechanisms are used in concert: Secure Boot anchors the chain at the hardware level (protecting U-Boot), while Verified Boot extends the chain into the OS components (protecting the kernel and ramdisk). Neither alone is sufficient for full-stack protection.
Section 11: Complete Boot Flow with Verified Boot
This section presents the complete end-to-end boot flow from power-on through kernel execution, showing exactly where each verification step occurs, what is being verified, and what key material is used.
11.1 Full Boot Flow Diagram
| Power On / Hardware Reset | vSoC ROM (Primary Boot Loader / PBL) [Immutable] | - Reads BOOT_CONFIG fuses to determine boot device | - Loads SPL from storage (eMMC/SPI/NAND) | - Verifies SPL signature using OTP-burned public key hash | [KEY: ROM_KEY burned into OTP fuses at manufacturing] | - If signature FAIL: halt / enter emergency download mode | - If signature PASS: jump to SPL entry point vSPL (Secondary Program Loader) | - Initializes minimal hardware: SRAM, clocks | - Loads U-Boot proper from storage | - Verifies U-Boot using SPL's embedded public key | [KEY: SPL_PUB_KEY embedded in SPL DTB at build time] | - Checks U-Boot rollback-index vs. RPMB/fuse counter | - If signature FAIL: halt | - If signature PASS: jump to U-Boot entry point vU-Boot Proper | - Full hardware initialization (DDR, storage, display) | - Loads kernel FIT image (kernel.itb) from storage/network | - Calls fit_image_verify_required_sigs(): | 1. Locate public key in u-boot.dtb by key-name-hint | 2. Recompute SHA256 hash of kernel + DTB + ramdisk | 3. RSA-2048 verify: stored_sig == RSA(hash) | [KEY: KERNEL_PUB_KEY embedded in U-Boot DTB at build time] | - Check FIT rollback-index vs. hardware fuse counter | - If any check FAIL: "Signature check failed" -> HALT | - If all checks PASS: load components to RAM addresses vKernel FIT Image Unpacked | - kernel@1 -> loaded to 0x80080000 (ARM64 example) | - fdt@1 -> loaded to DTB_LOAD_ADDR | - ramdisk@1 -> loaded to INITRD_LOAD_ADDR vU-Boot do_bootm_linux() | - Prepares kernel command line in DTB /chosen node | - Disables D-cache (ARM64 requirement before kernel entry) | - Calls kernel entry: entry(0, 0, dtb_phys_addr) | (x0=DTB physical address, x1=0, x2=0, x3=0) vLinux Kernel (arch/arm64/kernel/head.S) | - Sets up page tables, enables MMU | - start_kernel() -> device probing, mount rootfs vdm-verity (optional, for root filesystem integrity) | - Block-level SHA256 tree over entire root partition | - Rooted by hash stored in verified DTB (from U-Boot) | - Any modified block -> I/O error -> kernel panic or remount ro vLinux Userspace / Android Framework |

11.2 Key Material Flow Summary
The following table summarizes where each key is stored and what it verifies in the complete chain:
| Boot Stage | Key Location | What is Verified | Fallback on Failure |
| ROM -> SPL | OTP fuses (public key hash) | SPL binary | Halt / EDL mode (no OS recoverable action) |
| SPL -> U-Boot | SPL DTB (embedded at build) | U-Boot FIT image | Halt (hang()), no kernel loaded |
| U-Boot -> Kernel | U-Boot DTB (embedded at build) | Kernel + DTB + ramdisk FIT image | Boot halted, error printed. Device stuck at U-Boot prompt if USB console attached. |
| Kernel -> Rootfs | Roothash in DTB /chosen or cmdline | Root filesystem block hash tree | I/O error on any tampered block; panic or remount ro depending on dm-verity mode |

Section 12: Debugging Verified Boot Issues
Verified boot failures are common during development and integration. This section covers the most common failure modes, how to diagnose them, and tools for inspecting FIT image structure and U-Boot internals.
12.1 Common Failures and Root Causes
| Error Message | Root Cause | How to Fix |
| "Signature check failed" | Key mismatch, image tampered, wrong FIT built | Verify the FIT was signed with the key matching the one in u-boot.dtb. Rebuild: mkimage -F -k keys/ -K u-boot.dtb -r kernel.itb |
| "No signatures found" | CONFIG_REQUIRE_SIGNATURE=y but image is unsigned | Re-run mkimage signing step. Or set CONFIG_REQUIRE_SIGNATURE=n for development (never in production) |
| "RSA: public key 'dev' not found" | mkimage -K step was skipped, or wrong u-boot.dtb was used during U-Boot build | Re-run mkimage -K step, then rebuild U-Boot. Verify with: fdtdump u-boot.dtb | grep "key-dev" |
| "Rollback attack detected! image=3, fuse=5" | FIT rollback-index (3) < hardware fuse counter (5) | Rebuild FIT with rollback-index >= 5 and re-sign. Never burn a fuse counter higher than intended. |
| "FIT images not compatible" | FIT architecture or OS type does not match U-Boot expectations | Check .its file: arch must match (arm/arm64), os must be "linux", type must be "kernel" |
| "Bad Magic Number" | Wrong load address, or the FIT image was not written to storage correctly | Verify the FIT magic (0xd00dfeed for DTB-based FIT). Check storage write succeeded. Check load address. |
| "sha256,rsa2048: Bad signature" | SHA256 hash of image data does not match signed hash — image was modified after signing | Rebuilding FIT image automatically clears and recomputes all hashes. Ensure no post-signing modification of the .itb file. |


12.2 Enable Verbose Output for Debugging
Two methods to enable detailed output during verification:
Method 1: Kconfig (compile-time)
| # Enable verbose FIT output in defconfig:CONFIG_FIT_VERBOSE=y# This produces output like:# FIT: Verifying signature for 'conf@1'# FIT: Checking sha256 hash of kernel@1# FIT: sha256: 3d4a2ef9... OK# FIT: Checking sha256 hash of fdt@1# FIT: sha256: 7b1c4e21... OK# FIT: RSA: verify with key dev# FIT: Signature valid |

Method 2: U-Boot Environment Variable
| # From U-Boot command line:setenv verify_debug 1# Or enable at compile time with:CONFIG_FIT_VERBOSE=yCONFIG_RSA_VERIFY_WITH_PKEY=y |

12.3 Dump and Inspect FIT Image Structure
These commands are invaluable for diagnosing FIT image issues before even running U-Boot:
| # List FIT image contents (summary)mkimage -l kernel.itb# Full FIT DTS dump including all signature and hash datafdtdump kernel.itb# Check specific nodefdtdump kernel.itb | grep -A 30 "configurations"# Extract the embedded signature bytesfdtdump kernel.itb | grep "value ="# Verify U-Boot DTB contains the public keyfdtdump u-boot.dtb | grep -A 20 "signature"# Check what key name is embeddedfdtdump u-boot.dtb | grep "key-"# Verify the key matches (compare modulus)# From certificate:openssl x509 -in keys/dev.crt -noout -modulus | md5sum# From U-Boot DTB:fdtdump u-boot.dtb | grep "rsa,modulus" | md5sum# These should match |

12.4 Debug in U-Boot Shell
When a device boots to the U-Boot shell (with USB console access), use these commands to manually test verification:
| # Load FIT image from eMMC partition 1mmc dev 0ext4load mmc 0:1 $loadaddr /boot/kernel.itb# Attempt to verify (will print detailed error if fails)iminfo $loadaddr# Manually check FIT structurefdt addr $loadaddrfdt print /configurations# Check what key U-Boot has embeddedfdt addr $fdtcontroladdrfdt print /signature# Try booting with forced signature check debugsetenv bootargs "console=ttyS0,115200"bootm $loadaddr# To temporarily bypass for development (NEVER in production):# Requires: CONFIG_REQUIRE_SIGNATURE=n in defconfig# There is NO runtime bypass if REQUIRE_SIGNATURE=y is compiled in |

12.5 Verification Failure Decision Tree
Use the following diagnostic approach when facing verified boot failures:
| Verified boot failure? | +-- Error: "public key not found" | --> Check: fdtdump u-boot.dtb | grep "key-" | --> Fix: mkimage -K, rebuild U-Boot | +-- Error: "Signature check failed" | +-- Was the FIT signed at all? | | --> mkimage -l kernel.itb (look for "Sign algo:") | +-- Was it signed with the SAME key as in U-Boot? | --> Compare modulus hashes (see Section 12.3) | +-- Error: "Rollback attack detected" | --> FIT rollback-index < fuse counter | --> Increment rollback-index in .its, rebuild, re-sign | +-- Error: "No signatures found" | --> FIT image was not signed | --> Run mkimage signing step | +-- Hangs silently after "Verifying Hash Integrity" --> Hash check failed but VERBOSE not enabled --> Enable CONFIG_FIT_VERBOSE=y, rebuild U-Boot |

Summary: U-Boot Verified Boot Quick Reference
| Component | Role in Verified Boot |
| FIT image (.itb) | Single container for kernel + DTB + ramdisk + SHA256 hashes + RSA signatures. Created from .its source file using mkimage. |
| mkimage -F -k -K -r | Signs FIT image with private key AND embeds public key into U-Boot DTB. The -r flag marks the key as required — boot fails without valid signature. |
| u-boot.dtb /signature/key-<name> | Node in U-Boot's own device tree containing the RSA public key (modulus, exponent) and precomputed Montgomery parameters. Read at boot time by rsa_verify(). |
| rsa_verify() in lib/rsa/ | Core verification function: finds public key in DTB, computes SHA256 hash of image regions, performs RSA PKCS#1 v1.5 verify using Montgomery arithmetic. |
| CONFIG_REQUIRE_SIGNATURE=y | CRITICAL: This Kconfig option makes verification mandatory. Without it, unsigned images still boot (insecure). Must always be set in production builds. |
| rollback-index + fuse counter | Anti-rollback protection. rollback-index in FIT must be >= hardware fuse counter (OCOTP on i.MX, QFPROM on Qualcomm, RPMB generic). |
| SPL verified boot | Extends chain: ROM key verifies SPL, SPL key verifies U-Boot, U-Boot key verifies kernel. Each level uses a separate key pair for independence. |
| HAB (i.MX) | NXP High Assurance Boot. ROM-resident library verifies software via X.509 certificate chain and IVT format. Enforced by OCOTP fuse HAB_CFG=0b11. |
| QFPROM (Qualcomm) | Qualcomm Fuse ROM. OEM_PK_HASH fuse stores public key hash. SECURE_BOOT_EN fuse enforces verification. ANTI_ROLLBACK fuses store per-image version counters. |
| dm-verity | Linux kernel block-level integrity for root filesystem. SHA256 Merkle tree over all data blocks. Root hash provided by verified DTB /chosen node. Completes the chain to rootfs. |


This document covered the complete U-Boot Verified Boot implementation — from the conceptual chain of trust through to platform-specific integration details on Qualcomm and i.MX6 platforms. The verified boot mechanism forms a critical security layer in any embedded Linux product, bridging the hardware-anchored secure boot (PBL/ROM) and the runtime OS integrity (dm-verity).
