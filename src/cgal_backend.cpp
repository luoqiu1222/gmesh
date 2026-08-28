// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "mesh.hpp"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/boost/graph/Face_filtered_graph.h>
#include <CGAL/boost/graph/copy_face_graph.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/connected_components.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <vector>

namespace gmesh::detail {
namespace {

namespace PMP = CGAL::Polygon_mesh_processing;
using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using SurfaceMesh = CGAL::Surface_mesh<Kernel::Point_3>;
using Face = boost::graph_traits<SurfaceMesh>::face_descriptor;
using Halfedge = boost::graph_traits<SurfaceMesh>::halfedge_descriptor;

SurfaceMesh soup_to_surface(const Mesh &source)
{
    std::vector<Kernel::Point_3> points;
    std::vector<std::vector<std::size_t>> polygons;
    points.reserve(source.vertices.size());
    polygons.reserve(source.triangles.size());
    for (const Point &point : source.vertices)
        points.emplace_back(point.x, point.y, point.z);
    for (const Triangle &triangle : source.triangles)
        polygons.push_back({triangle[0], triangle[1], triangle[2]});

    PMP::repair_polygon_soup(points, polygons);
    SurfaceMesh result;
    PMP::polygon_soup_to_polygon_mesh(points, polygons, result);
    PMP::remove_degenerate_faces(result);
    PMP::remove_isolated_vertices(result);
    PMP::duplicate_non_manifold_vertices(result);
    return result;
}

double signed_volume(const SurfaceMesh &mesh)
{
    double result = 0.0;
    for (const Face face : mesh.faces()) {
        std::array<Kernel::Point_3, 3> points;
        std::size_t count = 0;
        for (const auto vertex : CGAL::vertices_around_face(mesh.halfedge(face), mesh)) {
            if (count < points.size())
                points[count] = mesh.point(vertex);
            ++count;
        }
        if (count != 3)
            continue;
        const auto &a = points[0];
        const auto &b = points[1];
        const auto &c = points[2];
        result += (CGAL::to_double(a.x()) *
                       (CGAL::to_double(b.y()) * CGAL::to_double(c.z()) -
                        CGAL::to_double(b.z()) * CGAL::to_double(c.y())) -
                   CGAL::to_double(a.y()) *
                       (CGAL::to_double(b.x()) * CGAL::to_double(c.z()) -
                        CGAL::to_double(b.z()) * CGAL::to_double(c.x())) +
                   CGAL::to_double(a.z()) *
                       (CGAL::to_double(b.x()) * CGAL::to_double(c.y()) -
                        CGAL::to_double(b.y()) * CGAL::to_double(c.x()))) / 6.0;
    }
    return result;
}

bool is_not_three_dimensional(const SurfaceMesh &mesh)
{
    if (mesh.is_empty())
        return true;
    double min_x = std::numeric_limits<double>::max();
    double min_y = min_x;
    double min_z = min_x;
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = max_x;
    double max_z = max_x;
    for (const auto vertex : mesh.vertices()) {
        const auto &point = mesh.point(vertex);
        const double x = CGAL::to_double(point.x());
        const double y = CGAL::to_double(point.y());
        const double z = CGAL::to_double(point.z());
        min_x = std::min(min_x, x); max_x = std::max(max_x, x);
        min_y = std::min(min_y, y); max_y = std::max(max_y, y);
        min_z = std::min(min_z, z); max_z = std::max(max_z, z);
    }
    const std::array<double, 3> size{max_x - min_x, max_y - min_y, max_z - min_z};
    const double min_dim = *std::min_element(size.begin(), size.end());
    const double max_dim = *std::max_element(size.begin(), size.end());
    constexpr double epsilon = 1e-9;
    if (min_dim <= epsilon || max_dim <= epsilon || min_dim / max_dim <= 1e-6)
        return true;
    const double volume = std::abs(signed_volume(mesh));
    const double bbox_volume = size[0] * size[1] * size[2];
    return volume <= epsilon || (bbox_volume > 0.0 && volume / bbox_volume <= 1e-6);
}

bool repair_part(SurfaceMesh &mesh, RepairDiagnostics &diagnostics, std::string &error)
{
    PMP::remove_degenerate_faces(mesh);
    PMP::remove_isolated_vertices(mesh);
    PMP::duplicate_non_manifold_vertices(mesh);

    SurfaceMesh united;
    if (PMP::corefine_and_compute_union(mesh, mesh, united)) {
        mesh = std::move(united);
        diagnostics.union_succeeded = true;
    }

    if (!CGAL::is_closed(mesh)) {
        std::vector<Halfedge> borders;
        PMP::extract_boundary_cycles(mesh, std::back_inserter(borders));
        diagnostics.holes_found += borders.size();
        for (const Halfedge border : borders) {
            const std::size_t before = mesh.number_of_faces();
            PMP::triangulate_and_refine_hole(mesh, border);
            if (mesh.number_of_faces() > before)
                ++diagnostics.holes_filled;
        }
    }

    if (!CGAL::is_closed(mesh)) {
        error = "repair failed: mesh is still open after CGAL hole filling";
        return false;
    }
    if (!PMP::does_bound_a_volume(mesh))
        PMP::orient_to_bound_a_volume(mesh);
    return true;
}

Mesh surface_to_mesh(SurfaceMesh &source)
{
    Mesh result;
    auto index_map = source.add_property_map<SurfaceMesh::Vertex_index, std::uint32_t>(
        "v:gmesh-index", 0).first;
    result.vertices.reserve(source.number_of_vertices());
    for (const auto vertex : source.vertices()) {
        const auto &point = source.point(vertex);
        index_map[vertex] = static_cast<std::uint32_t>(result.vertices.size());
        result.vertices.push_back({CGAL::to_double(point.x()), CGAL::to_double(point.y()),
                                   CGAL::to_double(point.z())});
    }
    result.triangles.reserve(source.number_of_faces());
    for (const Face face : source.faces()) {
        Triangle triangle{};
        std::size_t count = 0;
        for (const auto vertex : CGAL::vertices_around_face(source.halfedge(face), source)) {
            if (count < 3)
                triangle[count] = index_map[vertex];
            ++count;
        }
        if (count == 3)
            result.triangles.push_back(triangle);
    }
    return result;
}

} // namespace

bool deep_repair(Mesh &mesh, RepairDiagnostics &diagnostics, std::string &error)
{
    try {
        SurfaceMesh source = soup_to_surface(mesh);
        if (source.is_empty()) {
            error = "CGAL polygon-soup cleanup removed every triangle";
            return false;
        }

        auto component_map = source.add_property_map<Face, std::size_t>("f:component", 0).first;
        const std::size_t component_count = PMP::connected_components(source, component_map);
        diagnostics.parts_found += component_count;

        SurfaceMesh repaired;
        for (std::size_t component = 0; component < component_count; ++component) {
            CGAL::Face_filtered_graph<SurfaceMesh> filtered(source);
            filtered.set_selected_faces(component, component_map);
            SurfaceMesh part;
            CGAL::copy_face_graph(filtered, part);
            if (is_not_three_dimensional(part)) {
                ++diagnostics.parts_removed;
                continue;
            }
            if (!repair_part(part, diagnostics, error))
                return false;
            repaired.join(part);
        }

        if (repaired.is_empty()) {
            error = "every connected part was empty, planar, too thin, or negligible";
            return false;
        }
        PMP::stitch_borders(repaired);
        PMP::remove_isolated_vertices(repaired);
        mesh = surface_to_mesh(repaired);
        return !mesh.vertices.empty() && !mesh.triangles.empty();
    } catch (const std::exception &exception) {
        error = exception.what();
        return false;
    }
}

} // namespace gmesh::detail
