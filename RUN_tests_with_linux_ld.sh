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
  -d tst-utimensat \
  -d tst-ifaddrs \
  -d tst-net_if_test \
  -d tst-symlink-rofs \
  -d tst-symlink \
  -d tst-ttyname \
  -d tst-semaphore \
  -d tst-time \
  -d tst-stdio-rofs \
  -d tst-string \
  -d tst-wctype \
  -v -m modules/tests-with-linux-ld/usr.manifest

#tst-ifaddrs and tst-net_if_test - hang affected by netlink (expects to receive data without send):

#/lib/x86_64-lin   0  0.136303231 syscall_socket(3 <= 16 524291 0)
#/lib/x86_64-lin   0  0.136304557 syscall_bind(0 <= 3 0x2000005ff92c 12)
#/lib/x86_64-lin   0  0.136306196 syscall_getsockname(0 <= 3 0x2000005ff92c 0x2000005ff928)
#/lib/x86_64-lin   0  0.136969134 syscall_sendto(20 <= 3 0x5ff900 20 0 0x2000005ff8c0 12)
#/lib/x86_64-lin   0  0.136972174 syscall_recvmsg(132 <= 3 0x2000005ff8c0 0)

#vs

#0x0000400000e1c040 /lib/x86_64-lin  0         0.103243876 syscall_socket       3 <= 16 3 0
#0x0000400000e1c040 /lib/x86_64-lin  0         0.103245312 syscall_bind         0 <= 3 0x00002000005ff76c 12
#0x0000400000e1c040 /lib/x86_64-lin  0         0.103246894 syscall_getsockname  0 <= 3 0x00002000005ff784 0x00002000005ff768
#0x0000400000e1c040 /lib/x86_64-lin  0         0.103257735 syscall_sendmsg      17 <= 3 0x00002000005ff790 0
#0x0000400000e1c040 /lib/x86_64-lin  0         0.104648636 syscall_write        0x19 <= 1 0x00002000008002a0 0x19
#0x0000400000e1c040 /lib/x86_64-lin  0         0.104652062 syscall_recvmsg      132 <= 3 0x00002000005ff790 0
#0x0000400000e1c040 /lib/x86_64-lin  0         0.106929243 syscall_write        0x1b <= 1 0x00002000008002a0 0x1b

#tst-symlink* fail second file mode

#tst-ttyname fails with same 6 out 8 on Linux and OSv

#tst-semaphore - tests/tst-semaphore.c:63: main: Assertion `named_sem1 != SEM_FAILED' failed (before succeeds).

#tst-time - /home/wkozaczuk/projects/osv-master/tests/tst-time.cc(441): error: in "time_clock_gettime_unknown": check 22 == (*__errno_location()) has failed [22 != 1]
#*** 1 failure is detected in the test module "tst-time"
#ASSERT_EQ(EINVAL, errno);

#tst-wctype.cc - fails on Linux
#tst-string.cc - fails on Linux
#tst-stdio-rofs - hangs on Linux

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

#--------
# How are errno (thread local) set under DL linux on OSv
