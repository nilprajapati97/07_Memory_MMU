Proc & Sysfs Filesystems
Comprehensive Interview Guide
Staff Engineer Level — Linux Kernel Internals

|  |


Topics Covered
| Q1–Q7: The /proc Filesystem | Q8–Q13: The sysfs Filesystem |
| Q14: proc vs sysfs Comparison | Q15–Q17: Advanced sysfs Topics |
| Q18–Q19: Debugging & Pitfalls | Q20–Q21: Device Tree & debugfs |
| Quick Reference: Staff-Level Talking Points |  |


| ⚠️ This document contains all 21 interview Q&A entries for /proc and sysfs filesystems, including code examples, architecture diagrams, comparison tables, and a Quick Reference section for Staff Engineer interviews. |


# 1.  The /proc Filesystem
|  |


Q1: What is /proc and why does it exist?
Answer:
/proc is a pseudo-filesystem (virtual filesystem) mounted at /proc. It does NOT exist on disk — it lives entirely in RAM and is created/destroyed dynamically by the kernel.

- Purpose: Provides a user-space interface to kernel internal data structures, process information, and runtime configuration.
- Backed by: The procfs kernel subsystem (fs/proc/).
- Mount type: proc (mounted as: mount -t proc proc /proc).

| ⚠️ Staff-level key point: /proc was the original mechanism for exporting kernel info. Over time it became a dumping ground for unrelated data, which led to the creation of sysfs. |


Q2: How is /proc implemented internally in the kernel?
Answer:
| User space: cat /proc/meminfo | v VFS layer (open/read/write) | v procfs registered handlers | v seq_file / single_open callbacks | v Kernel data structures |


Each /proc entry is created using proc_create() or proc_mkdir(). The kernel registers struct proc_ops (previously struct file_operations before Linux 5.6):

| static const struct proc_ops my_proc_ops = { .proc_open = my_open, .proc_read = seq_read, .proc_lseek = seq_lseek, .proc_release = single_release,};// Creating the entry:proc_create("my_entry", 0444, NULL, &my_proc_ops); |


The seq_file interface is the standard way to implement /proc reads for large or sequential data. It handles pagination automatically via start, next, stop, and show callbacks.

Q3: What is the seq_file interface and why is it needed?
Answer:
The problem: If your /proc file outputs large data, a simple read handler must manage buffer sizes, offsets, and partial reads manually — error-prone and complex.

seq_file solves this by providing an iterator-based abstraction:

| static void *my_start(struct seq_file *s, loff_t *pos) { ... }static void *my_next(struct seq_file *s, void *v, loff_t *pos) { ... }static void my_stop(struct seq_file *s, void *v) { ... }static int my_show(struct seq_file *s, void *v) { seq_printf(s, "Data: %d\n", *(int *)v); return 0;}static const struct seq_operations my_seq_ops = { .start = my_start, .next = my_next, .stop = my_stop, .show = my_show,}; |


| # | Function | Role |
| 1 | start() | Initialize iterator, return first element |
| 2 | show() | Format current element into buffer |
| 3 | next() | Advance to next element |
| 4 | (repeat) | Repeat show/next until next() returns NULL |
| 5 | stop() | Cleanup resources |


Q4: What are the important entries under /proc?
Answer:
| Entry | Description |
| /proc/[pid]/ | Per-process directory (maps, status, fd, etc.) |
| /proc/[pid]/maps | Virtual memory mappings of the process |
| /proc/[pid]/status | Human-readable process status |
| /proc/[pid]/fd/ | Open file descriptors (symlinks) |
| /proc/[pid]/cmdline | Command line arguments |
| /proc/[pid]/task/ | Per-thread subdirectories |
| /proc/meminfo | System memory statistics |
| /proc/cpuinfo | CPU details (per-core) |
| /proc/interrupts | IRQ counters per CPU |
| /proc/iomem | Physical memory map (I/O regions) |
| /proc/ioports | I/O port allocations |
| /proc/kallsyms | All kernel symbols with addresses |
| /proc/modules | Loaded kernel modules |
| /proc/sys/ | Tunable kernel parameters (sysctl) |
| /proc/devices | Registered char/block device major numbers |
| /proc/filesystems | Supported filesystem types |
| /proc/slabinfo | SLAB allocator statistics |
| /proc/vmstat | Virtual memory statistics |
| /proc/buddyinfo | Buddy allocator fragmentation info |
| /proc/zoneinfo | Per-zone memory info (NUMA) |


Q5: What is /proc/sys/ and how does sysctl work?
Answer:
/proc/sys/ exposes tunable kernel parameters at runtime.

| # Readcat /proc/sys/net/ipv4/ip_forward# Writeecho 1 > /proc/sys/net/ipv4/ip_forward# Or using sysctl toolsysctl net.ipv4.ip_forward=1 |


Kernel-side registration:
| static struct ctl_table my_table[] = { { .procname = "my_param", .data = &my_variable, .maxlen = sizeof(int), .mode = 0644, .proc_handler = proc_dointvec, }, { } // sentinel};static struct ctl_table_header *hdr;hdr = register_sysctl("kernel", my_table); |


| Subdirectory | Key Parameters |
| /proc/sys/kernel/ | shmmax, pid_max, hostname, panic, perf_event_paranoid |
| /proc/sys/net/ | tcp_keepalive, ip_forward, tcp_fin_timeout, rmem_max |
| /proc/sys/vm/ | swappiness, dirty_ratio, overcommit_memory, drop_caches |
| /proc/sys/fs/ | file-max, inotify/max_watches, pipe-max-size |


Q6: How does /proc/[pid]/maps work and what information does it provide?
Answer:
The maps file shows the virtual memory layout of a process. Sample output:

| address perms offset dev inode pathname7f8a1000-7f8a5000 r-xp 00000000 08:01 131074 /lib/libc.so.67f8a5000-7f8a7000 rw-p 00004000 08:01 131074 /lib/libc.so.67ffff000-80000000 rw-p 00000000 00:00 0 [stack] |


| Field | Description |
| perms | r=read, w=write, x=execute, p=private / s=shared |
| offset | Offset into the mapped file (0 for anonymous) |
| dev | Device (major:minor) |
| inode | Inode of the mapped file (0 for anonymous) |
| pathname | [heap], [stack], [vdso], or file path |


| ⚠️ Staff-level insight: This is backed by iterating over vm_area_struct linked list (or maple tree in newer kernels >=6.1) in the process's mm_struct. |


Q7: What is the difference between /proc/iomem and /proc/ioports?
Answer:
| Aspect | /proc/iomem | /proc/ioports |
| What | Physical address space (MMIO regions) | I/O port address space |
| Access | Memory-mapped I/O (ioremap) | inb()/outb() port I/O |
| ARM usage | Primary method — ALL peripherals are MMIO | Rarely used (x86-centric) |
| Registration | request_mem_region() | request_region() |
| Addr width | Full physical address width | 16-bit (0x0000–0xFFFF on x86) |


| ⚠️ ARM-specific (important for Qualcomm): ARM SoCs use ONLY MMIO. There is no I/O port space. All peripheral registers are memory-mapped and accessed via ioremap() + readl()/writel(). |


# 2.  The sysfs Filesystem
|  |


Q8: What is sysfs and why was it created?
Answer:
sysfs is a pseudo-filesystem mounted at /sys that exports the kernel's device model (kobjects, devices, drivers, buses, classes) to user space in a structured, hierarchical manner.

Why sysfs was created:
- /proc became cluttered with non-process data
- No structured representation of device hierarchy
- Needed a clean, one-value-per-file interface
- Needed to represent relationships (device → driver → bus)

| ⚠️ Key principle: ONE VALUE PER FILE (unlike /proc which often has multi-line, multi-value files). This is a fundamental ABI contract of sysfs. |


Q9: What is the internal architecture of sysfs?
Answer:
| sysfs (user-visible) | v kernfs (VFS backend) | v kobject / kset / ktype | v Kernel device model objects (struct device, struct device_driver, struct bus_type, struct class) |


| Data Structure | Role | sysfs Representation |
| kobject | Fundamental building block; reference-counted kernel object | One directory in sysfs |
| kset | A collection of kobjects (container). Generates uevents. | Directory containing member kobject directories |
| ktype | Defines behavior: default attributes, sysfs_ops, release() | Determines files (attributes) in the directory |
| kernfs | Actual VFS backend (replaced direct sysfs VFS ops in Linux 3.14+) | Manages inodes, dentries, directory tree |


The core kobject data structure:
| struct kobject { const char *name; struct list_head entry; struct kobject *parent; struct kset *kset; const struct kobj_type *ktype; struct kernfs_node *sd; // sysfs directory entry struct kref kref; // reference count // ...}; |


Q10: What is the directory structure of /sys?
Answer:
| /sys/├── block/ → Block devices (sda, nvme0n1)├── bus/ → Bus types (pci, usb, i2c, spi, platform)│ └── <bus>/│ ├── devices/ → Symlinks to devices on this bus│ └── drivers/ → Drivers registered for this bus├── class/ → Device classes (net, block, tty, input)│ └── <class>/│ └── <device> → Symlinks to actual device├── dev/ → Devices by dev_t (char/, block/)│ ├── char/ → major:minor → symlink to device│ └── block/├── devices/ → ** Actual device hierarchy ** (the real tree)│ ├── system/ → System devices (cpu, memory)│ └── platform/ → Platform devices├── firmware/ → Firmware interfaces (acpi, devicetree, efi)├── fs/ → Filesystem parameters├── kernel/ → Kernel subsystems (slab, mm, debug)├── module/ → Loaded modules and their parameters└── power/ → System power management |


| ⚠️ Staff-level insight: /sys/devices/ is the canonical location. Everything else (/sys/bus/*/devices/, /sys/class/*/) contains SYMLINKS pointing back to /sys/devices/. The canonical tree is never duplicated. |


Q11: How do you create sysfs attributes for a device driver?
Answer:
Method 1: Using DEVICE_ATTR macro (most common for device drivers)

| static ssize_t my_attr_show(struct device *dev, struct device_attribute *attr, char *buf){ return sysfs_emit(buf, "%d\n", my_value); // sysfs_emit, NOT sprintf}static ssize_t my_attr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count){ int ret; ret = kstrtoint(buf, 10, &my_value); if (ret) return ret; return count;}static DEVICE_ATTR_RW(my_attr); // Creates dev_attr_my_attr// In probe():device_create_file(&pdev->dev, &dev_attr_my_attr);// Better: use attribute groups (preferred):static struct attribute *my_attrs[] = { &dev_attr_my_attr.attr, NULL,};ATTRIBUTE_GROUPS(my); // Creates my_groupsstatic struct platform_driver my_driver = { .driver = { .name = "my_driver", .dev_groups = my_groups, // Kernel handles create/remove }, .probe = my_probe, .remove = my_remove,}; |


Method 2: Using kobject directly

| static struct kobj_attribute my_kobj_attr = __ATTR(my_file, 0644, my_show, my_store);static struct attribute *attrs[] = { &my_kobj_attr.attr, NULL,};static struct attribute_group attr_group = { .attrs = attrs,};struct kobject *kobj = kobject_create_and_add("my_dir", kernel_kobj);sysfs_create_group(kobj, &attr_group); |


| ⚠️ Critical staff-level point: Always use sysfs_emit() instead of sprintf()/scnprintf() — it is buffer-overflow safe (bounded to PAGE_SIZE) and is the modern kernel standard since Linux 5.10. |


Q12: What are kobject, kset, and ktype? How do they relate?
Answer:
| kset (container) ├── kobject A (ktype = X) ├── kobject B (ktype = X) └── kobject C (ktype = Y) |


| Component | Role | sysfs Representation |
| kobject | Basic building block; reference-counted kernel object | A directory in sysfs |
| kset | Collection of kobjects; sends uevents to userspace | Directory containing member kobject directories |
| ktype | Defines behavior: default attributes, sysfs_ops, release() | Determines files (attributes) in the directory |


Lifecycle:
| // 1. Initializekobject_init(&kobj, &my_ktype);// 2. Add to sysfskobject_add(&kobj, parent, "name");// 3. Send uevent (notifies udev)kobject_uevent(&kobj, KOBJ_ADD);// 4. When done -- drop referencekobject_put(&kobj); // When refcount -> 0, ktype->release() called |


Q13: How does udev interact with sysfs?
Answer:
| Kernel User Space | | | 1. Device discovered | | 2. kobject_add() | | 3. kobject_uevent(KOBJ_ADD) | |----netlink socket------------>| | 4. udevd receives uevent | 5. udevd reads /sys/... attributes | 6. udevd matches rules (/etc/udev/rules.d/) | 7. udevd creates /dev/xxx node | 8. udevd runs RUN commands |


Key uevent environment variables:
| Variable | Description |
| ACTION | add, remove, change, move |
| DEVPATH | sysfs path (/devices/platform/...) |
| SUBSYSTEM | Bus/class name (e.g., "platform", "usb", "i2c") |
| MAJOR/MINOR | Device numbers for /dev node creation |
| DEVNAME | Suggested device node name |


Monitoring and triggering uevents:
| # Monitor uevents in real-timeudevadm monitor --kernel --udev# Trigger a uevent manuallyecho add > /sys/devices/.../uevent |


| ⚠️ Staff-level: The uevent is sent via NETLINK_KOBJECT_UEVENT netlink socket. udevd listens on this socket and uses the DEVPATH to read additional attributes from sysfs before processing rules. |


# 3.  Comparison, Advanced Topics & Debugging
|  |


Q14: What is the difference between /proc and /sys?
Answer:
| Aspect | /proc (procfs) | /sys (sysfs) |
| Primary purpose | Process info + misc kernel info | Device model hierarchy |
| Design principle | Multi-value files, free-form format | One value per file (strict) |
| Backed by | proc_ops + seq_file | kobject + kernfs |
| Hierarchy | Flat (PID dirs + misc files) | Structured device tree |
| Hotplug/uevent | No | Yes (kobject uevents) |
| Device relationships | Not represented | Bus → Device → Driver links |
| Typical tools | top, ps, free, sysctl | udev, device mgmt, power mgmt |
| Writable params | /proc/sys/* via sysctl | Device attributes via store() |
| Introduced | Linux 1.0 (1994) | Linux 2.6 (2003) |


Q15: How does sysfs handle binary attributes?
Answer:
For non-text data (firmware blobs, EEPROM contents), sysfs provides bin_attribute:

| static struct bin_attribute my_bin_attr = { .attr = { .name = "eeprom", .mode = 0444, }, .size = EEPROM_SIZE, .read = my_bin_read, .write = my_bin_write, // optional};static ssize_t my_bin_read(struct file *filp, struct kobject *kobj, struct bin_attribute *attr, char *buf, loff_t off, size_t count){ // Read from hardware into buf memcpy(buf, eeprom_data + off, count); return count;} |


Example real-world usage: /sys/bus/i2c/devices/0-0050/eeprom — reading an I2C EEPROM connected to bus 0 at address 0x50.

Q16: What is kernfs and how does it relate to sysfs?
Answer:
kernfs (introduced in Linux 3.14) is the generic VFS backend that sysfs is built on top of.

Before kernfs (the problem): sysfs directly implemented VFS operations, tightly coupling kobject lifetime with VFS inode lifetime — causing race conditions and complexity.

After kernfs (the solution):
| User space → VFS → kernfs → sysfs callbacks → kobject |


- kernfs manages the directory tree, inodes, and dentries independently
- sysfs registers attributes as kernfs_node entries
- Decouples kobject lifetime from VFS lifetime — eliminates race conditions
- Also used by the cgroup filesystem (cgroups v2)

Q17: What are device attribute groups and why prefer them?
Answer:
| static DEVICE_ATTR_RO(status);static DEVICE_ATTR_RW(config);static struct attribute *my_attrs[] = { &dev_attr_status.attr, &dev_attr_config.attr, NULL,};// Optional: conditionally show/hide attributesstatic umode_t my_is_visible(struct kobject *kobj, struct attribute *attr, int index){ if (attr == &dev_attr_config.attr && !has_config_support) return 0; // Hide this attribute return attr->mode;}static const struct attribute_group my_group = { .attrs = my_attrs, .is_visible = my_is_visible, .name = "my_subdir", // Optional: creates a subdirectory};ATTRIBUTE_GROUPS(my); |


Why prefer attribute groups over individual device_create_file():
| # | Benefit | Detail |
| 1 | Atomic creation/removal | All attributes created or none — no partial state |
| 2 | Automatic cleanup | Kernel handles removal on driver unbind (no memory leaks) |
| 3 | is_visible() support | Conditionally hide/show attributes based on hardware capability |
| 4 | No race conditions | Attributes exist before uevent is sent to udev |
| 5 | Cleaner code | Set .dev_groups in driver struct — no manual lifecycle management |


Q18: How do you debug issues with /proc and /sys?
Answer:
Debugging /proc:
| # Trace system calls on procfs readsstrace cat /proc/meminfo# Check per-process proc entriesls -la /proc/<pid>/# Trace kernel-side read syscallsecho 1 > /sys/kernel/debug/tracing/events/syscalls/sys_enter_read/enable |


Debugging /sys:
| # Monitor uevents from kernel and udevudevadm monitor --kernel --udev# Trigger uevent manually for a deviceecho add > /sys/devices/.../uevent# Check device-driver bindingls -la /sys/bus/platform/drivers/<driver>/# Trace filesystem attribute accessecho 1 > /sys/kernel/debug/tracing/events/filemap/enable# Check kobject reference counts (if debugfs enabled)cat /sys/kernel/debug/kobject_debug |


Using T32/CrashScope (crash dump analysis):
- Walk kobject trees from kset pointers in crash dump
- Inspect device->kobj.sd for kernfs_node state
- Check reference counts (kref) for kobject leak debugging
- Correlate sysfs path with device struct to identify hung/stale devices

Q19: What are common pitfalls when working with /proc and sysfs?
Answer:
| # | Pitfall | Correct Approach |
| 1 | Buffer overflow in show() | Use sysfs_emit() instead of sprintf(); it is bounded to PAGE_SIZE |
| 2 | Wrong return from store() | Must return count on success, not 0; returning 0 causes infinite retry loop |
| 3 | Race condition on device removal | Use proper locking; hold device reference while accessing data |
| 4 | Not using attribute groups | Manual device_create_file() can race with uevent; use .dev_groups instead |
| 5 | Violating one-value-per-file | Putting multiple values in one sysfs file breaks ABI contract |
| 6 | Forgetting proc_remove() | Call proc_remove() in module exit; leaking proc entries causes kernel oops on read |
| 7 | Using /proc for device info | Device-related data belongs in /sys; /proc is for process and misc kernel info |
| 8 | Not checking kstrtoint() errors | In store functions, always validate user input and handle conversion errors |
| 9 | Holding locks in show/store | Can cause deadlocks if user space process is killed while in show/store; use trylock patterns |


Q20: How does Device Tree relate to sysfs on ARM/Qualcomm platforms?
Answer:
| ⚠️ This is highly relevant for Qualcomm Staff Engineer interviews — understanding the DT-to-sysfs pipeline demonstrates deep platform knowledge. |


| Device Tree (DTS/DTB) | v OF (Open Firmware) parsing at boot | v platform_device created for each DT node | v Appears in /sys/devices/platform/ | v /sys/firmware/devicetree/ -- raw DT exposed via sysfs |


- /sys/firmware/devicetree/base/ — the entire flattened device tree exposed as a directory hierarchy
- Each DT node = directory, each property = file (binary content)
- of_find_node_by_name(), of_property_read_u32() — kernel APIs to parse DT
- Platform devices created from DT get sysfs entries under /sys/devices/platform/

Reading DT properties via sysfs:
| # Read a DT property (binary)hexdump /sys/firmware/devicetree/base/model# List all DT nodes with compatible stringsfind /sys/firmware/devicetree/base/ -name compatible# Read a specific property as ASCIIcat /sys/firmware/devicetree/base/compatible |


Qualcomm-specific: Qualcomm SoCs use Device Tree extensively. The qcom, compatible strings identify Qualcomm-specific bindings:

| Compatible String | Qualcomm Peripheral |
| qcom,geni-i2c | GENI-based I2C controller (QUP v3) |
| qcom,qup-spi | QUP-based SPI controller |
| qcom,msm-uart | MSM serial/UART controller |
| qcom,spmi-pmic | SPMI protocol PMIC interface |
| qcom,arm-smmu | ARM SMMU (IOMMU) used on Snapdragon SoCs |


Q21: What is the debugfs filesystem and how does it differ?
Answer:
| Aspect | procfs | sysfs | debugfs |
| Mount point | /proc | /sys | /sys/kernel/debug |
| Purpose | Process info, sysctl | Device model | Debug / development only |
| ABI stability | Stable (mostly) | Stable (strict) | NO ABI guarantee |
| Production use | Yes | Yes | NO — debug only |
| Default access | World-readable | World-readable | Root only (0700) |


debugfs usage example:
| // Creating debugfs entriesstruct dentry *dir = debugfs_create_dir("my_driver", NULL);// Simple integer valuedebugfs_create_u32("my_debug_val", 0644, dir, &my_val);// Custom file with full file_operationsdebugfs_create_file("my_debug_file", 0644, dir, NULL, &my_fops);// Cleanup on module exitdebugfs_remove_recursive(dir); |


| ⚠️ Key rule: debugfs entries should NEVER be relied upon in production code, scripts, or user-space applications. They can be removed, renamed, or restructured at any time without notice. |


# 4.  Quick Reference — Staff-Level Talking Points
|  |


For a Staff Engineer interview, emphasize these key differentiators that demonstrate depth beyond basic knowledge:

## 4.1  ABI Stability

| Filesystem | ABI Status | Implication |
| sysfs (/sys) | STABLE | Changes require formal deprecation; user-space scripts can rely on paths |
| procfs /proc/sys | STABLE | sysctl names are stable; safe for production use in scripts |
| debugfs | NO GUARANTEE | Internal tool only; never use in production scripts or shipped code |


## 4.2  Modern Best Practices (API Evolution)

| Context | Old/Wrong Way | Modern/Correct Way |
| sysfs show() output | sprintf(buf, ...) | sysfs_emit(buf, ...) |
| Attribute creation | device_create_file() per attr | .dev_groups with ATTRIBUTE_GROUPS() |
| /proc file ops (5.6+) | struct file_operations | struct proc_ops |
| Input parsing in store() | sscanf(buf, ...) | kstrtoint() / kstrtoul() |
| Store() return value | return 0 | return count (on success) |


## 4.3  kernfs Abstraction Layer

- Introduced in Linux 3.14 to decouple sysfs from direct VFS implementation
- sysfs sits on kernfs; kernfs also powers cgroup filesystem
- Eliminates kobject-lifetime / VFS-lifetime race conditions
- struct kernfs_node is the fundamental unit (replaces direct inode)
- Accessed via device->kobj.sd pointer in kernel data structures

## 4.4  Device Model Mastery

| kobject (reference-counted base object) | ├── kset (collection of kobjects, sends uevents) ├── ktype (behavior: sysfs_ops, release(), default_attrs) └── kernfs_node (VFS representation)struct device (embeds kobject) | ├── struct device_driver (bound driver) ├── struct bus_type (bus it lives on) ├── struct class (device class) └── device_attribute[] (sysfs attributes) |


- kobject → kset → ktype hierarchy is the foundation of the device model
- uevent flow: kobject_uevent() → netlink → udevd → /etc/udev/rules.d/ → /dev node
- Bus-device-driver binding: /sys/bus/<bus>/devices/ and /sys/bus/<bus>/drivers/
- Driver bind/unbind: echo device_name > /sys/bus/<bus>/drivers/<driver>/bind

## 4.5  ARM / Qualcomm Platform Context

| Topic | Key Points |
| No I/O port space | ARM uses ONLY MMIO. /proc/ioports is effectively empty on ARM SoCs. All peripheral registers accessed via ioremap() + readl()/writel(). |
| Device Tree → sysfs | DT nodes parsed at boot → platform_device created → appears in /sys/devices/platform/. Raw DT available at /sys/firmware/devicetree/ |
| Qualcomm compatible strings | qcom,geni-i2c / qcom,qup-spi / qcom,msm-uart / qcom,spmi-pmic / qcom,arm-smmu identify Qualcomm-specific bindings |
| Power management | /sys/devices/.../power/ attributes control runtime PM. wakeup, control, autosuspend_delay_ms are key sysfs files for power debugging. |
| Clock/regulator framework | /sys/kernel/debug/clk/ shows clock tree. /sys/kernel/debug/regulator/ shows regulator states. These are debugfs (not stable ABI). |


## 4.6  Key API Quick Reference

procfs APIs:
| // Create/remove entriesproc_create(name, mode, parent, proc_ops) -> struct proc_entry*proc_mkdir(name, parent) -> struct proc_entry*proc_remove(entry)// seq_file helpersseq_printf(m, fmt, ...) // formatted outputseq_puts(m, str) // simple string outputseq_read() // generic read (use in proc_ops.proc_read)// sysctl registrationregister_sysctl(path, table) -> struct ctl_table_header*unregister_sysctl_table(hdr) |


sysfs / kobject APIs:
| // kobject lifecyclekobject_init(kobj, ktype)kobject_add(kobj, parent, fmt, ...) // add to sysfskobject_create_and_add(name, parent) // alloc + init + addkobject_put(kobj) // decrement refcountkobject_uevent(kobj, action) // send uevent// Attribute creationsysfs_create_file(kobj, attr) // single attributesysfs_create_group(kobj, grp) // attribute groupsysfs_remove_group(kobj, grp)// Device attributes (preferred for drivers)DEVICE_ATTR_RO(name) // read-only, 0444DEVICE_ATTR_RW(name) // read-write, 0644DEVICE_ATTR_WO(name) // write-only, 0200ATTRIBUTE_GROUPS(name) // create name_groups[]// Output in show()sysfs_emit(buf, fmt, ...) // safe, PAGE_SIZE bounded |


debugfs APIs:
| debugfs_create_dir(name, parent) -> struct dentry*debugfs_create_file(name, mode, dir, data, fops)debugfs_create_u32(name, mode, dir, &val)debugfs_create_u64(name, mode, dir, &val)debugfs_create_bool(name, mode, dir, &val)debugfs_remove_recursive(dentry) // cleanup all at once |


## 4.7  Interview Answer Framework

| ⚠️ When asked any question about /proc or /sys, structure your answer: (1) What it is, (2) Internal kernel implementation, (3) User-space interaction, (4) Best practices / pitfalls, (5) Qualcomm/ARM relevance. |


| If Asked About... | Mention These Staff-Level Points |
| /proc internals | proc_ops (5.6+), seq_file iterator pattern, VFS layer, no disk storage, per-PID namespace isolation |
| sysfs design | kobject/kset/ktype trinity, kernfs VFS backend (3.14+), one-value-per-file ABI contract, stable ABI vs debugfs |
| Device model | kobject_add → kernfs_node, uevent → netlink → udevd pipeline, bus-device-driver binding, symlink structure |
| Writing a driver attr | DEVICE_ATTR_RW macro, sysfs_emit not sprintf, return count on success, attribute groups over create_file, is_visible() for conditional attrs |
| Debugging | udevadm monitor, strace, ftrace events, kobject walk in T32/CrashScope, kref inspection for leaks |
| ARM/Qualcomm | MMIO-only (no ioports), DT → platform_device → sysfs pipeline, /sys/firmware/devicetree/, qcom, compatible strings, runtime PM sysfs |


|  |


End of Document — Proc & Sysfs Filesystems Interview Guide
21 Questions • All Code Examples • All Tables • All Diagrams • Quick Reference
