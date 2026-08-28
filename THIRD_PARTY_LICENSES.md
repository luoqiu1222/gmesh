# Third-party licenses

No third-party source dependency is currently compiled or linked into the
project skeleton.

## Planned dependency reviews

The following projects are planned, but are **not integrated yet**. A dependency
may not be merged until its exact version, used packages/files, license choice,
copyright notices, source archive, and local modifications are recorded.

| Dependency | Version/commit | Planned purpose | License under review | Source |
|---|---|---|---|---|
| ADMesh | Not pinned | STL parsing and light mesh repair | GPL-2.0-or-later, based on upstream source notices | [admesh/admesh](https://github.com/admesh/admesh) |
| CGAL | Not pinned | Polygon-soup and topology repair | Package-specific GPL-3.0-or-later, LGPL-3.0-or-later, or commercial | [CGAL](https://www.cgal.org/) |

ADMesh distributes the GPL version 2 text and source files that grant the
option to use version 2 or any later version. If integrated with GPLv3-only or
GPLv3-or-later CGAL packages, gmesh will exercise ADMesh's “or later” option and
distribute the combined CLI under GPL-3.0-or-later.

CGAL licensing is package-specific. The exact headers and packages used must be
audited against the CGAL distribution's license metadata. A top-level statement
that “CGAL is GPL/LGPL” is not sufficient.

## Required records for an integrated dependency

- exact release and commit;
- download/source URL and archive checksum;
- every package or source directory compiled into gmesh;
- SPDX/license identifier for those files;
- original copyright and attribution notices;
- local patches and modification dates;
- build and linkage method;
- corresponding-source location for the released binary.

Review the exact packages and source files used, not only a library's top-level
license description.
