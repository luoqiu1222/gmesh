// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace gmesh {

enum class RepairStatus : std::uint8_t {
    success,
    not_implemented,
    invalid_argument,
    input_error,
    output_error,
    repair_failed,
};

enum class RepairMode : std::uint8_t {
    import,
    deep,
    all,
};

struct MeshStats {
    std::uint64_t vertices   = 0;
    std::uint64_t triangles  = 0;
    std::uint64_t open_edges = 0;
    bool          closed     = false;
};

struct RepairOptions {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::filesystem::path report_path;
    RepairMode            mode      = RepairMode::all;
    bool                  overwrite = false;
};

struct RepairReport {
    RepairStatus status = RepairStatus::repair_failed;
    std::string  message;
    MeshStats    input;
    MeshStats    output;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return status == RepairStatus::success;
    }
};

// External seam for callers and tests. Mesh parsing, repair strategy, output
// validation, and report serialization remain implementation details.
[[nodiscard]] RepairReport repair_file(const RepairOptions &options);

[[nodiscard]] std::string_view to_string(RepairStatus status) noexcept;
[[nodiscard]] std::string_view to_string(RepairMode mode) noexcept;
[[nodiscard]] std::string_view version() noexcept;

} // namespace gmesh
