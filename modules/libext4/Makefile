arch=x64
include ../common.gmk

module_out := $(out)/modules/libext4

CFLAGS = -fPIC -std=gnu99 $(INCLUDES) -I../lwext4/upstream/lwext4/include -I../lwext4/upstream/lwext4/build_lib_only/include \
	 -D_KERNEL -D_GNU_SOURCE -Wall

# the build target executable:
TARGET = libext4
CC_FILES := ext_vfsops.c ext_vnops.c
OBJ_FILES := $(addprefix $(module_out)/,$(CC_FILES:.c=.o))
DEPS := $(OBJ_FILES:.o=.d)

LIBS = -L../lwext4/upstream/lwext4/build_lib_only/src/ -llwext4

$(module_out)/$(TARGET).so: $(OBJ_FILES)
	$(call quiet, $(CC) $(CFLAGS) $(LDFLAGS) -shared -o $(module_out)/$(TARGET).so $^ $(LIBS), LINK $@)

$(module_out)/%.o: %.c
	$(call quiet, $(CC) $(CFLAGS) -c -o $@ $<, CXX $@)

init:
	@echo "  MKDIRS"
	$(call very-quiet, mkdir -p $(module_out))
.PHONY: init

module: init $(module_out)/$(TARGET).so
	echo '/usr/lib/fs/libext4.so: ./modules/libext4/libext4.so' > usr.manifest

clean:
	rm -f $(TARGET)*.so usr.manifest
	$(call very-quiet, $(RM) -rf $(module_out))
