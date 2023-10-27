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
  -d tst-symlink-rofs \
  -d tst-symlink \
  -d tst-ttyname \
  -d tst-semaphore \
  -d tst-time \
  -d tst-stdio-rofs \
  -d tst-string \
  -d tst-wctype \
  -v -m modules/tests-with-linux-ld/usr.manifest

#tst-symlink* fail second file mode

#tst-ttyname fails with same 6 out 8 on Linux and OSv

#tst-semaphore - tests/tst-semaphore.c:63: main: Assertion `named_sem1 != SEM_FAILED' failed (before succeeds).

#tst-time - /home/wkozaczuk/projects/osv-master/tests/tst-time.cc(441): error: in "time_clock_gettime_unknown": check 22 == (*__errno_location()) has failed [22 != 1]
#*** 1 failure is detected in the test module "tst-time"
#ASSERT_EQ(EINVAL, errno);

#tst-wctype.cc - fails on Linux
#tst-string.cc - fails on Linux
#tst-stdio-rofs - hangs on Linux

#tst-eventfd - vary rarely fails in thread::complete() with page fault

#tst-semaphore - fails because there is no /dev/shm/*
#0x0000400000e1c040 /lib/x86_64-lin  0         0.175472213 syscall_openat       -1 <= -100 "/dev/shm/sem.name" 0400002 0
#0x0000400000e1c040 /lib/x86_64-lin  0         0.175845244 syscall_getrandom    -1 <= 0x200000601548 8 1
#0x0000400000e1c040 /lib/x86_64-lin  0         0.175848770 syscall_fstatat      -1 <= -100 "/dev/shm/sem.dwSrRW" 0x0000200000601450 0400
#0x0000400000e1c040 /lib/x86_64-lin  0         0.175850779 syscall_openat       -1 <= -100 "/dev/shm/sem.dwSrRW" 0302 511

#tst-brk: /home/wkozaczuk/projects/osv-master/tests/tst-brk.c:48: main: Assertion `sbrk(0) == current_break + new_area_size' failed.
#tst-pthread-clock: /home/wkozaczuk/projects/osv-master/tests/tst-pthread-clock.cc:34: pclock: Assertion `clock_gettime(cid, &ts) != -1' failed.
#tst-realloc: /home/wkozaczuk/projects/osv-master/tests/tst-realloc.cc:44: test_usable_size: Assertion `expected_usable_size == malloc_usable_size(ptr)' failed.
#tst-shm: syscall(): unimplemented system call 29 Cannot create shm segment for key 1: Function not implemented
#tst-sigwait: /home/wkozaczuk/projects/osv-master/tests/tst-sigwait.cc:41: main: Assertion `sig == SIGUSR1' failed. unimplemented system call 128

#--------
# How are errno (thread local) set under DL linux on OSv
