TESTDIRS=test_hello test_ping_serve

TESTS=$(subst test, _test, $(TESTDIRS))
CLEANS=$(subst test, _clean, $(TESTDIRS))

all: $(TESTS)
clean: $(CLEANS)

.PHONY: force_it

_test_%: force_it
	make -C $(subst _test, test, $@)

_clean_%: force_it
	make -C $(subst _clean, test, $@) clean


