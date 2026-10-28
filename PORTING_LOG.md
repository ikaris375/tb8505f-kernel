# Acer One 8 T482L Kernel Porting Log

Last updated: 2026-10-28

## Goal

Port and build a kernel for the Acer One 8 T482L using the available MediaTek kernel tree and stock `boot.img` / `lk.bin` for reference. Test kernels have been flashed to `boot`; device-side tests and current stock-system state are documented below.

## Findings

- `/tmp/boot.img` is an Android ARM64 boot image with a 2 KiB page size. Its kernel is about 9.45 MB and its ramdisk is about 795 KB.
- `/tmp/lk.bin` contains MediaTek LK strings identifying the MT6761 platform.
- The stock kernel extracted from `boot.img` identifies `CONFIG_MACH_MT6761=y`, `CONFIG_MTK_PLATFORM="mt6761"`, and project `tb8766p1_64_bsp`.
- The stock bootargs include `androidboot.hardware=mt6761`.
- The repository has a matching `tb8766p1_64_bsp` DT overlay and defconfig. The pre-existing workspace `.config` instead selected `anna_row_wifi`; it was left unchanged.
- The existing root `build.log` records an earlier link failure with unresolved camera calibration, display recovery, and audio symbols. Its configuration differs from the extracted stock config.

## Project file changes

- `build.sh`: identifies the Acer T482L target, uses the Acer defconfig in `out/acer_t482l/.config`, preserves the workspace `.config`, uses detected CPU count, and directs compiler temp files to `build/tmp/`.
- `arch/arm64/configs/acer_t482l_defconfig`: added using the MT6761 / `tb8766p1_64_bsp` platform settings; camera calibration and the unavailable Microtrust TEE driver are disabled for bring-up.
- `PORTING_LOG.md`: this work log.
- No kernel source, device tree, or workspace `.config` has been changed during this session.

## Build work

- Extracted the stock kernel, ramdisk, and stock DTB under `/tmp/acer-stock/` for inspection.
- An initial out-of-tree attempt using the stock config stopped because this legacy tree rejects a separate output directory when the source tree already contains `.config` / `include/config`. It also selected camera and panel drivers absent from this checkout.
- A later attempt identified another unavailable component: the stock config requests Microtrust TEE version `300`, while this checkout only contains the `280` implementation.
- To preserve workspace files, a clean tracked source snapshot was extracted to `/tmp/acer-stock/source/` and a separate build output/config was created at `/tmp/acer-stock/out/`.
- The isolated config keeps the stock MT6761 project and `tb8766p1_64_bsp` overlay, disables unavailable camera sensors/calibration and the missing TEE driver, and selects the available `ili9881c_bns080h030hd981d31b_wxga_hsd8` panel driver.
- The full build log is `out/acer_t482l/build.log`.

### Build update

- The first isolated build attempt stopped because `/tmp` ran out of space while GCC wrote temporary assembly files, not because of a source compile failure.
- Restarted from a clean tracked snapshot under `build/acer_t482l_src/`, with output under `out/acer_t482l/` and `TMPDIR=build/tmp/`. The workspace filesystem had about 11 GB free at the start.
- The first link found unresolved AW87519 amplifier functions because the stock-extracted config had that option disabled. The repository's `tb8766p1_64_bsp_defconfig` enables AW87519, so the isolated config was updated and the build resumed.
- The available space on the project filesystem is sufficient; the connected SD card has not been used.
- After enabling AW87519, the build completed: `vmlinux`, `Image.gz-dtb`, the MT6761 base DTB, and the `tb8766p1_64_bsp` overlay were generated.
- Candidate kernel: `out/acer_t482l/arch/arm64/boot/Image.gz-dtb` (9.7 MiB; SHA-256 `e2b6e394c5c42bb4a8fbc6cb65928accdb921ee6242135f5f69aeaaf73aa4a9a`).
- Generated DTBs: `out/acer_t482l/arch/arm64/boot/dts/mediatek/mt6761.dtb` and `out/acer_t482l/arch/arm64/boot/dts/mediatek/tb8766p1_64_bsp.dtb`.
- The new Acer defconfig was regenerated in a fresh scratch output and confirmed to retain MT6761, the `tb8766p1_64_bsp` overlay, the selected LCM driver, AW87519, and the intended camera/TEE disables.
- The updated `build.sh` was not rerun after its final edits; the successful candidate was built from the isolated clean source snapshot using the matching adjusted config. No image has been repacked or flashed.

## Next steps

1. Review the candidate image and prepare a boot image using the stock boot header and ramdisk.
2. Confirm panel, camera, and TEE support against the tablet's DTBO / vendor files before expecting those features to work.
3. Flash only after the candidate boot image is reviewed and the user requests it.

### Boot image repack (2026-10-28)

- Repacked the built kernel into a separate test image at `out/acer_t482l/boot-acer-t482l-test.img`; the original `/tmp/boot.img` remains untouched.
- Preserved the stock Android 10 / header v2 settings, 2 KiB page size, load offsets, command line, ramdisk, and separate DTB.
- Repacking verification with `unpack_bootimg` succeeded. The extracted kernel SHA-256 matches `Image.gz-dtb` (`e2b6e394c5c42bb4a8fbc6cb65928accdb921ee6242135f5f69aeaaf73aa4a9a`); ramdisk and DTB hashes match the stock extracted components. The resulting file is padded to the stock image size of 32 MiB.
- Test image SHA-256: `47b9e07bf14c2efa39c46cb6ca1e723e84f85737a86d736272e216f214deab51`.
- No ADB, fastboot, or recovery command has been run since the user requested permission before using fastboot or recovery. Awaiting explicit permission before device commands or flashing.

### First device boot attempt and pstore review (2026-10-28)

- In TWRP, cleared the prior `console-ramoops` and `pmsg-ramoops-0` files, then rebooted to fastboot.
- Fastboot identified product `tb8766p1_64_bsp`; `boot` partition size was `0x02000000` (32 MiB), matching the padded candidate image. Flashing `boot-acer-t482l-test.img` completed with `Sending` and `Writing` both `OKAY`, then the tablet was rebooted.
- User reported that the tablet stayed at the splash logo and rebooted, and manually returned it to TWRP.
- Retrieved the resulting pstore files to `/tmp/acer-console-ramoops.log` and `/tmp/acer-pmsg-ramoops.log`.
- The console tail reports kernel `4.9.190+ #2` and `Hardware name: MT8766A (DT)`. It contains Android `init` and charger activity, and ends with `reboot: Restarting system with command 'bootloader'`.
- No `Kernel panic`, `Oops`, or fatal exception appears in the saved console tail. It includes a shutdown call trace through `primary_display_suspend()` / `mtkfb_shutdown()` (kernel taint includes `W`), and pmsg contains a Keymaster 3.0 passthrough error.
- The pstore console starts around uptime 78 seconds and only contains the tail of the session; it does not include the initial boot sequence. The explicit `bootloader` reboot command and display shutdown trace do not, by themselves, identify the cause of the reported splash loop. Preserve these logs and inspect a longer boot log / recovery logs before changing kernel or DT configuration.

### expdb diagnosis and corrected configuration (2026-10-28)

- Read-only copied `/dev/block/mmcblk0p4` (`expdb`, 20 MiB) to `/tmp/acer-expdb.raw`; SHA-256 `5bd7377405c84452175eb6b8fa9c140eb3a4786dc641ba7addd61a3b25574f49`. The initial `dd` output included its 84-byte transfer summary after the partition; `.raw` is the exact 20 MiB partition data.
- expdb contains `Kernel panic - not syncing: Fatal exception` from kernel `4.9.117 #2`, which matches the test kernel in `out/acer_t482l/vmlinux`. TWRP reports a separate `4.9.190+` kernel.
- Panic stack: `__memset_io` -> `mrdump_panic_init` -> `mrdump_mini_init`, during early kernel init; a secondary `bad_mode` trace also passes through `mrdump_mini_init`.
- The failed build config saved as `out/acer_t482l/.config.pre-expdb` had `CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR=0`, while LK's memory map reserves `ram_console` at `0x47C80000` and `minirdump` at `0x47D70000`. The Acer defconfig already sets the matching DRAM address, size, and MRDUMP options. This config mismatch is the likely cause of the invalid early minidump memory mapping and boot panic.
- Regenerated `out/acer_t482l/.config` from `acer_t482l_defconfig`; it now contains `CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR=0x47C80000` and `CONFIG_MTK_AEE_MRDUMP=y`.
- Rebuild with `./build.sh` reached the final link but failed with `No space left on device`; only about 740 MiB remained. The connected SD has 67 GiB free but is mounted read-only, and remount requires sudo authentication unavailable in this session. No corrected kernel or boot image was produced; the earlier test image is the panicking build and must not be flashed again.

### Corrected build and repack (2026-10-28)

- After space was freed, reran `./build.sh` successfully with the regenerated Acer defconfig. The corrected config retains `CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR=0x47C80000` and `CONFIG_MTK_AEE_MRDUMP=y`.
- The script writes the rebuilt kernel to the source-tree path `arch/arm64/boot/Image.gz-dtb`; its SHA-256 is `ae236c6f9d3aa019d371e36dae45c47056346ccb64a66e681ec829925cfe8f1a`.
- Repacked as `out/acer_t482l/boot-acer-t482l-test-fixed.img`, preserving stock header parameters, ramdisk, and separate DTB. `unpack_bootimg` verification confirmed the embedded kernel hash matches the rebuilt artifact and ramdisk/DTB hashes match the stock components.
- Fixed image SHA-256: `3e3a3256bfbe4b446e4ca39615e78f295b17a320828aef101a9ff8151b2d6471`. Image is 32 MiB to match the boot partition size.
- The earlier `out/acer_t482l/boot-acer-t482l-test.img` is the panicking build and remains untouched. The corrected image has not been flashed; get fresh permission before any fastboot or recovery command.

### Fixed-image boot attempt and new pstore result (2026-10-28)

- Before the second test, cleared all 20 MiB of `expdb` on-device and verified the full partition contained zero nonzero bytes. The previous expdb contents remain saved at `/tmp/acer-expdb.raw`.
- Rebooted to fastboot, confirmed product `tb8766p1_64_bsp` and a 32 MiB boot partition, flashed `boot-acer-t482l-test-fixed.img` successfully (`Sending` and `Writing` both `OKAY`), then rebooted.
- The user observed another splash/reboot cycle and manually returned to TWRP.
- Retrieved the new pstore console to `/tmp/acer-console-fixed-attempt.log` (SHA-256 `18c6a837ae1b2378f392088f25fc09eee7f3dc67e2ae96dbd344c69ec19b5da0`) and copied expdb to `/tmp/acer-expdb-fixed-boot.raw` (20 MiB, SHA-256 `ca421cef3fee434a39e12cbc6cd24c94ed85b902bb73e628c6d35a764a12e21d`). No readable panic strings were found in this expdb copy; pstore contains the useful trace.
- The new kernel (`4.9.117+ #31`) successfully initializes MRDUMP at the correct reserved address: `mirdump: reserved 47d70000+3000`. The previous fatal exception in `mrdump_mini_init()` is gone. MRDUMP full-dump setup logs an LK-version mismatch, but this is separate from the observed panic.
- Kernel boot continues into Android init. Init mounts `/system` and `/vendor`, switches root to `/system`, then exits with status 127 (`Kernel panic - not syncing: Attempted to kill init!`). No earlier fatal kernel exception appears. This points to a userspace/init handoff problem; the captured kernel log does not reveal why init exits.
- No additional kernel changes were made after the corrected build. The fixed image remains flashed; do not replace it until the init failure is understood.

### Direct expdb reread and init handoff follow-up (2026-10-28)

- On the next boot attempt with `mrdump.lk=MRDUMP08`, MRDUMP initialized successfully (`mrdump_full_init: MT-RAMDUMP enabled done`), and Android 11 GSI init progressed through `linkerconfig`. Retrieved the TWRP pstore console to `/tmp/acer-console-expdb-enabled.log` (SHA-256 `2483b25202df04241744ab795652bb4e706c950255750bbde2d9aa9dc40ec45b`, 200,498 bytes).
- Immediately before the recorded fatal exception, display logs show repeated ESD check failures and a backlight level transition to zero. The trace enters `disp_pwm_set_backlight_cmdq()` -> `disp_pwm_set_enabled()` -> `disp_pwm_set_drverIC_en()` -> `lcm_backlight_enable()`; the faulting PC is `pinctrl_select_state+0` and LR is `lcm_backlight_enable+0x50` in `drivers/misc/mediatek/lcm/sm5109.c`. That helper calls `pinctrl_select_state(_lcm_gpio, ...)` without checking whether `_lcm_gpio` was initialized. This is a confirmed null/invalid-handle crash on the PWM shutdown path.
- The Acer defconfig selects `ili9881c_bns080h030hd981d31b_wxga_hsd8`, while the generic PWM driver calls the SM5109 backlight helper. This mismatch is a strong porting defect candidate; verify the actual stock panel/backlight GPIO setup before choosing between removing that call for this target or implementing the correct GPIO handling. Do not infer the physical panel solely from the temporary `NT35521 START_KERNEL` printk marker; that string is in `init/main.c` as a diagnostic marker.
- The trace is generated while PID 1 is already executing `kernel_restart()` and shutting devices down. It therefore confirms a fatal secondary crash during shutdown, but does not by itself prove that this is the original cause of Android init's reboot request. The earlier display ESD failures are relevant but their causal role is not yet established.
- No source or device changes were made for this investigation. expdb remains effectively empty despite successful MRDUMP initialization; pstore contains the actionable trace.

### Backlight shutdown fault guard and rebuilt test image (2026-10-28)

- Updated `drivers/misc/mediatek/lcm/sm5109.c`: `lcm_backlight_enable()` now checks that the optional SM5109 pinctrl handle and selected LED state are valid before calling `pinctrl_select_state()`. If unavailable, it logs a warning and returns `-ENODEV` instead of causing a null/invalid-handle kernel fault during shutdown. This is defensive; it does not establish that SM5109 is the correct backlight driver for the Acer panel.
- Ran `./build.sh` successfully with `acer_t482l_defconfig`. Built `arch/arm64/boot/Image.gz-dtb` SHA-256: `b1dd29d6771b294b823ca1b097daf5108c6666e1e26d76927100d4ae4a581ada`.
- Repacked `out/acer_t482l/boot-acer-t482l-backlight-fix.img` from that kernel plus the prior test image's header settings, ramdisk, DTB, and `mrdump.lk=MRDUMP08` command line. It is padded to 32 MiB (boot partition size); image SHA-256: `f5e0ddf545ae6e17cac8146c0aa3b7b591bb59e700e21258f5ceaaba0b02adf3`.
- Re-unpacked the candidate to verify its kernel hash matches the build and its ramdisk/DTB hashes match the source test image. No device commands or flashing were performed for this update.
- Remaining blocker: the prior trace shows PID 1 calling `kernel_restart()` before the secondary backlight fault, but the saved pstore does not include the userspace reason for that reboot. The new image is intended to remove the secondary crash and allow a clearer next-boot diagnosis; a successful Android boot is not yet confirmed.

### Backlight-fix boot attempt and fresh pstore (2026-10-28)

- Cleared `/sys/fs/pstore` in TWRP, rebooted into fastboot, confirmed product `tb8766p1_64_bsp` and `boot` partition size `0x02000000`, flashed `boot-acer-t482l-backlight-fix.img` successfully, and issued `fastboot reboot`.
- The tablet returned to TWRP. Pulled the new console record to `/tmp/acer-console-backlight-fix.log` (262,132 bytes; SHA-256 `912a6d5491451641eb30078612c98f869e11a835a72cd5c01ea3301626707206`).
- The defensive SM5109 guard worked: at 3.62 seconds it logged `backlight GPIO state unavailable; skipping`; the prior `pinctrl_select_state` fault does not recur. There is no `Kernel panic` or fatal exception in this record.
- Display ESD checking fails continuously. The panel check reads `0x98` while the selected ILI9881C driver expects `0x87`; ESD recovery also cannot request `DSI_TE-eint` (IRQ 63) because GPIO 44 is flagged as output. After 20 failed recovery attempts, ESD checking is disabled. This strongly points to a panel/DT pin configuration mismatch, though the log alone does not identify the correct panel driver or pin mux.
- At 8.60 seconds, PID 268 (`init`) logs `Reboot ending, jumping to kernel`; at 17.285 seconds PID 1 calls `kernel_restart()` with command `recovery`. The log does not show the userspace reason that requested reboot to recovery. The image therefore boots farther without the secondary backlight panic, but Android still does not complete boot.
- No source changes were made after this test image was built. Next investigate the stock panel identity and DSI TE/backlight pin configuration, plus the init/recovery reboot request, before building another candidate.

### Panel ESD and early-init diagnostic build (2026-10-28)

- Unpacked `/tmp/boot.img` and inspected the stock LK strings and current boot log. The kernel's runtime LCM name, `ili9881_bns080h030hd981d31d_wxga_hsd8`, matches the panel entry in stock LK; no panel-driver swap was justified. The driver source reports 800x1280, while the Acer defconfig/output config had `CONFIG_LCM_WIDTH="720"`.
- Updated `arch/arm64/configs/acer_t482l_defconfig` and `out/acer_t482l/.config` to set `CONFIG_LCM_WIDTH="800"`, matching the selected panel.
- Updated the selected ILI9881 BNS080 driver's `params->dsi.esd_check_enable` to `0` for bring-up. The previous pstore showed this driver's expected DCS 0x0A response was `0x87`, while the panel consistently returned `0x98`; meanwhile the TE EINT path could not claim GPIO 44 because it was configured as output. ESD/TE recovery is disabled until the correct status byte and TE pin mux are confirmed.
- Read-only inspection of the installed GSI/vendor reports Android 11 (`Phh/treble_arm64_bvN/...:11`) with vendor platform policy 29.0; the GSI includes a 29.0 compatibility mapping. The prior kernel log starts SELinux policy compilation but does not record its result before init begins shutdown. This does not prove a policy mismatch.
- Rebuilt successfully with `./build.sh`. Final `arch/arm64/boot/Image.gz-dtb` SHA-256: `82301e2c051327dfdb16bd459c93f79973ca3d68379446e4d7f8113b3b0d9cde`.
- Prepared `out/acer_t482l/boot-acer-t482l-diagnostic.img` using stock header settings, ramdisk and DTB, and command line `bootopt=64S3,32N2,64N2 buildvariant=user mrdump.lk=MRDUMP08 selinux=0`. The `selinux=0` flag is a one-boot diagnostic to test whether early SELinux loading is involved; do not treat this image as a secure daily-use build. Image SHA-256: `3563f4694b131477a04297bd2289922d8c96c144a690a26a8b94f119d4298864`, size 32 MiB.
- Re-unpack verification confirms embedded kernel hash equals the build, ramdisk and DTB hashes match the stock-derived source image, and the command line contains the intended diagnostic flag. Next flash/reboot and inspect pstore; whether the tablet reaches Android second stage will distinguish an early policy-path failure from another init issue.

#### SELinux-off diagnostic result and normal-SELinux test image

- The `selinux=0` diagnostic image was flashed and booted. Fresh pstore `/tmp/acer-console-selinux-test.log` (167,774 bytes; SHA-256 `25f06f2dfa7ae04cebdfa1feeb08b79fbdb07faf1ff352e79c14f643a23e8d6f`) records `SELinux: Disabled at boot`, then Android init failed to mount `selinuxfs`, logged `Init encountered errors starting first stage, aborting`, and rebooted with command `bootloader`. This is the expected consequence of disabling SELinux for this init; it is not evidence of a kernel panic or a permanent fix.
- No ESD recovery/TE IRQ errors appear in this boot log, confirming the ESD-disable change suppresses that loop. The backlight guard also returns without fault.
- Prepared `out/acer_t482l/boot-acer-t482l-esd-fix.img` from the same rebuilt kernel and stock-derived ramdisk/DTB, removing the `selinux=0` diagnostic argument while retaining `mrdump.lk=MRDUMP08`. SHA-256 `f7a50ed01413445d529f102a2462874f26fb6248bf03e47b13b3aed5b9f8666d`, size 32 MiB. Unpack verification confirms the kernel, ramdisk, and DTB hashes are correct.
- The SELinux-off diagnostic image was only a test. The next test image keeps kernel SELinux enabled and checks whether the ESD/panel changes let normal init progress.

- Directly reread `/dev/block/by-name/expdb` from TWRP into `/tmp/acer-expdb-direct-current.raw` (exactly 20 MiB). SHA-256 `ca421cef3fee434a39e12cbc6cd24c94ed85b902bb73e628c6d35a764a12e21d`, identical to the immediately preceding dump; only 4 bytes in the entire partition are nonzero, with no readable panic/crash text. expdb currently contains no useful new report.
- Inspected the stock boot ramdisk locally: it contains a static AArch64 Android 10 `init` (1,842,080 bytes) plus both `fstab.mt6761` and `fstab.mt8766`.
- The remaining pstore failure is after the kernel starts Android init, mounts `/system` and `/vendor`, and switches root to `/system`; init then exits 127. Need read-only inspection of the mounted system's `/system/bin/init` and dynamic linker to diagnose the failed second-stage handoff. No further device or kernel changes have been made.

### GSI init and expdb parameter follow-up (2026-10-28)

- User confirmed Android 11 Phh GSI is installed. The actual image identity is `Phh/treble_arm64_bvN/phhgsi_arm64_ab:11/RD2A.211001.002/...`; `ro.treble.enabled=true`.
- Correction to the prior init inspection: the GSI image has an absolute top-level `bin -> /system/bin` symlink. Pulling through that path resolved against TWRP's root, so the first `init`/`init.rc` files pulled were from TWRP, not the GSI. The corrected files were pulled from the nested GSI paths under `/tmp/acer-system-ro/system/`.
- Actual GSI init is Android 11, dynamically linked with interpreter `/system/bin/bootstrap/linker64`; that bootstrap linker exists. Bootstrap `libc.so`, `libm.so`, and `libdl.so` are present, and the other libraries listed in init's DT_NEEDED are present. A read-only `chroot ... linker64 --list` attempt in TWRP segfaulted and is inconclusive. The captured kernel log still does not identify why PID 1 exits with status 127.
- Kernel source defines `MRDUMP_GO_DUMP` as `MRDUMP08`; `mrdump_full.c` exposes parameter `mrdump.lk`, and LK contains the `MRDUMP08` marker. The failed boot's kernel command line did not include `mrdump.lk=MRDUMP08`, matching the empty LK string and `MT-RAMDUMP init failed` message. This explains why no new expdb dump was written.
- Prepared `out/acer_t482l/boot-acer-t482l-expdb-test.img` with `mrdump.lk=MRDUMP08` appended to the stock header command line. `unpack_bootimg` verifies the new argument and confirms the kernel matches the corrected build and stock ramdisk/DTB remain unchanged. Image SHA-256: `4cce17d5ba3bfd4ee0a2f0ad6d7914d71afd4dfd82a4134f58ba22a2794311a2`.
- This candidate has not been flashed. It targets MRDUMP/expdb support; the GSI init exit still needs a boot attempt with useful crash capture to confirm its cause.

### Expdb-enabled image flashed (2026-10-28)

- User authorized future fastboot/recovery commands without separate permission requests.
- Verified the connected device as `tb8766p1_64_bsp` and boot partition size as 32 MiB.
- Flashed `out/acer_t482l/boot-acer-t482l-expdb-test.img` to `boot`; fastboot `Sending` and `Writing` both returned `OKAY`.
- Rebooted the tablet to test the `mrdump.lk=MRDUMP08` boot argument. Awaiting the boot result before pulling the next pstore/expdb contents.

### Stock `super` kernel test and Oops diagnosis (2026-10-28)

- User restored the stock `super` image; the candidate Acer boot image remained the kernel under test. Rebooted from TWRP to Android to test the candidate against the stock system.
- The tablet showed the splash briefly and rebooted; user forced it back to TWRP. Pulled pstore to `/tmp/acer-console-stock-system-boot.log` (213,913 bytes; SHA-256 `1db6bf70bd9078a4f7ae9e34223fb928b227630fc5b7c0e663bdfa8f456e983d`). expdb was read-only copied to `/tmp/acer-expdb-stock-system.raw` (20 MiB; SHA-256 `237e308471131b64e407dba27bda27c593ae7697a339eab14fd840367d0351`); it is all zeroes. TWRP had no `pmsg-ramoops-0` file for this attempt.
- This is the first saved log showing the candidate kernel (`4.9.117+ #34`) starting the restored stock system. MRDUMP initializes at reserved `0x47d70000`; the prior early `mrdump_mini_init()` panic is not present.
- At 3.405 seconds, PID 272 (`ueventd`) faults while `copy_process()` duplicates the parent process's memory map. `scripts/faddr2line` maps `copy_process.constprop.0+0x1808` into `dup_mmap()` (`kernel/fork.c:689`), specifically the `tmp->vm_ops->open(tmp)` callback. The attempted callback address is `0xffffffee00000000`, unmapped; this is a kernel Oops, followed by the recovery reboot. The record has no userspace explanation beyond the Oops/reboot sequence.
- Early boot also emits an unrelated warning from `ccci_util_fo_init()` when `__ioremap_caller()` rejects a modem log buffer mapping; modem initialization then disables MD1. `cfg80211` warns that `db.txt` is empty. These warnings precede the Oops and are not yet linked to it.
- The callback pointer suggests a bad or corrupted VMA operation pointer. The captured register/stack data does not identify the mapping or driver that supplied it. No kernel source/config workaround has been made; next targeted step is diagnostic logging of the VMA's `vm_ops`, `open` pointer, and backing file immediately before this callback, then confirm whether the pointer is corrupt and identify the mapping before changing behavior.

### VMA callback instrumentation build (2026-10-28)

- Added temporary diagnostic logging in `kernel/fork.c` immediately before the `tmp->vm_ops->open(tmp)` call. It logs only when the current task is `ueventd`, and records the VMA range, `vm_ops` address, `open` callback address, and backing file (or identifies an anonymous mapping). This does not skip or alter the callback; it is intended to identify the bad mapping on the next boot.
- Ran `./build.sh` successfully. Kernel reports `4.9.117+ #35`; `arch/arm64/boot/Image.gz-dtb` SHA-256 is `9aad658a90a902b8b378b9fb2345d13dfeef802c8b0cdd0a33460a8886a6b558`.
- Repacked `out/acer_t482l/boot-acer-t482l-vma-diagnostic.img` with the stock boot header settings, stock ramdisk and DTB, and `mrdump.lk=MRDUMP08`. Image size is 32 MiB; SHA-256 `dac5c652ed94d75f4c3a7846376a03d56df01f8dce0ae68280100caf0223baac`.
- Unpack verification: embedded kernel hash matches the build; ramdisk SHA-256 matches stock (`a0668351a5864fac97f908b721523f60988b320bab74ef88ac93382ade405819`); DTB SHA-256 matches stock (`ae62e7958de96a67fe860e57672ac1aa31d3916d10381c914b06db9089191d76`).
- This is a diagnostic image, not a confirmed fix. It has not been flashed. The next test should capture the `dup_mmap: ueventd` line immediately before the fault to identify the backing VMA and correct the responsible driver or mapping.

### VMA diagnostic boot result and stock baseline check (2026-10-28)

- Cleared `/sys/fs/pstore/console-ramoops` and any pmsg record in TWRP, then rebooted to fastboot. Confirmed product `tb8766p1_64_bsp` and 32 MiB `boot`; flashed `boot-acer-t482l-vma-diagnostic.img` successfully and rebooted.
- User observed a brief splash/reboot and forced TWRP. Fresh pstore is `/tmp/acer-console-vma-diagnostic.log` (195,806 bytes; SHA-256 `a6080f69c5b080be44643eb28846154db89aedf1cb7f92e713ae78d56d1ebef6`). expdb was copied to `/tmp/acer-expdb-vma-diagnostic.raw` (20 MiB; SHA-256 `237e308471131b64e407dba27bda27c593ae7697a339d1eab14fd840367d0351`), matching the prior all-zero partition. No `pmsg-ramoops-0` file was present.
- The candidate kernel is `4.9.117+ #35`; MRDUMP initialized. This attempt did not reproduce the earlier `ueventd` Oops. Instead, Android PID 1 received signal 11 and the kernel panicked with `Attempted to kill init! exitcode=0x0000000b` at 3.302 seconds, while parsing vendor init files. No `ueventd` process or `dup_mmap: ueventd` diagnostic line appears before the panic. The callback instrumentation therefore did not identify the original bad mapping; this is a distinct/earlier userspace failure and is not evidence the kernel issue is fixed.
- In TWRP, mounted `/dev/block/mapper/system` read-only at `/tmp/stock_system_ro`; its `system/build.prop` identifies `Acer_One8-T4-82L`, Android 10 / SDK 29, confirming the stock system logical partition is active. Mount exists only in the recovery session.
- Attempted to reboot to bootloader and run `fastboot boot /tmp/boot.img` as a non-flashing stock-kernel baseline. The device disconnected during the reboot transition; fastboot reported `< waiting for any device >`, and subsequent ADB/fastboot enumeration found no device. The stock kernel was not booted. Current `boot` partition remains the VMA diagnostic image unless the user changes it.
- Next decisive step: when the tablet enumerates again, test the untouched stock `/tmp/boot.img` against this confirmed stock `super` (temporary `fastboot boot`, if supported). If stock boots, capture the PID 1 SIGSEGV address/context from a targeted kernel diagnostic before modifying VM callback behavior. Do not treat the temporary logging as a fix.

### Recovery logs after pixelated/stuck screen report (2026-10-28)

- User forced the tablet back to TWRP after the pixelated screen. TWRP is connected over ADB and runs kernel `4.9.190+ #2`.
- Current `/sys/fs/pstore/console-ramoops` was pulled to `/tmp/acer-console-pixel-stall.log` (262,132 bytes; SHA-256 `a8031f74540094f1d4abb8d2774cfc24d9552b2ae2b79e2ac1d8c9226d30b32d`). Its retained tail is TWRP's own runtime (roughly 43–169 seconds), not the failed candidate kernel boot. At 169.95 seconds it records the explicit `reboot,bootloader` request issued from recovery. It also contains repeated TWRP-side panel ESD failures (`0x98` read versus `0x87` expected, and unavailable `DSI_TE-eint` IRQ 63); no direct log message explains the visible pixels.
- The current expdb copy `/tmp/acer-expdb-pixel-stall.raw` is 20 MiB (SHA-256 `ebd440a9811fccf450c455b378d3e1f1891e1c2fc5f20a3413c9c82e87f676e4`) with 3,586,682 nonzero bytes. Its section directory includes `SYS_RAMCONSOLE_RAW`, `SYS_PSTORE_RAW`, `SYS_MINI_RDUMP`, and `SYS_KERNEL_LOG_RAW_169932`; readable kernel-log text includes the same TWRP reboot-to-bootloader event at uptime 169.95. This is a recovery reboot dump, not evidence of a crash dump from the pixelated candidate boot.
- Current pmsg was pulled to `/tmp/acer-pmsg-pixel-stall.log` (18,093 bytes; SHA-256 `28ba95609c1d8ad17aa436c83e31b84de60ec242fe5486dcfce0ea1ba27dd1fa`); it contains recovery service/SELinux messages, not a candidate-kernel crash trace.
- The earlier candidate log remains `/tmp/acer-console-vma-diagnostic.log` (kernel `4.9.117+ #35`), with PID 1 SIGSEGV at 3.302 seconds before `ueventd`. No VMA diagnostic line was emitted. `expdb` from that earlier attempt was all zeroes.
- Tried capturing TWRP's framebuffer with `adb exec-out screencap -p`; TWRP reports `screencap: inaccessible or not found`, so visual confirmation over ADB is unavailable. No kernel source change or new flash was made in this follow-up.

### Temporary stock boot image display comparison (2026-10-28)

- From TWRP, rebooted to fastboot and ran `fastboot boot /tmp/boot.img`. Fastboot reported `Sending 'boot.img'` and `Booting` both `OKAY`. This was a one-time in-memory boot; the boot partition was not written. The installed boot partition remains the diagnostic kernel image.
- User reports and provided a photo showing the same full-screen black/white/gray pixel noise after this stock boot image was loaded. Thus the artifact is not limited to the newly built diagnostic kernel. The test used the saved stock boot image against the restored stock `super`; it does not by itself distinguish a panel/DSI hardware issue, a bootloader/kernel handoff issue, or a mismatch in the saved stock image.
- No kernel source/config changes or further flashes were made. Next comparison is whether the display is clean in bootloader and TWRP; if those are also corrupted, prioritize panel/display hardware and flex/connector inspection over kernel source changes. If only Android boot is corrupted, capture a fresh candidate/stock boot trace and compare panel initialization.

### Display artifact confirmed before Android boot (2026-10-28)

- User reports black/white pixel noise on the LK boot selection screen (recovery/fastboot chooser). TWRP does **not** show the same pixel pattern; it has only a white glow at the display edges.
- The same noisy pattern was also reported during the one-time stock `/tmp/boot.img` boot, so it is not limited to the newly built Android kernel. The different TWRP symptom leaves both display hardware (LCD/flex/connector/backlight) and LK/TWRP panel initialization in scope. Earlier TWRP pstore showed an ESD status mismatch (`0x98` versus expected `0x87`) and unavailable TE IRQ 63, but those messages alone do not prove hardware failure.
- No kernel changes or flashes made in response. The display symptoms need to be distinguished before attributing them to the Android kernel; then resume kernel boot testing with a reliable display.

### PID 1 fatal-signal diagnostic image (2026-10-28)

- The last candidate log shows Android PID 1 dying with SIGSEGV (`exitcode=0x0000000b`) while parsing vendor init files, but the retained log does not identify the fault address or userspace registers.
- Repacked the last built kernel from `boot-acer-t482l-vma-diagnostic.img` (kernel SHA-256 `9aad658a90a902b8b378b9fb2345d13dfeef802c8b0cdd0a33460a8886a6b558`) with the stock ramdisk and DTB. Added `print-fatal-signals=1` and retained `mrdump.lk=MRDUMP08` on the command line. No kernel source changed for this diagnostic.
- Image: `out/acer_t482l/boot-acer-t482l-sigdiag.img`, padded to 32 MiB, SHA-256 `29834a52ddc3bcbb11ae7fc243def6968407113e63a1a9ef6344f03fae42312d`. `unpack_bootimg` confirms the requested parameters and command line.
- No flash was performed in this step. Device enumeration returned no fastboot device; ADB daemon startup was denied by the sandbox (`Operation not permitted`). The image awaits a working device connection.
- This only adds crash diagnostics. It does not claim to fix the pre-Android LK display noise or TWRP edge glow, which have also been reported with stock boot/display paths.

#### Diagnostic boot attempt (2026-10-28)

- Cleared `/sys/fs/pstore` in TWRP, rebooted to fastboot, flashed `boot-acer-t482l-sigdiag.img` to `boot` (`Sending` / `Writing` both `OKAY`), and rebooted. The user reported a brief splash followed by a reboot, then forced the tablet back to TWRP. The user also reports the pixelated display artifact is absent on this attempt.
- The pstore console pulled afterward is byte-for-byte the earlier TWRP log (SHA-256 `a8031f74540094f1d4abb8d2774cfc24d9552b2ae2b79e2ac1d8c9226d30b32d`), so it is stale and does not contain this boot. TWRP currently runs `4.9.190+ #2` and reports `androidboot.bootreason=lk_crash`.
- Read-only copied the 20 MiB expdb partition to `/tmp/acer-expdb-sigdiag.raw` (SHA-256 `f6a561142affa8269d654b14919f092681dfdbae04e8c43e6c377605a03374a3`). This contains the current candidate's full kernel log: `4.9.117+ #35` started Android and reached `post-fs-data`, then PID 1 Oopsed at about 5.2 seconds while starting `/system/bin/vdc checkpoint prepareCheckpoint`.
- The exception is in `copy_process()` during `clone()`, immediately after `anon_vma_fork()`, while handling a file-backed VMA. The register dump has an invalid low userspace-looking value (`0x2e747369737265a8`) being dereferenced as a kernel file pointer; the fault address is `0x74736973726590`. The boot argument `print-fatal-signals=1` is present, but this is a kernel paging Oops rather than a userspace fatal signal, so it does not print a user register report.
- This advances beyond the earlier PID 1 SIGSEGV but still does not boot. The display artifact being absent is encouraging but does not prove its cause. No new source change was made. Next inspect the exact VMA/file pointer corruption before applying any guard that would skip or alter a mapping.

### Invalid VMA file-pointer diagnostic (2026-10-28)

- Mapped the expdb faulting instruction with the matching root `vmlinux` from build #35: `copy_process()` inlined VMA duplication faults at `file_inode(file)` while handling `tmp->vm_file`; the pointer value is non-canonical userspace-looking data. This is a kernel Oops during init's `clone()`, not a userspace SIGSEGV.
- Added a diagnostic in `kernel/fork.c` immediately after `file = tmp->vm_file`: for PID `init`, if a non-null pointer fails `virt_addr_valid()`, log the VMA start/end, flags, raw `vm_file`, and raw `vm_ops`. It does not dereference or change the mapping.
- `./build.sh` succeeded. Kernel build #36 is `arch/arm64/boot/Image.gz-dtb`, SHA-256 `2c9859a109a02f93536d2f9e6af8417585c6e6ec8b19a132d355b495a8838f5d`.
- Repacked `out/acer_t482l/boot-acer-t482l-vmfile-diagnostic.img` with the stock ramdisk and DTB, preserving stock header settings and `mrdump.lk=MRDUMP08 print-fatal-signals=1`. Image SHA-256 `b8df2fe2d618f03f79c96a25da2ff599928ecd7a9a7bd02f95234c17040dd787`, padded to 32 MiB. `unpack_bootimg` verified the image parameters and command line.
- Cleared pstore, flashed the image to `boot` (`Sending` / `Writing` both `OKAY`), and rebooted. Awaiting the boot result and a fresh expdb dump to determine which VMA is corrupt.

### Fatal-signal detail capture and panel handoff test (2026-10-28)

- Added fatal-signal detail logging in `kernel/signal.c`: for fatal signals, log `si_code`, `si_addr`, and the current process code/data/stack ranges. This is intended to symbolize user-space faults despite ASLR.
- Build #37 completed and was packaged as `out/acer_t482l/boot-acer-t482l-siginfo-diagnostic.img`, retaining stock ramdisk/DTB and `mrdump.lk=MRDUMP08 print-fatal-signals=1`. The image was flashed after clearing pstore and expdb; flash and reboot returned `OKAY`.
- Fresh #37 console `/tmp/acer-console-siginfo-diagnostic.log` (262,132 bytes, SHA-256 `bba1774c4f80fd190baf90da959acb225d223979930681baff29149a7c873b87`) and expdb `/tmp/acer-expdb-siginfo-diagnostic.raw` (20 MiB, SHA-256 `98b64b4f642ae21cc2122de21b2375b1962459cd38048374a64bcd7a3c87d484`) show kernel #37 running Android through at least 94 seconds with no kernel panic. The earlier PID 1 SIGSEGV did not recur in this run. `servicemanager` received SIGABRT (signal 6) at 4.19 seconds, followed by recurring Binder failures from `vdc` and `vold`; the log does not show `sys.boot_completed` or prove the UI came up. User reports the pixelated display artifact returned and manually forced TWRP.
- Panel probe selects `ili9881_bns080h030hd981d31d_wxga_hsd8`; LK hands the kernel `is_lcm_inited=1`, causing the display code to trust LK's panel state. The Linux ILI driver therefore skips its own reset/init sequence.
- Changed `drivers/misc/mediatek/video/mt6765/videox/mtkfb.c` to force `is_lcm_inited=0` only when the parsed panel name exactly matches the Acer ILI9881 panel. This tests whether re-running the selected Linux panel initialization fixes Android-side rendering. It does not affect LK's pre-kernel chooser.
- Build #38 succeeded. Kernel image `arch/arm64/boot/Image.gz-dtb` SHA-256 `c8dbc4410b02cb946f53fbedfccc2688dea4d9bfa2c9d833330f0737f1dc3d48`; repacked as `out/acer_t482l/boot-acer-t482l-panel-reinit.img`, 32 MiB, SHA-256 `acf1e472d02dcde5c83fdf3d0ec68efdd8330d2465d036338128f674d0e5a0c8`. `unpack_bootimg` confirms stock header settings, ramdisk/DTB, and diagnostic command line.
- Image #38 has **not** been flashed. At setup, the tablet appeared in fastboot; `fastboot reboot recovery` returned `OKAY`, but both ADB and fastboot enumeration then showed no device. The next step is to reconnect or return the tablet to TWRP/fastboot, clear old pstore/expdb, and test #38.

#### #38 panel-reinit result and #39 rollback test

- After TWRP reconnected, cleared pstore and expdb, flashed `boot-acer-t482l-panel-reinit.img`, and rebooted. The candidate log reports DSI buffer underrun near 1.4 seconds, then many invalid page-table entries and an Oops in `free_pgd_range()` at about 9.69 seconds while Android init was executing a program. The kernel had reached second-stage init/SELinux setup. This panel-reinit experiment did not produce a stable boot; its effect on the pixelation was not confirmed.
- Compared timer-related config against `/tmp/acer-stock-kernel.config`: stock and port both use `CONFIG_HZ=250`, `CONFIG_ARM_ARCH_TIMER=y`, `CONFIG_NO_HZ=y`, and `CONFIG_SCHED_HRTICK=y`. There is no HZ/timer config mismatch found.
- Removed the scoped forced-panel-init override from `drivers/misc/mediatek/video/mt6765/videox/mtkfb.c`; retained fatal-signal and VMA diagnostic code. Build #39 completed, kernel image SHA-256 `9839ceb7966bbaa0f066277dbb6b0a6b7a3620d7673be27a2554463da8d76e12`.
- Repacked and verified `out/acer_t482l/boot-acer-t482l-mm-diagnostic.img` (32 MiB, SHA-256 `c463be608bca807d003c610895bdd1b9c89cdadcabbae5106133338992c5b865`) with stock header settings, ramdisk and DTB, plus `mrdump.lk=MRDUMP08 print-fatal-signals=1`.
- Cleared old pstore/expdb, flashed #39 to `boot` (`Sending` / `Writing` both `OKAY`), and rebooted. Fresh console `/tmp/acer-console-mm-diagnostic.log` (245,990 bytes, SHA-256 `d172cc99542cccd28a112397f05e8a9f3594d9beb914099f50b70fcc18f0237d`) identifies #39 and records an Oops at 9.677 seconds in `dst_release()+0x18`, called from `skb_release_head_state()` while `logd.writer` frees an skb; x0 is `0x8`, an invalid `dst_entry` pointer. The device returned to TWRP. Expdb SHA-256 `ca421cef3fee434a39e12cbc6cd24c94ed85b902bb73e628c6d35a764a12e21d` has no new readable dump.
- Interpretation: the 9.7-second printk timestamp is elapsed kernel time at the first fault. Panic reporting continues afterward (the #39 call trace is timestamped around 10.68 seconds). The fault points to invalid networking skb/destination state, not clock scaling. The boot partition currently contains #39.

### SLUB corruption diagnostic build (#40, 2026-10-28)

- The #39 fault passes `0x8` from `skb->_skb_refdst` to `dst_release()` while `logd.writer` frees a socket buffer. The networking code is detecting an already-invalid skb field; changing `dst_release()` would mask corruption without fixing its source.
- Enabled `CONFIG_SLUB_DEBUG=y` and `CONFIG_PAGE_OWNER=y` in `arch/arm64/configs/acer_t482l_defconfig` and the active build config. This activates the boot command line's existing `slub_debug=OFZPU page_owner=on` requests so SLUB can report damaged objects and page ownership can help trace page allocations.
- `./build.sh` completed successfully. The kernel is `4.9.117+ #40`; built kernel SHA-256 `7a127618f8db5f3f1340a2b9932f409d832c1174a4620206da4478e3fc5b8987`.
- Packaged as `out/acer_t482l/boot-acer-t482l-slub-diagnostic.img`, padded to the 32 MiB boot partition. SHA-256 `e996d9ac0cf291738e73da2c4c561c81f89e97416663424846877edf596db063`. Unpack verification confirms the embedded kernel matches the build and stock ramdisk (`a0668351a5864fac97f908b721523f60988b320bab74ef88ac93382ade405819`) and DTB (`ae62e7958de96a67fe860e57672ac1aa31d3916d10381c914b06db9089191d76`) are unchanged. The image includes `mrdump.lk=MRDUMP08 print-fatal-signals=1`.
- This was a diagnostic candidate, not a confirmed fix. It was subsequently flashed and tested; see the next section. The previous `/tmp` console capture is no longer available in the current session, so the result below is from the saved test notes pending a fresh device pull.

### SLUB diagnostic result and #41 invalid-pointer fault

- The saved notes report boot reached about 58 seconds before a crash, later than the earlier ~9.7-second failures. Fresh expdb now verifies the crash: `/tmp/acer-expdb-slub-current.raw` is 20 MiB, SHA-256 `89cd88e02dcd1e7c8dccf5aedc696df1a2ca86015e8e1320cce52837a42ba225`. The fresh pstore directory in TWRP was empty.
- The fault is a translation fault, not an alignment fault: PID 414 (`hwservicemanage`) hit `Unable to handle kernel paging request` at `0x6761742e676f44`, with PC `vsnprintf+0x730`. `scripts/faddr2line` maps this to `lib/vsprintf.c:2101` while reading the variadic integer argument. The caller is `proc_flush_task_mnt()` at `fs/proc/base.c:3008`, formatting a PID with `snprintf(buf, ..., "%d", pid)`, called from `release_task()` during process exit. The variadic argument cursor is invalid by the time `vsnprintf()` reads it; the dump does not reveal where it was corrupted.
- The dump confirms `SLUB: HWalign=64` and the command line includes `slub_debug=OFZPU page_owner=on`, but contains no SLUB object-corruption report before the Oops. This does not rule out stack or non-SLUB memory corruption.
- The notes identify the crashing kernel as `4.9.117+ #41`, although the packaged #40 image was built as `#40`; that build-number discrepancy is unresolved. Repacking itself does not increment the kernel build number.
- The boot reached Android's `hwservicemanage` and other userspace services, suggesting Android initialization progressed. This is not a working boot: it crashed and returned to TWRP. The current recovery is `4.9.190+ #2`; boot reason is `PowerKey`. ADB is connected again.

### Fresh SLUB diagnostic expdb review (2026-10-29)

- Copied `/dev/block/by-name/expdb` read-only from TWRP to `/tmp/acer-expdb-slub-current.raw`; SHA-256 `89cd88e02dcd1e7c8dccf5aedc696df1a2ca86015e8e1320cce52837a42ba225`. TWRP's `/sys/fs/pstore` was empty.
- The dump identifies the crashing kernel as `4.9.117+ #41`, while the previously packaged image was recorded as #40. This image/build discrepancy remains unresolved; confirm the exact kernel artifact before another flash.
- No source fix has been made from this trace because the invalid variadic argument pointer has no identified writer. Next step is to inspect the #41 caller/stack state against the exact matching `vmlinux`, then add targeted diagnostics or fix the originating corruption once identified. Do not mask it in `vsnprintf()` or `proc_flush_task()`.
- The matching #41 `vmlinux` is not present in the current outputs. The available #42 `vmlinux` maps the same relative frames to `proc_flush_task_mnt()` at `fs/proc/base.c:3008` and `vsnprintf()` at `lib/vsprintf.c:2101`; this confirms the source path but is not an exact-build symbol match.

### Targeted proc-exit diagnostic build (#43, 2026-10-29)

- Added a `hwservicemanage`-only `pr_err()` immediately before the first `snprintf()` in `proc_flush_task_mnt()` (`fs/proc/base.c`). It records current/target IDs and task, mount, local buffer, marker, and stack addresses; it does not change PID cleanup behavior.
- `./build.sh` completed successfully. Kernel is `4.9.117+ #43`; `arch/arm64/boot/Image.gz-dtb` SHA-256 `59e6701a487dff00ec822dc4b450fe750df76bf22c4472673cd0f9240759e762`.
- Packaged as `out/acer_t482l/boot-acer-t482l-procdiag.img`, padded to 32 MiB; SHA-256 `1e5c3b4d956d346f0e67620c148aee5e93122e0ab6b58a6c779e72eec3feb485`. Unpack verification confirms the embedded kernel matches the build and the stock ramdisk (`a0668351a5864fac97f908b721523f60988b320bab74ef88ac93382ade405819`) and DTB (`ae62e7958de96a67fe860e57672ac1aa31d3916d10381c914b06db9089191d76`) are unchanged. The image includes `mrdump.lk=MRDUMP08 print-fatal-signals=1`.
- Cleared on-device pstore and expdb after saving the previous expdb copy on the host, flashed #43 to `boot`, and rebooted. Fastboot `Sending`, `Writing`, and `Rebooting` all returned `OKAY`.
- User forced the tablet back to TWRP. Fresh pstore was pulled to `/tmp/acer-console-procdiag-current.log` (240,693 bytes; SHA-256 `3796b625fba0ad5563dfc66335cc9eedd641fc23b7d4e7a5a889722c17d1ce77`). It identifies #43 and runs through at least 10.256 seconds. Six `proc_flush_task_mnt` diagnostics were emitted for `hwservicemanage` child processes (PIDs 356, 358, 359, 361, 362, 363); their task, stack, buffer, and mount addresses look like valid kernel addresses. No Oops or SLUB corruption report is present in this pstore capture.
- Read-copied expdb is `/tmp/acer-expdb-procdiag-current.raw` (20 MiB; SHA-256 `1219716f7bef5c1e90fb5d21d6af657502e1ef4b40b21651a78b609198c75f81`). Its #43 log contains `BoringSSL crypto self tests failed` at about 4.92 seconds, followed by init requesting `reboot,bootloader,boringssl-self-check-failed`. This is an explicit userspace-triggered reboot, not a kernel panic/Oops. Expdb's BoringSSL shutdown and pstore's later 6–10 second diagnostic lines cannot be confidently reconciled as one uninterrupted boot; the retained buffers may represent separate #43 attempts. Keep both captures and avoid treating the self-test failure as proof of the earlier variadic-pointer fault's cause.
- No source changes were made after #43 and no further device commands were issued during log review. The kernel is still not confirmed working.

### Crypto HWCAP isolation build (#44, 2026-10-29)

- Confirmed the device runs Cortex-A53 cores. TWRP's `uname` reports its own recovery kernel (`4.9.190+ #2`); this is separate from the image on the boot partition. Read-only SHA-256 of `/dev/block/by-name/boot` is `1e5c3b4d956d346f0e67620c148aee5e93122e0ab6b58a6c779e72eec3feb485`, matching the #43 candidate exactly. `/tmp/boot.img` is the stock `4.9.190+ #2` image, with stock ramdisk and DTB hashes matching the recorded values.
- AOSP init source confirms that the BoringSSL self-test is run in a child process and explicitly requests a bootloader reboot if `BORINGSSL_self_test()` returns failure. The kernel log does not identify which test failed. The #43 pstore and expdb records still appear to span separate attempts.
- For an A/B diagnostic only, changed `arch/arm64/kernel/cpufeature.c` to log the computed `elf_hwcap`, then mask AES, PMULL, SHA1, SHA2, and CRC32 capability flags. This forces generic software crypto implementations and leaves FP/ASIMD enabled. If the BoringSSL reboot disappears, investigate capability exposure/accelerated crypto; if it remains, this hypothesis is weakened. This is not a production fix and the mask should be removed after diagnosis.
- `./build.sh` succeeded. Built `Image.gz-dtb` SHA-256 `afdf02b4804ac69ceee43214fd62d0dfcbf04bda844d499246059840fb95c477`. Packaged `out/acer_t482l/boot-acer-t482l-hwcap-diagnostic.img` with the stock header, ramdisk, and DTB, plus existing diagnostic command-line flags; padded to 32 MiB, SHA-256 `7262096ed18438b116ab204f96bde7b76e9027e5466fecc9525d775bd9656ec3`. Unpack verification confirms the embedded kernel hash and stock ramdisk/DTB hashes. Boot result is recorded below.
- Correction: #44 was flashed directly to `boot` and the partition hash confirmed as `7262096e…9656ec3`. After reboot, the tablet returned to TWRP. Fresh pstore contains only TWRP's `4.9.190+ #2` session; expdb still contains the prior #43 failure and has no #44 banner or HWCAP runtime lines. Therefore this test does not tell us whether the HWCAP mask changed the Android result.
- The retained #43 expdb gives a stronger lead: PID 1 faulted in `copy_process()` at `dup_mmap()` while init was starting the BoringSSL self-test. Fault address is `0x3d0900000fa0`; the subsequent BoringSSL self-test failure/reboot is likely downstream of this process-creation fault, though causality is not proven. The exact #43 `vmlinux` is missing; mapping the same offset through #45's `vmlinux` is only approximate. Do not patch out the BoringSSL reboot or skip `vm_ops` callbacks.

### Invalid VMA callback diagnostic build (#45, 2026-10-29)

- Added a narrowly gated log in `kernel/fork.c` before the `tmp->vm_ops->open` check. When PID 1 is cloning a VMA whose copied `vm_ops` pointer is below `PAGE_OFFSET`, it records old/new VMA addresses, ranges, flags, files, and the bad pointer without dereferencing it. The existing `ueventd` callback log remains.
- `./build.sh` succeeded. `Image.gz-dtb` SHA-256 `e5e3dd1d3b821ddacca5c04bc2a7c392c61851ae1b96e38ede2eb70c2cb5d1be`; kernel release identifies #45. Packaged `out/acer_t482l/boot-acer-t482l-vma-init-diag.img`, padded to 32 MiB, SHA-256 `4761a4b53ed6e149428aeac181a83dca4db60a9732707b0eadef2971504ad7bf`. Unpack verification confirms matching kernel, stock ramdisk and stock DTB hashes.
- Before the #45 test, returned to TWRP, confirmed the boot partition still held #44 (`7262096e…9656ec3`), removed only `console-ramoops` and `pmsg-ramoops-0`, and verified `/sys/fs/pstore` was empty. Then flashed #45 directly with `fastboot flash boot`; `Sending` and `Writing` both returned `OKAY`. Rebooted; user forced the tablet back to TWRP.
- Pulled fresh files to `/tmp/acer-console-vma-init45.log` (6,963 bytes, SHA-256 `daf44f31f2937d6b836eefefaee89d82c42c1dd466224870f49fb847d4b2128a`), `/tmp/acer-pmsg-vma-init45.log` (26,060 bytes, SHA-256 `450ae2963eebf45a83ef4f5a8cf84413f8ec554e7ce41619cb5dcc67b9484ca3`), and `/tmp/acer-expdb-vma-init45.raw` (20 MiB, SHA-256 `c5f6323f53bfbf15db6c4ff5a0cd5b39d0456fb5a2cbd8fbde8c3ba657d557e3`). Current `boot` partition hash verifies #45 (`4761a4b5…04ad7bf`). The pstore file is TWRP's `4.9.190+ #2` console only. expdb still contains the older #43 Oops and has no #45 banner or `dup_mmap: init invalid vm_ops` diagnostic. Thus #45's Android failure point remains uncaptured; do not claim the VMA log fired or that the HWCAP test passed/failed.

### Stock M4U configuration diagnostic build (#46, 2026-10-29)

- Compared the embedded stock boot configuration extracted from `/tmp/boot.img` against the port's active config. Memory/VA basics match (4K pages, 39-bit VA), but the stock kernel has `CONFIG_MTK_M4U=y` and `CONFIG_MTK_IOMMU_V2` disabled, while the port selected `CONFIG_MTK_PSEUDO_M4U=y` plus IOMMU v2. This is a plausible DMA/IOMMU mismatch given the unrelated invalid kernel pointers, but remains a hypothesis pending A/B boot evidence.
- Changed `arch/arm64/configs/acer_t482l_defconfig` and active `out/acer_t482l/.config` to select stock `MTK_M4U`, disable pseudo-M4U and disable MTK IOMMU v2. Removed the unrelated temporary crypto HWCAP mask from `arch/arm64/kernel/cpufeature.c`; retained the targeted VMA diagnostic in `kernel/fork.c`.
- Enabling stock M4U initially exposed a missing include path for the driver-local `m4u_port.h`. Added `-I$(src)` to `drivers/misc/mediatek/m4u/mt6761/Makefile`; rebuild then succeeded.
- Built `Image.gz-dtb` SHA-256 `32cf50b9904db00ec2f40abb1ab95e82061ebfa2b029f13c81b7c0546dcd159c`. Packaged `out/acer_t482l/boot-acer-t482l-m4u-stockdiag.img`, padded to 32 MiB, SHA-256 `a5866493adb4ff6197358ddcf5c88af19cf5d3248f2ec0e1bfafba28ffa439ff`. Unpack verification confirms embedded kernel, stock ramdisk (`a0668351…405819`), and stock DTB (`ae62e795…191d76`) hashes.
- Cleared pstore in TWRP and verified it empty before flashing #46 directly; `fastboot flash` and reboot returned `OKAY`. The boot partition hash now matches #46. User forced the tablet to TWRP. Fresh pulled pstore is bit-for-bit identical to the preceding TWRP capture and contains only recovery kernel `4.9.190+ #2`; expdb remains the old #43 record and contains no #46 banner. Therefore the #46 Android result is not present in retained logs. The stock M4U hypothesis has not been verified as a fix.

### Expdb and recovery-kernel provenance (2026-10-29)

- The #45 and #46 expdb pulls are byte-for-byte identical: 20 MiB, SHA-256 `c5f6323f53bfbf15db6c4ff5a0cd5b39d0456fb5a2cbd8fbde8c3ba657d557e3`. They contain a MediaTek AEE section named `SYS_KERNEL_LOG_RAW_155916904`, with the #43 `copy_process()` Oops, followed by a `4.9.190+ #2` recovery log. No #45/#46 kernel banner or their diagnostic lines are in that record. This is a retained AEE/MRDUMP record, not proof that TWRP is restoring logs from the boot partition. Clearing expdb only clears existing records; a later kernel/AEE dump can repopulate it. The #43 Oops remains because no newer AEE dump replaced it in the #45/#46 captures.
- Read-only extraction of `/dev/block/by-name/recovery` (partition `mmcblk0p2`, 32 MiB, SHA-256 `080e736973b835b5c36275f53631c26bc7843da499041ffbe6373af3471da8b5`) confirms it is a TWRP Android boot image whose embedded kernel reports `4.9.190+ #2` (clang 9, Dec 2021). The saved stock `/tmp/boot.img` also contains `4.9.190+ #2`. Version `4.9.117+` is the ported kernel built from this source tree; it is not the stock/TWRP kernel version. The running TWRP `uname` matches the recovery partition.
- Current boot partition SHA-256 is #46 `a5866493adb4ff6197358ddcf5c88af19cf5d3248f2ec0e1bfafba28ffa439ff`; current TWRP session does not imply that #46 is the kernel running now.

### Next steps

1. Re-establish ADB from TWRP, then pull pstore and expdb. The current host session reports no ADB device, and the previous `/tmp` console capture is unavailable.
2. Use the fresh faulting PC, registers, and call trace with the exact flashed kernel's `vmlinux` to identify the invalid pointer and its source. Confirm which build actually booted; the noted `#41` versus packaged `#40` mismatch is unresolved.
3. Do not clear userdata or add kmemleak as a response to this alignment fault. Neither action is supported by the available trace; kmemleak tracks leaks rather than arbitrary memory overwrites.


### Build #42 - Debug configuration update (2026-10-27)

- Changed `CONFIG_PANIC_ON_OOPS=y` → disabled to get full crash traces instead of immediate reboot
- Increased `CONFIG_PANIC_TIMEOUT=1` → 10 to allow more debug output
- Kernel rebuild successful: Image.gz-dtb (10.1 MB)
- Repacked boot image with stock ramdisk and dtb at `out/acer_t482l/boot-acer-t482l-debug.img` (11 MB)

