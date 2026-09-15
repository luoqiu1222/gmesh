# STEP import in the independent gmesh executable

Windows now uses `gmesh.exe convert` for STEP/STP and `gmesh.exe repair` for STL.
No Open CASCADE DLLs are required by the distributed Windows executable. It uses
the Windows system libraries and Microsoft Visual C++ runtime.

The source reference is the user's OrcaSlicer checkout:
`src/libslic3r/Format/STEP.cpp`, `Step::mesh`, and `deps/OCCT/OCCT.cmake`.
Orca pins OCCT 7.6.0 and uses absolute deflection, face placements, and reversed
triangle winding. This implementation reuses the existing bounded RMIP protocol
adapter, rather than depending on Orca's GUI and slicer model. Assembly product
names/colors are not imported; solids retain separate source groups.

`bounding_box_ratio` is converted to an absolute deflection using the longest
dimension of the whole CAD model. `absolute` is already in millimeters. Angular
deflection is in radians. Millimeter output is supported.

The former parser silently skipped faces with missing triangulations. This
implementation checks every emitted solid face. Failed faces receive local
ShapeFix wire/gap repair and finer linear deflection (at most 0.00001 mm).
If necessary, angular deflection falls back to 0.5 radians and is reported in
`warnings`. Failed planar faces are sampled from their original geometric wires
and rebuilt, including inner boundaries. Recovered mesh area must agree with
CAD face area within 5% or 0.0001 mm², whichever is larger. Faces below 1e-10 mm²
that cannot be tessellated are counted as degenerate and reported. All other
unresolved missing faces fail the conversion before geometry is emitted.

This is an import recovery procedure; it does not claim every source CAD model
will be watertight. Approximate boundaries of recovered planar faces may differ
slightly from the triangulation of their neighbors.

### Boundary continuity and shading

Independent healing can move a failed curved face's trim endpoints, producing
a visible slit even when every face has triangles. Recovery now runs on an
isolated topology copy. Where the original neighboring triangulations provide
complete boundaries, their exact polylines are projected to the CAD surface UV
domain and triangulated using CGAL constrained Delaunay triangulation with
even/odd domain marking. This also handles intersecting trim loops. Interior
vertices are refined against surface deflection and angular deviation, with
a 100,000-vertex/16-iteration limit. Refinement preserves boundary constraints;
intersection vertices are interpolated along the original shared polylines.
Candidates that fail the CAD area check are discarded in favor of the existing
validated recovery mesh. When a missing curved face has a complete shared
boundary but that boundary would collapse most of its CAD area, gmesh cleans
the shape and retries coordinated tessellation at 60% of the original linear
deflection before emitting any faces. This lets OCCT regenerate the face and its
neighbors together instead of accepting a closed but visually empty patch.

Vertex normals now come from CAD surface derivatives when UV coordinates are
available, preserving curved shading independently of triangle density. At
singular derivatives the triangulation normal remains the fallback. Triangle
winding is checked against those normals using the actual output float32
positions, and corrected triangles are counted in `reorientedTriangleCount`.

The plate regression additionally checks triangle/normal agreement, samples
long boundary pairs to detect geometric gaps despite different subdivisions,
rejects sub-millimeter isolated boundary loops, and checks that the formerly
missing curved patch retains visible area at the frontend medium preset.
The original long slit had a sampled gap of 0.02194 mm; the shared-boundary fix
reduced it to about 0.0000025 mm before subsequent interior refinement. This is
a geometry check, not a screenshot comparison or a watertightness guarantee.

## Build

STEP support is optional and defaults off for ordinary local builds. CI builds
static OCCT 7.6.0 and enables STEP for the Windows, Linux, and universal macOS
artifacts.
Build OCCT **7.6.0** statically first, with no TBB or visualization dependencies:

```powershell
cmake -S <OCCT-7_6_0> -B occt-build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRARY_TYPE=Static `
  -DBUILD_MODULE_FoundationClasses=OFF -DBUILD_MODULE_ModelingData=OFF `
  -DBUILD_MODULE_ModelingAlgorithms=OFF -DBUILD_MODULE_Visualization=OFF `
  -DBUILD_MODULE_ApplicationFramework=OFF -DBUILD_MODULE_DataExchange=OFF `
  -DBUILD_MODULE_Draw=OFF `
  '-DBUILD_ADDITIONAL_TOOLKITS=TKSTEP TKSTEP209 TKSTEPAttr TKSTEPBase TKXSBase TKShHealing TKMesh TKPrim TKTopAlgo TKGeomAlgo TKBRep TKGeomBase TKG3d TKG2d TKMath TKernel' `
  -DUSE_TBB=OFF -DUSE_FREETYPE=OFF -DUSE_TCL=OFF `
  -DINSTALL_DIR=<occt-install> -DINSTALL_DIR_LIB=lib `
  -DINSTALL_DIR_INCLUDE=include/opencascade -DINSTALL_DIR_CMAKE=cmake
cmake --build occt-build
cmake --install occt-build
cmake -S . -B build-step -DGMESH_ENABLE_STEP=ON `
  -DOpenCASCADE_DIR=<occt-install>/cmake -DGMESH_BUILD_TESTS=ON
cmake --build build-step --config Release
ctest --test-dir build-step -C Release --output-on-failure
```

Use a Visual Studio x64 developer shell on Windows. Existing offline ADMesh,
Boost, and CGAL source variables still apply. Distribute the executable with
its license notices and corresponding source/build materials.

## Protocol and verification

```powershell
gmesh.exe convert --input model.stp --format step --output-stdio `
  --linear-deflection-type absolute --linear-deflection 0.003 `
  --angular-deflection 0.5 --parallel
node tests/step_regression.cjs <gmesh.exe> <安装板.stp> 593
```

`--input-stdin --input-name model.stp` accepts file bytes instead of a path.
Stdout contains RMIP version 1 binary frames; diagnostics belong on stderr.
Position/normal chunks are float32 and indices are uint32. The report adds
`faceCount`, `tessellatedFaceCount`, `recoveredFaceCount`,
`degenerateFaceCount`, and `warnings`. Cancellation terminates the child process.

The original plate reproducer failed with exit 5 and a missing-triangulation
error. Regression checks all five frontend presets and verifies that all 593
faces are accounted for, only one negligible-area face is excluded, and the
reported buffer sizes and protocol termination agree. The original user CAD
file is not redistributed in this repository.

Windows, Linux, and macOS distribution artifacts contain the same STEP-enabled
gmesh command-line interface.
