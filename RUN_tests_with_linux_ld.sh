./scripts/test.py \
  -d java-perms \
  -d java_no_wrapper \
  -d java_isolated \
  -d java_non_isolated \
  -d tcp_close_without_reading_on_qemu \
  -d tracing_smoke_test \
  \
  -d tst-dlfcn.so \
  -d tst-futimesat.so \
  -d tst-ifaddrs.so \
  -d tst-kill.so \
  -d tst-net_if_test.so \
  -d tst-select-timeout.so \
  -d tst-symlink-rofs.so \
  -d tst-symlink.so \
  -d tst-ttyname.so \
  -d tst-utimensat.so \
  \
  -d tst-eventfd.so -v
