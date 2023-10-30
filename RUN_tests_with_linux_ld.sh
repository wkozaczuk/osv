./scripts/test.py \
  -d tst-dns-resolver \
  -d java_isolated \
  -d java_non_isolated \
  -d tracing_smoke_test \
  -d tst-utimensat \
  -d tst-utimes \
  -d tst-stdio-rofs \
  -d tst-wctype \
  -v -m modules/tests-with-linux-ld/usr.manifest

#tst-wctype.cc - fails on Linux
#tst-stdio-rofs - hangs on Linux

#--------- Potentially TO DO and/or address
#tst-utimensat.cc - fails with 2 errors on Linux vs 1 on OSv
#tst-utimes.cc - fails on OSv

#tst-sigwait: /home/wkozaczuk/projects/osv-master/tests/tst-sigwait.cc:41: main: Assertion `sig == SIGUSR1' failed. unimplemented system call 128

#tst-eventfd - vary rarely fails in thread::complete() with page fault
