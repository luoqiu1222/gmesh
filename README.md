# gmesh

`gmesh` is an independent AGPL command-line tool for inspecting and repairing
triangle meshes. It is designed to run as a separate process and exchange
standard mesh files with callers.

The repair core carries both OrcaSlicer's STL import-repair behavior and its
explicit CGAL deep-repair behavior without depending on Orca's GUI, wxWidgets,
project model, painting data, or slicer pipeline.

## Build

Requirements:

- CMake 3.20 or newer
- A C++17 compiler
- Git (when CMake downloads the pinned ADMesh source)
- macOS 12 or newer for the default macOS build

CMake downloads ADMesh 0.98.5, Boost 1.84.0, and CGAL 5.6.3 by default. For offline builds,
set `GMESH_ADMESH_SOURCE_DIR`, `GMESH_BOOST_SOURCE_DIR`, `GMESH_CGAL_SOURCE_DIR`, and
`GMESH_FETCH_DEPENDENCIES=OFF`.

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
  --report repair-report.json `
  --mode all
```

On macOS and Linux the executable is named `gmesh` rather than `gmesh.exe`.

`--mode import` runs Orca's ADMesh import sequence without hole filling.
`--mode deep` runs connected-part filtering and CGAL repair. `--mode all`
(the default) runs both, matching an STL that later reaches Orca's explicit
“Fix Model” operation. This release accepts STL input and writes binary STL.

## Design

The public module interface is intentionally small:

```cpp
gmesh::RepairReport gmesh::repair_file(const gmesh::RepairOptions &options);
```

Argument parsing, process exit codes, mesh I/O, geometry engines,
and report serialization stay behind this seam. See
[`docs/architecture.md`](docs/architecture.md) and
[`docs/cli-contract.md`](docs/cli-contract.md).
The line-by-line migration checklist is in
[`docs/orca-repair-migration.md`](docs/orca-repair-migration.md).

## Licensing

This project is licensed under `AGPL-3.0-only` because its repair behavior
is derived from OrcaSlicer source. See [`LICENSE`](LICENSE).
Third-party dependencies must be recorded in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) before release.

ADMesh 0.98.5 and CGAL 5.6.3 are integrated at build time. Their records are in
[`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md). CI rejects project source
files that omit the required AGPL SPDX identifier.

## Security

Mesh files are untrusted input. Please read [`SECURITY.md`](SECURITY.md) before
adding a parser or repair backend.
