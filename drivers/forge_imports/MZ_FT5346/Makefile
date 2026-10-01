include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/touchpanel/MZ_FT5346/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/MZ_FT5346/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/

obj-y += focaltech_ctl.o focaltech_driver.o focaltech_ex_fun.o
obj-y += self-test/
# MZ_FT5346.a: self-test/MZ_FT5346.o self-test/ini.o
#	$(AR) rcs $@ $^

