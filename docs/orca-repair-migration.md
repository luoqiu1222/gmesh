# Orca repair migration manifest

This manifest is the hand-off checklist for the standalone repair CLI. It maps
the OrcaSlicer repair behavior into gmesh and makes deliberate exclusions
explicit.

## Migrated geometry behavior

| Orca source behavior | gmesh implementation | CLI mode |
|---|---|---|
| STL loading through ADMesh | `src/admesh_backend.cpp` | all modes |
| Exact facet-neighbor matching | `run_orca_import_sequence()` | `import`, `all` |
| Two nearby-edge passes, starting at shortest edge and increasing by bounding diameter / 10000 | `run_orca_import_sequence()` | `import`, `all` |
| Degenerate and fully unconnected facet removal | `run_orca_import_sequence()` | `import`, `all` |
| ADMesh hole filling intentionally disabled | documented in `run_orca_import_sequence()` | `import`, `all` |
| Normal-direction and normal-value repair | `run_orca_import_sequence()` | `import`, `all` |
| Positive-volume orientation and neighbor verification | `run_orca_import_sequence()` | `import`, `all` |
| Rebuild exact connectivity after degenerates are removed | `run_orca_import_sequence()` | `import`, `all` |
| Split into connected parts | `deep_repair()` | `deep`, `all` |
| Remove empty, planar, too-thin and negligible-volume parts | `is_not_three_dimensional()` | `deep`, `all` |
| CGAL polygon-soup repair | `soup_to_surface()` | `deep`, `all` |
| Remove degenerate faces and isolated vertices | `soup_to_surface()`, `repair_part()` | `deep`, `all` |
| Duplicate non-manifold vertices | `soup_to_surface()`, `repair_part()` | `deep`, `all` |
| Self-union to retain the outer shell, continuing if union fails | `repair_part()` | `deep`, `all` |
| Extract boundary cycles and triangulate/refine holes | `repair_part()` | `deep`, `all` |
| Reject a result that remains open | `repair_part()`, `repair_file()` | `deep`, `all` |
| Orient a closed mesh to bound a volume | `repair_part()` | `deep`, `all` |

## Standalone additions

- Atomic-style temporary output before placing the final STL.
- Versioned JSON report with input/output topology and repair counters.
- Stable process exit codes for invalid arguments, input, repair, output and
  unexpected failures.
- Explicit `import`, `deep`, and `all` policies.
- Windows, Linux, and macOS build definitions; macOS defaults to Universal 2.

## Deliberately not migrated

These Orca operations are application integration, not mesh-repair geometry:

- wxWidgets progress dialog, localization and worker-thread UI signaling;
- model-volume insertion/deletion and unique-ID management;
- painting save/remap/restore;
- convex-hull and 2D convex-hull cache invalidation;
- object bounding-box invalidation and undo/redo integration;
- slicer configuration, G-code, printer profiles and 3MF project handling.

The standalone equivalent of progress, cancellation, hostile-input limits and
timeouts remains future work. Their absence does not change the migrated repair
algorithm, but callers should enforce a process timeout until native limits are
implemented.

## Licensing consequence

The repair sequence and deep-repair policy were migrated from OrcaSlicer, so
gmesh is distributed under `AGPL-3.0-only`. ADMesh 0.98.5 is used under its
GPL-2.0-or-later grant, Boost 1.84.0 under BSL-1.0, and the used CGAL 5.6.3 mesh
processing packages under GPL-3.0-or-later. See `THIRD_PARTY_LICENSES.md` for
the pinned source records and hashes.
