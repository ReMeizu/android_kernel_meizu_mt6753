# Meizu M5s kernel

Linux 3.18.19 sources for the M1612 / MT6753 board. The board uses the
MediaTek `mt6735` build platform with eight CPU cores. This branch preserves
the native board sources and a reproducible kernel configuration.

| Component | Source and status |
| --- | --- |
| Board description | M1612 memory, pin, bus and interrupt resources; compiled DTB reproduced exactly |
| Display | `ili9881_CA_hd720_dsi_vdo_yassy` panel driver compiles; hardware acceptance pending |
| Touch | MZ_FT5346 FocalTech driver adapted for M1612; stock firmware compatibility unverified, automatic firmware upgrade disabled |
| Charging | BQ25890 and its charging wrapper compile; hardware acceptance pending |
| Sensors | MC3xxx, PA22x and QMCX983 drivers compile; hardware acceptance pending |
| Radio | Legacy MediaTek connectivity and modem sources; Wi-Fi, Bluetooth, GPS and telephony require device tests |

The native kernel has compiled previously. This clean source publication has
not yet passed a new full kernel build or a device boot. It is not a flash
release or a claim of Android 13 compatibility.

Build configuration: `arch/arm64/configs/m5s_verified.config`.
Board DTS: `arch/arm64/boot/dts/m5s.dts`.
Generated board text: `drivers/misc/mediatek/mach/mt6735/m5s/dct/dct/`.
Run `olddefconfig`, `drvgen`, then `Image.gz-dtb` with the pinned AOSP GCC 4.9
toolchain. The board text is already generated; no opaque DrvGen host program
is required for this board.

Original `COPYING`, `CREDITS` and per-file copyright/license notices are
preserved. Firmware source files inherited from the original public kernel
repository remain unchanged and retain their original provenance.
