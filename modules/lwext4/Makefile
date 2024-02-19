src = $(shell readlink -f ../..)
module-dir = $(src)/modules/lwext4

all: module
module: lwext4

lwext4: upstream/lwext4/.git upstream/lwext4/build_lib_only/src/liblwext4.so

.PHONY: lwext4

upstream/lwext4/.git:
	mkdir -p $(module-dir)/upstream && cd $(module-dir)/upstream && \
	git clone --depth 1 https://github.com/gkostka/lwext4.git

upstream/lwext4/build_lib_only/src/liblwext4.so:
	cd $(module-dir)/upstream/lwext4 && make -DLWEXT4_BUILD_SHARED_LIB=ON generic && cd build_generic && make

clean:
	cd $(module-dir) && rm -rf upstream
