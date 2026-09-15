# Third-party licenses

gmesh downloads or accepts local copies of the following pinned source
dependencies and compiles them into, or includes them in, the CLI build.

| Dependency | Version/commit | Purpose | License used | Source |
|---|---|---|---|---|
| OrcaSlicer | migrated behavior from the 2026-08-28 source snapshot | Repair sequence and policy | AGPL-3.0-only | [OrcaSlicer](https://github.com/SoftFever/OrcaSlicer) |
| ADMesh | 0.98.5, commit `70ca24a9b4e6d8aa05e8572e768110dad9b4d47b` | STL parsing and import repair | GPL-2.0-or-later | [admesh/admesh](https://github.com/admesh/admesh/tree/v0.98.5) |
| Boost | 1.84.0 | Header support required by CGAL | BSL-1.0 | [boostorg/boost](https://github.com/boostorg/boost/releases/tag/boost-1.84.0) |
| CGAL | 5.6.3 | Polygon-soup and topology repair | GPL-3.0-or-later for the used packages; supporting files also include LGPL-3.0-or-later and BSL-1.0 notices | [CGAL 5.6.3](https://github.com/CGAL/cgal/releases/tag/v5.6.3) |

ADMesh source files grant GPL version 2 or any later version. gmesh exercises
that “or later” option when combining ADMesh with AGPLv3-only project code.
The compiled ADMesh files are `connect.c`, `normals.c`, `shared.c`, `stl_io.c`,
`stlinit.c`, and `util.c`; no local patch is applied.

CGAL is used without a commercial license. The directly included repair,
corefinement, orientation, connected-components, border stitching, hole
triangulation, Surface_mesh, kernel, and Boost Graph adapters are used under
CGAL's GPLv3-or-later path. The source archive SHA-256 is
`5d577acb4a9918ccb960491482da7a3838f8d363aff47e14d703f19fd84733d4`.

Boost's source archive SHA-256 is
`cc4b893acf645c9d4b698e9a0f08ca8846aa5d6c68275c14c3e7949c24109454`.

## Distribution requirements

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
