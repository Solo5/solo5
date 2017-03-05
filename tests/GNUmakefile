# Copyright (c) 2015-2017 Contributors as noted in the AUTHORS file
#
# This file is part of Solo5, a unikernel base layer.
#
# Permission to use, copy, modify, and/or distribute this software
# for any purpose with or without fee is hereby granted, provided
# that the above copyright notice and this permission notice appear
# in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
# CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
# OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

TESTDIRS=test_hello test_globals test_ping_serve test_blk \
         test_exception test_fpu test_time test_quiet

UKVM_TESTS=$(subst test, _test_ukvm, $(TESTDIRS))
VIRTIO_TESTS=$(subst test, _test_virtio, $(TESTDIRS))
MUEN_TESTS=$(subst test, _test_muen, $(TESTDIRS))
CLEANS=$(subst test, _clean, $(TESTDIRS))

all: $(UKVM_TESTS) $(VIRTIO_TESTS) $(MUEN_TESTS)

ukvm: $(UKVM_TESTS)

virtio: $(VIRTIO_TESTS)

muen: $(MUEN_TESTS)

clean: $(CLEANS)

.PHONY: force_it

_test_ukvm%: force_it
	$(MAKE) -C $(subst _test_ukvm, test, $@) ukvm

_test_virtio%: force_it
	$(MAKE) -C $(subst _test_virtio, test, $@) virtio

_test_muen%: force_it
	$(MAKE) -C $(subst _test_muen, test, $@) muen

_clean_%: force_it
	$(MAKE) -C $(subst _clean, test, $@) clean
