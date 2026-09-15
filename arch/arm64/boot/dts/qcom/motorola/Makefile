# Add 1k of padding to the DTBs to allow for environment variables
# to be runtime added by the bootloader (i.e. /chosen properties)
DTC_FLAGS += -p 1024

dtb-$(CONFIG_DEEN_DTB) += msm8953-deen-pvt1.dtb
dtb-$(CONFIG_DEEN_DTB) += msm8953-deen-pvt2.dtb

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
