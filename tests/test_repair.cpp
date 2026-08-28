// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "gmesh/repair.hpp"

#include <filesystem>
#include <fstream>
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
    constexpr int faces[10][3] = {
        {0, 2, 1}, {0, 3, 2}, {0, 1, 5}, {0, 5, 4}, {1, 2, 6},
        {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7},
    };
    std::ofstream output(path, std::ios::binary);
    const char header[80]{};
    output.write(header, sizeof(header));
    const std::uint32_t face_count = 10;
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
    if (!repaired.succeeded() || !repaired.output.closed || repaired.output.open_edges != 0) {
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
        report_text.find("\"holes_filled\": 1") == std::string::npos) {
        std::cerr << "repair report did not contain the expected topology diagnostics\n";
        return 1;
    }
    report_stream.close();
    std::filesystem::remove_all(test_root);
    return 0;
}
