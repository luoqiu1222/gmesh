# macOS support

## Supported targets

The default macOS build produces a Universal 2 command-line executable:

- Apple Silicon: `arm64`;
- Intel Mac: `x86_64`;
- minimum deployment target: macOS 12.0.

The project does not use AppKit and does not create an `.app` bundle. It is a
regular CLI executable intended to be launched directly or as a child process.

## Build

Install Xcode Command Line Tools and CMake, then run:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGMESH_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Verify the binary architectures:

```bash
lipo -info build/src/gmesh
lipo -verify_arch arm64 x86_64 build/src/gmesh
```

## Single-architecture development build

For faster local iteration, disable the universal build:

```bash
cmake -S . -B build-dev \
  -DCMAKE_BUILD_TYPE=Debug \
  -DGMESH_MACOS_UNIVERSAL=OFF
cmake --build build-dev
```

The resulting architecture follows the current compiler host unless
`CMAKE_OSX_ARCHITECTURES` is set explicitly.

## Distribution checklist

Before shipping a macOS binary:

1. Run the complete test suite on macOS.
2. Verify both architectures with `lipo`.
3. Confirm the deployment target with `otool -l build/src/gmesh`.
4. Package the GPL license, notices, and corresponding-source location.
5. Code-sign the executable with the distributing organization's Developer ID.
6. Notarize the final archive if it is downloaded outside the Mac App Store.

Signing and notarization credentials are release-environment concerns and must
not be stored in this repository.
