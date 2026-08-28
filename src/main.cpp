// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gmesh/repair.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace {

constexpr int exit_success          = 0;
constexpr int exit_invalid_argument = 1;
constexpr int exit_repair_failed    = 5;
constexpr int exit_internal_error   = 10;

void print_help()
{
    std::cout
        << "gmesh " << gmesh::version() << '\n'
        << "Independent triangle-mesh repair CLI\n\n"
        << "Usage:\n"
        << "  gmesh --help\n"
        << "  gmesh --version\n"
        << "  gmesh --license\n"
        << "  gmesh repair --input <mesh> --output <mesh> [options]\n\n"
        << "Repair options:\n"
        << "  --input <path>    Input mesh path\n"
        << "  --output <path>   Repaired mesh path\n"
        << "  --report <path>   Optional machine-readable report path\n"
        << "  --overwrite       Permit replacing an existing output file\n";
}

void print_license()
{
    std::cout << "gmesh is licensed under GNU GPL version 3 or later.\n"
              << "See the LICENSE file distributed with this program.\n";
}

bool read_value(const int argc, char *argv[], int &index, std::filesystem::path &destination)
{
    if (index + 1 >= argc)
        return false;
    destination = argv[++index];
    return true;
}

int run_repair(const int argc, char *argv[])
{
    gmesh::RepairOptions options;

    for (int index = 2; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--input") {
            if (!read_value(argc, argv, index, options.input_path)) {
                std::cerr << "error: --input requires a path\n";
                return exit_invalid_argument;
            }
        } else if (argument == "--output") {
            if (!read_value(argc, argv, index, options.output_path)) {
                std::cerr << "error: --output requires a path\n";
                return exit_invalid_argument;
            }
        } else if (argument == "--report") {
            if (!read_value(argc, argv, index, options.report_path)) {
                std::cerr << "error: --report requires a path\n";
                return exit_invalid_argument;
            }
        } else if (argument == "--overwrite") {
            options.overwrite = true;
        } else {
            std::cerr << "error: unknown argument: " << argument << '\n';
            return exit_invalid_argument;
        }
    }

    const gmesh::RepairReport report = gmesh::repair_file(options);
    if (!report.succeeded()) {
        std::cerr << "repair failed [" << gmesh::to_string(report.status) << "]: "
                  << report.message << '\n';
        return report.status == gmesh::RepairStatus::invalid_argument
                   ? exit_invalid_argument
                   : exit_repair_failed;
    }
    return exit_success;
}

} // namespace

int main(const int argc, char *argv[])
{
    try {
        if (argc <= 1) {
            print_help();
            return exit_invalid_argument;
        }

        const std::string_view command = argv[1];
        if (command == "--help" || command == "-h") {
            print_help();
            return exit_success;
        }
        if (command == "--version") {
            std::cout << "gmesh " << gmesh::version() << '\n';
            return exit_success;
        }
        if (command == "--license") {
            print_license();
            return exit_success;
        }
        if (command == "repair")
            return run_repair(argc, argv);

        std::cerr << "error: unknown command: " << command << '\n';
        return exit_invalid_argument;
    } catch (const std::exception &error) {
        std::cerr << "internal error: " << error.what() << '\n';
        return exit_internal_error;
    }
}
