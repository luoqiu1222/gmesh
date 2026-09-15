// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace mrp::model_import {

enum class ModelFormat {
    Auto,
    Step,
    Unknown,
};

inline std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

inline ModelFormat parseModelFormat(const std::string &value) {
    const auto normalized = lowercase(value);
    if (normalized.empty() || normalized == "auto")
        return ModelFormat::Auto;
    if (normalized == "step" || normalized == "stp")
        return ModelFormat::Step;
    return ModelFormat::Unknown;
}

inline ModelFormat detectModelFormat(const std::string &sourceName) {
    const auto extension = lowercase(std::filesystem::path(sourceName).extension().string());
    if (extension == ".step" || extension == ".stp")
        return ModelFormat::Step;
    return ModelFormat::Unknown;
}

inline const char *modelFormatName(ModelFormat format) {
    switch (format) {
    case ModelFormat::Auto:
        return "auto";
    case ModelFormat::Step:
        return "step";
    default:
        return "unknown";
    }
}

} // namespace mrp::model_import
