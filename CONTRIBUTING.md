# Contributing

## Build and test

```powershell
cmake -S . -B build -DGMESH_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Requirements

- Keep the public repair interface small and platform independent.
- Add tests for every behavior change.
- Treat mesh files as hostile input.
- Record every dependency and its exact license in `THIRD_PARTY_LICENSES.md`.
- Do not add proprietary code or code with an unknown license.

Contributions are licensed under the project's `AGPL-3.0-only` license.
