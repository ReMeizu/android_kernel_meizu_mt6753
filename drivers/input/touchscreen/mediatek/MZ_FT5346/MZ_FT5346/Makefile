include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/touchpanel/MZ_FT5346/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/MZ_FT5346/
ccflags-y += -Wno-error=unused-variable -Wno-error=unused-function -Wno-error=date-time -Wno-error=declaration-after-statement -Wno-error=unused-result
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(subst ",,$(CONFIG_ARCH_MTK_PROJECT))/dct/dct/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/MZ_FT5346/MZ_FT5346/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/

obj-y += focaltech_ctl.o focaltech_driver.o focaltech_ex_fun.o
obj-y += self-test/
# mcap_5x46_test_lib.a: self-test/mcap_5x46_test_lib.o self-test/ini.o
#	$(AR) rcs $@ $^

