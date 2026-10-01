#
# Makefile for misc devices that really don't fit anywhere else.
#



define FILTER_PROJECT_DRV
ifeq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/$(MTK_IMGSENSOR_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(1)),)
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/$(MTK_IMGSENSOR_PLATFORM)/$(1)/*.c)$(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/$(MTK_IMGSENSOR_PLATFORM)/$(1)/*/Makefile),)
platform_drv += $(1)
endif
else
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/$(MTK_IMGSENSOR_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(1)/*.c)$(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/$(MTK_IMGSENSOR_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(1)/*/Makefile),)
project_drv += camera_project/$(ARCH_MTK_PROJECT)/$(1)
endif
endif
endef

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
MTK_IMGSENSOR_PLATFORM := mt6753
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

obj-y				+= kd_sensorlist.o

$(info -= project path  $(srctree)/drivers/misc/mediatek/imgsensor/src/$(MTK_IMGSENSOR_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/ =-)

ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/mt6753/camera_project/$(ARCH_MTK_PROJECT)/camera_hw),)
obj-y				+= camera_project/$(ARCH_MTK_PROJECT)/camera_hw/
$(info by porject kd_camera_hw)
else
obj-y                           += camera_hw/
endif

$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_PROJECT_DRV,$(drv))))

# This downstream sensor list may reference these mt6753 platform drivers even
# when an older adapted target's CONFIG_CUSTOM_KERNEL_IMGSENSOR string does not
# list them.  m2note has a stock S5K3L2XX/OV5670-only list, so keep stale
# S5K3M2/OV5693 providers out of that build instead of exposing wrong CAM_CAL
# devices to userspace.
ifneq ($(ARCH_MTK_PROJECT),m2note)
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/mt6753/ov5693_mipi_raw/*.c),)
ifeq ($(filter ov5693_mipi_raw,$(platform_drv)),)
platform_drv += ov5693_mipi_raw
endif
endif
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/mt6753/ov5693_ofilm_mipi_raw/*.c),)
ifeq ($(filter ov5693_ofilm_mipi_raw,$(platform_drv)),)
platform_drv += ov5693_ofilm_mipi_raw
endif
endif
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/mt6753/s5k3m2_mipi_raw/*.c),)
ifeq ($(filter s5k3m2_mipi_raw,$(platform_drv)),)
platform_drv += s5k3m2_mipi_raw
endif
endif
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/mt6753/s5k3m2_sunny_mipi_raw/*.c),)
ifeq ($(filter s5k3m2_sunny_mipi_raw,$(platform_drv)),)
platform_drv += s5k3m2_sunny_mipi_raw
endif
endif
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/imgsensor/src/mt6753/s5k3l2xx_mipi_raw/*/Makefile),)
ifeq ($(filter s5k3l2xx_mipi_raw,$(platform_drv)),)
platform_drv += s5k3l2xx_mipi_raw
endif
endif
endif

custom_kernel_imgsensor_by_platform := $(addsuffix /, $(platform_drv))
custom_kernel_imgsensor_by_project := $(addsuffix /, $(project_drv))

obj-y += $(custom_kernel_imgsensor_by_platform)
obj-y += $(custom_kernel_imgsensor_by_project)

$(info imgsensor drv by platform $(platform_drv))
$(info imgsensor drv by project $(project_drv))
