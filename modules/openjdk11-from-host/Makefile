.PHONY: module clean

SRC = $(shell readlink -f ../..)

javac_exe_path = /usr/lib/jvm/java-11-openjdk-arm64/bin/javac
javac_bin_path = $(shell dirname $(javac_exe_path))
java_jdk_path = $(shell dirname $(javac_bin_path))

very-quiet = $(if $V, $1, @$1)

module:
	#$(shell echo $(java_jdk_path))
	$(call very-quiet, $(SRC)/scripts/manifest_from_host.sh $(java_jdk_path)/lib/libsunec.so > usr.manifest)
	$(call very-quiet, $(SRC)/scripts/manifest_from_host.sh -li libfreeblpriv3.so >> usr.manifest)

clean:
	rm -rf usr.manifest
