# Automated tests

This is an automated test suite for Solo5.

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
