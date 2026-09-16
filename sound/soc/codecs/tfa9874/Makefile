# in-kernel build (ported from msm-3.18 mot8953-deen)
# additional debugging
#TFA_DEBUG=1

# add version number derived from Git (empty when no tags, keep log clean)
PLMA_HOSTSW_DIR=$(realpath -f $(srctree)/$(src))
GIT_VERSION=$(shell cd $(PLMA_HOSTSW_DIR) 2>/dev/null && git describe --tags --dirty --match "v[0-9]*.[0-9]*.[0-9]*" 2>/dev/null)
EXTRA_CFLAGS += -DTFA98XX_GIT_VERSIONS=\"$(GIT_VERSION)\"

# debugging support (also enables trace_printk)
EXTRA_CFLAGS += -DDEBUG -DTFA9874_NONDSP_STEREO

EXTRA_CFLAGS += -I$(src)/inc
EXTRA_CFLAGS += -Werror

snd-soc-tfa9874-objs += tfa98xx.o
snd-soc-tfa9874-objs += tfa_container.o
snd-soc-tfa9874-objs += tfa_dsp.o
snd-soc-tfa9874-objs += tfa_init.o

ifdef TFA_DEBUG
EXTRA_CFLAGS += -DTFA_DEBUG -DDEBUG
snd-soc-tfa9874-objs += tfa_debug.o
endif

obj-$(CONFIG_SND_SOC_TFA9874) := snd-soc-tfa9874.o
subdir-ccflags-y := $(call cc-disable-warning, enum-conversion)
