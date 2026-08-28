# Changelog

## 0.3.0 - 2026-08-28

- Matched Orca's model-warning semantics for repairs performed during import.
- Reported remaining open edges as non-manifold-edge warnings.
- Added stable warning details to JSON and human-readable CLI diagnostics.

## 0.2.0 - 2026-08-28

- Migrated Orca's ADMesh STL import-repair sequence.
- Migrated its CGAL deep repair, component filtering, hole filling and orientation.
- Added `import`, `deep`, and `all` CLI modes plus JSON repair diagnostics.
- Pinned ADMesh 0.98.5 and CGAL 5.6.3 build dependencies.
- Relicensed the derived project under AGPL-3.0-only with Orca attribution.

## [Unreleased]

### Added

- Initial C++17/CMake project structure.
- Stable CLI skeleton and exit-code contract.
- Public `repair_file()` module interface.
- Core and CLI smoke tests.
- Universal Apple Silicon and Intel macOS build configuration.
- SPDX source declarations and automated license checks.
- ADMesh and CGAL pre-integration compliance records.
