# CI/CD Workflows

This directory contains GitHub Actions workflows for automated building of pxlib.

## Build Workflow

The `build.yml` workflow automatically builds pxlib on multiple platforms:

- **Linux**: Ubuntu with both GCC and Clang compilers
- **Windows**: Windows with MSVC compiler
- **macOS**: macOS with Clang compiler

### Triggers

The workflow runs on:
- Push to `master`, `main`, `develop`, or any `copilot/**` branch
- Pull requests to `master`, `main`, or `develop` branches
- Manual trigger via workflow_dispatch

### Artifacts

Built libraries are uploaded as artifacts for 7 days:
- `pxlib-linux-gcc` - Linux build with GCC
- `pxlib-linux-clang` - Linux build with Clang
- `pxlib-windows` - Windows build with MSVC
- `pxlib-macos` - macOS build with Clang

### Local Building

To build locally, use CMake:

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

For more information, see the main [README](../../README) file.
