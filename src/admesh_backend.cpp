// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "mesh.hpp"

extern "C" {
#include <stl.h>
}

#include <algorithm>
#include <array>
#include <limits>
#include <map>

namespace gmesh::detail {
namespace {

class StlFile {
public:
    StlFile() { stl_initialize(&value); }
    ~StlFile() { stl_close(&value); }
    StlFile(const StlFile &) = delete;
    StlFile &operator=(const StlFile &) = delete;

    stl_file value{};
};

bool open_stl(const std::string &path, StlFile &stl, std::string &error)
{
    std::vector<char> mutable_path(path.begin(), path.end());
    mutable_path.push_back('\0');
    stl_open(&stl.value, mutable_path.data());
    if (stl_get_error(&stl.value) || stl.value.stats.number_of_facets <= 0) {
        error = "ADMesh could not read a non-empty STL file";
        return false;
    }
    return true;
}

std::size_t manifold_winding_conflicts(const stl_file &stl)
{
    using Point = std::array<float, 3>;
    using Edge = std::pair<Point, Point>;
    std::map<Edge, std::pair<int, int>> edges;
    for (int face = 0; face < stl.stats.number_of_facets; ++face) {
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const auto &from = stl.facet_start[face].vertex[corner];
            const auto &to = stl.facet_start[face].vertex[(corner + 1) % 3];
            Point a{from.x, from.y, from.z}, b{to.x, to.y, to.z};
            if (a == b) continue;
            const int direction = a < b ? 1 : -1;
            if (b < a) std::swap(a, b);
            auto &use = edges[{a, b}];
            ++use.first;
            use.second += direction;
        }
    }
    return std::count_if(edges.begin(), edges.end(), [](const auto &entry) {
        return entry.second.first == 2 && entry.second.second != 0;
    });
}

void fix_normal_directions_without_spreading_conflicts(stl_file &stl)
{
    // ADMesh's orientation traversal can revisit and flip an already oriented
    // face across a non-manifold connection. On re-import this can spread a
    // local conflict across thousands of otherwise valid edges, which become
    // large artificial holes in the subsequent component split.
    const auto conflicts_before = manifold_winding_conflicts(stl);
    const auto stats_before = stl.stats;
    const std::vector<stl_facet> facets(
        stl.facet_start, stl.facet_start + stl.stats.number_of_facets);
    const std::vector<stl_neighbors> neighbors(
        stl.neighbors_start, stl.neighbors_start + stl.stats.number_of_facets);
    stl_fix_normal_directions(&stl);
    if (manifold_winding_conflicts(stl) > conflicts_before) {
        std::copy(facets.begin(), facets.end(), stl.facet_start);
        std::copy(neighbors.begin(), neighbors.end(), stl.neighbors_start);
        stl.stats = stats_before;
    }
}

void run_orca_import_sequence(stl_file &stl, RepairDiagnostics &diagnostics)
{
    stl_check_facets_exact(&stl);
    stl.stats.facets_w_1_bad_edge =
        stl.stats.connected_facets_2_edge - stl.stats.connected_facets_3_edge;
    stl.stats.facets_w_2_bad_edge =
        stl.stats.connected_facets_1_edge - stl.stats.connected_facets_2_edge;
    stl.stats.facets_w_3_bad_edge =
        stl.stats.number_of_facets - stl.stats.connected_facets_1_edge;

    float tolerance = stl.stats.shortest_edge;
    const float increment = stl.stats.bounding_diameter / 10000.0F;
    for (int iteration = 0;
         iteration < 2 && stl.stats.connected_facets_3_edge < stl.stats.number_of_facets;
         ++iteration) {
        stl_check_facets_nearby(&stl, tolerance);
        tolerance += increment;
    }

    if (stl.stats.connected_facets_3_edge < stl.stats.number_of_facets)
        stl_remove_unconnected_facets(&stl);

    // Orca deliberately does not call ADMesh hole filling here. Complex holes
    // are handled by the explicit CGAL repair stage instead.
    fix_normal_directions_without_spreading_conflicts(stl);
    stl_fix_normal_values(&stl);
    stl_calculate_volume(&stl);
    stl_verify_neighbors(&stl);
    if (stl.stats.number_of_facets > 0 && stl.stats.degenerate_facets > 0)
        stl_check_facets_exact(&stl);

    diagnostics.repaired.edges_fixed +=
        static_cast<std::uint64_t>(std::max(0, stl.stats.edges_fixed));
    diagnostics.repaired.degenerate_facets +=
        static_cast<std::uint64_t>(std::max(0, stl.stats.degenerate_facets));
    diagnostics.repaired.facets_removed +=
        static_cast<std::uint64_t>(std::max(0, stl.stats.facets_removed));
    diagnostics.repaired.facets_reversed +=
        static_cast<std::uint64_t>(std::max(0, stl.stats.facets_reversed));
    diagnostics.repaired.backwards_edges +=
        static_cast<std::uint64_t>(std::max(0, stl.stats.backwards_edges));
}

bool convert_to_mesh(stl_file &stl, Mesh &mesh, std::string &error)
{
    stl_generate_shared_vertices(&stl);
    if (stl_get_error(&stl) || stl.stats.shared_vertices <= 0 || stl.v_indices == nullptr) {
        error = "ADMesh failed to generate shared vertices";
        return false;
    }

    mesh.vertices.clear();
    mesh.triangles.clear();
    mesh.vertices.reserve(static_cast<std::size_t>(stl.stats.shared_vertices));
    mesh.triangles.reserve(static_cast<std::size_t>(stl.stats.number_of_facets));

    for (int index = 0; index < stl.stats.shared_vertices; ++index) {
        const stl_vertex &point = stl.v_shared[index];
        mesh.vertices.push_back({point.x, point.y, point.z});
    }
    for (int index = 0; index < stl.stats.number_of_facets; ++index) {
        const v_indices_struct &face = stl.v_indices[index];
        if (face.vertex[0] < 0 || face.vertex[1] < 0 || face.vertex[2] < 0) {
            error = "ADMesh produced an invalid triangle index";
            return false;
        }
        mesh.triangles.push_back({
            static_cast<std::uint32_t>(face.vertex[0]),
            static_cast<std::uint32_t>(face.vertex[1]),
            static_cast<std::uint32_t>(face.vertex[2]),
        });
    }
    return !mesh.vertices.empty() && !mesh.triangles.empty();
}

bool load_impl(const std::string &path, const bool repair, Mesh &mesh,
               RepairDiagnostics &diagnostics, std::string &error)
{
    StlFile stl;
    if (!open_stl(path, stl, error))
        return false;
    if (repair)
        run_orca_import_sequence(stl.value, diagnostics);
    return convert_to_mesh(stl.value, mesh, error);
}

} // namespace

bool load_and_import_repair(const std::string &path, Mesh &mesh,
                            RepairDiagnostics &diagnostics, std::string &error)
{
    return load_impl(path, true, mesh, diagnostics, error);
}

bool load_without_repair(const std::string &path, Mesh &mesh, std::string &error)
{
    RepairDiagnostics unused;
    return load_impl(path, false, mesh, unused, error);
}

} // namespace gmesh::detail
