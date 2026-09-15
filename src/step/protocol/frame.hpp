// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>

namespace mrp::model_import::protocol {

constexpr std::uint32_t kMagic = 0x50494D52; // RMIP
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kHeaderBytes = 24;
constexpr std::size_t kDefaultChunkBytes = 4 * 1024 * 1024;

enum class FrameType : std::uint16_t {
    Hello = 1,
    Progress = 2,
    MeshInfo = 3,
    PositionChunk = 4,
    NormalChunk = 5,
    IndexChunk = 6,
    GroupTable = 7,
    Report = 8,
    End = 9,
    Error = 100,
};

} // namespace mrp::model_import::protocol
