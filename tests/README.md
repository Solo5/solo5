# Automated tests

This is an automated test suite for Solo5. These tests are run by our automated
CI system, which is somewhat documented at https://github.com/Solo5/solo5-ci.

When developing Solo5, you should run at least the basic test suite to verify
that your changes are good.

## Basic test suite

To run the test suite, first ensure your host environment is set up by running
(as root):

    ./setup-tests.sh

Then, run the test suite with:

    ./run-tests.sh

Some tests / hypervisors require root privileges. For best results, run the
test suite as root.

To see full output from all tests (not just the failures), run the test suite
with the `-v` option.

When adding tests, please use the following conventions:

1. Each test goes in its own subdirectory.
2. On success the test should print `SUCCESS` on the console and return from
   `solo5_app_main()`. This will halt the unikernel.
3. Add your tests to `run-tests.sh` for automatic invocation.

## End to end tests

Work in progress. Here be dragons. **Ask @mato before modifying this or
anything in build.sh that refers to it**.

The `e2e-mirage-solo5` folder contains an "end to end" test for Mirage/Solo5.
Note that this folder is deliberately not included in release tarballs of Solo5.

The folder is a git subtree snapshot of https://github.com/mato/e2e-mirage-solo5,
added to this repository with:

```
git remote add -f e2e-mirage-solo5 git@github.com:mato/e2e-mirage-solo5
git subtree add --prefix tests/e2e-mirage-solo5 e2e-mirage-solo5 master --squash

```

This test (well, its driver script) is currently Linux-specific and only tests
the `hvt` target.

To run the E2E tests **against a checked out copy of Solo5**, apart from the
instructions in `e2e-mirage-solo5/README.md`, you need to perform the following
additional step from the root of the checked out Solo5 tree:

```
rm -f tests/e2e-mirage-solo5/universe/solo5-bindings-hvt/*
ln -sf $(readlink -f .) tests/e2e-mirage-solo5/universe/solo5-bindings-hvt/local
```
