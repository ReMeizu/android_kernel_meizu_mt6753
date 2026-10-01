include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/dct/dct
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/mt6735/m2note/dct/dct
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
CFLAGS_gt9xx_driver.o += -Wno-error=unused-function -Wno-error=unused-result
CFLAGS_gt9xx_openshort.o += -Wno-error=unused-function

ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/touchpanel/MZ_GT9XX/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/MZ_GT9XX/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/

CFLAGS_goodix_tool.o += -Wno-error=date-time
obj-y	+=  goodix_tool.o gt9xx_driver.o gt9xx_openshort.o gt9xx_update.o

