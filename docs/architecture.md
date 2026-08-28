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
        ├── read and validate input
        ├── inspect topology
        ├── repair geometry
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
future mesh I/O and repair dependencies
```

The core module must never depend on the executable or CLI parser.

## Proprietary caller integration

`gmesh` runs as an independent process. A caller should exchange standard mesh
files and a versioned report, not link `gmesh_core`, share memory, or exchange
compiler-specific C++ objects.

## Planned increments

1. Add robust STL input and output.
2. Add read-only topology inspection.
3. Integrate the selected repair engine and record its exact license.
4. Add post-repair closedness and geometry validation.
5. Add atomic output and JSON report serialization.
6. Add hostile-input limits, cancellation, and time budgets.
