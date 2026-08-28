// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "gmesh/repair.hpp"
#include "mesh.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace gmesh {
namespace {

using detail::Mesh;
using detail::RepairDiagnostics;

struct Edge {
    std::uint32_t a;
    std::uint32_t b;
    bool operator==(const Edge &other) const noexcept { return a == other.a && b == other.b; }
};

struct EdgeHash {
    std::size_t operator()(const Edge &edge) const noexcept
    {
        return (static_cast<std::size_t>(edge.a) << 32U) ^ edge.b;
    }
};

MeshStats stats_for(const Mesh &mesh)
{
    std::unordered_map<Edge, std::uint32_t, EdgeHash> uses;
    for (const detail::Triangle &triangle : mesh.triangles) {
        for (std::size_t edge = 0; edge < 3; ++edge) {
            std::uint32_t a = triangle[edge];
            std::uint32_t b = triangle[(edge + 1) % 3];
            if (a > b)
                std::swap(a, b);
            ++uses[{a, b}];
        }
    }
    std::uint64_t open_edges = 0;
    for (const auto &entry : uses)
        if (entry.second == 1)
            ++open_edges;
    return {static_cast<std::uint64_t>(mesh.vertices.size()),
            static_cast<std::uint64_t>(mesh.triangles.size()), open_edges,
            !mesh.triangles.empty() && open_edges == 0};
}

std::string lower_extension(const std::filesystem::path &path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return extension;
}

std::string json_escape(const std::string &value)
{
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default: output << (character < 0x20 ? '?' : static_cast<char>(character));
        }
    }
    return output.str();
}

bool write_report(const std::filesystem::path &path, const RepairOptions &options,
                  const RepairReport &report, const RepairDiagnostics &diagnostics,
                  std::string &error)
{
    if (path.empty())
        return true;
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = "could not open report path";
        return false;
    }
    output << "{\n"
           << "  \"schema_version\": 1,\n"
           << "  \"status\": \"" << to_string(report.status) << "\",\n"
           << "  \"mode\": \"" << to_string(options.mode) << "\",\n"
           << "  \"message\": \"" << json_escape(report.message) << "\",\n"
           << "  \"input\": {\"vertices\": " << report.input.vertices
           << ", \"triangles\": " << report.input.triangles
           << ", \"open_edges\": " << report.input.open_edges
           << ", \"closed\": " << (report.input.closed ? "true" : "false") << "},\n"
           << "  \"output\": {\"vertices\": " << report.output.vertices
           << ", \"triangles\": " << report.output.triangles
           << ", \"open_edges\": " << report.output.open_edges
           << ", \"closed\": " << (report.output.closed ? "true" : "false") << "},\n"
           << "  \"warnings\": {\"has_warning\": "
           << (report.warnings.has_warning() ? "true" : "false")
           << ", \"auto_repaired\": {\"count\": "
           << report.warnings.auto_repaired.count()
           << ", \"edges_fixed\": " << report.warnings.auto_repaired.edges_fixed
           << ", \"degenerate_facets\": "
           << report.warnings.auto_repaired.degenerate_facets
           << ", \"facets_removed\": " << report.warnings.auto_repaired.facets_removed
           << ", \"facets_reversed\": " << report.warnings.auto_repaired.facets_reversed
           << ", \"backwards_edges\": " << report.warnings.auto_repaired.backwards_edges
           << "}, \"remaining_errors\": {\"non_manifold_edges\": "
           << report.warnings.non_manifold_edges << "}},\n"
           << "  \"repairs\": {\"edges_fixed\": " << diagnostics.repaired.edges_fixed
           << ", \"degenerate_facets\": " << diagnostics.repaired.degenerate_facets
           << ", \"facets_removed\": " << diagnostics.repaired.facets_removed
           << ", \"facets_reversed\": " << diagnostics.repaired.facets_reversed
           << ", \"backwards_edges\": " << diagnostics.repaired.backwards_edges
           << ", \"parts_found\": " << diagnostics.parts_found
           << ", \"parts_removed\": " << diagnostics.parts_removed
           << ", \"holes_found\": " << diagnostics.holes_found
           << ", \"holes_filled\": " << diagnostics.holes_filled
           << ", \"self_union_succeeded\": "
           << (diagnostics.union_succeeded ? "true" : "false") << "}\n}\n";
    if (!output) {
        error = "failed while writing report";
        return false;
    }
    return true;
}

RepairReport fail(const RepairStatus status, std::string message, const MeshStats input = {})
{
    return {status, std::move(message), input, {}};
}

bool place_output(const std::filesystem::path &temporary,
                  const std::filesystem::path &destination, const bool overwrite,
                  const std::int64_t unique, std::string &error)
{
    std::error_code filesystem_error;
    std::filesystem::path backup = destination;
    backup += ".gmesh-" + std::to_string(unique) + ".backup";
    const bool destination_exists = std::filesystem::exists(destination, filesystem_error);
    if (filesystem_error) {
        error = "could not inspect output path";
        return false;
    }
    if (destination_exists) {
        if (!overwrite) {
            error = "output exists; pass --overwrite to replace it";
            return false;
        }
        std::filesystem::rename(destination, backup, filesystem_error);
        if (filesystem_error) {
            error = "could not preserve existing output before replacement";
            return false;
        }
    }

    std::filesystem::rename(temporary, destination, filesystem_error);
    if (filesystem_error) {
        if (destination_exists) {
            std::error_code restore_error;
            std::filesystem::rename(backup, destination, restore_error);
            error = restore_error ? "could not place output or restore the previous file"
                                  : "could not move repaired STL into place; previous file restored";
        } else {
            error = "could not move repaired STL into place";
        }
        return false;
    }
    if (destination_exists)
        std::filesystem::remove(backup, filesystem_error);
    return true;
}

} // namespace

RepairReport repair_file(const RepairOptions &options)
{
    if (options.input_path.empty())
        return fail(RepairStatus::invalid_argument, "input path is required");
    if (options.output_path.empty())
        return fail(RepairStatus::invalid_argument, "output path is required");
    if (lower_extension(options.input_path) != ".stl" ||
        lower_extension(options.output_path) != ".stl")
        return fail(RepairStatus::invalid_argument, "this release accepts STL input and output only");

    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(options.input_path, filesystem_error))
        return fail(RepairStatus::input_error, "input STL does not exist or is not a regular file");
    if (std::filesystem::exists(options.output_path, filesystem_error) && !options.overwrite)
        return fail(RepairStatus::invalid_argument, "output exists; pass --overwrite to replace it");
    if (!options.report_path.empty() && std::filesystem::exists(options.report_path, filesystem_error) &&
        !options.overwrite)
        return fail(RepairStatus::invalid_argument, "report exists; pass --overwrite to replace it");

    Mesh mesh;
    RepairDiagnostics diagnostics;
    std::string error;
    const bool loaded = options.mode == RepairMode::deep
                            ? detail::load_without_repair(options.input_path.string(), mesh, error)
                            : detail::load_and_import_repair(options.input_path.string(), mesh,
                                                             diagnostics, error);
    if (!loaded)
        return fail(RepairStatus::input_error, std::move(error));

    const MeshStats input_stats = stats_for(mesh);
    if (options.mode != RepairMode::import && !detail::deep_repair(mesh, diagnostics, error))
        return fail(RepairStatus::repair_failed, std::move(error), input_stats);
    const MeshStats output_stats = stats_for(mesh);
    if (mesh.triangles.empty())
        return fail(RepairStatus::repair_failed, "repair produced an empty mesh", input_stats);
    if (options.mode != RepairMode::import && !output_stats.closed)
        return fail(RepairStatus::repair_failed, "deep repair did not produce a closed mesh", input_stats);

    const std::int64_t unique = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path temporary = options.output_path;
    temporary += ".gmesh-" + std::to_string(unique) + ".tmp";
    if (!detail::write_binary_stl(temporary.string(), mesh, error)) {
        std::filesystem::remove(temporary, filesystem_error);
        return fail(RepairStatus::output_error, std::move(error), input_stats);
    }
    if (!place_output(temporary, options.output_path, options.overwrite, unique, error)) {
        std::filesystem::remove(temporary, filesystem_error);
        return fail(RepairStatus::output_error, std::move(error), input_stats);
    }

    RepairReport report{RepairStatus::success, "repair completed", input_stats, output_stats};
    report.warnings.auto_repaired = diagnostics.repaired;
    report.warnings.non_manifold_edges = output_stats.open_edges;
    if (!write_report(options.report_path, options, report, diagnostics, error)) {
        report.status = RepairStatus::output_error;
        report.message = std::move(error);
    }
    return report;
}

std::string_view to_string(const RepairStatus status) noexcept
{
    switch (status) {
    case RepairStatus::success:          return "success";
    case RepairStatus::not_implemented:  return "not_implemented";
    case RepairStatus::invalid_argument: return "invalid_argument";
    case RepairStatus::input_error:      return "input_error";
    case RepairStatus::output_error:     return "output_error";
    case RepairStatus::repair_failed:    return "repair_failed";
    }
    return "unknown";
}

std::string_view to_string(const RepairMode mode) noexcept
{
    switch (mode) {
    case RepairMode::import: return "import";
    case RepairMode::deep: return "deep";
    case RepairMode::all: return "all";
    }
    return "unknown";
}

std::string_view version() noexcept { return "0.3.0"; }

} // namespace gmesh
