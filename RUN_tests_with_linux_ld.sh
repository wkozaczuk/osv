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
