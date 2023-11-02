./scripts/test.py \
  -d tst-dns-resolver \
  -d tracing_smoke_test \
  -d tst-stdio-rofs \
  -d tst-wctype \
  -v -m modules/tests-with-linux-ld/usr.manifest

#tst-wctype.cc - fails on Linux
#tst-stdio-rofs - hangs on Linux

### - For Java:
# needs librt.so.1 (also libz.so?)
# Also needs to run as /usr/lib/jvm/java/bin/java

#--------- Potentially TO DO and/or address
#tst-sigwait: /home/wkozaczuk/projects/osv-master/tests/tst-sigwait.cc:41: main: Assertion `sig == SIGUSR1' failed. unimplemented system call 128

#tst-eventfd - vary rarely fails in thread::complete() with page fault

#Cmdline: /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 /tests/tst-eventfd
#syscall(): unimplemented system call 334
#eventfd: running basic test:  PASS
#eventfd: Running semaphore_test:  PASS
#eventfd: running simple threaded test:  PASS
#eventfd: running poll test: page fault outside application, addr: 0x0000200085790000
#[registers]
#RIP: 0x000000004037efbf <sched::thread::complete()+143>
#RFL: 0x0000000000010202  CS:  0x0000000000000008  SS:  0x0000000000000010
#RAX: 0x0000200085790920  RBX: 0x000000004086ea60  RCX: 0x00004000023e9090  RDX: 0x0000000000000000
#RSI: 0x0000000000001fa0  RDI: 0x000000004086e7e0  RBP: 0x0000400002403ae0  R8:  0x000000004086e7e0
#R9:  0xfffffffffffff988  R10: 0x0000000000000000  R11: 0x000040000091e190  R12: 0x00004000023e9040
#R13: 0xfffffffffffff988  R14: 0x0000000000000000  R15: 0x000040000091e190  RSP: 0x0000400002403ad0
#Aborted
#
#[backtrace]
#0x0000000040205999 <???+1075861913>
#0x000000004030bd53 <page_fault+147>
#0x000000004030aa82 <???+1076931202>
#0x000000004037f115 <sched::thread::exit()+21>
#0x00000000403702ad <syscall+12173>
#0x00000000403717ae <syscall_wrapper+126>
#0x00000000403306d1 <???+1077085905>
#0x000020007ec94a65 <???+2127120997>
#Test tst-eventfd FAILED

