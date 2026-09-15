// SPDX-License-Identifier: AGPL-3.0-only
#include "core/boundary_triangulation.hpp"
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <CGAL/mark_domain_in_triangulation.h>
#include <boost/property_map/property_map.hpp>
#include <stdexcept>
#include <unordered_map>
namespace mrp::model_import {
BoundaryMesh triangulateBoundary(
    const std::vector<std::vector<std::array<double, 2>>> &rings,
    const std::function<bool(const std::array<std::array<double, 2>, 3> &)> &refine) {
    using Kernel = CGAL::Exact_predicates_inexact_constructions_kernel;
    using Vertex = CGAL::Triangulation_vertex_base_with_info_2<std::uint32_t, Kernel>;
    using Face = CGAL::Constrained_triangulation_face_base_2<Kernel>;
    using Data = CGAL::Triangulation_data_structure_2<Vertex, Face>;
    using Mesh =
        CGAL::Constrained_Delaunay_triangulation_2<Kernel, Data, CGAL::Exact_predicates_tag>;
    Mesh mesh;
    for (const auto &ring : rings) {
        if (ring.size() < 3)
            continue;
        std::vector<Mesh::Vertex_handle> vertices;
        for (const auto &p : ring) {
            auto vertex = mesh.insert(Kernel::Point_2(p[0], p[1]));
            if (vertices.empty() || vertices.back() != vertex)
                vertices.push_back(vertex);
        }
        if (vertices.size() > 1 && vertices.front() == vertices.back())
            vertices.pop_back();
        if (vertices.size() < 3)
            continue;
        for (std::size_t i = 0; i < vertices.size(); ++i)
            mesh.insert_constraint(vertices[i], vertices[(i + 1) % vertices.size()]);
    }
    BoundaryMesh result;
    if (mesh.dimension() != 2)
        return result;
    std::unordered_map<Mesh::Face_handle, bool> inside;
    boost::associative_property_map<decltype(inside)> domains(inside);
    for (int attempt = 0;; ++attempt) {
        inside.clear();
        CGAL::mark_domain_in_triangulation(mesh, domains);
        if (!refine)
            break;
        std::vector<Kernel::Point_2> additions;
        for (auto face = mesh.finite_faces_begin(); face != mesh.finite_faces_end(); ++face) {
            if (!get(domains, face))
                continue;
            std::array<std::array<double, 2>, 3> points;
            for (int i = 0; i < 3; ++i)
                points[i] = {CGAL::to_double(face->vertex(i)->point().x()),
                             CGAL::to_double(face->vertex(i)->point().y())};
            if (refine(points))
                additions.emplace_back((points[0][0] + points[1][0] + points[2][0]) / 3,
                                       (points[0][1] + points[1][1] + points[2][1]) / 3);
        }
        if (additions.empty())
            break;
        if (attempt >= 16 || mesh.number_of_vertices() + additions.size() > 100000)
            throw std::runtime_error("STEP shared surface refinement exceeded limits");
        for (const auto &point : additions)
            mesh.insert(point);
    }
    for (auto vertex = mesh.finite_vertices_begin(); vertex != mesh.finite_vertices_end();
         ++vertex) {
        vertex->info() = static_cast<std::uint32_t>(result.points.size());
        result.points.push_back(
            {CGAL::to_double(vertex->point().x()), CGAL::to_double(vertex->point().y())});
    }
    for (auto face = mesh.finite_faces_begin(); face != mesh.finite_faces_end(); ++face) {
        if (!get(domains, face))
            continue;
        for (int i = 0; i < 3; ++i)
            result.indices.push_back(face->vertex(i)->info());
    }
    return result;
}
} // namespace mrp::model_import
