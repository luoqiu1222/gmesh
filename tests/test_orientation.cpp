// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "gmesh/repair.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

std::size_t winding_conflicts(const std::filesystem::path &path)
{
    using Point = std::array<float, 3>;
    using Edge = std::pair<Point, Point>;
    std::map<Edge, std::pair<int, int>> edges;
    std::ifstream input(path, std::ios::binary);
    input.seekg(80);
    std::uint32_t count = 0;
    input.read(reinterpret_cast<char *>(&count), sizeof(count));
    for (std::uint32_t i = 0; i < count; ++i) {
        std::array<Point, 3> points{};
        input.seekg(12, std::ios::cur);
        input.read(reinterpret_cast<char *>(points.data()), sizeof(points));
        input.seekg(2, std::ios::cur);
        for (std::size_t j = 0; j < 3; ++j) {
            Point a = points[j], b = points[(j + 1) % 3];
            const int direction = a < b ? 1 : -1;
            if (b < a) std::swap(a, b);
            auto &use = edges[{a, b}];
            ++use.first;
            use.second += direction;
        }
    }
    if (!input) return static_cast<std::size_t>(-1);
    return std::count_if(edges.begin(), edges.end(), [](const auto &entry) {
        return entry.second.first == 2 && entry.second.second != 0;
    });
}

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "gmesh-orientation-test";
    std::filesystem::create_directories(directory);
    gmesh::RepairOptions options;
    options.input_path = std::filesystem::path(GMESH_TEST_DATA_DIR) /
                         "nonmanifold-orientation-conflict.stl";
    options.output_path = directory / "nonmanifold.stl";
    options.mode = gmesh::RepairMode::import;
    options.overwrite = true;
    const auto imported = gmesh::repair_file(options);
    if (!imported.succeeded() || imported.output.triangles != 22 ||
        winding_conflicts(options.output_path) != 0) {
        std::cerr << "import propagated a non-manifold orientation conflict onto valid edges\n";
        return 1;
    }
    // The guard must still allow useful orientation repairs on an orientable
    // shell. Face (0, 1, 2) below deliberately faces into the tetrahedron.
    const auto tetrahedron = directory / "tetrahedron.stl";
    {
        const std::array<std::array<float, 3>, 4> points{{
            {0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
        const int faces[4][3]{{0, 1, 3}, {0, 1, 2}, {0, 3, 2}, {1, 2, 3}};
        std::ofstream out(tetrahedron, std::ios::binary);
        const std::array<char, 80> header{};
        const std::uint32_t count = 4;
        const std::array<float, 3> normal{};
        const std::uint16_t attribute = 0;
        out.write(header.data(), header.size());
        out.write(reinterpret_cast<const char *>(&count), sizeof(count));
        for (const auto &face : faces) {
            out.write(reinterpret_cast<const char *>(normal.data()), sizeof(normal));
            for (const int vertex : face)
                out.write(reinterpret_cast<const char *>(points[vertex].data()),
                          sizeof(points[vertex]));
            out.write(reinterpret_cast<const char *>(&attribute), sizeof(attribute));
        }
    }
    options.input_path = tetrahedron;
    options.output_path = directory / "oriented.stl";
    const auto oriented = gmesh::repair_file(options);
    if (!oriented.succeeded() || !oriented.output.closed ||
        oriented.warnings.auto_repaired.facets_reversed == 0 ||
        winding_conflicts(options.output_path) != 0) {
        std::cerr << "valid normal-direction repair was not applied\n";
        return 1;
    }
    for (const auto &name : {"nonmanifold.stl", "tetrahedron.stl", "oriented.stl"})
        std::filesystem::remove(directory / name);
    return 0;
}
