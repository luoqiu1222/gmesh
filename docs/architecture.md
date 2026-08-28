# Architecture

## External seam

The external module interface is `gmesh::repair_file()`. Callers provide file
paths and policy options, then receive a value-type report. They do not need to
know which parser, topology representation, or geometry engine is used.

```text
CLI argument parser
        │
        ▼
repair_file(options)                 external seam
        │
        ├── ADMesh STL import / import repair
        ├── convert to an internal indexed mesh
        ├── split and filter connected parts
        ├── CGAL deep repair
        ├── validate repaired mesh
        ├── write output atomically
        └── produce report
```

This is a deep module: a small interface hides the repair workflow and
concentrates error handling and verification in one place.

## Dependency direction

```text
gmesh executable
      ↓
gmesh_core
      ↓
ADMesh 0.98.5 + CGAL 5.6.3
```

The core module must never depend on the executable or CLI parser.

## Proprietary caller integration

`gmesh` runs as an independent process. A caller should exchange standard mesh
files and a versioned report, not link `gmesh_core`, share memory, or exchange
compiler-specific C++ objects.

Install rules therefore ship the executable and license/notice files only.
`gmesh_core` and its C++ header are repository-internal build/test seams, not a
supported binary integration SDK.

## Implemented repair closure

The migrated closure includes Orca's two-pass nearby-edge policy, isolated and
degenerate facet removal, normal/volume correction, connected-component
splitting, planar/thin/negligible component filtering, polygon-soup cleanup,
non-manifold vertex duplication, self-union, hole triangulation/refinement,
closedness validation and outward orientation. GUI progress, painting remap,
convex-hull cache updates, wxWidgets, and slicer data types stay outside this
standalone module.

Hostile-input limits, cancellation and time budgets remain future work.
