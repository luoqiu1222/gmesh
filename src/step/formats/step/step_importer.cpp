// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "formats/step/step_importer.hpp"
#include "core/boundary_triangulation.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <sstream>
#include <utility>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GProp_GProps.hxx>
#include <Geom2d_Curve.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <Poly.hxx>
#include <Poly_PolygonOnTriangulation.hxx>
#include <Poly_Triangle.hxx>
#include <Poly_Triangulation.hxx>
#include <STEPControl_Reader.hxx>
#include <ShapeAnalysis_Surface.hxx>
#include <ShapeFix_Face.hxx>
#include <ShapeFix_Wire.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>
#include <cmath>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <stdexcept>

namespace mrp::model_import {
namespace {

constexpr double kBoundaryApproximationMaxArea = 0.01;

double meshArea(const Handle(Poly_Triangulation) & mesh, const TopLoc_Location &location) {
    double area = 0;
    const auto transform = location.Transformation();
    for (int i = 1; i <= mesh->NbTriangles(); ++i) {
        int a, b, c;
        mesh->Triangle(i).Get(a, b, c);
        const auto p = mesh->Node(a).Transformed(transform);
        const auto q = mesh->Node(b).Transformed(transform);
        const auto r = mesh->Node(c).Transformed(transform);
        area += gp_Vec(p, q).Crossed(gp_Vec(p, r)).Magnitude() * 0.5;
    }
    return area;
}

// Recover a trimmed surface using the exact polylines already used by its neighbors.
// Re-meshing a healed wire independently can change its endpoints or segmentation.
Handle(Poly_Triangulation) meshSharedBoundary(const TopoDS_Face &face, TopLoc_Location &location,
                                              const ImportOptions &options) {
    const auto surface = BRep_Tool::Surface(face, location);
    ShapeAnalysis_Surface inverse(surface);
    BRepAdaptor_Surface adaptor(face);
    const auto transform = location.Transformation();
    const auto undo = transform.Inverted();
    std::vector<std::pair<gp_Pnt2d, gp_Pnt>> boundaryPoints;
    std::vector<std::pair<std::pair<gp_Pnt2d, gp_Pnt>, std::pair<gp_Pnt2d, gp_Pnt>>>
        boundarySegments;
    const auto outer = BRepTools::OuterWire(face);
    auto makeWire = [&](const TopoDS_Wire &wire) {
        const auto begin = boundaryPoints.size();
        std::vector<std::array<double, 2>> ring;
        gp_Pnt previous;
        bool havePoint = false;
        for (BRepTools_WireExplorer edges(wire, face); edges.More(); edges.Next()) {
            const auto edge = edges.Current();
            Handle(Poly_PolygonOnTriangulation) nodes;
            Handle(Poly_Triangulation) neighbor;
            TopLoc_Location neighborLocation;
            BRep_Tool::PolygonOnTriangulation(edge, nodes, neighbor, neighborLocation);
            if (nodes.IsNull() || neighbor.IsNull())
                throw std::runtime_error("STEP recovery has no shared boundary mesh");
            Standard_Real first, last;
            const auto curve = BRep_Tool::CurveOnSurface(edge, face, first, last);
            for (int i = 1; i <= nodes->NbNodes(); ++i) {
                const int j = edge.Orientation() == TopAbs_REVERSED ? nodes->NbNodes() + 1 - i : i;
                const auto point = neighbor->Node(nodes->Node(j))
                                       .Transformed(neighborLocation.Transformation())
                                       .Transformed(undo);
                auto uv = inverse.ValueOfUV(point, 1e-7);
                if (!curve.IsNull() && nodes->HasParameters()) {
                    const auto reference = curve->Value(nodes->Parameter(j));
                    if (adaptor.IsUPeriodic())
                        uv.SetX(uv.X() + std::round((reference.X() - uv.X()) / adaptor.UPeriod()) *
                                             adaptor.UPeriod());
                    if (adaptor.IsVPeriodic())
                        uv.SetY(uv.Y() + std::round((reference.Y() - uv.Y()) / adaptor.VPeriod()) *
                                             adaptor.VPeriod());
                }
                const gp_Pnt flat(uv.X(), uv.Y(), 0);
                if (!havePoint || flat.Distance(previous) > 1e-9) {
                    ring.push_back({uv.X(), uv.Y()});
                    boundaryPoints.emplace_back(uv, point);
                    previous = flat;
                    havePoint = true;
                }
            }
        }
        for (auto i = begin; i < boundaryPoints.size(); ++i) {
            const auto j = i + 1 == boundaryPoints.size() ? begin : i + 1;
            if (boundaryPoints[i].first.SquareDistance(boundaryPoints[j].first) > 1e-18)
                boundarySegments.emplace_back(boundaryPoints[i], boundaryPoints[j]);
        }
        return ring;
    };
    std::vector<std::vector<std::array<double, 2>>> rings{makeWire(outer)};
    for (TopExp_Explorer wires(face, TopAbs_WIRE); wires.More(); wires.Next()) {
        const auto wire = TopoDS::Wire(wires.Current());
        if (!wire.IsSame(outer))
            rings.push_back(makeWire(wire));
    }
    const auto flat = triangulateBoundary(rings, [&](const auto &uv) {
        const double u = (uv[0][0] + uv[1][0] + uv[2][0]) / 3;
        const double v = (uv[0][1] + uv[1][1] + uv[2][1]) / 3;
        gp_Pnt center;
        gp_Vec du, dv;
        surface->D1(u, v, center, du, dv);
        const auto centerNormal = du.Crossed(dv);
        gp_XYZ average(0, 0, 0);
        for (const auto &p : uv) {
            gp_Pnt point;
            surface->D1(p[0], p[1], point, du, dv);
            average += point.XYZ() / 3;
            const auto normal = du.Crossed(dv);
            if (normal.SquareMagnitude() > 1e-20 && centerNormal.SquareMagnitude() > 1e-20 &&
                normal.Angle(centerNormal) > options.angularDeflection)
                return true;
        }
        return center.Distance(gp_Pnt(average)) > std::min(options.linearDeflection, 0.003);
    });
    if (flat.indices.empty())
        return {};
    std::stable_sort(boundarySegments.begin(), boundarySegments.end(),
                     [](const auto &a, const auto &b) {
                         return a.first.second.SquareDistance(a.second.second) >
                                b.first.second.SquareDistance(b.second.second);
                     });
    Handle(Poly_Triangulation) result = new Poly_Triangulation(
        static_cast<int>(flat.points.size()), static_cast<int>(flat.indices.size() / 3), true);
    for (int i = 1; i <= result->NbNodes(); ++i) {
        const auto p = flat.points[i - 1];
        const gp_Pnt2d uv(p[0], p[1]);
        auto point = surface->Value(uv.X(), uv.Y());
        for (const auto &segment : boundarySegments) {
            const gp_Vec2d direction(segment.first.first, segment.second.first);
            const gp_Vec2d offset(segment.first.first, uv);
            const double t = offset.Dot(direction) / direction.SquareMagnitude();
            if (t >= -1e-9 && t <= 1 + 1e-9 &&
                offset.Subtracted(direction.Multiplied(t)).SquareMagnitude() < 1e-16) {
                point = segment.first.second.Translated(
                    gp_Vec(segment.first.second, segment.second.second).Multiplied(t));
                break;
            }
        }
        result->SetNode(i, point);
        result->SetUVNode(i, uv);
    }
    for (int i = 1; i <= result->NbTriangles(); ++i) {
        const auto offset = (i - 1) * 3;
        result->SetTriangle(i, Poly_Triangle(flat.indices[offset] + 1, flat.indices[offset + 1] + 1,
                                             flat.indices[offset + 2] + 1));
    }
    return result;
}

bool wouldExceedOutputLimit(const ImportResult &result, const ImportOptions &options,
                            std::uint64_t addedVertices, std::uint64_t addedTriangles) {
    const std::uint64_t vertices = result.vertexCount() + addedVertices;
    const std::uint64_t triangles = result.triangleCount() + addedTriangles;
    if (vertices > options.maxVertices || vertices > std::numeric_limits<std::uint32_t>::max() ||
        triangles > options.maxTriangles) {
        return true;
    }
    const std::uint64_t bytes =
        vertices * 6ull * sizeof(float) + triangles * 3ull * sizeof(std::uint32_t);
    return bytes > options.maxOutputBytes;
}

ImportErrorCode appendShape(const TopoDS_Shape &shape, const ImportOptions &options,
                            ImportResult &result) {
    for (TopExp_Explorer faceExplorer(shape, TopAbs_FACE); faceExplorer.More();
         faceExplorer.Next()) {
        TopoDS_Face face = TopoDS::Face(faceExplorer.Current());
        ++result.faceCount;
        TopLoc_Location location;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);
        if (triangulation.IsNull() || triangulation->NbNodes() == 0 ||
            triangulation->NbTriangles() == 0) {
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(face, properties);
            bool acceptedBoundaryApproximation = false;
            // Zero-area surfaces cannot cover a visible opening. Preserve this fact in the report.
            if (std::abs(properties.Mass()) < 1e-10) {
                ++result.degenerateFaceCount;
                result.warnings.push_back(
                    "STEP face " + std::to_string(result.faceCount) +
                    " has negligible area (<1e-10 mm^2) and cannot be tessellated");
                continue;
            }
            BRepBuilderAPI_Copy isolated(face, false, false);
            ShapeFix_Face healing(TopoDS::Face(isolated.Shape()));
            healing.AutoCorrectPrecisionMode() = 1;
            healing.FixWireTool()->FixGaps2dMode() = 1;
            healing.FixWireTool()->FixGaps3dMode() = 1;
            healing.Perform();
            auto repaired = healing.Face();
            BRepTools::Clean(repaired);
            BRepMesh_IncrementalMesh retry(repaired, std::min(options.linearDeflection, 1e-5),
                                           false, options.angularDeflection, false);
            triangulation = BRep_Tool::Triangulation(repaired, location);
            if (triangulation.IsNull() || triangulation->NbTriangles() == 0) {
                BRepTools::Clean(repaired);
                BRepMesh_IncrementalMesh stable(repaired, std::min(options.linearDeflection, 1e-5),
                                                false, std::max(options.angularDeflection, 0.5),
                                                false);
                triangulation = BRep_Tool::Triangulation(repaired, location);
                if (!triangulation.IsNull() && triangulation->NbTriangles() > 0) {
                    result.warnings.push_back(
                        "STEP face " + std::to_string(result.faceCount) +
                        " required angular deflection fallback to 0.5 radians");
                }
            }
            if (BRepAdaptor_Surface(face).GetType() == GeomAbs_Plane)
                triangulation.Nullify();
            if (triangulation.IsNull() || triangulation->NbTriangles() == 0) {
                // Sample the original geometric boundaries of skinny planar faces. Keep inner
                // wires.
                BRepAdaptor_Surface surface(face);
                if (surface.GetType() != GeomAbs_Plane)
                    return ImportErrorCode::TessellationFailed;
                const auto outer = BRepTools::OuterWire(face);
                auto polygonWire = [&](const TopoDS_Wire &wire) {
                    BRepBuilderAPI_MakePolygon polygon;
                    gp_Pnt previous;
                    bool havePoint = false;
                    for (BRepTools_WireExplorer edges(wire, face); edges.More(); edges.Next()) {
                        const auto edge = edges.Current();
                        BRepAdaptor_Curve curve(edge);
                        GCPnts_QuasiUniformDeflection sample(
                            curve, std::min(options.linearDeflection, 1e-5));
                        if (!sample.IsDone() || sample.NbPoints() > 1000000)
                            throw std::runtime_error(
                                "STEP planar boundary sampling failed or exceeded limits");
                        for (int i = 1; i <= sample.NbPoints(); ++i) {
                            const auto point = sample.Value(edge.Orientation() == TopAbs_REVERSED
                                                                ? sample.NbPoints() + 1 - i
                                                                : i);
                            if (!havePoint || point.Distance(previous) > 1e-7) {
                                polygon.Add(point);
                                previous = point;
                                havePoint = true;
                            }
                        }
                    }
                    polygon.Close();
                    if (!polygon.IsDone())
                        throw std::runtime_error("STEP planar boundary is not closed");
                    return polygon.Wire();
                };
                BRepBuilderAPI_MakeFace planar(surface.Plane(), polygonWire(outer), true);
                for (TopExp_Explorer wires(face, TopAbs_WIRE); wires.More(); wires.Next()) {
                    auto wire = TopoDS::Wire(wires.Current());
                    if (!wire.IsSame(outer))
                        planar.Add(polygonWire(wire));
                }
                if (!planar.IsDone())
                    return ImportErrorCode::TessellationFailed;
                repaired = planar.Face();
                repaired.Orientation(face.Orientation());
                BRepMesh_IncrementalMesh planarMesh(repaired,
                                                    std::min(options.linearDeflection, 1e-5), false,
                                                    options.angularDeflection, false);
                triangulation = BRep_Tool::Triangulation(repaired, location);
            }
            if (triangulation.IsNull() || triangulation->NbTriangles() == 0)
                return ImportErrorCode::TessellationFailed;
            if (BRepAdaptor_Surface(face).GetType() != GeomAbs_Plane) {
                try {
                    TopLoc_Location sharedLocation;
                    auto shared = meshSharedBoundary(face, sharedLocation, options);
                    const double sharedArea =
                        shared.IsNull() ? 0.0 : meshArea(shared, sharedLocation);
                    const bool sharedAreaMatches =
                        std::abs(sharedArea - properties.Mass()) <=
                        std::max(1e-4, std::abs(properties.Mass()) * 0.05);
                    // Coarse neighboring meshes can collapse a tiny curved face's area while
                    // still providing the only boundary that closes the surrounding shell.
                    acceptedBoundaryApproximation =
                        !sharedAreaMatches && !shared.IsNull() && sharedArea > 0.0 &&
                        std::abs(properties.Mass()) <= kBoundaryApproximationMaxArea;
                    if (!shared.IsNull() &&
                        (sharedAreaMatches || acceptedBoundaryApproximation)) {
                        triangulation = shared;
                        location = sharedLocation;
                        repaired = face;
                        if (!sharedAreaMatches) {
                            result.warnings.push_back(
                                "STEP face " + std::to_string(result.faceCount) +
                                " used a boundary-conforming approximation for a surface below " +
                                "0.01 mm^2");
                        }
                    }
                } catch (const Standard_Failure &) {
                    result.warnings.push_back(
                        "STEP shared boundary recovery could not project a wire");
                } catch (const std::runtime_error &) {
                    result.warnings.push_back(
                        "STEP shared boundary recovery requires a complete neighboring mesh");
                }
            }
            // A successful polygon triangulation must still cover the original CAD face.
            const double triangleArea = meshArea(triangulation, location);
            if (!acceptedBoundaryApproximation &&
                std::abs(triangleArea - properties.Mass()) >
                std::max(1e-4, std::abs(properties.Mass()) * 0.05)) {
                return ImportErrorCode::TessellationFailed;
            }
            face = repaired;
            ++result.recoveredFaceCount;
        }
        ++result.tessellatedFaceCount;
        if (wouldExceedOutputLimit(result, options,
                                   static_cast<std::uint64_t>(triangulation->NbNodes()),
                                   static_cast<std::uint64_t>(triangulation->NbTriangles()))) {
            return ImportErrorCode::ResourceLimit;
        }

        if (!triangulation->HasNormals())
            Poly::ComputeNormals(triangulation);
        const std::uint32_t vertexBase = static_cast<std::uint32_t>(result.vertexCount());
        const auto transform = location.Transformation();
        const bool reversed = face.Orientation() == TopAbs_REVERSED;
        BRepAdaptor_Surface shadingSurface(face);

        for (int node = 1; node <= triangulation->NbNodes(); ++node) {
            const gp_Pnt point = triangulation->Node(node).Transformed(transform);
            result.positions.push_back(static_cast<float>(point.X()));
            result.positions.push_back(static_cast<float>(point.Y()));
            result.positions.push_back(static_cast<float>(point.Z()));

            gp_Dir normal = triangulation->Normal(node);
            normal.Transform(transform);
            if (triangulation->HasUVNodes()) {
                const auto uv = triangulation->UVNode(node);
                gp_Pnt surfacePoint;
                gp_Vec du, dv;
                shadingSurface.D1(uv.X(), uv.Y(), surfacePoint, du, dv);
                auto analytical = du.Crossed(dv);
                if (analytical.SquareMagnitude() > 1e-20) {
                    normal = gp_Dir(analytical);
                    if (transform.IsNegative())
                        normal.Reverse();
                }
            }
            if (reversed)
                normal.Reverse();
            result.normals.push_back(static_cast<float>(normal.X()));
            result.normals.push_back(static_cast<float>(normal.Y()));
            result.normals.push_back(static_cast<float>(normal.Z()));
        }

        for (int triangle = 1; triangle <= triangulation->NbTriangles(); ++triangle) {
            int first = 0;
            int second = 0;
            int third = 0;
            triangulation->Triangle(triangle).Get(first, second, third);
            if (reversed != transform.IsNegative())
                std::swap(second, third);
            const auto a = (vertexBase + first - 1) * 3ull;
            const auto b = (vertexBase + second - 1) * 3ull;
            const auto c = (vertexBase + third - 1) * 3ull;
            const gp_Pnt p(result.positions[a], result.positions[a + 1], result.positions[a + 2]);
            const gp_Pnt q(result.positions[b], result.positions[b + 1], result.positions[b + 2]);
            const gp_Pnt r(result.positions[c], result.positions[c + 1], result.positions[c + 2]);
            const gp_Vec average(
                result.normals[a] + result.normals[b] + result.normals[c],
                result.normals[a + 1] + result.normals[b + 1] + result.normals[c + 1],
                result.normals[a + 2] + result.normals[b + 2] + result.normals[c + 2]);
            if (gp_Vec(p, q).Crossed(gp_Vec(p, r)).Dot(average) < 0) {
                std::swap(second, third);
                ++result.reorientedTriangleCount;
            }
            result.indices.push_back(vertexBase + static_cast<std::uint32_t>(first - 1));
            result.indices.push_back(vertexBase + static_cast<std::uint32_t>(second - 1));
            result.indices.push_back(vertexBase + static_cast<std::uint32_t>(third - 1));
        }
    }
    return ImportErrorCode::None;
}

} // namespace

ImportOutcome StepImporter::import(std::istream &input, const std::string &sourceName,
                                   const ImportOptions &options, const ProgressCallback &progress) {
    // OCCT 8.0 on Windows can fast-fail when ReadStream receives a UTF-8 name
    // containing non-ASCII characters. The stream is already open, so this name
    // is diagnostic metadata only and must not be the original filesystem path.
    (void)sourceName;
    constexpr const char *kOcctStreamName = "model.step";
    ImportOutcome outcome;
    outcome.result.sourceFormat = "step";
    outcome.result.sourceUnit = options.linearUnit;

    try {
        progress("read", 10, "Reading STEP data");
        Interface_Static::SetCVal("xstep.cascade.unit", "MM");
        STEPControl_Reader reader;
        if (reader.ReadStream(kOcctStreamName, input) != IFSelect_RetDone) {
            outcome.errorCode = ImportErrorCode::ReadFailed;
            outcome.error = "Open CASCADE could not read the STEP stream";
            return outcome;
        }

        progress("transfer", 30, "Transferring STEP entities");
        if (reader.NbRootsForTransfer() == 0 || reader.TransferRoots() == 0) {
            outcome.errorCode = ImportErrorCode::TransferFailed;
            outcome.error = "Open CASCADE could not transfer STEP entities";
            return outcome;
        }

        TopoDS_Shape rootShape = reader.OneShape();
        if (rootShape.IsNull()) {
            outcome.errorCode = ImportErrorCode::TransferFailed;
            outcome.error = "STEP transfer produced no shape";
            return outcome;
        }

        progress("tessellate", 45, "Tessellating STEP shapes");
        double deflection = options.linearDeflection;
        if (lowercase(options.linearDeflectionType) == "bounding_box_ratio") {
            Bnd_Box bounds;
            BRepBndLib::Add(rootShape, bounds, false);
            double x, y, z, X, Y, Z;
            bounds.Get(x, y, z, X, Y, Z);
            deflection *= std::max({X - x, Y - y, Z - z});
        }
        BRepMesh_IncrementalMesh mesher(rootShape, deflection, false, options.angularDeflection,
                                        options.parallel);
        if (!mesher.IsDone()) {
            outcome.result.warnings.push_back("STEP shape could not be fully tessellated");
        }

        std::uint32_t sourceIndex = 0;
        bool foundSolid = false;
        int solidIndex = 0;
        for (TopExp_Explorer solidExplorer(rootShape, TopAbs_SOLID); solidExplorer.More();
             solidExplorer.Next(), ++solidIndex) {
            foundSolid = true;
            const std::uint64_t groupStart = outcome.result.indices.size();
            const auto error = appendShape(solidExplorer.Current(), options, outcome.result);
            if (error != ImportErrorCode::None) {
                outcome.errorCode = error;
                outcome.error = error == ImportErrorCode::TessellationFailed
                                    ? "STEP face has no triangulation; refusing incomplete geometry"
                                    : "model import exceeded configured resource limits";
                return outcome;
            }
            const std::uint64_t groupCount = outcome.result.indices.size() - groupStart;
            if (groupCount > 0) {
                outcome.result.groups.push_back(SourceGroup{groupStart, groupCount, sourceIndex++,
                                                            "step",
                                                            "solid-" + std::to_string(solidIndex)});
            }
        }
        if (!foundSolid) {
            const std::uint64_t groupStart = outcome.result.indices.size();
            const auto error = appendShape(rootShape, options, outcome.result);
            if (error != ImportErrorCode::None) {
                outcome.errorCode = error;
                outcome.error = error == ImportErrorCode::TessellationFailed
                                    ? "STEP face has no triangulation; refusing incomplete geometry"
                                    : "model import exceeded configured resource limits";
                return outcome;
            }
            const std::uint64_t groupCount = outcome.result.indices.size() - groupStart;
            if (groupCount > 0) {
                outcome.result.groups.push_back(
                    SourceGroup{groupStart, groupCount, sourceIndex++, "step", "shape-0"});
            }
        }
        progress("tessellate", 85, "Tessellating STEP shapes");

        if (outcome.result.indices.empty()) {
            outcome.errorCode = ImportErrorCode::TessellationFailed;
            outcome.error = "STEP import produced no triangles";
            return outcome;
        }
        progress("output", 90, "Preparing model import output");
        return outcome;
    } catch (const Standard_Failure &failure) {
        outcome.errorCode = ImportErrorCode::TransferFailed;
        outcome.error = failure.GetMessageString() ? failure.GetMessageString() : "";
        if (outcome.error.empty())
            outcome.error = "Open CASCADE raised an import error";
        return outcome;
    } catch (const std::exception &error) {
        outcome.errorCode = ImportErrorCode::TransferFailed;
        outcome.error = error.what();
        return outcome;
    }
}

} // namespace mrp::model_import
