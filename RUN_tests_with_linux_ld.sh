./scripts/test.py \
  -d tst-dns-resolver \
  -d java-perms \
  -d java_no_wrapper \
  -d java_isolated \
  -d java_non_isolated \
  -d tcp_close_without_reading_on_qemu \
  -d tracing_smoke_test \
  -d tst-dlfcn \
  -d tst-futimesat \
  -d tst-ifaddrs \
  -d tst-net_if_test \
  -d tst-symlink-rofs \
  -d tst-symlink \
  -d tst-ttyname \
  -d tst-utimensat \
  -d tst-semaphore \
  -d tst-ctype \
  -d tst-stdio-rofs \
  -d tst-string \
  -d tst-time \
  -d tst-wctype \
  -v -m modules/tests-with-linux-ld/usr.manifest

#tst-pthread-clock: /home/wkozaczuk/projects/osv-master/tests/tst-pthread-clock.cc:34: pclock: Assertion `clock_gettime(cid, &ts) != -1' failed.
#tst-realloc: /home/wkozaczuk/projects/osv-master/tests/tst-realloc.cc:44: test_usable_size: Assertion `expected_usable_size == malloc_usable_size(ptr)' failed.
#syscall(): unimplemented system call 29 Cannot create shm segment for key 1: Function not implemented
#tst-sigwait: /home/wkozaczuk/projects/osv-master/tests/tst-sigwait.cc:41: main: Assertion `sig == SIGUSR1' failed. unimplemented system call 128

--------------------------
#Cmdline: /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 /tests/tst-faccessat - this one works
#syscall(): unimplemented system call 334
#syscall(): unimplemented system call 439
#syscall(): unimplemented system call 439
#syscall(): unimplemented system call 439
#syscall(): unimplemented system call 439
#syscall(): unimplemented system call 439
#syscall(): unimplemented system call 439
#SUMMARY: 11 tests, 0 failures
