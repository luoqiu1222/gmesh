# Test data

Only commit small mesh fixtures whose license and provenance are documented.
Prefer hand-authored fixtures that isolate one topology defect.

Planned categories include truncated files, degenerate triangles, open
boundaries, non-manifold topology, inconsistent winding, self intersections,
and NaN or infinite coordinates.

`open-nonmanifold-65.stl` was supplied by the project owner as a regression
fixture. The Orca-compatible import pass leaves nine open edges; Orca's CGAL
hole-fill path closes one of two boundary cycles but cannot close the remaining
three-edge cycle. Full repair must close it without moving the original surface
or replacing the model with an offset approximation.

`dominant-plane-complex-hole.stl` was supplied by the project owner as a
regression fixture. Its largest non-planar boundary combines a planar base rim
with two raised side openings. Filling the whole cycle at once creates a
triangle from the base apex across the empty interior. Full repair must split
the raised runs from the dominant plane before triangulation.
Its curved side walls also contain boundary vertices duplicated at identical
positions. Treating the resulting pinched boundaries as large holes produces
inward dents. Regression checks sample patch vertices, edge midpoints and face
centroids against an intact wall section, with a 0.020 mm deviation limit.

`frontend-roundtrip-complex-hole.stl` is the same user model after the frontend
decoded and re-encoded its triangle soup. It previously triggered a Windows
access violation in CGAL's global volume predicate after every boundary had
already been closed. Full repair must complete without a native crash.
The same wall-shape check runs after undoing the frontend's centering offset.

`test_curved_hole.cpp` generates a cylindrical wall with a missing patch. It
checks surface deviation and preservation of all source vertices, both in its
original orientation and after rotation, translation, and scaling. It needs no
external fixture or geometry-library dependency in the test executable.

`nonmanifold-orientation-conflict.stl` is a 22-face subset minimized from the
project owner's frontend-exported `top2.stl`. Import orientation propagation
previously introduced winding conflicts on manifold edges; on the full mesh
this created thousand-edge artificial holes and stalled reconstruction. The
orientation test also checks that an ordinary reversed tetrahedron face is
still repaired.

`test_large_hole.cpp` generates a 1,536-edge non-planar rim with a dominant
planar section. Its 20-second CTest deadline catches unbounded candidate-plane
enumeration; the bounded search must still close the complete rim.
