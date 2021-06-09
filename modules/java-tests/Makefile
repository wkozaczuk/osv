include ../common.gmk

module_out := $(out)/modules/java-tests
export module_out

include ../java-base/common.gmk

javac_version := $(shell javac -version 2>&1 | grep -oP "javac \d+\.\d+")

java-targets := $(module_out)/java_isolated.so

isolated-jar-target := $(java-base-path)/runjava-isolated/target/runjava-isolated.jar

tests-jar-target := tests/target/runjava-tests.jar

tests-isolates-target := tests-isolates/target/tests-isolates-jar-with-dependencies.jar

tests-jre-extension-target := tests-jre-extension/target/tests-jre-extension.jar

$(module_out)/java_isolated.o: $(src)/modules/java-base/java.cc | init
	$(call quiet, $(CXX) $(CXXFLAGS) -o $@ -c $(src)/modules/java-base/java.cc -MMD, CXX $@)

$(module_out)/java_isolated.so: $(module_out)/java_isolated.o $(out)/modules/java-base/jvm/java_api.o $(out)/modules/java-base/jvm/jni_helpers.o $(out)/modules/java-base/balloon/jvm_balloon.o
	$(call quiet, $(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -o $@ $^, LINK $@)

init:
	@echo "  MKDIRS"
	$(call very-quiet, mkdir -p $(module_out))

comma := ,

$(isolated-jar-target): $(wildcard $(java-base-path)/runjava-isolated/src/main/java/io/osv/isolated/*.java) \
		$(wildcard $(java-base-path)/runjava-isolated/src/main/java/io/osv/util/*.java) \
		$(wildcard $(java-base-path)/runjava-isolated/src/main/java/io/osv/jul/*.java) \
		$(wildcard $(java-base-path)/runjava-common/src/main/java/io/osv/*.java) \
		$(wildcard $(java-base-path)/runjava-common/src/main/java/io/osv/util/*.java) \
		$(java-base-path)/pom.xml $(java-base-path)/runjava-common/pom.xml \
		$(java-base-path)/runjava-isolated/pom.xml
	$(call quiet, cd $(java-base-path) && JAVA_HOME=$(java_jdk_path) mvn -q --projects :runjava-common$(comma):runjava-isolated package -DskipTests=true, MVN $@)

$(tests-jre-extension-target): $(wildcard tests-jre-extension/src/main/java/tests/*.java) \
		tests-jre-extension/pom.xml pom.xml
	$(call quiet, cd $(src)/modules/java-tests && JAVA_HOME=$(java_jdk_path) mvn -q --projects :tests-jre-extension package -DskipTests=true, MVN $@)

$(tests-isolates-target): $(isolated-jar-target) $(wildcard tests-isolates/src/main/java/tests/*.java) \
		pom.xml tests-isolates/pom.xml
	$(call quiet, cd $(src)/modules/java-tests && \
	JAVA_HOME=$(java_jdk_path) mvn -q --projects :runjava-common$(comma):runjava-non-isolated$(comma):runjava-isolated$(comma):tests-isolates package -DskipTests=true, MVN $@)

$(tests-jar-target): $(isolated-jar-target) $(tests-isolates-target) $(tests-jre-extension-target) \
		$(wildcard tests/src/main/java/io/osv/*.java) \
		$(wildcard tests/src/main/java/tests/*.java) \
		pom.xml tests/pom.xml
	$(call quiet, cd $(src)/modules/java-tests && \
	JAVA_HOME=$(java_jdk_path) mvn -q --projects :runjava-common$(comma):runjava-non-isolated$(comma):runjava-isolated$(comma):tests-isolates$(comma):runjava-tests$(comma):tests-jre-extension package -DskipTests=true, MVN $@)

module: $(java-targets) $(tests-jar-target)

clean:
	cd $(src)/modules/java-tests && mvn -q clean
	-rm -f dependency-reduced-pom.xml
	$(call very-quiet, $(RM) -rf $(module_out))
