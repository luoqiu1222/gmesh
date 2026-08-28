# License compliance

## Project license

gmesh repair source is derived from OrcaSlicer and is licensed under
`AGPL-3.0-only`. Project C++ source and header files carry this SPDX identifier.
The root `LICENSE` and `LICENSES/AGPL-3.0-only.txt` contain the
GNU Affero GPL version 3 text.

## ADMesh integration

ADMesh's upstream `COPYING` contains GNU GPL version 2. Its source-file notices
permit redistribution and modification under GPL version 2 or, at the
recipient's option, any later version.

gmesh pins ADMesh 0.98.5 and compiles its six repair/I/O C sources. It uses the
“or later” permission to combine them into the AGPLv3-only program. Releases
must preserve:

- upstream copyright notices;
- the GPL-2.0-or-later source notices;
- an unmodified GPL version 2 license text;
- exact upstream version and source URL;
- prominent notices for modified files and modification dates;
- all local changes as corresponding source.

## CGAL integration

CGAL uses package- and file-specific licensing. Foundation packages are often
LGPL-3.0-or-later, while many higher-level geometry algorithms are
GPL-3.0-or-later; commercial licensing is also available.

For each dependency update:

1. Pin an exact CGAL release or commit.
2. List every included header and package.
3. Read the SPDX notice on those files and package license metadata.
4. Decide whether each package is used under GPL, LGPL, or a purchased
   commercial license.
5. Record the decision and evidence in `THIRD_PARTY_LICENSES.md`.
6. Preserve the matching license and copyright notices in binary and source
   distributions.

The distributed CLI and its complete corresponding source must comply with the
project's AGPLv3-only terms and the applicable CGAL package terms.

## Release gate

A release containing third-party code is blocked until all of the following are
true:

- dependency versions and checksums are pinned;
- license identifiers are known for every compiled file/package;
- third-party notices contain no placeholders;
- patches and generated build inputs are included in corresponding source;
- the binary reports its version and source location;
- the release archive includes `LICENSE`, `LICENSES/`, `NOTICE.md`, and
  `THIRD_PARTY_LICENSES.md`;
- a clean environment can rebuild the distributed CLI from published source.

## Relationship to a proprietary caller

This license review covers the independent gmesh CLI. Whether another program
is a separate work depends on the actual integration. The intended integration
uses process execution and standard mesh/report files; it does not link gmesh,
share memory, or exchange compiler-specific objects.
