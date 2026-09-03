// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "gmesh/repair.hpp"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int main()
{
    // A long rim with a raised side opening reproduces the dominant-plane
    // search used by frontend-exported meshes, without a large STL fixture.
    constexpr std::size_t segments = 1536;
    constexpr double pi = 3.14159265358979323846;
    using Point = std::array<float, 3>;
    using Triangle = std::array<Point, 3>;
    std::vector<Point> bottom, top;
    for (std::size_t i = 0; i < segments; ++i) {
        const double angle = 2.0 * pi * double(i) / segments;
        const float x = float(20.0 * std::cos(angle));
        const float y = float(10.0 * std::sin(angle));
        const double raised = i > segments * 3 / 4
            ? 5.0 * std::sin(4.0 * pi * double(i - segments * 3 / 4) / segments)
            : 0.0;
        bottom.push_back({x, y, 0.0F});
        top.push_back({x, y, float(10.0 + raised)});
    }
    std::vector<Triangle> triangles;
    for (std::size_t i = 0; i < segments; ++i) {
        const std::size_t j = (i + 1) % segments;
        triangles.push_back({bottom[i], bottom[j], top[j]});
        triangles.push_back({bottom[i], top[j], top[i]});
        triangles.push_back({Point{0, 0, 0}, bottom[j], bottom[i]});
    }
    const auto directory = std::filesystem::temp_directory_path() / "gmesh-large-hole-test";
    std::filesystem::create_directories(directory);
    const auto input = directory / "input.stl";
    const auto output = directory / "output.stl";
    {
        std::ofstream stream(input, std::ios::binary);
        const std::array<char, 80> header{};
        const auto count = static_cast<std::uint32_t>(triangles.size());
        const Point normal{0, 0, 0};
        const std::uint16_t attribute = 0;
        stream.write(header.data(), header.size());
        stream.write(reinterpret_cast<const char *>(&count), sizeof(count));
        for (const auto &triangle : triangles) {
            stream.write(reinterpret_cast<const char *>(normal.data()), sizeof(normal));
            stream.write(reinterpret_cast<const char *>(triangle.data()), sizeof(triangle));
            stream.write(reinterpret_cast<const char *>(&attribute), sizeof(attribute));
        }
        if (!stream) return 1;
    }
    gmesh::RepairOptions options;
    options.input_path = input;
    options.output_path = output;
    options.mode = gmesh::RepairMode::all;
    options.overwrite = true;
    const auto report = gmesh::repair_file(options);
    std::filesystem::remove(input);
    std::filesystem::remove(output);
    if (report.status != gmesh::RepairStatus::success ||
        report.input.open_edges != segments || !report.output.closed ||
        report.output.triangles <= report.input.triangles) {
        std::cerr << "large non-planar rim was not closed: " << report.message << '\n';
        return 1;
    }
    return 0;
}
