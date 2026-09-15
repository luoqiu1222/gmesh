// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <string>

#include "core/model_format.hpp"

namespace mrp::model_import {

struct ImportOptions {
    ModelFormat format = ModelFormat::Auto;
    std::string linearUnit = "millimeter";
    std::string linearDeflectionType = "bounding_box_ratio";
    double linearDeflection = 0.005;
    double angularDeflection = 0.05;
    bool parallel = false;
    std::uint64_t maxInputBytes = 2ull * 1024ull * 1024ull * 1024ull;
    std::uint64_t maxVertices = 50'000'000;
    std::uint64_t maxTriangles = 50'000'000;
    std::uint64_t maxOutputBytes = 2ull * 1024ull * 1024ull * 1024ull;
};

} // namespace mrp::model_import
