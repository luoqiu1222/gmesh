// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "gmesh/repair.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <iostream>
#include <iterator>
#include <string>

namespace {

void write_open_cube(const std::filesystem::path &path)
{
    constexpr double vertices[8][3] = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
    };
    // The two top faces are deliberately omitted.
    constexpr int faces[11][3] = {
        {0, 2, 1}, {0, 3, 2}, {0, 1, 5}, {0, 5, 4}, {1, 2, 6},
        {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7},
        {0, 0, 1}, // Repeated vertex: ADMesh must report/remove this facet.
    };
    std::ofstream output(path, std::ios::binary);
    const char header[80]{};
    output.write(header, sizeof(header));
    const std::uint32_t face_count = 11;
    output.write(reinterpret_cast<const char *>(&face_count), sizeof(face_count));
    for (const auto &face : faces) {
        const float normal[3]{};
        output.write(reinterpret_cast<const char *>(normal), sizeof(normal));
        for (const int index : face) {
            const float point[3]{static_cast<float>(vertices[index][0]),
                                 static_cast<float>(vertices[index][1]),
                                 static_cast<float>(vertices[index][2])};
            output.write(reinterpret_cast<const char *>(point), sizeof(point));
        }
        const std::uint16_t attribute = 0;
        output.write(reinterpret_cast<const char *>(&attribute), sizeof(attribute));
    }
}

bool contains_complex_hole_bridge(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    input.seekg(80);
    std::uint32_t face_count = 0;
    input.read(reinterpret_cast<char *>(&face_count), sizeof(face_count));
    for (std::uint32_t face = 0; face < face_count; ++face) {
        float normal[3]{};
        float coordinates[9]{};
        std::uint16_t attribute = 0;
        input.read(reinterpret_cast<char *>(normal), sizeof(normal));
        input.read(reinterpret_cast<char *>(coordinates), sizeof(coordinates));
        input.read(reinterpret_cast<char *>(&attribute), sizeof(attribute));
        bool contains_apex = false;
        bool reaches_raised_interior = false;
        for (std::size_t vertex = 0; vertex < 3; ++vertex) {
            const float x = coordinates[vertex * 3];
            const float y = coordinates[vertex * 3 + 1];
            const float z = coordinates[vertex * 3 + 2];
            contains_apex = contains_apex ||
                            (std::abs(x - 32.0F) < 0.001F &&
                             std::abs(y - 290.0F) < 0.001F && std::abs(z) < 0.001F);
            reaches_raised_interior = reaches_raised_interior || (x > 50.0F && z > 1.0F);
        }
        if (contains_apex && reaches_raised_interior)
            return true;
    }
    return false;
}

bool preserves_curved_wall(const std::filesystem::path &path,
                          const double offset_x = 0.0, const double offset_y = 0.0,
                          const double offset_z = 0.0)
{
    // Fit to the intact z=16.1356 section of dominant-plane-complex-hole.stl.
    // Its existing facets deviate by less than 0.010 mm from this circle.
    // Check triangle interiors too: retaining boundary vertices alone does
    // not prevent a broad chord from cutting through the curved wall.
    constexpr double center_x = 81.37447612;
    constexpr double center_y = 310.79309619;
    constexpr double radius = 42.34811975;
    std::ifstream input(path, std::ios::binary);
    input.seekg(80);
    std::uint32_t face_count = 0;
    input.read(reinterpret_cast<char *>(&face_count), sizeof(face_count));
    double max_inward_deviation = 0.0;
    std::size_t samples = 0;
    const auto check_point = [&](const double raw_x, const double raw_y, const double raw_z) {
        const double x = raw_x + offset_x;
        const double y = raw_y + offset_y;
        const double z = raw_z + offset_z;
        if (x <= 121.0 || x >= 124.0 || y <= 296.0 || y >= 306.0 ||
            z < 16.0 || z >= 45.0)
            return;
        ++samples;
        max_inward_deviation = std::max(max_inward_deviation,
            radius - std::hypot(x - center_x, y - center_y));
    };
    for (std::uint32_t face = 0; face < face_count; ++face) {
        float normal[3]{};
        float points[3][3]{};
        std::uint16_t attribute = 0;
        input.read(reinterpret_cast<char *>(normal), sizeof(normal));
        input.read(reinterpret_cast<char *>(points), sizeof(points));
        input.read(reinterpret_cast<char *>(&attribute), sizeof(attribute));
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const auto &a = points[corner];
            const auto &b = points[(corner + 1) % 3];
            check_point(a[0], a[1], a[2]);
            check_point((double(a[0]) + b[0]) * 0.5,
                        (double(a[1]) + b[1]) * 0.5,
                        (double(a[2]) + b[2]) * 0.5);
        }
        check_point((double(points[0][0]) + points[1][0] + points[2][0]) / 3.0,
                    (double(points[0][1]) + points[1][1] + points[2][1]) / 3.0,
                    (double(points[0][2]) + points[1][2] + points[2][2]) / 3.0);
    }
    if (!input || samples < 40 || max_inward_deviation > 0.020) {
        std::cerr << "curved wall dent: " << max_inward_deviation
                  << " mm (limit 0.020 mm, samples " << samples << ")\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    const gmesh::RepairReport empty_report = gmesh::repair_file({});
    if (empty_report.status != gmesh::RepairStatus::invalid_argument) {
        std::cerr << "empty options must be rejected\n";
        return 1;
    }

    gmesh::RepairOptions options;
    options.input_path  = "missing-input.stl";
    options.output_path = "output.stl";
    const gmesh::RepairReport report = gmesh::repair_file(options);
    if (report.status != gmesh::RepairStatus::input_error) {
        std::cerr << "missing input must be reported as an input error\n";
        return 1;
    }

    if (gmesh::to_string(gmesh::RepairMode::import) != "import" ||
        gmesh::to_string(gmesh::RepairMode::deep) != "deep" ||
        gmesh::to_string(gmesh::RepairMode::all) != "all")
        return 1;

    const std::filesystem::path test_root =
        std::filesystem::temp_directory_path() / "gmesh-core-tests";
    std::filesystem::create_directories(test_root);
    const std::filesystem::path input = test_root / "open-cube.stl";
    const std::filesystem::path output = test_root / "repaired-cube.stl";
    const std::filesystem::path report_path = test_root / "repair.json";
    write_open_cube(input);

    gmesh::RepairOptions integration;
    integration.input_path = input;
    integration.output_path = output;
    integration.report_path = report_path;
    integration.mode = gmesh::RepairMode::all;
    integration.overwrite = true;
    const gmesh::RepairReport repaired = gmesh::repair_file(integration);
    if (!repaired.succeeded() || !repaired.output.closed || repaired.output.open_edges != 0 ||
        !repaired.warnings.auto_repaired.repaired() ||
        repaired.warnings.has_remaining_errors()) {
        std::cerr << "open cube repair failed: " << repaired.message << '\n';
        return 1;
    }
    if (!std::filesystem::is_regular_file(output) || !std::filesystem::is_regular_file(report_path))
        return 1;
    std::ifstream report_stream(report_path);
    const std::string report_text((std::istreambuf_iterator<char>(report_stream)),
                                  std::istreambuf_iterator<char>());
    if (report_text.find("\"status\": \"success\"") == std::string::npos ||
        report_text.find("\"closed\": true") == std::string::npos ||
        report_text.find("\"holes_filled\": 1") == std::string::npos ||
        report_text.find("\"degenerate_facets\": 1") == std::string::npos) {
        std::cerr << "repair report did not contain the expected topology diagnostics\n";
        return 1;
    }
    report_stream.close();

    const std::filesystem::path import_output = test_root / "import-repaired.stl";
    const std::filesystem::path import_report_path = test_root / "import-repair.json";
    write_open_cube(input);
    integration.output_path = import_output;
    integration.report_path = import_report_path;
    integration.mode = gmesh::RepairMode::import;
    const gmesh::RepairReport imported = gmesh::repair_file(integration);
    if (!imported.succeeded() || !imported.warnings.has_warning() ||
        imported.warnings.non_manifold_edges != 4 ||
        !imported.warnings.auto_repaired.repaired()) {
        std::cerr << "Orca-compatible import warning state was not preserved\n";
        return 1;
    }
    std::ifstream import_report_stream(import_report_path);
    const std::string import_report_text((std::istreambuf_iterator<char>(import_report_stream)),
                                         std::istreambuf_iterator<char>());
    if (import_report_text.find("\"non_manifold_edges\": 4") == std::string::npos) {
        std::cerr << "remaining non-manifold edges were not serialized\n";
        return 1;
    }
    import_report_stream.close();

    const std::filesystem::path reconstruction_output = test_root / "reconstructed.stl";
    const std::filesystem::path reconstruction_report = test_root / "reconstruction.json";
    integration.input_path =
        std::filesystem::path(GMESH_TEST_DATA_DIR) / "open-nonmanifold-65.stl";
    integration.output_path = reconstruction_output;
    integration.report_path = reconstruction_report;
    integration.mode = gmesh::RepairMode::all;
    const gmesh::RepairReport reconstructed = gmesh::repair_file(integration);
    if (!reconstructed.succeeded() || !reconstructed.output.closed ||
        reconstructed.output.open_edges != 0 || reconstructed.output.triangles > 100) {
        std::cerr << "non-manifold surface reconstruction failed: "
                  << reconstructed.message << '\n';
        return 1;
    }
    std::ifstream reconstruction_report_stream(reconstruction_report);
    const std::string reconstruction_report_text(
        (std::istreambuf_iterator<char>(reconstruction_report_stream)),
        std::istreambuf_iterator<char>());
    if (reconstruction_report_text.find("\"closed\": true") == std::string::npos ||
        reconstruction_report_text.find("\"surface_rebuilt\"") != std::string::npos) {
        std::cerr << "topology-preserving repair diagnostics were not recorded\n";
        return 1;
    }
    reconstruction_report_stream.close();

    const std::filesystem::path complex_output = test_root / "complex-hole-repaired.stl";
    integration.input_path =
        std::filesystem::path(GMESH_TEST_DATA_DIR) / "dominant-plane-complex-hole.stl";
    integration.output_path = complex_output;
    integration.report_path = test_root / "complex-hole-report.json";
    const gmesh::RepairReport complex = gmesh::repair_file(integration);
    if (!complex.succeeded() || !complex.output.closed || complex.output.open_edges != 0 ||
        contains_complex_hole_bridge(complex_output)) {
        std::cerr << "dominant-plane complex hole repair created a cross-cavity bridge\n";
        return 1;
    }
    if (!preserves_curved_wall(complex_output))
        return 1;

    const std::filesystem::path frontend_roundtrip_output =
        test_root / "frontend-roundtrip-repaired.stl";
    integration.input_path = std::filesystem::path(GMESH_TEST_DATA_DIR) /
                             "frontend-roundtrip-complex-hole.stl";
    integration.output_path = frontend_roundtrip_output;
    integration.report_path = test_root / "frontend-roundtrip-report.json";
    const gmesh::RepairReport frontend_roundtrip = gmesh::repair_file(integration);
    if (!frontend_roundtrip.succeeded() || !frontend_roundtrip.output.closed ||
        frontend_roundtrip.output.open_edges != 0 ||
        contains_complex_hole_bridge(frontend_roundtrip_output)) {
        std::cerr << "frontend roundtrip repair crashed or produced invalid geometry: "
                  << frontend_roundtrip.message << '\n';
        return 1;
    }
    if (!preserves_curved_wall(frontend_roundtrip_output, 97.0, 290.0, 25.0))
        return 1;

    integration.input_path = complex_output;
    integration.output_path = test_root / "complex-hole-repaired-again.stl";
    integration.report_path = test_root / "complex-hole-repaired-again.json";
    const gmesh::RepairReport repeated = gmesh::repair_file(integration);
    if (!repeated.succeeded() || !repeated.output.closed || repeated.output.open_edges != 0) {
        std::cerr << "repeated complete repair is not idempotent: "
                  << repeated.message << '\n';
        return 1;
    }

    std::filesystem::remove_all(test_root);
    return 0;
}
