# License compliance

## Project license

Original gmesh source code is licensed under `GPL-3.0-or-later`. Project C++
source and header files carry this SPDX identifier. The root `LICENSE` and
`LICENSES/GPL-3.0-or-later.txt` contain the GNU GPL version 3 text.

## Planned ADMesh integration

ADMesh's upstream `COPYING` contains GNU GPL version 2. Its source-file notices
permit redistribution and modification under GPL version 2 or, at the
recipient's option, any later version.

If ADMesh is combined with GPLv3-or-later CGAL packages, gmesh will use the
“or later” permission and distribute the combined work under
`GPL-3.0-or-later`. Integration must preserve:

- upstream copyright notices;
- the GPL-2.0-or-later source notices;
- an unmodified GPL version 2 license text;
- exact upstream version and source URL;
- prominent notices for modified files and modification dates;
- all local changes as corresponding source.

## Planned CGAL integration

CGAL uses package- and file-specific licensing. Foundation packages are often
LGPL-3.0-or-later, while many higher-level geometry algorithms are
GPL-3.0-or-later; commercial licensing is also available.

Before integration:

1. Pin an exact CGAL release or commit.
2. List every included header and package.
3. Read the SPDX notice on those files and package license metadata.
4. Decide whether each package is used under GPL, LGPL, or a purchased
   commercial license.
5. Record the decision and evidence in `THIRD_PARTY_LICENSES.md`.
6. Preserve the matching license and copyright notices in binary and source
   distributions.

If any required CGAL repair package is GPL-3.0-or-later, the distributed gmesh
CLI and its complete corresponding source must comply with GPLv3-or-later.

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
