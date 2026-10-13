# Building a Custom Multi-Platform FPGA-SoC Linux Kernel on WSL2

*A practical guide from zero to a cross-compiled kernel (ikwzm reference, Linux 6.12.100)*

---

## 1. Objective and reference

This guide builds a custom Linux kernel for FPGA-SoC platforms from WSL2 by cross-compiling a vanilla kernel with the ikwzm patch set. The result is a kernel with FPGA Manager, FPGA Bridge, FPGA Region and Device Tree Overlay support, plus decompiled Device Trees to use as blueprints. It is the starting point for Agilex 7, Agilex 9 and Jetson kernels (see Section 9).

| Item | Detail |
|---|---|
| **Repository** | `https://github.com/ikwzm/FPGA-SoC-Linux-Kernel-6.12` (maintainer: KAWAZOME Ichiro) |
| **Tag used** | `6.12.100-armv7-fpga-1` |
| **Hardware** | AMD/Xilinx Zynq-7000 (ZYBO, ZYBO-Z7, PYNQ-Z1); Intel/Altera Cyclone V (DE0-Nano-SoC, DE10-Nano) |
| **Features** | Single image for Xilinx and Intel SoCs; `CONFIG_OF_OVERLAY`, `CONFIG_FPGA_MGR`, `CONFIG_FPGA_BRIDGE`, `CONFIG_FPGA_REGION`; USB ChipIdea patch for PYNQ-Z1 |

---

## 2. Prepare the WSL2 environment

### Step 1: Update the system and install build tools

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential bc bison flex libssl-dev libncurses5-dev \
    u-boot-tools device-tree-compiler wget git curl patch debhelper cpio \
    gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf qemu-system-arm
```

- `qemu-system-arm`: QEMU system emulator for 32-bit ARM, required for the boot tests in Sections 10 and 11.
- `gcc-arm-linux-gnueabihf`, `binutils-arm-linux-gnueabihf`: 32-bit ARM cross-compiler and linker/assembler.
- `device-tree-compiler`: provides `dtc`. `u-boot-tools`: U-Boot image tools.
- `debhelper`, `cpio`: required by the `deb-pkg` target (the build aborts without them).
- `bc`, `bison`, `flex`, `libssl-dev`, `libncurses5-dev`: kernel build and `menuconfig` dependencies.

Verify the cross-compiler:

```bash
arm-linux-gnueabihf-gcc --version
```

Expected output:

```text
arm-linux-gnueabihf-gcc (Ubuntu ...) ...    # version string varies by Ubuntu release
```

Verify QEMU:

```bash
qemu-system-arm --version
```

Expected output (representative; version varies by Ubuntu release):

```text
QEMU emulator version ...
```

### Step 2: Work in the native Linux filesystem

```bash
cd ~
pwd
```

> Expected: `/home/YOUR_USERNAME`. If you see `/mnt/c/...` you are on the Windows mount; go back to `~`. Building there is slow and breaks file permissions.

---

## 3. Fetch the sources

### Step 3: Clone the ikwzm reference repository

```bash
git clone --branch 6.12.100-armv7-fpga-1 --depth 1 \
    https://github.com/ikwzm/FPGA-SoC-Linux-Kernel-6.12.git ikwzm-ref
```

The "detached HEAD" warning is normal: Git checked out a tag, not a branch. Confirm the contents:

```bash
cd ~/ikwzm-ref
ls -l
```

You must see `files/` (defconfig), `patches/` (three `.diff` files), `doc/`, `devicetrees/`, `scripts/`, and the pre-built `.deb` packages and `vmlinuz-6.12.100-armv7-fpga-1`.

### Step 4: Clone the vanilla Linux 6.12.100 source and create a build branch

```bash
cd ~
git clone --depth 1 -b v6.12.100 \
    git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git \
    linux-6.12.100-armv7-fpga
cd ~/linux-6.12.100-armv7-fpga
git describe --tags
git checkout -b custom-fpga-build
```

Expected output:

```text
v6.12.100
Switched to a new branch 'custom-fpga-build'
```

> The download is about 300 to 500 MB. If `git.kernel.org` is unreachable, use the HTTPS URL `https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git` instead.

---

## 4. Patch and configure

### Step 5: Apply the three patches (run inside the kernel directory, in this order)

```bash
patch -p1 < ../ikwzm-ref/patches/linux-6.12.100-armv7-fpga.diff
patch -p1 < ../ikwzm-ref/patches/linux-6.12.100-armv7-fpga-usb-ulpi.diff
patch -p1 < ../ikwzm-ref/patches/linux-6.12.100-armv7-fpga-builddeb.diff
```

Each patch prints `patching file ...` lines and must report no `FAILED` hunks.

### Step 6: Install the custom defconfig and commit a checkpoint

```bash
cp ../ikwzm-ref/files/armv7_fpga_defconfig arch/arm/configs/
ls -l arch/arm/configs/armv7_fpga_defconfig
git add --all
git commit -m "Apply ikwzm FPGA-SoC patches and custom defconfig"
echo 0 > .version
```

### Step 7: Confirm the FPGA subsystem is enabled

```bash
grep "CONFIG_FPGA" arch/arm/configs/armv7_fpga_defconfig | head -n 5
```

Expected output:

```text
CONFIG_FPGA=y
CONFIG_FPGA_MGR_SOCFPGA=y
CONFIG_FPGA_MGR_ZYNQ_FPGA=y
CONFIG_FPGA_BRIDGE=y
CONFIG_FPGA_REGION=y
```

`=y` means the driver is built into the kernel binary (`=m` would be a loadable module). Built-in is preferred for boot-time FPGA management.

---

## 5. Cross-compile the kernel

### Step 8: Set the cross-compilation environment

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export DTC_FLAGS=--symbols
echo $ARCH $CROSS_COMPILE
```

> These variables last only for the current terminal session; set them again after reopening the terminal. `DTC_FLAGS=--symbols` makes `dtc` emit symbol information, which Device Tree Overlays need at runtime.

### Step 9: Generate the config and build

```bash
make armv7_fpga_defconfig
make clean
make -j$(nproc) deb-pkg
```

The build prints thousands of lines and ends by creating the `linux-image`, `linux-headers` and `linux-libc-dev` packages. Do not interrupt it. If the machine runs out of RAM, use `make -j2 deb-pkg`.

---

## 6. Verify the build artifacts

### Step 10: Locate packages, kernel image and Device Tree blobs

```bash
cd ~
ls -lh *.deb
find ~/linux-6.12.100-armv7-fpga -name "zImage" -o -name "vmlinuz*" 2>/dev/null
find ~/linux-6.12.100-armv7-fpga -name "*.dtb" | grep -v "empty_root" | head -n 10
```

A `-dirty` suffix in the version string (e.g. `6.12.100-armv7-fpga-g52a355b23cd2-dirty`) only means uncommitted changes existed at build time. It is harmless.

| Artifact | Location | Purpose |
|---|---|---|
| `linux-image-*.deb` | `~/` | Installable kernel and modules |
| `linux-headers-*.deb` | `~/` | Headers for out-of-tree modules |
| `linux-libc-dev_*.deb` | `~/` | C library development headers |
| `zImage` | `<kernel dir>/arch/arm/boot/` | Compressed kernel binary |
| `*.dtb` | `<kernel dir>/arch/arm/boot/dts/` | Compiled Device Trees |
| `.config` | `<kernel dir>/` | Active kernel configuration |

---

## 7. Analyse the Device Tree

### Step 11: Decompile a DTB into readable source

```bash
cd ~/linux-6.12.100-armv7-fpga/arch/arm/boot/dts/intel/socfpga
dtc -I dtb -O dts -o ~/intel-soc-reference.dts socfpga_cyclone5_de0_nano_soc.dtb
head -n 60 ~/intel-soc-reference.dts
```

Warnings such as `simple_bus_reg ... missing or empty reg/ranges property` are normal when decompiling and can be ignored.

### Step 12: Extract the FPGA bridge, manager and region nodes

```bash
grep -A 10 "fpga-mgr\|fpga-bridge\|fpga-region" ~/intel-soc-reference.dts
```

Expected output (abridged):

```text
fpga_bridge@ff500000 { compatible = "altr,socfpga-hps2fpga-bridge"; ... };
fpgamgr@ff706000     { compatible = "altr,socfpga-fpga-mgr"; ... };
fpga-region0 {
    compatible = "fpga-region";
    fpga-mgr = <0x24>;
    fpga-bridges = <0x25 0x26 0x27 0x28>;
};
```

> These addresses (e.g. `0xff500000`) are specific to Cyclone V. For Agilex 7/9 take them from the Intel Agilex Hard Processor System Technical Reference Manual.

---

## 8. Final verification and troubleshooting

### Step 13: Confirm both FPGA Manager drivers are built in

```bash
grep "CONFIG_FPGA_MGR" ~/linux-6.12.100-armv7-fpga/.config
```

Expected output:

```text
CONFIG_FPGA_MGR_SOCFPGA=y
# CONFIG_FPGA_MGR_SOCFPGA_A10 is not set
CONFIG_FPGA_MGR_ZYNQ_FPGA=y
...
```

`CONFIG_FPGA_MGR_SOCFPGA=y` (Intel) and `CONFIG_FPGA_MGR_ZYNQ_FPGA=y` (Xilinx) together confirm that one kernel image manages FPGA hardware on both vendors' platforms.

**Common errors and fixes**

| Error | Fix |
|---|---|
| `unmet build dependencies: debhelper-compat ... cpio` | `sudo apt install -y debhelper cpio`, then rebuild |
| `can't find file to patch` / `Hunk #X FAILED` | Run from inside the kernel directory; check `git describe --tags` prints `v6.12.100` |
| `Can't find default configuration ...armv7_fpga_defconfig` | Defconfig not copied; repeat Step 6 |
| `openssl/ssl.h: No such file or directory` | `sudo apt install -y libssl-dev` |
| `Killed` during compilation | Out of RAM; use `make -j2 deb-pkg` |
| `No space left on device` | Free disk space or move the build to a larger partition |
| `Unable to locate package gcc-arm-linux-gnueabihf` | `sudo add-apt-repository universe`, then `sudo apt update` |
| `unable to connect to git.kernel.org` | Use the HTTPS clone URL (Step 4) |

---

## 9. Adapting this workflow to Agilex 7/9 and Jetson

The workflow above targets 32-bit ARMv7. For 64-bit ARM targets change the toolchain, base configuration and Device Trees:

```bash
sudo apt install -y gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
```

| Target | Base configuration | Device Tree directory |
|---|---|---|
| Intel Agilex 7/9 | Generic arm64 `defconfig`; vendor tree: `altera-fpga/linux-socfpga` | `arch/arm64/boot/dts/intel/` |
| NVIDIA Jetson | NVIDIA L4T kernel (or OE4T `meta-tegra`); arm64 `defconfig` as mainline baseline | `arch/arm64/boot/dts/nvidia/` |

> `socfpga_defconfig` and `tegra_defconfig` exist for 32-bit ARM only; on arm64 start from the generic `defconfig` or the vendor tree. Jetson GPU/DLA support depends on NVIDIA's out-of-tree modules, so Jetson realistically keeps its own kernel image. Verify these paths in the exact tree you use.

After loading the base config, run `make menuconfig` and confirm these options:

```text
CONFIG_FPGA=y
CONFIG_FPGA_MGR=y
CONFIG_FPGA_BRIDGE=y
CONFIG_FPGA_REGION=y
CONFIG_OF_OVERLAY=y
CONFIG_OF_CONFIGFS=y
```

**Device Tree for Agilex:** use `intel-soc-reference.dts` as a template, then (1) set the `compatible` string for your board, (2) replace every address with values from the Agilex HPS Technical Reference Manual, and (3) update the CPU nodes to the Cortex-A53 (or later) cores of your device.

---

## 10. Boot the custom kernel in QEMU and verify its identity

This section boots the custom Linux 6.12.100 kernel in QEMU using the `vexpress-a9` virtual machine. It also documents how to troubleshoot Device Tree Blob (DTB) errors and verify that QEMU is running the intended ikwzm-based kernel rather than the separate upstream Linux build.

### Step 14: Understand the QEMU test environment

QEMU allows us to test kernel booting and development without requiring the physical target hardware.

| Component       | Configuration                     | Purpose                                                         |
| --------------- | --------------------------------- | --------------------------------------------------------------- |
| Virtual machine | `vexpress-a9`                     | Emulates an ARM Versatile Express platform with a Cortex-A9 CPU |
| Kernel          | Custom ikwzm-based Linux 6.12.100 | Kernel being tested                                             |
| Kernel image    | `arch/arm/boot/zImage`            | Compressed ARM kernel image                                     |
| Device Tree     | `vexpress-v2p-ca9.dtb`            | Describes the emulated VExpress-A9 hardware                     |
| RAM             | 512 MiB                           | Memory allocated to the virtual machine                         |
| Root filesystem | `~/qemu-test/initramfs.cpio.gz`   | Minimal BusyBox userspace containing `/init` and a shell        |
| Console         | `ttyAMA0`                         | Serial console used to interact with the kernel                 |

**Important distinction:** QEMU is emulating the VExpress-A9 platform, not an Intel Agilex FPGA board. Booting an FPGA-focused kernel in QEMU verifies that the kernel works with the emulated platform; it does not validate the kernel's FPGA Manager, DMA, or hardware-specific functionality on a physical board.

### Step 15: Locate the kernel image and Device Tree Blob

First, navigate to the custom kernel source tree:

```bash
cd ~/linux-6.12.100-armv7-fpga
```

Verify that the kernel image exists:

```bash
ls -lh arch/arm/boot/zImage
```

Expected output (representative; size and date will vary):

```text
-rwxr-xr-x 1 YOUR_USERNAME YOUR_USERNAME 9.1M Oct 13 08:57 arch/arm/boot/zImage
```

Locate the VExpress-A9 DTB:

```bash
find arch/arm/boot -name 'vexpress-v2p-ca9.dtb'
```

Expected output:

```text
arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb
```

In this kernel tree, the resulting path is `arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb`.

Verify the DTB file:

```bash
file arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb
```

Expected output (representative; wording differs by `file` version):

```text
arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb: Device Tree Blob version 17, size=..., boot CPU=0, ...
```

Expected output identifies it as a Device Tree Blob. Its size may vary with the source and build configuration.

**Why the exact path matters:** Kernel source trees can organize Device Trees into architecture- or vendor-specific subdirectories. A correct filename is insufficient if QEMU is given the wrong path.

### Step 16: Build the Device Trees if the DTB is missing

If the find command returns no result, build the Device Trees using the ARM cross-compiler:

```bash
cd ~/linux-6.12.100-armv7-fpga
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- dtbs
```

Expected output (representative; if everything is already built, there may be little or no output):

```text
  DTC     arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb
  DTC     arch/arm/boot/dts/intel/socfpga/socfpga_cyclone5_de0_nano_soc.dtb
  ...
```

Then locate the file again:

```bash
find arch/arm/boot -name 'vexpress-v2p-ca9.dtb'
```

Expected output:

```text
arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb
```

During our setup, the DTB was initially missing because the Device Trees had not yet been built in this source tree. Running the dtbs target successfully generated the VExpress-A9 DTB, along with other enabled platform Device Trees.

### Step 17: Configure the VExpress platform, PL011 serial console and initramfs support

During the first boot attempts, QEMU started but displayed no Linux kernel messages. Enabling the VExpress platform, the PL011 UART console and initramfs support resolved the issue. These options must be enabled before the kernel is rebuilt and launched in Step 19.

Run from `~/linux-6.12.100-armv7-fpga`:

```bash
scripts/config --enable ARCH_VEXPRESS
scripts/config --enable SERIAL_AMBA_PL011
scripts/config --enable SERIAL_AMBA_PL011_CONSOLE
scripts/config --enable BLK_DEV_INITRD

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
```

Verify the configuration:

```bash
grep -E 'CONFIG_(ARCH_VEXPRESS|SERIAL_AMBA_PL011|SERIAL_AMBA_PL011_CONSOLE|BLK_DEV_INITRD)=' .config
```

Expected output:

```text
CONFIG_ARCH_VEXPRESS=y
CONFIG_SERIAL_AMBA_PL011=y
CONFIG_SERIAL_AMBA_PL011_CONSOLE=y
CONFIG_BLK_DEV_INITRD=y
```

Rebuild the kernel and Device Tree Blobs:

```bash
make -j"$(nproc)" ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- zImage dtbs
```

The release string (for example the Git hash and `-dirty` suffix) is derived from the source tree's state at build time and can change after a rebuild. Always confirm it with `make ... kernelrelease` (Step 20).

### Step 18: Prepare the initramfs mount points and archive

The minimal initramfs does not automatically contain every standard Linux filesystem directory. `/proc` and `/sys` are mount points for the kernel's procfs and sysfs virtual filesystems, and the directories must exist before `/init` can mount those filesystems. This is part of the initial root-filesystem preparation and must be done before the archive is first used to boot. Create the mount points:

```bash
cd ~/qemu-test/rootfs-arm
mkdir -p proc sys dev
```

Expected output (representative):

```bash
ls -d proc sys dev
```

```text
dev  proc  sys
```

Without a successful procfs mount, commands such as `cat /proc/version` and `cat /proc/cmdline` cannot access their expected files (Step 20).

Build (or rebuild) the initramfs archive after creating the directories:

```bash
find . -print0 | cpio --null -o --format=newc | gzip -9 > ~/qemu-test/initramfs.cpio.gz
gzip -t ~/qemu-test/initramfs.cpio.gz
```

Expected output (representative): `cpio` prints a block count; `gzip -t` prints nothing when the archive is valid.

```text
<N> blocks
```

### Step 19: Boot the custom kernel in QEMU

Run the following command from `~/linux-6.12.100-armv7-fpga`:

```bash
qemu-system-arm \
  -audio none \
  -M vexpress-a9 \
  -m 512M \
  -kernel arch/arm/boot/zImage \
  -dtb arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb \
  -initrd ~/qemu-test/initramfs.cpio.gz \
  -append "console=ttyAMA0,115200 rdinit=/init loglevel=8" \
  -serial stdio \
  -monitor none \
  -no-reboot
```

The arguments serve these purposes:

- `-audio none`: disables host audio, avoiding PulseAudio/PipeWire warnings unrelated to the kernel.
- `-M vexpress-a9`: selects the emulated ARM platform.
- `-m 512M`: allocates 512 MiB of RAM.
- `-kernel`: specifies the kernel image.
- `-dtb`: specifies the hardware description for the emulated platform.
- `-initrd`: loads the compressed initramfs containing the minimal userspace.
- `console=ttyAMA0,115200`: directs kernel console output to the PL011 serial port at 115200 baud.
- `rdinit=/init`: runs `/init` from the initramfs as the initial userspace process.
- `loglevel=8`: prints all kernel messages to the console.
- `-serial stdio`: connects the guest serial port to the current terminal.
- `-monitor none`: disables the QEMU monitor.
- `-no-reboot`: prevents QEMU from automatically restarting if the guest kernel shuts down or panics.

A successful boot shows the PL011 serial console becoming active, `/init` executing, the message `Minimal ARM Linux booted successfully`, and the BusyBox shell prompt.

Expected output (representative; the kernel messages printed at `loglevel=8` are omitted; the `/init` message appears only if the custom `/init` script prints it):

```text
...
Minimal ARM Linux booted successfully
/bin/sh: can't access tty; job control turned off
/ #
```

The warning `/bin/sh: can't access tty; job control turned off` is expected in this minimal setup, which has no controlling terminal. It does not prevent basic shell commands from working.

### Step 20: Verify which kernel is running

A successful boot alone does not prove that QEMU loaded the intended kernel. Linux exposes the running kernel's release and build information through several interfaces. Run these commands individually inside the QEMU shell:

```sh
uname -a
```

Shows the kernel release, build number, build time and architecture. It uses a system call and does not depend on `/proc`.

Output recorded in the latest test run (rebuilds change the build number, date and time):

```text
Linux (none) 6.12.100-armv7-fpga-gaa9f345ad80c #3 SMP PREEMPT Tue Oct 13 19:52:24 UTC 2026 armv7l GNU/Linux
```

The commands below read files under `/proc` and work only after `/proc` has been mounted successfully by `/init` (Step 18). If the mount failed, they report that the file cannot be found.

```sh
cat /proc/version
```

Shows the kernel version string with the build user and host, the ARM cross-compiler information, the build number and the build timestamp.

Expected output (representative):

```text
Linux version 6.12.100-armv7-fpga-gaa9f345ad80c (...) (arm-linux-gnueabihf-gcc ...) #3 SMP PREEMPT ...
```

```sh
cat /proc/sys/kernel/osrelease
```

Prints only the kernel release string, which should equal the `kernelrelease` output of the tree you built.

Expected output (should match `make ... kernelrelease` for the build being tested; latest observed release):

```text
6.12.100-armv7-fpga-gaa9f345ad80c
```

```sh
cat /proc/cmdline
```

Shows the command line passed by QEMU `-append`.

Expected output (representative; other parameters may be present):

```text
console=ttyAMA0,115200 rdinit=/init loglevel=8
```

```sh
cat /proc/cpuinfo
```

Shows the emulated CPU as seen by the kernel.

Expected output (representative; values vary):

```text
processor       : 0
model name      : ARMv7 Processor ...
...
```

```sh
cat /proc/meminfo
```

Shows memory statistics for the 512 MiB virtual machine.

Expected output (representative; values vary):

```text
MemTotal:       <value> kB
MemFree:        <value> kB
...
```

The kernel release can also be obtained from the source tree in the WSL terminal. Exit QEMU using `Ctrl+A`, followed by `X`, and run:

```bash
cd ~/linux-6.12.100-armv7-fpga
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- kernelrelease
```

Expected output (must match the release reported by the running kernel; latest observed):

```text
6.12.100-armv7-fpga-gaa9f345ad80c
```

Compare this with the separate upstream source tree:

```bash
cd ~/qemu-test/linux-6.12.100
make ARCH=arm kernelrelease
```

Output recorded in an earlier test run:

```text
6.12.100
```

This command assumes the upstream tree's configuration and build environment are usable. If it fails, check that `.config` exists (`ls -l .config`) or read the base version from the top of the tree's `Makefile` (`head -n 5 Makefile`).

The matching custom release string is strong evidence that the running kernel came from the ikwzm-based build. The upstream tree's different release string distinguishes the two builds.

> **Note:** Kernel release strings can be customized. For stronger provenance, retain the source revision, build configuration, and exact kernel artifact used to launch QEMU. A matching release string alone is not cryptographic proof of a binary's origin. A successful QEMU boot also does not validate the physical Agilex FPGA hardware: QEMU is emulating `vexpress-a9`.

### Step 21: Understand the kernel release suffix (-dirty)

Earlier recorded builds of this setup reported a release such as:

```text
6.12.100-armv7-fpga-g52a355b23cd2-dirty
```

The latest observed release (Step 20) is `6.12.100-armv7-fpga-gaa9f345ad80c`, without the `-dirty` suffix. The suffix depends on the state of the source tree when the build was made.

The components indicate:

- `6.12.100`: the Linux kernel base version.
- `armv7-fpga`: the custom local version identifying the FPGA-focused build.
- `g52a355b23cd2` / `gaa9f345ad80c`: the abbreviated Git revision identifier (it differs between builds of different commits).
- `dirty`: the source tree contained changes that Git considered uncommitted when the version string was generated.

The suffix is not an error and does not prevent the kernel from booting. It is useful for distinguishing custom builds.

### Step 22: Check the FPGA-related kernel configuration

After exiting QEMU, inspect the active configuration of the custom kernel:

```bash
cd ~/linux-6.12.100-armv7-fpga
grep -E '^CONFIG_(FPGA|FPGA_MGR_SOCFPGA|FPGA_BRIDGE|FPGA_REGION|OF_OVERLAY)=' .config
```

Output recorded in an earlier test run (output can differ with another configuration; no line for an option means it is not set in the form this `grep` matches):

```text
CONFIG_FPGA=y
CONFIG_FPGA_MGR_SOCFPGA=y
CONFIG_FPGA_BRIDGE=y
CONFIG_FPGA_REGION=y
CONFIG_OF_OVERLAY=y
```

Optional separate check for explicitly disabled options (no output if all five are enabled):

```bash
grep -E '^# CONFIG_(FPGA|FPGA_MGR_SOCFPGA|FPGA_BRIDGE|FPGA_REGION|OF_OVERLAY) is not set' .config
```

These entries establish that the corresponding framework or driver options are enabled in the build configuration. They do not prove that a matching physical device exists or that its driver has successfully bound to hardware.

In particular, the VExpress-A9 Device Tree does not describe the actual SoCFPGA hardware required by the SoCFPGA FPGA Manager driver. Therefore, empty FPGA Manager or FPGA Bridge device directories in QEMU are not evidence that these configuration options were ignored.

### Troubleshooting Reference

| Symptom | Cause | Resolution |
|----------|---------|------------|
| Couldn't open `dtb` file ... | Incorrect DTB path or missing DTB | Locate the file with `find`; if necessary, build it using `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- dtbs`. |
| `find` returns no DTB files | Device Trees have not been built in this source tree | Run the `dtbs` target and repeat the search. |
| QEMU starts but no kernel messages appear | VExpress platform, PL011 console or initramfs support not enabled | Enable the options and rebuild (Step 17); check `console=ttyAMA0,115200` in `-append`. |
| `cat /proc/version` or `/proc/cmdline` reports no such file | `/proc` was not mounted because the mount point did not exist in the initramfs | Create `proc`, `sys`, `dev` in the rootfs and rebuild the archive (Step 18). |
| QEMU boots but the kernel release is unexpected | A different kernel image may have been supplied | Compare `/proc/sys/kernel/osrelease` with `make kernelrelease` in the intended source tree. |
| FPGA Manager directory is empty | No matching FPGA Manager device was instantiated and bound on the emulated platform | Check the Device Tree and platform support; do not assume that compiling the driver creates a hardware device. |
| `/bin/sh: can't access tty; job control turned off` | Minimal initramfs shell has no controlling terminal | Basic shell operation can continue; configure a proper controlling terminal later if job control is required. |
| PulseAudio or PipeWire warnings appear in the host terminal | Host audio configuration warnings | Add `-audio none` (included in Step 19); otherwise investigate the actual QEMU error if startup fails. |

Example diagnostics (representative):

```text
qemu-system-arm: Couldn't open dtb file arch/arm/boot/dts/...        # fix the DTB path; use the `find` output
```

```text
cat: can't open '/proc/version': No such file or directory           # /proc not mounted; see Step 18
```

---

## 11. External kernel module development and testing

This section demonstrates how to develop an external Linux kernel module for the custom ikwzm-based Linux 6.12.100 kernel. The module is cross-compiled for ARM, included in the BusyBox initramfs, and tested in QEMU.

The initial module, `kernel_data_logger`, verifies the module lifecycle before we introduce hardware-specific functionality such as MMIO, interrupts, and DMA.

> **Kernel release in this section:** the examples use the latest observed release, `6.12.100-armv7-fpga-gaa9f345ad80c`. Replace it with the output of `make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- kernelrelease` for the build you are testing. Earlier recorded runs used `6.12.100-armv7-fpga-g52a355b23cd2-dirty`.

### Step 23: Understand the external module workflow

An external kernel module is compiled separately from the main kernel image and loaded into a running kernel when required.

The workflow is:

1. Verify that the kernel supports loadable modules.
2. Prepare the kernel source tree for external module compilation.
3. Create the module source file and Makefile.
4. Cross-compile the module for the target architecture.
5. Copy the `.ko` file into the initramfs.
6. Boot the matching kernel in QEMU.
7. Load the module, inspect its logs, and unload it.

**Important:** The module must be built against the appropriate kernel source tree and configuration. A module built for a different kernel release or incompatible configuration may be rejected.

### Step 24: Verify kernel module support

Exit QEMU using `Ctrl+A`, followed by `X`. In the WSL terminal, navigate to the ikwzm-based kernel tree:

```bash
cd ~/linux-6.12.100-armv7-fpga
```

Check the relevant configuration options:

```bash
grep -E '^CONFIG_MODULES=|^CONFIG_MODVERSIONS=' .config
```

Expected output (representative; only options set in your `.config` appear, and `CONFIG_MODULES=y` is required for loadable modules):

```text
CONFIG_MODULES=y
```

Interpret the results:

- `CONFIG_MODULES=y`: loadable kernel modules are enabled.
- `CONFIG_MODVERSIONS=y`: the kernel uses symbol versioning to check compatibility between modules and exported kernel symbols.
- If `CONFIG_MODULES` is disabled, external modules cannot be loaded until module support is enabled and the kernel is rebuilt.

### Step 25: Prepare the kernel source tree

Run:

```bash
cd ~/linux-6.12.100-armv7-fpga

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules_prepare
```

Expected output (representative; exact output varies, and there may be little output if already prepared):

```text
make: Entering directory '/home/YOUR_USERNAME/linux-6.12.100-armv7-fpga'
...
make: Leaving directory '/home/YOUR_USERNAME/linux-6.12.100-armv7-fpga'
```

This prepares generated headers and other build infrastructure needed for external module compilation.

Confirm the kernel release:

```bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- kernelrelease
```

Expected output (must match the release reported by the running kernel; latest observed):

```text
6.12.100-armv7-fpga-gaa9f345ad80c
```

Check whether the symbol-version file exists:

```bash
ls -lh Module.symvers 2>/dev/null || echo "Module.symvers is not present"
```

Expected output (illustrative; permissions, size and date are not verified), if the file exists:

```text
-rw-r--r-- ... Module.symvers
```

or, if it does not:

```text
Module.symvers is not present
```

**Important:** `modules_prepare` does not generate a complete `Module.symvers` when `CONFIG_MODVERSIONS=y`. In that situation, a full kernel build may be required to generate the symbol-version information needed for a compatible external module.

### Step 26: Create the module source directory

Create the project directory:

```bash
mkdir -p ~/KERNEL_DEV/kernel_data_logger
cd ~/KERNEL_DEV/kernel_data_logger
```

The project structure will initially be:

```text
kernel_data_logger/
├── kernel_data_logger.c
└── Makefile
```

Expected project structure (these files appear only after Steps 27 and 28; immediately after `mkdir` the directory may be empty):

```text
kernel_data_logger/
├── kernel_data_logger.c
└── Makefile
```

The `.c` file contains the module implementation, while the Makefile instructs the kernel build system to compile it.

### Step 27: Write the kernel module
Here you can paste this code Snippet directly into your Linux Terminal (it will automatically create the `.C` file)
Create `kernel_data_logger.c`:

```bash
cat > kernel_data_logger.c <<'EOF'
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

static int __init kernel_data_logger_init(void)
{
    pr_info("kernel_data_logger: Module loaded successfully\n");
    pr_info("kernel_data_logger: Ready for data-logging development\n");
    return 0;
}

static void __exit kernel_data_logger_exit(void)
{
    pr_info("kernel_data_logger: Module unloaded successfully\n");
}

module_init(kernel_data_logger_init);
module_exit(kernel_data_logger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Design Project");
MODULE_DESCRIPTION("First external kernel module for FPGA data logger development");
MODULE_VERSION("1.0");
EOF
```

Verify the file:

```bash
cat kernel_data_logger.c
```

The output should match the source above, including both lifecycle functions, the logging messages and the module metadata. The closing `EOF` must not be part of the file.

#### Understanding the source code

| Element | Purpose |
|---|---|
| `#include <linux/init.h>` | Provides initialization and exit annotations. |
| `#include <linux/module.h>` | Provides kernel module definitions and metadata macros. |
| `#include <linux/kernel.h>` | Provides kernel facilities, including logging macros. |
| `static int __init kernel_data_logger_init(void)` | Initialization function executed when the module loads successfully enough to begin initialization. |
| `pr_info()` | Writes informational messages to the kernel log. |
| `return 0;` | Indicates successful initialization. A negative error code would indicate failure. |
| `static void __exit kernel_data_logger_exit(void)` | Cleanup function called when the module is unloaded. |
| `module_init(...)` | Registers the module initialization function. |
| `module_exit(...)` | Registers the module cleanup function. |
| `MODULE_LICENSE("GPL")` | Declares the module's license and avoids the proprietary-module taint associated with a non-GPL-compatible license declaration. |
| `MODULE_AUTHOR`, `MODULE_DESCRIPTION`, `MODULE_VERSION` | Provide module metadata. |

The module currently logs messages only. It does not yet acquire data or interact with FPGA hardware.

### Step 28: Create the external module Makefile

Create a file named exactly `Makefile`:

```bash
cat > Makefile <<'EOF'
obj-m += kernel_data_logger.o
EOF
```

Verify the file:

```bash
cat Makefile
```

Expected output:

```makefile
obj-m += kernel_data_logger.o
```

The key directive is:

```makefile
obj-m += kernel_data_logger.o
```

Here, `obj-m` tells the kernel build system to build `kernel_data_logger.o` as an external loadable module. The resulting module is `kernel_data_logger.ko`.

Unlike a standalone userspace C program, this Makefile delegates compilation and linking to the kernel's build system.

### Step 29: Compile the module for ARM

Use the same ikwzm-based kernel tree and ARM cross-compiler used for the running kernel.

First, prepare the build tree:

```bash
cd ~/linux-6.12.100-armv7-fpga

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules_prepare
```

Then build the external module:

```bash
cd ~/KERNEL_DEV/kernel_data_logger

make -C ~/linux-6.12.100-armv7-fpga \
    ARCH=arm \
    CROSS_COMPILE=arm-linux-gnueabihf- \
    M="$PWD" \
    modules
```

Expected output (representative, illustrative sequence, not a verbatim log; messages and paths vary by kernel build system version and configuration):

```text
  CC [M]  .../kernel_data_logger.o
  MODPOST .../Module.symvers
  CC [M]  .../kernel_data_logger.mod.o
  LD [M]  .../kernel_data_logger.ko
```

The options mean:

- `-C`: enter the specified kernel source/build directory.
- `ARCH=arm`: select the 32-bit ARM kernel architecture.
- `CROSS_COMPILE=arm-linux-gnueabihf-`: use the ARM cross-compilation toolchain.
- `M="$PWD"`: identify the external module directory.
- `modules`: build the loadable module.

Verify the output:

```bash
file kernel_data_logger.ko
```

Expected output (representative; wording may vary, the key point is an ARM relocatable module, not an x86-64 executable):

```text
kernel_data_logger.ko: ELF 32-bit LSB relocatable, ARM, EABI5 version 1 (SYSV), BuildID[sha1]=..., not stripped
```

Inspect the module metadata:

```bash
modinfo kernel_data_logger.ko
```

Expected output (representative; field order and optional fields can vary):

```text
filename:       .../kernel_data_logger.ko
version:        1.0
description:    First external kernel module for FPGA data logger development
author:         Kernel Design Project
license:        GPL
...
vermagic:       6.12.100-armv7-fpga-gaa9f345ad80c ...
```

Check `vermagic`, which records the kernel release and other compatibility-related build information. It should begin with the release of the intended running kernel.

If compilation fails due to missing `Module.symvers`, symbol versioning, or mismatched generated headers, resolve that problem before proceeding. Do not assume that `modules_prepare` alone always provides everything required.

### Step 30: Locate the initramfs root filesystem

Our existing initramfs source directory is:

```text
~/qemu-test/rootfs-arm
```

The compressed initramfs used by QEMU is:

```text
~/qemu-test/initramfs.cpio.gz
```

The initramfs contains a minimal BusyBox userspace, including `/init`, a shell, and basic utilities, and the `proc`, `sys` and `dev` mount points created in Step 18. The external module must be copied into this filesystem so that `insmod` can access it after boot.

Calculate the kernel release dynamically:

```bash
KREL=$(make -s -C ~/linux-6.12.100-armv7-fpga \
    ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- kernelrelease)

echo "$KREL"
```

Expected output (latest observed; yours must match the build being tested):

```text
6.12.100-armv7-fpga-gaa9f345ad80c
```

Create a directory for the module:

```bash
mkdir -p ~/qemu-test/rootfs-arm/lib/modules/$KREL/extra
```

Copy the compiled module:

```bash
cp ~/KERNEL_DEV/kernel_data_logger/kernel_data_logger.ko \
   ~/qemu-test/rootfs-arm/lib/modules/$KREL/extra/
```

Expected destination (host-side path; inside QEMU the corresponding path begins with `/lib/modules/`):

```text
~/qemu-test/rootfs-arm/lib/modules/6.12.100-armv7-fpga-gaa9f345ad80c/extra/kernel_data_logger.ko
```

The resulting path is:

```text
/lib/modules/<kernel-release>/extra/kernel_data_logger.ko
```

The `extra` directory is a convenient location for a manually installed external module. It is not mandatory for `insmod`, which accepts a direct file path.

### Step 31: Rebuild the initramfs

Back up the current archive before rebuilding it:

```bash
cp ~/qemu-test/initramfs.cpio.gz \
   ~/qemu-test/initramfs.cpio.gz.bak
```

Rebuild the compressed archive from the initramfs root, using the same command as in Step 18:

```bash
cd ~/qemu-test/rootfs-arm

find . -print0 | cpio --null -o --format=newc | gzip -9 > ~/qemu-test/initramfs.cpio.gz
gzip -t ~/qemu-test/initramfs.cpio.gz
```

This packages the root filesystem using the `newc` CPIO format and compresses it with gzip. The `gzip -t` test prints nothing when the archive is valid.

The command assumes that the existing root filesystem is complete and that the current shell has the required `cpio` and `gzip` tools. Verify the resulting archive before booting:

```bash
ls -lh ~/qemu-test/initramfs.cpio.gz
```

Expected output (representative; the file should exist with a nonzero size, exact size and timestamp vary):

```text
-rw-r--r-- 1 YOUR_USERNAME YOUR_USERNAME <size> <date> /home/YOUR_USERNAME/qemu-test/initramfs.cpio.gz
```

If rebuilding fails, retain the backup and inspect the error rather than replacing the archive with an incomplete filesystem.

### Step 32: Boot QEMU with the module-containing initramfs

From the ikwzm kernel tree, run (same command as Step 19):

```bash
cd ~/linux-6.12.100-armv7-fpga

qemu-system-arm \
  -audio none \
  -M vexpress-a9 \
  -m 512M \
  -kernel arch/arm/boot/zImage \
  -dtb arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb \
  -initrd ~/qemu-test/initramfs.cpio.gz \
  -append "console=ttyAMA0,115200 rdinit=/init loglevel=8" \
  -serial stdio \
  -monitor none \
  -no-reboot
```

Wait for the BusyBox shell prompt:

```text
/ #
```

Expected output (the prompt appears only after the kernel has booted and `/init` has launched the shell):

```text
/ #
```

### Step 33: Verify the module file inside QEMU

At the QEMU shell, list the module directory:

```sh
ls -R /lib/modules
```

Expected output (representative; shown as a tree for clarity, the directory name must match the kernel release used to build the module):

```text
/lib/modules/
└── 6.12.100-armv7-fpga-gaa9f345ad80c/
    └── extra/
        └── kernel_data_logger.ko
```

The module should be present under the kernel-release directory:

```text
/lib/modules/6.12.100-armv7-fpga-gaa9f345ad80c/extra/kernel_data_logger.ko
```

The exact path depends on the release used to build the module.

### Step 34: Load the module

Use `insmod` with the exact path shown in the previous step:

```sh
insmod /lib/modules/6.12.100-armv7-fpga-gaa9f345ad80c/extra/kernel_data_logger.ko
```

Output recorded in an earlier test run (the first line is a diagnostic taint warning, not a load failure):

```text
kernel_data_logger: loading out-of-tree module taints kernel.
kernel_data_logger: Module loaded successfully
kernel_data_logger: Ready for data-logging development
```

If the kernel accepts the module, the kernel log will include messages similar to:

```text
kernel_data_logger: Module loaded successfully
kernel_data_logger: Ready for data-logging development
```

Inspect the most recent kernel messages:

```sh
dmesg | tail -n 20
```

Expected output (relevant lines only; other kernel messages may appear before or after them):

```text
kernel_data_logger: loading out-of-tree module taints kernel.
kernel_data_logger: Module loaded successfully
kernel_data_logger: Ready for data-logging development
```

The messages are generated by the module's `pr_info()` calls.

### Step 35: Verify that the module is loaded

Run (this reads `/proc`, which `/init` must have mounted, see Step 18):

```sh
cat /proc/modules
```

Output recorded in an earlier test run (size and load address may differ between builds; the module name and `Live` state are the key checks):

```text
kernel_data_logger 12288 0 - Live 0xbf000000 (O)
```

The fields describe the module's name, size, use count, dependency information, state, and load address. The final `(O)` indicates that the module is out-of-tree.

The load address and module size may vary between builds.

### Step 36: Unload the module

Remove the module:

```sh
rmmod kernel_data_logger
```

Successful unloading may produce no direct terminal output.

Check the kernel log:

```sh
dmesg | tail -n 20
```

Output recorded in an earlier test run:

```text
kernel_data_logger: Module unloaded successfully
```

The expected cleanup message is:

```text
kernel_data_logger: Module unloaded successfully
```

Optionally, check that the module is no longer listed:

```sh
cat /proc/modules
```

The `kernel_data_logger` entry should be absent. Other loaded modules may still appear.

### Step 37: Understand the out-of-tree module taint message

During our successful load, the kernel printed:

```text
kernel_data_logger: loading out-of-tree module taints kernel.
```

This is expected when loading a module built outside the kernel source tree. Linux records the kernel's taint state so that diagnostic information can distinguish a kernel running with externally supplied code.

The message does not mean that the module failed to load. In our test, the subsequent initialization messages and `/proc/modules` entry confirmed success.

The taint state is diagnostic information; unloading the module does not necessarily clear the kernel's taint flag.

### Troubleshooting reference

| Error or symptom | Likely cause | Action |
|---|---|---|
| `CONFIG_MODULES` is not enabled | Kernel was built without loadable-module support | Enable module support and rebuild the kernel. |
| `Invalid module format` | Kernel release, architecture, configuration, or module compatibility mismatch | Compare `uname -r`, `modinfo`, `vermagic`, and the kernel build configuration. |
| `Unknown symbol` | A required kernel symbol is unavailable or symbol-version information is incompatible | Check dependencies, configuration, and `Module.symvers`; rebuild the module against the correct kernel build. |
| Missing `Module.symvers` | Symbol-version data was not generated or is unavailable | If `CONFIG_MODVERSIONS=y`, perform the appropriate full kernel build to generate the required symbol information. |
| `insmod: can't open ...` | Incorrect path or module not included in the initramfs | Check the path using `ls -R /lib/modules`. |
| Module loads but expected messages are absent | Different module binary or kernel log inspection issue | Check `dmesg`, module metadata, and the file copied into the initramfs. |
| `Module is in use` during removal | The module has active users or dependencies | Inspect its use count and remove dependencies or users before unloading. |
| Out-of-tree taint message | Module was built outside the kernel source tree | Expected diagnostic warning; verify the actual load result. |

Example diagnostics (representative; wording varies):

```text
insmod: can't insert '...kernel_data_logger.ko': invalid module format    # Invalid module format: kernel/module compatibility issue
kernel_data_logger: Unknown symbol <symbol_name> (err -2)                  # Unknown symbol: missing or incompatible symbol/dependency
insmod: can't open '/lib/modules/.../kernel_data_logger.ko': No such file or directory   # wrong path or file missing from initramfs
kernel_data_logger: loading out-of-tree module taints kernel.              # expected diagnostic, not a failure
```

### Completion criteria

The external module workflow is verified when all of the following succeed:

- The module compiles as an ARM `.ko` file.
- The module is present in the initramfs.
- The intended kernel boots in QEMU.
- `insmod` succeeds and the initialization messages appear in `dmesg`.
- `/proc/modules` lists `kernel_data_logger`.
- `rmmod` succeeds and the cleanup message appears in `dmesg`.

Verification sequence: ARM module compilation → module inclusion in initramfs → QEMU boot → successful `insmod` → initialization logs → `/proc/modules` entry → successful `rmmod` → cleanup log.

At this stage, the module verifies the kernel-module lifecycle only. The next development step is to add configurable parameters and a userspace-visible status interface, then proceed toward device-specific platform drivers, MMIO, interrupts, and DMA when the target hardware and its Device Tree are available.
