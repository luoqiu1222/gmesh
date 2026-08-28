# CLI contract

## Commands

```text
gmesh --help
gmesh --version
gmesh --license
gmesh repair --input <mesh.stl> --output <mesh.stl>
             [--report <path>] [--mode import|deep|all] [--overwrite]
```

## Exit codes

| Code | Meaning |
|---:|---|
| 0 | Success |
| 1 | Invalid command or argument |
| 2 | Input error |
| 3 | Unsupported format (reserved) |
| 4 | Empty mesh (reserved) |
| 5 | Repair failure or backend unavailable |
| 6 | Repaired mesh is still open (reserved) |
| 7 | Output error |
| 8 | Canceled (reserved) |
| 9 | Timed out (reserved) |
| 10 | Unexpected internal exception |

## Compatibility rules

- Existing option meanings and exit codes do not change within a major version.
- New report fields are additive; callers must ignore unknown fields.
- Machine-readable reports carry an independent `schema_version`.
- Diagnostics go to stderr; machine output goes to its report file.
- Paths are passed as process arguments, never through shell command strings.

## Repair modes

- `import`: ADMesh exact/nearby connectivity and normal repair; holes remain open.
- `deep`: CGAL connected-part filtering, polygon-soup repair, hole filling and orientation.
- `all`: import followed by deep repair; this is the default.

Deep repair only succeeds when its output is non-empty and closed. Report files
use `schema_version: 1`; repair counters are additive fields.

## Model warnings

Warning semantics match Orca's object-list indicator:

- `warnings.auto_repaired` contains fixed edges, degenerate facets, removed
  facets, reversed facets, and backwards edges. Its `count` is their sum.
- `warnings.remaining_errors.non_manifold_edges` contains the output mesh's
  open-edge count. This is the only remaining topology error Orca names in its
  model warning tooltip.
- `warnings.has_warning` is true when either an auto-repair counter is non-zero
  or non-manifold edges remain.

Warnings are informational: a completed repair still exits with code 0. The
caller must inspect the report when it wants to distinguish a clean model from
a successful result that should display Orca-compatible warnings.
