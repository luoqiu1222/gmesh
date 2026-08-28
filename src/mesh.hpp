// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gmesh::detail {

struct Point {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

using Triangle = std::array<std::uint32_t, 3>;

struct Mesh {
    std::vector<Point>    vertices;
    std::vector<Triangle> triangles;
};

struct RepairDiagnostics {
    std::uint64_t edges_fixed       = 0;
    std::uint64_t facets_removed    = 0;
    std::uint64_t facets_reversed   = 0;
    std::uint64_t parts_found       = 0;
    std::uint64_t parts_removed     = 0;
    std::uint64_t holes_found       = 0;
    std::uint64_t holes_filled      = 0;
    bool          union_succeeded   = false;
};

bool load_and_import_repair(const std::string &path, Mesh &mesh,
                            RepairDiagnostics &diagnostics, std::string &error);
bool load_without_repair(const std::string &path, Mesh &mesh, std::string &error);
bool deep_repair(Mesh &mesh, RepairDiagnostics &diagnostics, std::string &error);
bool write_binary_stl(const std::string &path, const Mesh &mesh, std::string &error);

} // namespace gmesh::detail
