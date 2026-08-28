# CLI contract

## Commands

```text
gmesh --help
gmesh --version
gmesh --license
gmesh repair --input <path> --output <path> [--report <path>] [--overwrite]
```

## Exit codes

| Code | Meaning |
|---:|---|
| 0 | Success |
| 1 | Invalid command or argument |
| 2 | Input error (reserved) |
| 3 | Unsupported format (reserved) |
| 4 | Empty mesh (reserved) |
| 5 | Repair failure or backend unavailable |
| 6 | Repaired mesh is still open (reserved) |
| 7 | Output error (reserved) |
| 8 | Canceled (reserved) |
| 9 | Timed out (reserved) |
| 10 | Unexpected internal exception |

## Compatibility rules

- Existing option meanings and exit codes do not change within a major version.
- New report fields are additive; callers must ignore unknown fields.
- Machine-readable reports carry an independent `schema_version`.
- Diagnostics go to stderr; machine output goes to its report file.
- Paths are passed as process arguments, never through shell command strings.
