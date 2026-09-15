// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
namespace mrp::model_import {
struct BoundaryMesh {
    std::vector<std::array<double, 2>> points;
    std::vector<std::uint32_t> indices;
};
BoundaryMesh triangulateBoundary(
    const std::vector<std::vector<std::array<double, 2>>> &rings,
    const std::function<bool(const std::array<std::array<double, 2>, 3> &)> &refine = {});
} // namespace mrp::model_import
