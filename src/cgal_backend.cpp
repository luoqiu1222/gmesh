// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "mesh.hpp"

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/boost/graph/Euler_operations.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/merge_border_vertices.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace gmesh::detail {
namespace {

namespace PMP = CGAL::Polygon_mesh_processing;
using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
using SurfaceMesh = CGAL::Surface_mesh<Kernel::Point_3>;
using Face = boost::graph_traits<SurfaceMesh>::face_descriptor;
using Halfedge = boost::graph_traits<SurfaceMesh>::halfedge_descriptor;
using Vertex = boost::graph_traits<SurfaceMesh>::vertex_descriptor;

using Edge = std::pair<std::uint32_t, std::uint32_t>;

std::vector<Mesh> split_into_edge_connected_parts(const Mesh &source)
{
    // Ported from OrcaSlicer's its_split/create_face_neighbors_index contract:
    // faces belong to the same part only when they share an oppositely-oriented
    // indexed edge. Merely touching at one vertex does not connect two parts.
    std::vector<std::array<std::int64_t, 3>> neighbors(
        source.triangles.size(), {-1, -1, -1});
    std::map<Edge, std::vector<std::pair<std::size_t, std::size_t>>> unmatched;

    for (std::size_t face_index = 0; face_index < source.triangles.size(); ++face_index) {
        const Triangle &face = source.triangles[face_index];
        for (std::size_t edge_index = 0; edge_index < 3; ++edge_index) {
            const Edge edge{face[edge_index], face[(edge_index + 1) % 3]};
            auto reverse = unmatched.find({edge.second, edge.first});
            if (reverse != unmatched.end() && !reverse->second.empty()) {
                const auto [other_face, other_edge] = reverse->second.front();
                reverse->second.erase(reverse->second.begin());
                if (reverse->second.empty())
                    unmatched.erase(reverse);
                neighbors[face_index][edge_index] = static_cast<std::int64_t>(other_face);
                neighbors[other_face][other_edge] = static_cast<std::int64_t>(face_index);
            } else {
                unmatched[edge].push_back({face_index, edge_index});
            }
        }
    }

    std::vector<bool> visited(source.triangles.size(), false);
    std::vector<Mesh> parts;
    for (std::size_t seed = 0; seed < source.triangles.size(); ++seed) {
        if (visited[seed])
            continue;

        std::vector<std::size_t> stack{seed};
        std::vector<std::size_t> faces;
        visited[seed] = true;
        while (!stack.empty()) {
            const std::size_t face = stack.back();
            stack.pop_back();
            faces.push_back(face);
            for (const std::int64_t neighbor : neighbors[face]) {
                if (neighbor >= 0 && !visited[static_cast<std::size_t>(neighbor)]) {
                    visited[static_cast<std::size_t>(neighbor)] = true;
                    stack.push_back(static_cast<std::size_t>(neighbor));
                }
            }
        }

        Mesh part;
        std::vector<std::uint32_t> vertex_images(
            source.vertices.size(), std::numeric_limits<std::uint32_t>::max());
        part.triangles.reserve(faces.size());
        part.vertices.reserve(std::min(source.vertices.size(), faces.size() * 3));
        for (const std::size_t face_index : faces) {
            Triangle remapped{};
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const std::uint32_t original = source.triangles[face_index][corner];
                std::uint32_t &image = vertex_images[original];
                if (image == std::numeric_limits<std::uint32_t>::max()) {
                    image = static_cast<std::uint32_t>(part.vertices.size());
                    part.vertices.push_back(source.vertices[original]);
                }
                remapped[corner] = image;
            }
            part.triangles.push_back(remapped);
        }
        parts.push_back(std::move(part));
    }
    return parts;
}

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

bool boundary_is_nearly_planar(const SurfaceMesh &mesh, const Halfedge border)
{
    std::vector<Kernel::Point_3> points;
    Halfedge current = border;
    do {
        points.push_back(mesh.point(mesh.target(current)));
        current = mesh.next(current);
    } while (current != border);
    if (points.size() < 4)
        return true;

    const Kernel::Point_3 &origin = points.front();
    std::size_t farthest = 1;
    double max_origin_distance = 0.0;
    for (std::size_t index = 1; index < points.size(); ++index) {
        const double distance = CGAL::to_double(CGAL::squared_distance(origin, points[index]));
        if (distance > max_origin_distance) {
            max_origin_distance = distance;
            farthest = index;
        }
    }
    if (max_origin_distance <= 1e-18)
        return true;

    const Kernel::Vector_3 baseline = points[farthest] - origin;
    std::size_t plane_point = 1;
    double max_cross_length = 0.0;
    for (std::size_t index = 1; index < points.size(); ++index) {
        const Kernel::Vector_3 cross = CGAL::cross_product(baseline, points[index] - origin);
        const double cross_length = CGAL::to_double(cross.squared_length());
        if (cross_length > max_cross_length) {
            max_cross_length = cross_length;
            plane_point = index;
        }
    }
    if (max_cross_length <= 1e-18)
        return true;

    const Kernel::Plane_3 plane(origin, points[farthest], points[plane_point]);
    const double scale = std::sqrt(max_origin_distance);
    const double tolerance = std::max(1e-8, scale * 1e-5);
    for (const Kernel::Point_3 &point : points) {
        const double distance = std::sqrt(
            CGAL::to_double(CGAL::squared_distance(point, plane)));
        if (distance > tolerance)
            return false;
    }
    return true;
}

bool has_border_halfedges(const SurfaceMesh &mesh)
{
    for (const Halfedge halfedge : mesh.halfedges()) {
        if (mesh.is_border(halfedge))
            return true;
    }
    return false;
}

struct CylinderPatch {
    Kernel::Point_3 center;
    Kernel::Vector_3 axis;
    Kernel::Vector_3 radial;
    Kernel::Vector_3 tangent;
    double radius;

    Kernel::Point_3 flatten(const Kernel::Point_3 &point) const
    {
        const auto offset = point - center;
        return {radius * std::atan2(CGAL::to_double(offset * tangent),
                                   CGAL::to_double(offset * radial)),
                CGAL::to_double(offset * axis), 0.0};
    }

    Kernel::Point_3 lift(const Kernel::Point_3 &point) const
    {
        const double angle = CGAL::to_double(point.x()) / radius;
        return center + axis * point.y() +
               radial * (radius * std::cos(angle)) +
               tangent * (radius * std::sin(angle));
    }
};

struct CircleFit {
    double x;
    double y;
    double radius;
};

std::optional<CircleFit> fit_circle(const std::vector<std::array<double, 2>> &points)
{
    if (points.size() < 3)
        return std::nullopt;
    double mx = 0.0, my = 0.0;
    for (const auto &p : points) { mx += p[0]; my += p[1]; }
    mx /= double(points.size()); my /= double(points.size());
    double xx = 0.0, xy = 0.0, yy = 0.0, xq = 0.0, yq = 0.0, qsum = 0.0;
    for (const auto &p : points) {
        const double x = p[0] - mx, y = p[1] - my;
        const double q = x * x + y * y;
        xx += x * x; xy += x * y; yy += y * y;
        xq += x * q; yq += y * q; qsum += q;
    }
    const double determinant = xx * yy - xy * xy;
    if (determinant <= (xx + yy) * (xx + yy) * 1e-10)
        return std::nullopt;
    const double x = (xq * yy - yq * xy) / (2.0 * determinant);
    const double y = (yq * xx - xq * xy) / (2.0 * determinant);
    const double radius = std::sqrt(qsum / double(points.size()) + x * x + y * y);
    if (!std::isfinite(radius))
        return std::nullopt;
    return CircleFit{x + mx, y + my, radius};
}

std::optional<CylinderPatch> fit_boundary_cylinder(
    const std::vector<Kernel::Point_3> &points)
{
    if (points.size() < 8)
        return std::nullopt;
    Kernel::Vector_3 mean(0.0, 0.0, 0.0);
    for (const auto &point : points)
        mean = mean + (point - CGAL::ORIGIN);
    const auto origin = CGAL::ORIGIN + mean / double(points.size());
    double scale = 0.0;
    std::vector<Kernel::Vector_3> edges;
    for (std::size_t i = 0; i < points.size(); ++i) {
        edges.push_back(points[(i + 1) % points.size()] - points[i]);
        scale = std::max(scale, std::sqrt(CGAL::to_double(
            (points[i] - origin).squared_length())) * 2.0);
    }
    if (scale <= 1e-9)
        return std::nullopt;
    std::vector<Kernel::Vector_3> axes;
    const auto add_axis = [&](const Kernel::Vector_3 &direction) {
        const double length = std::sqrt(CGAL::to_double(direction.squared_length()));
        if (length <= scale * 1e-10 || axes.size() >= 64)
            return;
        const auto axis = direction / length;
        if (std::none_of(axes.begin(), axes.end(), [&](const auto &other) {
                return std::abs(CGAL::to_double(axis * other)) > 1.0 - 1e-10;
            }))
            axes.push_back(axis);
    };
    const auto longest = *std::max_element(edges.begin(), edges.end(),
        [](const auto &a, const auto &b) { return a.squared_length() < b.squared_length(); });
    const double longest_length = std::sqrt(CGAL::to_double(longest.squared_length()));
    if (longest_length >= scale * 0.2) {
        const auto approximate_axis = longest / longest_length;
        const std::size_t samples = std::min<std::size_t>(64, points.size());
        for (std::size_t i = 0; i < samples; ++i) {
            const auto &a = points[i * (points.size() - 1) / (samples - 1)];
            for (std::size_t j = i + 1; j < samples; ++j) {
                const auto ab = points[j * (points.size() - 1) / (samples - 1)] - a;
                if (CGAL::to_double(ab.squared_length()) > scale * scale * 0.0625)
                    continue;
                for (std::size_t k = j + 1; k < samples; ++k) {
                    const auto ac = points[k * (points.size() - 1) / (samples - 1)] - a;
                    if (CGAL::to_double(ac.squared_length()) > scale * scale * 0.0625)
                        continue;
                    auto normal = CGAL::cross_product(ab, ac);
                    const double length = std::sqrt(CGAL::to_double(normal.squared_length()));
                    if (length <= scale * scale * 1e-10)
                        continue;
                    normal = normal / length;
                    if (std::abs(CGAL::to_double(normal * approximate_axis)) < 0.98)
                        continue;
                    std::size_t support = 0;
                    for (const auto &p : points)
                        if (std::abs(CGAL::to_double((p - a) * normal)) <= scale * 1e-6)
                            ++support;
                    if (support >= 4)
                        add_axis(normal);
                }
            }
        }
    }
    // A curved cross-section's plane normal gives an extrusion axis even
    // when the long sides of a damaged hole are diagonal rather than axial.
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const auto &a = edges[i];
        const auto &b = edges[(i + 1) % edges.size()];
        const double aa = CGAL::to_double(a.squared_length());
        const double bb = CGAL::to_double(b.squared_length());
        const auto normal = CGAL::cross_product(a, b);
        if (aa < scale * scale * 0.04 && bb < scale * scale * 0.04 &&
            CGAL::to_double(normal.squared_length()) > aa * bb * 1e-6)
            add_axis(normal);
    }
    std::sort(edges.begin(), edges.end(), [](const auto &a, const auto &b) {
        return a.squared_length() > b.squared_length();
    });
    for (std::size_t i = 0; i < std::min<std::size_t>(8, edges.size()); ++i)
        if (CGAL::to_double(edges[i].squared_length()) >= scale * scale * 0.04)
            add_axis(edges[i]);

    std::optional<CylinderPatch> best;
    std::size_t best_count = 0;
    double best_error = std::numeric_limits<double>::max();
    const double tolerance = scale * 2e-5;
    for (const auto &axis : axes) {
        const Kernel::Vector_3 seed = std::abs(CGAL::to_double(axis.x())) < 0.9
            ? Kernel::Vector_3(1.0, 0.0, 0.0) : Kernel::Vector_3(0.0, 1.0, 0.0);
        auto u = CGAL::cross_product(axis, seed);
        u = u / std::sqrt(CGAL::to_double(u.squared_length()));
        const auto v = CGAL::cross_product(axis, u);
        std::vector<std::array<double, 2>> projected;
        for (const auto &point : points) {
            const auto offset = point - origin;
            projected.push_back({CGAL::to_double(offset * u), CGAL::to_double(offset * v)});
        }
        // Small cross-surface spurs must not pull the fitted wall inward.
        // Seed from consecutive triples, then refit the supported majority.
        for (std::size_t seed_index = 0; seed_index <= projected.size(); ++seed_index) {
            auto circle = seed_index == projected.size() ? fit_circle(projected) :
                fit_circle({projected[seed_index], projected[(seed_index + 1) % projected.size()],
                            projected[(seed_index + 2) % projected.size()]});
            if (!circle)
                continue;
            std::vector<std::array<double, 2>> inliers;
            for (std::size_t iteration = 0; iteration < 3; ++iteration) {
                inliers.clear();
                for (const auto &p : projected)
                    if (std::abs(std::hypot(p[0] - circle->x, p[1] - circle->y) -
                                 circle->radius) <= tolerance)
                        inliers.push_back(p);
                if (inliers.size() * 5 < points.size() * 4)
                    break;
                const auto refined = fit_circle(inliers);
                if (!refined)
                    break;
                circle = refined;
            }
            if (inliers.size() < 8 || inliers.size() * 5 < points.size() * 4)
                continue;
            auto radial = u * -circle->x + v * -circle->y;
            const double radial_length = std::sqrt(CGAL::to_double(radial.squared_length()));
            if (radial_length < scale * 0.01)
                continue;
            radial = radial / radial_length;
            const CylinderPatch patch{origin + u * circle->x + v * circle->y, axis, radial,
                                      CGAL::cross_product(axis, radial), circle->radius};
            double min_angle = std::numeric_limits<double>::max();
            double max_angle = std::numeric_limits<double>::lowest();
            double error = 0.0;
            std::size_t count = 0;
            for (const auto &point : points) {
                const auto offset = point - patch.center;
                const double x = CGAL::to_double(offset * patch.radial);
                const double y = CGAL::to_double(offset * patch.tangent);
                const double distance = std::abs(std::hypot(x, y) - patch.radius);
                if (distance > tolerance || x <= 0.0)
                    continue;
                ++count;
                error += distance * distance;
                const double angle = std::atan2(y, x);
                min_angle = std::min(min_angle, angle);
                max_angle = std::max(max_angle, angle);
            }
            if (count * 5 < points.size() * 4 || max_angle - min_angle < 0.02 ||
                max_angle - min_angle >= 1.5)
                continue;
            if (count > best_count || (count == best_count && error < best_error)) {
                best = patch;
                best_count = count;
                best_error = error;
            }
        }
    }
    return best;
}

bool fill_cylindrical_hole(SurfaceMesh &mesh, const Halfedge border)
{
    std::vector<Kernel::Point_3> points;
    std::vector<Halfedge> halfedges;
    Halfedge current = border;
    do {
        halfedges.push_back(current);
        points.push_back(mesh.point(mesh.target(current)));
        current = mesh.next(current);
    } while (current != border);
    const auto cylinder = fit_boundary_cylinder(points);
    if (!cylinder)
        return false;

    double scale = 0.0;
    for (const auto &point : points)
        scale = std::max(scale, std::sqrt(CGAL::to_double(
            CGAL::squared_distance(point, points.front()))));
    const double tolerance = scale * 4e-5;
    const auto radial_error = [&](const Kernel::Point_3 &point) {
        const auto offset = point - cylinder->center;
        return std::hypot(CGAL::to_double(offset * cylinder->radial),
                          CGAL::to_double(offset * cylinder->tangent)) - cylinder->radius;
    };
    double min_angle = std::numeric_limits<double>::max();
    double max_angle = std::numeric_limits<double>::lowest();
    for (const auto &point : points) {
        if (std::abs(radial_error(point)) > tolerance)
            continue;
        const double angle = CGAL::to_double(cylinder->flatten(point).x()) / cylinder->radius;
        min_angle = std::min(min_angle, angle);
        max_angle = std::max(max_angle, angle);
    }
    const double max_chord_depth = cylinder->radius *
        (1.0 - std::cos((max_angle - min_angle) * 0.5)) + tolerance;

    // Keep the general triangulator's handling of weakly simple boundaries,
    // but constrain its new wall vertices and triangle spans to the surface.
    // The copy makes a declined patch leave the source topology untouched.
    SurfaceMesh candidate = mesh;
    auto on_surface = candidate.add_property_map<Vertex, bool>("v:cylinder-surface", false).first;
    auto chart = candidate.add_property_map<Vertex, Kernel::Point_3>("v:cylinder-chart").first;
    for (const auto vertex : candidate.vertices()) {
        chart[vertex] = cylinder->flatten(candidate.point(vertex));
        on_surface[vertex] = std::abs(radial_error(candidate.point(vertex))) <= tolerance;
    }
    // Keep short cross-surface spurs separate from the wall patch. Otherwise
    // a triangulator may connect their tips to the bottom of a tall opening.
    std::size_t first_on_surface = 0;
    while (first_on_surface < halfedges.size() &&
           !on_surface[candidate.target(halfedges[first_on_surface])])
        ++first_on_surface;
    if (first_on_surface == halfedges.size())
        return false;
    std::size_t index = (first_on_surface + 1) % halfedges.size();
    while (index != first_on_surface) {
        if (on_surface[candidate.target(halfedges[index])]) {
            index = (index + 1) % halfedges.size();
            continue;
        }
        const auto before = halfedges[(index + halfedges.size() - 1) % halfedges.size()];
        do {
            index = (index + 1) % halfedges.size();
        } while (index != first_on_surface &&
                 !on_surface[candidate.target(halfedges[index])]);
        const auto after = halfedges[index];
        if (!candidate.is_border(before) || !candidate.is_border(after) ||
            candidate.point(candidate.target(before)) == candidate.point(candidate.target(after)))
            return false;
        const auto side = CGAL::Euler::add_face_to_border(before, after, candidate);
        if (!PMP::triangulate_face(candidate.face(side), candidate))
            return false;
    }
    const auto fill_border = halfedges[first_on_surface];
    std::vector<Face> faces;
    std::vector<Vertex> vertices;
    PMP::triangulate_and_refine_hole(candidate, fill_border,
        CGAL::parameters::face_output_iterator(std::back_inserter(faces))
            .vertex_output_iterator(std::back_inserter(vertices)));
    if (faces.empty() || candidate.is_border(fill_border)) {
        return false;
    }
    for (const auto vertex : vertices) {
        const auto point = candidate.point(vertex);
        chart[vertex] = cylinder->flatten(point);
        if (std::abs(radial_error(point)) <= max_chord_depth) {
            candidate.point(vertex) = cylinder->lift(chart[vertex]);
            on_surface[vertex] = true;
        }
    }
    auto patch = candidate.add_property_map<Face, bool>("f:cylinder-patch", false).first;
    std::vector<Halfedge> pending;
    const auto enqueue = [&](const Face face) {
        patch[face] = true;
        for (const auto halfedge : CGAL::halfedges_around_face(candidate.halfedge(face), candidate))
            pending.push_back(halfedge);
    };
    for (const auto face : faces)
        enqueue(face);
    const auto face_on_surface = [&](const Face face) {
        for (const auto vertex : CGAL::vertices_around_face(candidate.halfedge(face), candidate))
            if (!on_surface[vertex])
                return false;
        return true;
    };
    std::size_t inserted = 0;
    while (!pending.empty()) {
        const Halfedge h = pending.back();
        pending.pop_back();
        const Halfedge opposite = candidate.opposite(h);
        if (candidate.is_border(h) || candidate.is_border(opposite) ||
            (!patch[candidate.face(h)] && !patch[candidate.face(opposite)]) ||
            !on_surface[candidate.source(h)] || !on_surface[candidate.target(h)])
            continue;
        // A pre-existing long chord on the hole rim also needs subdivision;
        // otherwise interior refinement converges toward that locked chord
        // indefinitely. Extend only into adjoining faces on the same wall.
        if ((!patch[candidate.face(h)] && !face_on_surface(candidate.face(h))) ||
            (!patch[candidate.face(opposite)] && !face_on_surface(candidate.face(opposite))))
            continue;
        const auto a = chart[candidate.source(h)];
        const auto b = chart[candidate.target(h)];
        // Bound angular span, including triangle interiors. Projecting only
        // newly refined vertices still leaves wide inward-cutting chords.
        if (std::abs(CGAL::to_double(a.x() - b.x())) <= cylinder->radius * 0.05)
            continue;
        if (++inserted > points.size() * 64) {
            return false;
        }
        const auto midpoint = CGAL::midpoint(a, b);
        const Halfedge added = CGAL::Euler::split_edge(h, candidate);
        const Vertex vertex = candidate.target(added);
        chart[vertex] = midpoint;
        candidate.point(vertex) = cylinder->lift(midpoint);
        on_surface[vertex] = true;
        for (const Halfedge incoming : {added, opposite}) {
            const Halfedge diagonal = CGAL::Euler::split_face(
                incoming, candidate.next(candidate.next(incoming)), candidate);
            enqueue(candidate.face(diagonal));
            enqueue(candidate.face(candidate.opposite(diagonal)));
        }
    }
    candidate.remove_property_map(chart);
    candidate.remove_property_map(patch);
    candidate.remove_property_map(on_surface);
    mesh = std::move(candidate);
    return true;
}

std::optional<Halfedge> split_hole_at_dominant_plane(SurfaceMesh &mesh,
                                                       const Halfedge border)
{
    std::vector<Halfedge> halfedges;
    std::vector<Kernel::Point_3> points;
    Halfedge current = border;
    do {
        halfedges.push_back(current);
        points.push_back(mesh.point(mesh.target(current)));
        current = mesh.next(current);
    } while (current != border);
    if (points.size() < 6 || boundary_is_nearly_planar(mesh, border))
        return std::nullopt;

    double min_x = std::numeric_limits<double>::max();
    double min_y = min_x;
    double min_z = min_x;
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = max_x;
    double max_z = max_x;
    for (const Kernel::Point_3 &point : points) {
        const double x = CGAL::to_double(point.x());
        const double y = CGAL::to_double(point.y());
        const double z = CGAL::to_double(point.z());
        min_x = std::min(min_x, x); max_x = std::max(max_x, x);
        min_y = std::min(min_y, y); max_y = std::max(max_y, y);
        min_z = std::min(min_z, z); max_z = std::max(max_z, z);
    }
    const double diagonal = std::sqrt((max_x - min_x) * (max_x - min_x) +
                                      (max_y - min_y) * (max_y - min_y) +
                                      (max_z - min_z) * (max_z - min_z));
    const double tolerance = std::max(1e-8, diagonal * 1e-5);
    const double squared_tolerance = tolerance * tolerance;

    std::optional<Kernel::Plane_3> dominant_plane;
    std::size_t dominant_count = 0;
    const std::size_t seed_count = std::min<std::size_t>(12, points.size());
    // Bound the number of plane hypotheses independently of rim length.
    // Scoring every pair of boundary points against the entire rim was cubic
    // and stalled on thousand-edge holes. Spread candidates around the rim,
    // but still validate their support against every original boundary point.
    const std::size_t candidate_count = std::min<std::size_t>(64, points.size());
    for (std::size_t sample = 0; sample < seed_count; ++sample) {
        const std::size_t first = sample * points.size() / seed_count;
        for (std::size_t second_sample = 0; second_sample < candidate_count; ++second_sample) {
            const std::size_t second = second_sample * points.size() / candidate_count;
            if (second == first)
                continue;
            for (std::size_t third_sample = second_sample + 1;
                 third_sample < candidate_count; ++third_sample) {
                const std::size_t third = third_sample * points.size() / candidate_count;
                if (third == first)
                    continue;
                const Kernel::Vector_3 normal = CGAL::cross_product(
                    points[second] - points[first], points[third] - points[first]);
                if (CGAL::to_double(normal.squared_length()) <= 1e-18)
                    continue;
                const Kernel::Plane_3 candidate(
                    points[first], points[second], points[third]);
                std::size_t inliers = 0;
                for (const Kernel::Point_3 &point : points) {
                    if (CGAL::to_double(CGAL::squared_distance(point, candidate)) <=
                        squared_tolerance)
                        ++inliers;
                }
                if (inliers > dominant_count) {
                    dominant_count = inliers;
                    dominant_plane = candidate;
                }
            }
        }
    }

    if (!dominant_plane || dominant_count * 5 < points.size() * 3)
        return std::nullopt;

    std::vector<bool> on_plane(points.size(), false);
    std::size_t first_on_plane = points.size();
    for (std::size_t index = 0; index < points.size(); ++index) {
        on_plane[index] = CGAL::to_double(
                              CGAL::squared_distance(points[index], *dominant_plane)) <=
                          squared_tolerance;
        if (on_plane[index] && first_on_plane == points.size())
            first_on_plane = index;
    }
    if (first_on_plane == points.size())
        return std::nullopt;

    struct Run {
        std::size_t before;
        std::size_t after;
    };
    std::vector<Run> runs;
    std::size_t index = (first_on_plane + 1) % points.size();
    while (index != first_on_plane) {
        if (on_plane[index]) {
            index = (index + 1) % points.size();
            continue;
        }
        const std::size_t before = (index + points.size() - 1) % points.size();
        do {
            index = (index + 1) % points.size();
        } while (index != first_on_plane && !on_plane[index]);
        if (index == first_on_plane && !on_plane[index])
            return std::nullopt;
        runs.push_back({before, index});
    }
    if (runs.empty())
        return std::nullopt;

    SurfaceMesh candidate = mesh;
    for (const Run &run : runs) {
        if (!candidate.is_border(halfedges[run.before]) ||
            !candidate.is_border(halfedges[run.after]))
            return std::nullopt;
        const Halfedge patch_halfedge = CGAL::Euler::add_face_to_border(
            halfedges[run.before], halfedges[run.after], candidate);
        const Face patch_face = candidate.face(patch_halfedge);
        if (!PMP::triangulate_face(patch_face, candidate))
            return std::nullopt;
    }
    mesh = std::move(candidate);
    return halfedges[first_on_plane];
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
    // A boundary may revisit one geometric point through distinct vertices.
    // Merge those copies before extracting holes, so separate small cracks
    // are not triangulated as one large patch cutting across a curved wall.
    PMP::merge_duplicated_vertices_in_boundary_cycles(mesh);
    constexpr std::size_t max_hole_fill_passes = 64;
    for (std::size_t pass = 0;
         pass < max_hole_fill_passes && has_border_halfedges(mesh); ++pass) {
        std::vector<Halfedge> borders;
        PMP::extract_boundary_cycles(mesh, std::back_inserter(borders));
        diagnostics.holes_found += borders.size();
        const std::size_t faces_before_pass = mesh.number_of_faces();
        bool topology_replaced = false;
        for (const Halfedge border : borders) {
            if (!mesh.is_border(border))
                continue;
            if (!boundary_is_nearly_planar(mesh, border) && fill_cylindrical_hole(mesh, border)) {
                ++diagnostics.holes_filled;
                topology_replaced = true;
                break;
            }
            const std::size_t before = mesh.number_of_faces();
            // A boundary can contain a dominant planar rim plus one or more
            // raised side openings. Filling that entire non-planar cycle in a
            // single pass lets CGAL bridge across the empty interior and
            // creates needle-like sheets. Carve those side openings into
            // separate faces first, then fill the remaining planar rim.
            // Re-extract boundaries after a successful transactional split:
            // assigning the candidate mesh invalidates every descriptor in
            // the current boundary snapshot except the returned one.
            const std::optional<Halfedge> split_border =
                split_hole_at_dominant_plane(mesh, border);
            const Halfedge fill_border = split_border.value_or(border);
            topology_replaced = split_border.has_value();
            if (boundary_is_nearly_planar(mesh, fill_border))
                PMP::triangulate_hole(mesh, fill_border);
            else
                PMP::triangulate_and_refine_hole(mesh, fill_border);
            if (mesh.number_of_faces() == before && mesh.is_border(fill_border)) {
                std::size_t edge_count = 0;
                Halfedge current = fill_border;
                do {
                    ++edge_count;
                    current = mesh.next(current);
                } while (current != fill_border);
                // CGAL's triangulators may decline an already minimal
                // triangular boundary. Closing it directly preserves every
                // source vertex and edge.
                if (edge_count == 3)
                    CGAL::Euler::fill_hole(fill_border, mesh);
            }
            if (mesh.number_of_faces() > before)
                ++diagnostics.holes_filled;
            if (topology_replaced)
                break;
        }
        if (mesh.number_of_faces() == faces_before_pass)
            break;
    }

    if (has_border_halfedges(mesh)) {
        std::size_t remaining_border_edges = 0;
        for (const Halfedge halfedge : mesh.halfedges()) {
            if (mesh.is_border(halfedge))
                ++remaining_border_edges;
        }
        error = "repair failed: mesh is still open after CGAL hole filling"
                " (boundary cycles found=" + std::to_string(diagnostics.holes_found) +
                ", filled=" + std::to_string(diagnostics.holes_filled) +
                ", remaining directed border edges=" +
                std::to_string(remaining_border_edges) + ")";
        return false;
    }
    // Each part is one edge-connected closed shell. Signed volume is enough
    // to orient it and avoids CGAL's global volume predicate, which can access
    // invalid memory for closed meshes that still self-intersect.
    if (signed_volume(mesh) < 0.0)
        PMP::reverse_face_orientations(mesh);
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
        const std::vector<Mesh> parts = split_into_edge_connected_parts(mesh);
        diagnostics.parts_found += parts.size();

        SurfaceMesh repaired;
        for (std::size_t part_index = 0; part_index < parts.size(); ++part_index) {
            const Mesh &source_part = parts[part_index];
            SurfaceMesh part = soup_to_surface(source_part);
            if (part.is_empty()) {
                ++diagnostics.parts_removed;
                continue;
            }
            if (is_not_three_dimensional(part)) {
                ++diagnostics.parts_removed;
                continue;
            }
            if (!repair_part(part, diagnostics, error)) {
                error = "part " + std::to_string(part_index + 1) + "/" +
                        std::to_string(parts.size()) + ": " + error;
                return false;
            }
            if (repaired.is_empty())
                repaired = std::move(part);
            else
                repaired.join(part);
        }

        if (repaired.is_empty()) {
            error = "every connected part was empty, planar, too thin, or negligible";
            return false;
        }
        if (has_border_halfedges(repaired))
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
