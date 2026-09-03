// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "gmesh/repair.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>

namespace {

using Point = std::array<double, 3>;
using Triangle = std::array<std::array<float, 3>, 3>;

struct Frame {
    std::array<Point, 3> axes;
    Point origin;
    double scale;

    Point to_world(const Point &point) const
    {
        Point result = origin;
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                result[j] += point[i] * axes[i][j] * scale;
        return result;
    }

    Point to_local(const std::array<float, 3> &point) const
    {
        Point result{};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                result[i] += (point[j] - origin[j]) * axes[i][j] / scale;
        return result;
    }
};

std::vector<Triangle> read_stl(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    input.seekg(80);
    std::uint32_t count = 0;
    input.read(reinterpret_cast<char *>(&count), sizeof(count));
    std::vector<Triangle> triangles(count);
    for (auto &triangle : triangles) {
        input.seekg(12, std::ios::cur);
        input.read(reinterpret_cast<char *>(triangle.data()), sizeof(triangle));
        input.seekg(2, std::ios::cur);
    }
    return input ? triangles : std::vector<Triangle>{};
}

void write_curved_wall(const std::filesystem::path &path, const Frame &frame)
{
    constexpr int segments = 24;
    std::vector<Point> points;
    for (const double z : {0.0, 10.0, 50.0})
        for (const double radius : {42.0, 45.0})
            for (int i = 0; i <= segments; ++i) {
                const double angle = -0.5 + double(i) / segments;
                points.push_back(frame.to_world({radius * std::cos(angle),
                                                radius * std::sin(angle), z}));
            }
    const auto index = [=](int level, int side, int i) {
        return level * 2 * (segments + 1) + side * (segments + 1) + i;
    };
    std::vector<std::array<int, 3>> faces;
    const auto quad = [&](int a, int b, int c, int d) {
        faces.push_back({a, b, c}); faces.push_back({a, c, d});
    };
    for (int level = 0; level < 2; ++level) {
        for (int i = 0; i < segments; ++i) {
            quad(index(level, 0, i + 1), index(level, 0, i),
                 index(level + 1, 0, i), index(level + 1, 0, i + 1));
            // Five outer-wall quads are missing, leaving a non-planar rim.
            if (!(level == 1 && i >= 10 && i < 15))
                quad(index(level, 1, i), index(level, 1, i + 1),
                     index(level + 1, 1, i + 1), index(level + 1, 1, i));
        }
        quad(index(level, 0, 0), index(level, 1, 0),
             index(level + 1, 1, 0), index(level + 1, 0, 0));
        quad(index(level + 1, 0, segments), index(level + 1, 1, segments),
             index(level, 1, segments), index(level, 0, segments));
    }
    for (int i = 0; i < segments; ++i) {
        quad(index(0, 0, i), index(0, 0, i + 1), index(0, 1, i + 1), index(0, 1, i));
        quad(index(2, 0, i + 1), index(2, 0, i), index(2, 1, i), index(2, 1, i + 1));
    }
    std::ofstream output(path, std::ios::binary);
    const char header[80]{};
    output.write(header, sizeof(header));
    const auto count = static_cast<std::uint32_t>(faces.size());
    output.write(reinterpret_cast<const char *>(&count), sizeof(count));
    for (const auto &face : faces) {
        const float normal[3]{};
        output.write(reinterpret_cast<const char *>(normal), sizeof(normal));
        for (const auto vertex : face) {
            const std::array<float, 3> p{float(points[vertex][0]), float(points[vertex][1]),
                                         float(points[vertex][2])};
            output.write(reinterpret_cast<const char *>(p.data()), sizeof(p));
        }
        const std::uint16_t attribute = 0;
        output.write(reinterpret_cast<const char *>(&attribute), sizeof(attribute));
    }
}

} // namespace

int main()
{
    const double a = std::sqrt(10.0), b = std::sqrt(14.0), c = std::sqrt(140.0);
    const std::vector<Frame> frames{
        Frame{{Point{1, 0, 0}, Point{0, 1, 0}, Point{0, 0, 1}}, {0, 0, 0}, 1.0},
        Frame{{Point{3 / a, 0, -1 / a}, Point{-2 / c, 10 / c, -6 / c},
               Point{1 / b, 2 / b, 3 / b}}, {200, -400, 70}, 0.25},
    };
    const auto root = std::filesystem::temp_directory_path() / "gmesh-curved-hole-tests";
    std::filesystem::create_directories(root);
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto input = root / (std::to_string(i) + "-input.stl");
        const auto output = root / (std::to_string(i) + "-output.stl");
        write_curved_wall(input, frames[i]);
        const auto report = gmesh::repair_file({input, output, {}, gmesh::RepairMode::all, true});
        if (!report.succeeded() || !report.output.closed) {
            std::cerr << "curved wall repair failed: " << report.message << '\n';
            return 1;
        }
        std::set<std::array<float, 3>> output_points;
        std::size_t samples = 0;
        double deviation = 0.0;
        const auto sample = [&](const Point &p) {
            if (p[0] > 44 && p[1] > -5 && p[1] < 7 && p[2] > 11 && p[2] < 49) {
                ++samples;
                deviation = std::max(deviation, std::abs(45.0 - std::hypot(p[0], p[1])));
            }
        };
        for (const auto &triangle : read_stl(output)) {
            Point center{};
            for (std::size_t j = 0; j < 3; ++j) {
                output_points.insert(triangle[j]);
                const auto p = frames[i].to_local(triangle[j]);
                const auto q = frames[i].to_local(triangle[(j + 1) % 3]);
                sample(p);
                sample({(p[0] + q[0]) / 2, (p[1] + q[1]) / 2, (p[2] + q[2]) / 2});
                for (std::size_t k = 0; k < 3; ++k) center[k] += p[k] / 3;
            }
            sample(center);
        }
        if (samples < 20 || deviation > 0.020) {
            std::cerr << "curved wall case " << i << " deviates by " << deviation
                      << " in model units (samples " << samples << ")\n";
            return 1;
        }
        for (const auto &triangle : read_stl(input))
            for (const auto &point : triangle)
                if (output_points.count(point) == 0) {
                    std::cerr << "repair moved an existing curved wall vertex\n";
                    return 1;
                }
    }
    std::filesystem::remove_all(root);
    return 0;
}
