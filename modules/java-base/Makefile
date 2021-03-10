include common.gmk
module_out := $(out)/modules/java-base

java-targets := $(module_out)/jni/monitor.so $(module_out)/jvm/jni_helpers.o $(module_out)/jvm/java_api.o $(module_out)/balloon/jvm_balloon.o

$(module_out)/%.o: %.cc | init
	$(call quiet, $(CXX) $(CXXFLAGS) -c -MMD  -o $@ $<, CXX $@)

$(module_out)/%.o: %.c | init
	$(call quiet, $(CC) $(CFLAGS) -c -MMD  -o $@ $<, CC $@)

$(module_out)/%.so: %.o
	$(call quiet, $(CXX) $(CXXFLAGS) -shared -o $@ $^, LINK $@)

module: all

all: $(init) $(java-targets)

init:
	@echo "  MKDIRS"
	$(call very-quiet, mkdir -p $(module_out)/jni)
	$(call very-quiet, mkdir -p $(module_out)/jvm)
	$(call very-quiet, mkdir -p $(module_out)/balloon)
.PHONY: init

clean:
	#$(call very-quiet, $(RM) -rf $(module_out))
