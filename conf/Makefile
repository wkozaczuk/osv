mode=release
ifeq (,$(wildcard conf/$(mode).mk))
    $(error unsupported mode $(mode))
endif

HOST_CXX := g++

detect_arch = $(word 1, $(shell { echo "x64        __x86_64__";  \
                                  echo "aarch64    __aarch64__"; \
                       } | $1 -E -xc - | grep ' 1$$'))

host_arch := $(call detect_arch, $(HOST_CXX))

# As an alternative to setting ARCH or arch, let's allow the user to
# directly set the CROSS_PREFIX environment variable, and learn its arch:
ifdef CROSS_PREFIX
    ARCH := $(call detect_arch, $(CROSS_PREFIX)gcc)
endif

ifndef ARCH
    ARCH := $(host_arch)
endif
arch := $(ARCH)

ifeq (,$(wildcard conf/$(arch).mk))
    $(error unsupported architecture $(arch))
endif

quiet = $(if $V, $1, @echo " $2"; $1)

out = build/$(mode).$(arch)

.PHONY: config
config: $(out)/gen/config/kernel_conf.mk
default_config: $(out)/.config

$(out)/kbuild/kconfig/conf $(out)/kbuild/kconfig/mconf: $(wildcard kbuild/kconfig/*.c)
	$(call quiet, mkdir -p $(out)/kbuild, MKDIR $(out)/kbuild)
	$(call quiet, cd $(out)/kbuild && make -s -C ../../../kbuild -f Makefile.sample O=`pwd` -j, MAKE kbuild)

CONF_FILES := conf/base.mk conf/$(mode).mk conf/$(arch).mk conf/profiles/$(arch)/kconfig $(wildcard conf/kconfig/*)

$(out)/.config: $(CONF_FILES) $(out)/kbuild/kconfig/conf
	$(call quiet, mode=$(mode) arch=$(arch) CONFIG_=CONF_ KCONFIG_AUTOHEADER=$(out)/gen/include/osv/kernel_config.h KCONFIG_AUTOCONFIG=$(out)/gen/config/kernel.conf KCONFIG_RUSTCCFG=$(out)/gen/include/osv/rustc_cfg KCONFIG_CONFIG=$(out)/.config $(out)/kbuild/kconfig/conf -s conf/kconfig/main --alldefconfig, CONF_DEF $(out)/.config)

$(out)/.config.yes: $(CONF_FILES) $(out)/kbuild/kconfig/conf
	$(call quiet, mode=$(mode) arch=$(arch) CONFIG_=CONF_ KCONFIG_AUTOHEADER=$(out)/gen/include/osv/kernel_yes_config.h KCONFIG_AUTOCONFIG=$(out)/gen/config_yes/kernel_yes.conf KCONFIG_RUSTCCFG=$(out)/gen/include/osv/rustc_cfg KCONFIG_CONFIG=$(out)/.config.yes $(out)/kbuild/kconfig/conf -s conf/kconfig/main --allyesconfig, CONF_YES $(out)/.config)

$(out)/gen/include/osv/kernel_config.h: $(out)/.config
	$(call quiet, mode=$(mode) arch=$(arch) CONFIG_=CONF_ KCONFIG_AUTOHEADER=$(out)/gen/include/osv/kernel_config.h KCONFIG_AUTOCONFIG=$(out)/gen/config/kernel.conf KCONFIG_RUSTCCFG=$(out)/gen/include/osv/rustc_cfg KCONFIG_CONFIG=$(out)/.config $(out)/kbuild/kconfig/conf -s conf/kconfig/main --syncconfig, SYNC $(out)/.config)

$(out)/gen/config/kernel_conf.mk: $(out)/gen/include/osv/kernel_config.h $(out)/.config.yes
	$(call quiet, sed 's/CONF/conf/' $(out)/gen/config/kernel.conf | sed 's/=y$$/=1/' | sed 's/\(conf_drivers_profile\)_\(.*\)=1/\1=\2/' > $(out)/gen/config/kernel_conf.mk, CONF_MK $(out)/gen/config/kernel_conf.mk)
	$(call quiet, scripts/gen-kernel-config-headers $(out)/gen/include/osv/kernel_config.h $(out)/gen/include/osv/kernel_yes_config.h, CONF_HEADERS $(out)/gen/include/osv/kernel_config_*)
