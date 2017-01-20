TESTDIRS=test_hello test_globals test_ping_serve test_blk test_nolibc

UKVM_TESTS=$(subst test, _test_ukvm, $(TESTDIRS))
VIRTIO_TESTS=$(subst test, _test_virtio, $(TESTDIRS))
CLEANS=$(subst test, _clean, $(TESTDIRS))

all: $(UKVM_TESTS) $(VIRTIO_TESTS)

ukvm: $(UKVM_TESTS)

virtio: $(VIRTIO_TESTS)

clean: $(CLEANS)

.PHONY: force_it

_test_ukvm%: force_it
	$(MAKE) -C $(subst _test_ukvm, test, $@) ukvm

_test_virtio%: force_it
	$(MAKE) -C $(subst _test_virtio, test, $@) virtio

_clean_%: force_it
	$(MAKE) -C $(subst _clean, test, $@) clean
