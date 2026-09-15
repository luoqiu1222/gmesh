// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mrp::model_import {

struct SourceGroup {
    std::uint64_t start = 0;
    std::uint64_t count = 0;
    std::uint32_t sourceIndex = 0;
    std::string sourceFormat;
    std::string name;
};

struct ImportResult {
    std::uint64_t faceCount = 0;
    std::uint64_t tessellatedFaceCount = 0;
    std::uint64_t recoveredFaceCount = 0;
    std::uint64_t degenerateFaceCount = 0;
    std::uint64_t reorientedTriangleCount = 0;
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<std::uint32_t> indices;
    std::vector<SourceGroup> groups;
    std::vector<std::string> warnings;
    std::string sourceFormat;
    std::string sourceUnit;

    std::uint64_t vertexCount() const {
        return static_cast<std::uint64_t>(positions.size() / 3);
    }

    std::uint64_t triangleCount() const {
        return static_cast<std::uint64_t>(indices.size() / 3);
    }
};

enum class ImportErrorCode {
    None = 0,
    ReadFailed,
    TransferFailed,
    TessellationFailed,
    ResourceLimit,
    UnsupportedFormat,
};

struct ImportOutcome {
    ImportResult result;
    ImportErrorCode errorCode = ImportErrorCode::None;
    std::string error;

    bool ok() const {
        return errorCode == ImportErrorCode::None;
    }
};

} // namespace mrp::model_import
