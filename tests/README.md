# Testing

This directory contains the test suite for the library using Check framework.

## Running Tests

### Building and Running Tests

```bash
# Build with tests enabled
cmake -DMOSS_BUILD_TESTS=ON ..
make

# Run tests using CTest (recommended)
ctest --verbose

# Or run tests directly
./test_main
```

### Check Framework Options

Check provides command-line options for test execution:

```bash
# Run tests with detailed output
./test_main -v

# Run tests in fork mode (default)
./test_main -f

# Run tests in no-fork mode
./test_main -n

# Run specific test suite
./test_main -s UmossBasicTest
```

### Code Coverage

Code coverage is automatically enabled by default when building tests in `Debug` mode with GCC or Clang. Coverage data is generated in `.gcda` and `.gcno` files.

To generate a coverage report, you can use tools like `gcov` and `lcov`:

```bash
# After running tests, generate coverage report
cd build
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
lcov --list coverage.info
genhtml coverage.info --output-directory coverage/
```

## Test Structure

- `test_main.c` - Main test file with Check test cases
- Add new test files as needed
