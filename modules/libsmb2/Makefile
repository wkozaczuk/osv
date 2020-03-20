src = $(shell readlink -f ../..)
module-dir = $(src)/modules/libsmb2

all: module
module: libsmb2

libsmb2: upstream/libsmb2/.git upstream/libsmb2/lib/libsmb2.so.3.0.0

.PHONY: libsmb2

upstream/libsmb2/.git:
	mkdir -p $(module-dir)/upstream && cd $(module-dir)/upstream && \
	git clone --depth 1 https://github.com/sahlberg/libsmb2.git

upstream/libsmb2/lib/libsmb2.so.3.0.0:
	cd $(module-dir)/upstream/libsmb2 && cmake . && make

clean:
	cd $(module-dir) && rm -rf upstream
