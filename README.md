# moss

moss is a C library template that provides a solid foundation for building C libraries with modern tooling and best practices.

## Table of Contents

- [Building the Project](#building-the-project)
- [Build Options](#build-options)
- [Using as a CMake Subdirectory](#using-as-a-cmake-subdirectory)
- [Creating a New Project from This Template](#creating-a-new-project-from-this-template)

## Building the Project

### Prerequisites

- CMake 3.16 or higher
- C99 compatible compiler (GCC or Clang)
- Unix-like operating system (Linux, macOS, BSD)
- Git (for cloning)

### Basic Build

```bash
# Clone the repository
git clone https://github.com/MyGroup/moss.git
cd moss

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make
```

### Build with Specific Options

```bash
# Build with all features enabled
cmake -DMOSS_BUILD_SHARED=ON -DMOSS_BUILD_EXAMPLE=ON -DMOSS_BUILD_TESTS=ON ..

# Build in Release mode
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## Build Options

The project supports several CMake options to customize the build:

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Debug` | Build type: `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |
| `MOSS_BUILD_SHARED` | `OFF` | Build library as a shared library instead of static |
| `MOSS_BUILD_EXAMPLE` | `ON` (standalone) | Build example program demonstrating library usage |
| `MOSS_BUILD_TESTS` | `ON` (standalone) | Build test programs |

### Build Type Details

- **Debug**: Includes debug symbols, no optimization, enables `__MOSS_DEBUG__` macro
- **Release**: Optimized for performance, no debug symbols
- **RelWithDebInfo**: Optimized with debug symbols
- **MinSizeRel**: Optimized for minimal size

### Compiler-Specific Features

- **GCC/Clang**: Uses `-Wall -Wextra -Wpedantic` with additional warnings
- Position-independent code enabled, C99 standard enforced

## Using as a CMake Subdirectory

To use this library in your own CMake project:

```cmake
# Add the library as a subdirectory
add_subdirectory(moss)

# Link the library to your target
target_link_libraries(your_target PRIVATE moss)
```

The library will be built as a static library by default. To use as a shared library:

```cmake
set(MOSS_BUILD_SHARED ON)
add_subdirectory(moss)
target_link_libraries(your_target PRIVATE moss)
```

## Creating a New Project from This Template

This repository serves as a template for creating new C library projects. Use the included setup script to create your own project:

### Using the Setup Script

```bash
curl -fsSL "https://raw.githubusercontent.com/burs1/templ.c/refs/heads/main/setup.sh" | bash -s mynewlib
```

### What the Script Does

1. **Clones the template**: Creates a copy of this repository
2. **Replaces project name**: Changes all "moss" references to your project name
3. **Replaces full project name**: Changes "moss" references to your full project name
4. **Handles case variations**: Updates `moss`, `MOSS`, and `Umoss` appropriately
5. **Initializes git**: Creates a fresh git repository for your project
6. **Provides next steps**: Shows instructions for customizing your new project
