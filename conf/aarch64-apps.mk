ifeq ($(arch),aarch64)
  #makefile_path_dir = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
  #osv_root = $(makefile_path_dir)/..
  osv_root = /home/wkozaczuk/projects/osv

  aarch64_gccbase = $(osv_root)/build/downloaded_packages/aarch64/gcc/install

  gcc-inc-base := $(dir $(shell find $(aarch64_gccbase)/ -name vector | grep -v -e debug/vector$$ -e profile/vector$$ -e experimental/vector$$))
  gcc-inc-base2 := $(dir $(shell find $(aarch64_gccbase)/ -name unwind.h))
  gcc-inc-base3 := $(dir $(shell dirname `find $(aarch64_gccbase)/ -name c++config.h | grep -v /32/`))

  aarch64_cxx_includes = -isystem $(gcc-inc-base)
  aarch64_cxx_includes += -isystem $(gcc-inc-base3)
  aarch64_gcc_includes = -isystem $(gcc-inc-base2)

  aarch64_includes = -isystem $(osv_root)/include/api/aarch64 $(aarch64_cxx_includes) $(aarch64_gcc_includes) -isystem $(osv_root)/include/api 
  aarch64_includes += -isystem $(osv_root)/build/release.aarch64/gen/include

  aarch64_libdir := $(dir $(shell find $(aarch64_gccbase)/ -name libstdc++.so))
  
  aarch64_boostbase = $(osv_root)/build/downloaded_packages/aarch64/boost/install
  boost-includes = -isystem $(aarch64_boostbase)/usr/include

  CROSS_COMMON_FLAGS = $(aarch64_includes) $(boost-includes) --sysroot $(aarch64_gccbase) -nostdinc
  #CROSS_COMMON_FLAGS = -isystem $(osv_root)/bsd/sys -isystem $(osv_root)/bsd -isystem $(osv_root)/bsd/aarch64 $(boost-includes) --sysroot $(aarch64_gccbase) -nostdinc
  CROSS_LDFLAGS = -L$(aarch64_libdir)

  CROSS_PREFIX = aarch64-linux-gnu-
  CXX=$(CROSS_PREFIX)g++
  CC=$(CROSS_PREFIX)gcc
  LD=$(CROSS_PREFIX)ld.bfd
  OBJCOPY=$(CROSS_PREFIX)objcopy
  export STRIP=$(CROSS_PREFIX)strip
endif
