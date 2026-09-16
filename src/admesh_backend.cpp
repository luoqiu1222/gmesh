// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "mesh.hpp"

extern "C" {
#include <stl.h>
}

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <unordered_map>

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

using Point = std::array<float, 3>;

struct Edge {
    Point a;
    Point b;

    bool operator==(const Edge &other) const noexcept
    {
        return a == other.a && b == other.b;
    }
};

struct EdgeHash {
    std::size_t operator()(const Edge &edge) const noexcept
    {
        std::size_t seed = 0;
        const auto combine = [&seed](const float value) {
            const auto hash = std::hash<float>{}(value);
            seed ^= hash + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        };
        for (const float value : edge.a)
            combine(value);
        for (const float value : edge.b)
            combine(value);
        return seed;
    }
};

struct ConflictUses {
    int count_before = 0;
    int direction_before = 0;
    int count_after = 0;
    int direction_after = 0;
};

Edge canonical_edge(const stl_facet &facet, const std::size_t corner, int &direction)
{
    const auto &from = facet.vertex[corner];
    const auto &to = facet.vertex[(corner + 1U) % 3U];
    Point a{from.x, from.y, from.z};
    Point b{to.x, to.y, to.z};
    if (a == b) {
        direction = 0;
        return {a, b};
    }
    direction = a < b ? 1 : -1;
    if (b < a)
        std::swap(a, b);
    return {a, b};
}

bool winding_changed(const stl_facet &before, const stl_facet &after)
{
    for (std::size_t corner = 0; corner < 3; ++corner) {
        const auto &a = before.vertex[corner];
        const auto &b = after.vertex[corner];
        if (a.x != b.x || a.y != b.y || a.z != b.z)
            return true;
    }
    return false;
}

std::pair<std::size_t, std::size_t> changed_edge_conflicts(
    const stl_file &stl, const std::vector<stl_facet> &facets_before)
{
    // Reversing a facet can only change winding counts on its three edges.
    // Index that small affected set, then scan the mesh once to preserve the
    // exact geometric-edge guard without building a map for every mesh edge.
    std::unordered_map<Edge, ConflictUses, EdgeHash> affected;
    for (int face = 0; face < stl.stats.number_of_facets; ++face) {
        if (!winding_changed(facets_before[static_cast<std::size_t>(face)],
                             stl.facet_start[face]))
            continue;
        for (std::size_t corner = 0; corner < 3; ++corner) {
            int direction = 0;
            const auto edge = canonical_edge(
                facets_before[static_cast<std::size_t>(face)], corner, direction);
            if (direction != 0)
                affected.try_emplace(edge);
        }
    }
    if (affected.empty())
        return {0, 0};

    for (int face = 0; face < stl.stats.number_of_facets; ++face) {
        for (std::size_t corner = 0; corner < 3; ++corner) {
            int direction = 0;
            const auto before_edge = canonical_edge(
                facets_before[static_cast<std::size_t>(face)], corner, direction);
            if (direction != 0) {
                if (auto found = affected.find(before_edge); found != affected.end()) {
                    ++found->second.count_before;
                    found->second.direction_before += direction;
                }
            }

            const auto after_edge = canonical_edge(stl.facet_start[face], corner, direction);
            if (direction != 0) {
                if (auto found = affected.find(after_edge); found != affected.end()) {
                    ++found->second.count_after;
                    found->second.direction_after += direction;
                }
            }
        }
    }

    std::size_t conflicts_before = 0;
    std::size_t conflicts_after = 0;
    for (const auto &[edge, uses] : affected) {
        static_cast<void>(edge);
        conflicts_before += uses.count_before == 2 && uses.direction_before != 0 ? 1U : 0U;
        conflicts_after += uses.count_after == 2 && uses.direction_after != 0 ? 1U : 0U;
    }
    return {conflicts_before, conflicts_after};
}

void fix_normal_directions_without_spreading_conflicts(stl_file &stl)
{
    // ADMesh's orientation traversal can revisit and flip an already oriented
    // face across a non-manifold connection. On re-import this can spread a
    // local conflict across thousands of otherwise valid edges, which become
    // large artificial holes in the subsequent component split.
    const auto stats_before = stl.stats;
    const std::vector<stl_facet> facets(
        stl.facet_start, stl.facet_start + stl.stats.number_of_facets);
    const std::vector<stl_neighbors> neighbors(
        stl.neighbors_start, stl.neighbors_start + stl.stats.number_of_facets);
    stl_fix_normal_directions(&stl);
    const auto [conflicts_before, conflicts_after] = changed_edge_conflicts(stl, facets);
    if (conflicts_after > conflicts_before) {
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
