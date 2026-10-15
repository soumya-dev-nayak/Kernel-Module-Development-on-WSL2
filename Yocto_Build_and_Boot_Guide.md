# Yocto Image Build and QEMU Boot Guide

## Scope and current status

This guide documents the verified, working procedure for building a Yocto Project image under Ubuntu/WSL2 and booting it in QEMU.

**Validated:** a Poky 5.0.21 (Scarthgap) `core-image-minimal` image for the `genericx86-64` machine, built with BitBake and booted successfully in QEMU with a working shell, filesystems, and network.

**Not validated:** any physical hardware, including the Intel Agilex FPGA platform, NVIDIA Jetson Orin, or MinnowBoard MAX. No FPGA drivers, device trees, or inter-SoC communication have been tested.

**Multi-architecture note:** this guide currently walks through `genericx86-64` as the first validated target. The same workflow (Parts 1–5) is intended to extend to further `MACHINE` values, including arm64-based boards (Agilex, Jetson Orin), as their BSP layers and machine definitions are added. Where a step is architecture-specific — the `MACHINE` value, the cross-toolchain, or the QEMU CPU model — it is marked **(x86_64-specific)**.

All host commands run under Ubuntu 26.04 LTS on WSL2. Each step states whether it runs on the **Ubuntu/WSL host** or **inside the Yocto/QEMU guest**.

This is the clean, reproducible path. Issues encountered and resolved along the way (locale generation, a patch-state error, a corrupted source cache, and a WSL crash) are not repeated in the steps below — they are collected once in the **Troubleshooting Reference** at the end, in case they recur.

---

## Part 1 — Host and Build Environment Setup

### 1.1 Objective

| Item | Value |
|---|---|
| Target machine **(x86_64-specific)** | `genericx86-64` |
| Distribution | Poky |
| Image | `core-image-minimal` |
| Yocto branch | Scarthgap (Yocto Project 5.0) |
| Poky release | 5.0.21 |
| Build directory | `build-agilex` |

### 1.2 Host environment

**Location:** Ubuntu/WSL host.

| Component | Verified version or value |
|---|---|
| Operating system | Ubuntu 26.04 LTS |
| Architecture | x86_64 |
| GCC | 15.2.0 |
| Python | 3.14.4 |
| Git | 2.53.0 |
| LZ4 | 1.10.0 |
| BitBake | 2.8.1 |
| Poky release | Scarthgap / 5.0.21 |

Ubuntu 26.04 is not validated by this version of the Yocto build system. This is a compatibility warning, not a build-blocking error.

### 1.3 Install host dependencies

**Location:** Ubuntu/WSL host.

```bash
sudo apt update
sudo apt install -y gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
    iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev xterm \
    python3-subunit mesa-common-dev zstd lz4 file locales libacl1
```

> `liblz4-tool` is not available on Ubuntu 26.04; `lz4` is used instead and is already reflected above.

Verify the tools:

```bash
gcc --version
python3 --version
git --version
lz4 --version
cat /etc/os-release
```

**Expected:** the versions listed in §1.2, with no errors from any of the five commands.

### 1.4 Clone Poky

**Location:** Ubuntu/WSL host.

```bash
mkdir -p ~/yocto
cd ~/yocto

git clone -b scarthgap https://git.yoctoproject.org/poky.git
```

```bash
git -C ~/yocto/poky branch --show-current
```

**Expected output:**

```text
scarthgap
```

The repository is at `/home/soumyadevnayak/yocto/poky`. Scarthgap is the Yocto Project 5.0 release series.

### 1.5 Initialise the build environment

**Location:** Ubuntu/WSL host.

```bash
cd ~/yocto/poky
source oe-init-build-env build-agilex
```

This creates and selects `/home/soumyadevnayak/yocto/poky/build-agilex` and configures the shell for BitBake. Every later `bitbake` command assumes this has been sourced in the current terminal session.

### 1.6 Configure the build

**Location:** Ubuntu/WSL host, inside `build-agilex`.

```bash
vi conf/local.conf
```

Set the target machine **(x86_64-specific — replace with the relevant `MACHINE` value for other architectures)**:

```bitbake
MACHINE ??= "genericx86-64"
```

Set the locale BitBake expects:

```bash
sudo locale-gen en_US.UTF-8
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
```

Verify it is available:

```bash
locale -a | grep -i en_US
```

**Expected output:**

```text
en_US.utf8
```

Add the build-parallelism limits, **before running any build**, to keep BitBake's task scheduling and compilation within the resources available under WSL2:

Open the configuration file:

```
vi ~/yocto/poky/build-agilex/conf/local.conf
```
### Go to the bottom of the file and add:

```
BB_NUMBER_THREADS = "4"
PARALLEL_MAKE = "-j 4"
```

- `BB_NUMBER_THREADS = "4"`: BitBake schedules up to four tasks at once.
- `PARALLEL_MAKE = "-j 4"`: Make-based compilation uses up to four parallel jobs.

### Verify the Settings:

```
grep -E '^(BB_NUMBER_THREADS|PARALLEL_MAKE)' \
~/yocto/poky/build-agilex/conf/local.conf
```
- Expected Output:

```
BB_NUMBER_THREADS = "4"
PARALLEL_MAKE = "-j 4"
```

These limits are required for stable, extended builds under WSL2 — a build run without them was observed to make WSL terminate unexpectedly partway through. Set them once, in `local.conf`, before starting the build in Part 2.

Confirm BitBake itself runs:

```bash
bitbake --version
```

**Expected output:**

```text
BitBake Build Tool Core version 2.8.1
```

---

## Part 2 — Build the Image

### 2.1 Start the build

**Location:** Ubuntu/WSL host.

```bash
set -o pipefail
bitbake core-image-minimal 2>&1 | tee -a ~/yocto-build.log
```

- `tee -a` shows the output in the terminal and appends it to `~/yocto-build.log`.
- `set -o pipefail` ensures a BitBake failure is visible as the pipeline's failure, rather than being hidden by `tee`'s successful exit status.

This can take a long time on first run, since it fetches and compiles everything `core-image-minimal` needs.

### 2.2 Expected output

Parsing stage:

```text
920 .bb files parsed
1878 targets
48 skipped
0 masked
0 errors
```

Build configuration reported:

```text
BB_VERSION       = "2.8.1"
BUILD_SYS        = "x86_64-linux"
TARGET_SYS       = "x86_64-poky-linux"
MACHINE          = "genericx86-64"
DISTRO           = "poky"
DISTRO_VERSION   = "5.0.21"
TUNE_FEATURES    = "m64 core2"
```

Active layers: `meta`, `meta-poky`, `meta-yocto-bsp`, all on Scarthgap at commit `4fb7016a07507683359ebd620f1951fd5ea44bfa`.

**Non-blocking warnings you may see:**

1. Ubuntu 26.04 has not been validated with this build-system version.
2. The WSL virtual disk may need optimising over time to avoid storage exhaustion.
3. The host kernel does not have `CONFIG_TASK_IO_ACCOUNTING` enabled.

### 2.3 Confirm build completion

**Location:** Ubuntu/WSL host.

A successful build ends with a line like:

```text
Tasks Summary: Attempted 4487 tasks of which 4439 didn't need to be rerun and all succeeded.
```

This means BitBake considered 4,487 tasks; 4,439 were reused from earlier work (normal in an incremental build); every task attempted in this run succeeded.

**Success condition:** BitBake exits successfully and reports that all attempted tasks succeeded. A task-progress message or a partially generated file is not proof of completion — if the build stops early or errors out, check `~/yocto-build.log` and the failing task's own log under `tmp/work/.../temp/` before changing anything.

---

## Part 3 — Verify the Build Artifacts

### 3.1 Check the deployed files

**Location:** Ubuntu/WSL host.

```bash
cd ~/yocto/poky/build-agilex

ls -lh tmp/deploy/images/genericx86-64/

ls -lh \
  tmp/deploy/images/genericx86-64/bzImage \
  tmp/deploy/images/genericx86-64/core-image-minimal-genericx86-64.rootfs.wic \
  tmp/deploy/images/genericx86-64/core-image-minimal-genericx86-64.rootfs.qemuboot.conf
```

**Expected:** the deployment directory contains the Linux `bzImage`, the `core-image-minimal` WIC image, and the generated QEMU boot configuration. The WIC image is approximately 160 MiB; the kernel file is approximately 12 MiB. Timestamped file names and symlinks can differ between builds — check the paths that actually exist.

### 3.2 Inspect the WIC disk layout

**Location:** Ubuntu/WSL host.

```bash
fdisk -l tmp/deploy/images/genericx86-64/core-image-minimal-genericx86-64.rootfs.wic
```

**Expected partition table** — GPT, with:

| Partition | Approximate size | Purpose |
|---|---:|---|
| EFI/boot | 28.3 MiB | Boot files and EFI bootloader |
| Linux root | 85.1 MiB | Root filesystem |
| Swap | 44 MiB | Swap partition |

**Success condition:** the build reports success, the expected artifacts exist and are non-empty, and the WIC image has this partition structure. The QEMU boot test in Part 4 is the further proof that the disk image actually works.

---

## Part 4 — Boot the Image in QEMU

### 4.1 Objective

Boot `core-image-minimal` in QEMU, since physical hardware is not currently available. This verifies that the image starts Linux, provides a shell, mounts its filesystems, and communicates over a virtual network. It does not validate the Intel Agilex FPGA, Jetson, or other physical hardware.

### 4.2 Confirm the artifacts and prepare firmware

**Location:** Ubuntu/WSL host.

```bash
cd ~/yocto/poky/build-agilex

IMAGE="$PWD/tmp/deploy/images/genericx86-64/core-image-minimal-genericx86-64.rootfs.wic"
CONF="$PWD/tmp/deploy/images/genericx86-64/core-image-minimal-genericx86-64.rootfs.qemuboot.conf"

test -s "$IMAGE" \
    && echo "WIC image found" \
    || { echo "ERROR: WIC image missing or empty"; exit 1; }
```

The disk image is EFI-formatted, so QEMU needs matching UEFI firmware **(x86_64-specific: OVMF)**:

```bash
ls -lh /usr/share/OVMF/OVMF_CODE_4M.fd \
       /usr/share/OVMF/OVMF_VARS_4M.fd

cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/OVMF_VARS_4M.fd
```

The first file is the UEFI firmware code (read-only). The second is copied to a writable location so QEMU has its own variable store instead of writing to the shared template; running the `cp` again resets it to default and does not touch the Yocto disk image itself.

### 4.3 Boot the image

**Location:** Ubuntu/WSL host.

```bash
qemu-system-x86_64 \
  -machine q35,i8042=off \
  -cpu IvyBridge \
  -m 1024 \
  -smp 4 \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/OVMF_VARS_4M.fd \
  -drive file="$IMAGE",format=raw \
  -boot order=c \
  -nographic \
  -no-reboot \
  -nic user,model=virtio-net-pci
```

| Option | Purpose |
|---|---|
| `qemu-system-x86_64` **(x86_64-specific)** | Starts QEMU's x86-64 virtual machine |
| `-machine q35,i8042=off` | Q35 virtual chipset, emulated i8042 controller disabled |
| `-cpu IvyBridge` **(x86_64-specific)** | CPU model matching the one in the generated `.qemuboot.conf` |
| `-m 1024` | 1024 MiB virtual RAM |
| `-smp 4` | Four virtual CPUs |
| 1st `-drive if=pflash...` | UEFI firmware code, read-only |
| 2nd `-drive if=pflash...` | Writable UEFI variable store |
| `-drive file="$IMAGE",format=raw` | The generated WIC disk image, as a raw virtual disk |
| `-boot order=c` | Boot from the virtual hard disk |
| `-nographic` | Serial console in the host terminal |
| `-no-reboot` | No automatic VM reboot if the guest requests one |
| `-nic user,model=virtio-net-pci` | Virtual NIC using QEMU user-mode networking |

The WIC image is attached directly as a writable disk, so changes made inside the guest can modify it — keep a backup or a separate copy for destructive experiments.

> For other machine/architecture targets, replace `qemu-system-x86_64`, `-cpu`, and the firmware with the equivalents for that architecture (for example `qemu-system-aarch64` and a matching CPU model for an arm64 target), and read the `qb_*` values from that machine's own generated `.qemuboot.conf`.

---

## Part 5 — Guest Verification

All commands in this part run **inside the Yocto/QEMU guest**, after Part 4's boot completes.

### 5.1 Log in

Wait for the login prompt, then enter:

```text
root
```

**Expected:**

```text
Poky (Yocto Project Reference Distro) 5.0.21 genericx86-64 /dev/ttyS0

genericx86-64 login:
```

The root account accepts login without a password, giving:

```text
root@genericx86-64:~#
```

### 5.2 Verify the kernel and CPU

```sh
uname -a
hostname
cat /proc/cpuinfo | head -n 10
```

> Use `head -n 10`, not `head -10` — the latter fails in this BusyBox environment.

**Expected kernel:**

```text
Linux genericx86-64 6.6.21-yocto-standard
```

**Expected CPU info:**

```text
vendor_id       : GenuineIntel
model name      : Intel Xeon E3-12xx v2 (Ivy Bridge)
```

`/proc/cpuinfo` lists four processors, numbered 0–3.

### 5.3 Verify memory and mounted filesystems

```sh
cat /proc/meminfo | head
free -h
df -h
cat /proc/mounts
```

**Expected memory:**

```text
MemTotal:         991564 kB
```

(Slightly below the 1024 MiB given to QEMU, since some memory is reserved or otherwise unavailable to the guest kernel.)

**Expected mount table (excerpt):**

```text
/dev/root / ext4 rw,relatime 0 0
/dev/sda1 /boot vfat rw,relatime,...
proc /proc proc rw,relatime 0 0
sysfs /sys sysfs rw,relatime 0 0
```

### 5.4 Verify the network interface

```sh
ip addr
```

**Expected:**

```text
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP>
    inet 10.0.2.15/24
```

With this QEMU user-mode networking setup: guest IP `10.0.2.15`, gateway `10.0.2.2`, DNS `10.0.2.3`.

### 5.5 Test connectivity and DNS

```sh
ping -c 3 10.0.2.2
ping -c 3 8.8.8.8
nslookup yoctoproject.org
```

| Test | Confirms |
|---|---|
| Ping gateway (`10.0.2.2`) | Communication with QEMU's virtual network gateway |
| Ping external IP (`8.8.8.8`) | Connectivity beyond the local virtual network |
| DNS lookup (`yoctoproject.org`) | Hostname resolution |

**Expected:** both ping tests return `3 packets transmitted, 3 packets received, 0% packet loss`; DNS returns:

```text
Server:    10.0.2.3

Name:      yoctoproject.org
Address 1: 3.131.150.69
```

(The returned IP can change over time — the check is that resolution succeeds.)

### 5.6 Check kernel log for userspace crashes

```sh
dmesg | grep -iE 'illegal instruction|invalid opcode|segfault'
```

**Expected:** no matches, and `hostname` prints:

```text
genericx86-64
```

An empty result here means the currently accessible kernel log has no matching messages — it is not exhaustive proof every process is error-free.

---

## Part 6 — Future Work

- Identify and verify the `MACHINE` values and BSP-layer compatibility for the intended arm64 hardware targets — the Intel Agilex FPGA platform and NVIDIA Jetson Orin — and extend Parts 1–5 to those targets.
- Add MinnowBoard MAX as an additional x86_64 target.
- Add FPGA drivers, platform-specific device trees, and inter-SoC communication once real or emulated hardware for those targets is available. A working `genericx86-64` build does not establish that the other platforms are supported by the same configuration.
- Investigate the unresolved `ip: SIOCGIFFLAGS: No such device` boot-time message noted in the troubleshooting reference below.

---

## Troubleshooting Reference

Issues actually encountered while producing this guide, kept here for reference rather than in the main steps above.

| Symptom | Likely cause / evidence | Resolution |
|---|---|---|
| `liblz4-tool` not found | Package not available on Ubuntu 26.04 | Install `lz4` instead (already reflected in §1.3) |
| BitBake reports `en_US.UTF-8` locale unavailable | Locale not generated | `sudo locale-gen en_US.UTF-8`; verify with `locale -a` (already reflected in §1.6) |
| `ERROR: dnf-native ... do_patch`, "already applied; check your series file" | Inconsistent patch state in the recipe's work directory | `bitbake -c clean dnf-native`, then rerun the build |
| WSL terminates partway through a build | Possible resource exhaustion (unconfirmed) | Set `BB_NUMBER_THREADS` / `PARALLEL_MAKE` as in §1.6 before building; if it recurs, run `wsl --shutdown` and resume — the build directory and caches are kept, so completed work is reused |
| `linux-yocto:do_unpack` reports a missing or invalid Git `HEAD` | The cached bare Git repository's `HEAD` reference is broken, even though the required commit is present | Locate the cached repo under `downloads/*linux-yocto.git` (`DL_DIR`); confirm with `git --git-dir=<path> show-ref --heads` that the required commit is reachable from the intended branch; then `git --git-dir=<path> symbolic-ref HEAD refs/heads/master`; retry with `bitbake -c unpack linux-yocto`. Do not force `HEAD` without confirming the commit is reachable first. |
| WIC image missing after a build | The build did not actually complete successfully | Confirm BitBake's final `Tasks Summary` line reports success before attempting to boot |
| `runqemu genericx86-64 nographic` fails, looking for `/dev/net/tun` | TUN device not available in this WSL setup | Use the direct `qemu-system-x86_64` command in §4.3 instead |
| `runqemu ... slirp` selects an ext4 image instead of the WIC image, or ignores a WIC path | `runqemu` image-type resolution did not behave as expected here | Launch QEMU directly (§4.3) |
| Boot stalls at `Booting from Hard Disk...` | Legacy SeaBIOS used against an EFI-formatted disk image | Boot with OVMF UEFI firmware, as in §4.2–§4.3 |
| Userspace `Illegal instruction` / invalid opcode (e.g. BusyBox `hostname` crash) | QEMU CPU model mismatched against what the image expects | Use the CPU model named in the generated `.qemuboot.conf` (`-cpu IvyBridge` for this target), as in §4.3 |
| `ip: SIOCGIFFLAGS: No such device` during guest boot | Not established | Did not block DHCP or networking in this run; unresolved, noted in Part 6 |
