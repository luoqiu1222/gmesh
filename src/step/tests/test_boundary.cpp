// SPDX-License-Identifier: AGPL-3.0-only
#include "core/boundary_triangulation.hpp"
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

using mrp::model_import::BoundaryMesh;
using mrp::model_import::triangulateBoundary;
namespace {
double area(const BoundaryMesh &mesh) {
    double sum = 0;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
        const auto a = mesh.points[mesh.indices[i]], b = mesh.points[mesh.indices[i + 1]],
                   c = mesh.points[mesh.indices[i + 2]];
        sum += std::abs((b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])) * .5;
    }
    return sum;
}
void require(bool value) {
    if (!value)
        throw std::runtime_error("boundary triangulation regression");
}
} // namespace
int main() {
    try {
        auto holed = triangulateBoundary(
            {{{0, 0}, {4, 0}, {4, 4}, {0, 4}}, {{1, 1}, {3, 1}, {3, 3}, {1, 3}}});
        require(std::abs(area(holed) - 12) < 1e-10);
        auto crossing = triangulateBoundary({{{0, 0}, {2, 2}, {0, 2}, {2, 0}}});
        require(std::abs(area(crossing) - 2) < 1e-10);
        require(triangulateBoundary({{{0, 0}, {1, 0}, {2, 0}}}).indices.empty());
        auto refined = triangulateBoundary({{{0, 0}, {2, 0}, {2, 2}, {0, 2}}}, [](const auto &p) {
            return std::abs((p[1][0] - p[0][0]) * (p[2][1] - p[0][1]) -
                            (p[1][1] - p[0][1]) * (p[2][0] - p[0][0])) > .2;
        });
        require(refined.points.size() > 4 && std::abs(area(refined) - 4) < 1e-10);
        std::map<std::pair<std::uint32_t, std::uint32_t>, int> edges;
        for (std::size_t i = 0; i < refined.indices.size(); i += 3)
            for (int j = 0; j < 3; ++j) {
                auto a = refined.indices[i + j], b = refined.indices[i + (j + 1) % 3];
                if (a > b)
                    std::swap(a, b);
                ++edges[{a, b}];
            }
        int boundary = 0;
        for (const auto &edge : edges)
            if (edge.second == 1)
                ++boundary;
        require(boundary == 4); // Refinement preserves the neighbors' original edge subdivisions.
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
