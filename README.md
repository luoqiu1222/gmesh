# gmesh

`gmesh` is an independent GPL command-line tool for inspecting and repairing
triangle meshes. It is designed to run as a separate process and exchange
standard mesh files with callers.

> Status: project skeleton. The CLI contract and module seam are in place;
> the geometry repair implementation is intentionally not implemented yet.

## Build

Requirements:

- CMake 3.20 or newer
- A C++17 compiler
- macOS 12 or newer for the default macOS build

```powershell
cmake -S . -B build -DGMESH_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On single-config generators:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGMESH_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### macOS

The default macOS configuration builds one Universal 2 executable containing
both Apple Silicon (`arm64`) and Intel (`x86_64`) code:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGMESH_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
lipo -info build/src/gmesh
```

Run it with:

```bash
./build/src/gmesh --version
```

To build only for the current Mac architecture:

```bash
cmake -S . -B build-current-arch \
  -DCMAKE_BUILD_TYPE=Release \
  -DGMESH_MACOS_UNIVERSAL=OFF
```

The minimum supported version defaults to macOS 12. It can be overridden with
`-DGMESH_MACOS_DEPLOYMENT_TARGET=<version>` before the first configure.

## Usage

```powershell
gmesh.exe --help
gmesh.exe --version
gmesh.exe --license

gmesh.exe repair `
  --input input.stl `
  --output repaired.stl `
  --report repair-report.json
```

On macOS and Linux the executable is named `gmesh` rather than `gmesh.exe`.

The `repair` command currently returns `not implemented`; this prevents the
skeleton from silently copying or corrupting a model before a real repair
engine is integrated.

## Design

The public module interface is intentionally small:

```cpp
gmesh::RepairReport gmesh::repair_file(const gmesh::RepairOptions &options);
```

Argument parsing, process exit codes, mesh I/O, the future geometry engine,
and report serialization stay behind this seam. See
[`docs/architecture.md`](docs/architecture.md) and
[`docs/cli-contract.md`](docs/cli-contract.md).

## Licensing

This project is licensed under `GPL-3.0-or-later`. See [`LICENSE`](LICENSE).
Third-party dependencies must be recorded in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) before release.

ADMesh and CGAL are planned but not yet integrated. Their package-level license
review and required attribution process are documented in
[`docs/license-compliance.md`](docs/license-compliance.md). CI rejects project
source files that omit the required SPDX identifier.

## Security

Mesh files are untrusted input. Please read [`SECURITY.md`](SECURITY.md) before
adding a parser or repair backend.
