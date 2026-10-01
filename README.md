# Meizu M2 Note kernel

Linux 3.18.19 sources for the M571 / MT6753 board. The native board uses the
MediaTek `mt6735` build platform. This branch consolidates the existing board
sources with a freshly generated native kernel configuration.

| Component | Source and status |
| --- | --- |
| Board description | Native M2 Note DTS and generated board text; compiled DTB reproduced exactly |
| Display | NT35532 Sharp/Panda and NT35596 AUO panel drivers compile; verify the panel selected by each device |
| Touch | MZ_GT9XX Goodix driver selected; hardware acceptance for this source snapshot pending |
| Audio | MediaTek PCM/I2S codec support and existing board fixes; full route tests pending |
| Modem | CCCI platform and transport sources; user-visible SIM/RIL remains open |
| Android | Native legacy baseline; Android 9 ROM integration pending, no Android 13/BPF compatibility claim |

Historical native kernels have compiled and booted. This clean source
publication has not yet passed a new full kernel build or a device boot.
It is not a flash release. The experimental cgroup/BPF success stubs are
not included in this baseline.

Build configuration: `arch/arm64/configs/m2note_verified.config`.
Board DTS: `arch/arm64/boot/dts/m2note.dts`.
Generated board text: `drivers/misc/mediatek/mach/mt6735/m2note/dct/dct/`.
Run `olddefconfig`, `drvgen`, then `Image.gz-dtb` with the pinned AOSP GCC 4.9
toolchain. The board text is already generated; no opaque DrvGen host program
is required for this board.

Original `COPYING`, `CREDITS` and per-file copyright/license notices are
preserved. Firmware source files inherited from the original public kernel
repository remain unchanged and retain their original provenance.
