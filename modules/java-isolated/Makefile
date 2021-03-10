include ../java-base/common.gmk
module_out := $(out)/modules/java-isolated

java-targets := $(module_out)/java.so

jar-targets := $(java-base-path)/runjava-isolated/target/runjava-isolated.jar

$(module_out)/java.o: $(java-base-path)/java.cc | init
	$(call quiet, $(CXX) $(CXXFLAGS) -o $@ -c $(java-base-path)/java.cc -MMD, CXX $@)

$(module_out)/java.so: $(module_out)/java.o $(module_out)/../java-base/jvm/java_api.o $(module_out)/../java-base/jvm/jni_helpers.o $(module_out)/../java-base/balloon/jvm_balloon.o
	$(call quiet, $(CXX) $(CXXFLAGS) -shared -o $@ $^, LINK $@)

init:
	@echo "  MKDIRS"
	$(call very-quiet, mkdir -p $(module_out))

comma := ,

$(jar-targets): $(wildcard $(java-base-path)/runjava-isolated/src/main/java/io/osv/isolated/*.java) \
		$(wildcard $(java-base-path)/runjava-isolated/src/main/java/io/osv/util/*.java) \
		$(wildcard $(java-base-path)/runjava-isolated/src/main/java/io/osv/jul/*.java) \
		$(wildcard $(java-base-path)/runjava-common/src/main/java/io/osv/*.java) \
		$(wildcard $(java-base-path)/runjava-common/src/main/java/io/osv/util/*.java) \
		$(java-base-path)/pom.xml $(java-base-path)/runjava-common/pom.xml \
		$(java-base-path)/runjava-isolated/pom.xml
	$(call quiet, cd $(java-base-path) && mvn -q --projects :runjava-common$(comma):runjava-isolated package -DskipTests=true, MVN $@)

module: $(java-targets) $(jar-targets)

clean:
	cd $(java-base-path) && mvn -q clean
	-rm -f dependency-reduced-pom.xml
	$(call very-quiet, $(RM) -rf $(module_out))
