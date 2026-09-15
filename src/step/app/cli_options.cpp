// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "app/cli_options.hpp"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace mrp::model_import {
namespace {

bool readValue(int argc, char **argv, int &index, std::string &value, std::string &error) {
    if (index + 1 >= argc) {
        error = std::string("missing value for ") + argv[index];
        return false;
    }
    value = argv[++index];
    return true;
}

bool parseDouble(const std::string &text, double &value) {
    errno = 0;
    char *end = nullptr;
    value = std::strtod(text.c_str(), &end);
    return errno == 0 && end != text.c_str() && *end == '\0' && std::isfinite(value) && value > 0.0;
}

bool parseUint64(const std::string &text, std::uint64_t &value) {
    if (text.empty() || text.front() == '-')
        return false;
    errno = 0;
    char *end = nullptr;
    const auto parsed = std::strtoull(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' || parsed == 0)
        return false;
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

} // namespace

CliParseResult parseCliOptions(int argc, char **argv) {
    CliParseResult result;
    if (argc < 2) {
        result.error = "missing command";
        return result;
    }

    const std::string command = argv[1];
    if (command == "--help" || command == "help") {
        result.options.showHelp = true;
        return result;
    }
    if (command == "--version" || command == "version") {
        result.options.showVersion = true;
        return result;
    }
    if (command != "convert") {
        result.error = "unknown command: " + command;
        return result;
    }

    for (int i = 2; i < argc && result.error.empty(); ++i) {
        const std::string argument = argv[i];
        std::string value;
        if (argument == "--help") {
            result.options.showHelp = true;
        } else if (argument == "--input") {
            if (readValue(argc, argv, i, value, result.error))
                result.options.inputPath = value;
        } else if (argument == "--input-stdin") {
            result.options.inputStdin = true;
        } else if (argument == "--input-name") {
            if (readValue(argc, argv, i, value, result.error))
                result.options.inputName = value;
        } else if (argument == "--output-stdio") {
            result.options.outputStdio = true;
        } else if (argument == "--format") {
            if (readValue(argc, argv, i, value, result.error)) {
                result.options.import.format = parseModelFormat(value);
                if (result.options.import.format == ModelFormat::Unknown) {
                    result.error = "unsupported format: " + value;
                }
            }
        } else if (argument == "--linear-unit") {
            if (readValue(argc, argv, i, value, result.error))
                result.options.import.linearUnit = value;
        } else if (argument == "--linear-deflection-type") {
            if (readValue(argc, argv, i, value, result.error)) {
                result.options.import.linearDeflectionType = value;
            }
        } else if (argument == "--linear-deflection") {
            if (readValue(argc, argv, i, value, result.error) &&
                !parseDouble(value, result.options.import.linearDeflection)) {
                result.error = "invalid --linear-deflection";
            }
        } else if (argument == "--angular-deflection") {
            if (readValue(argc, argv, i, value, result.error) &&
                !parseDouble(value, result.options.import.angularDeflection)) {
                result.error = "invalid --angular-deflection";
            }
        } else if (argument == "--parallel") {
            result.options.import.parallel = true;
        } else if (argument == "--max-input-bytes") {
            if (readValue(argc, argv, i, value, result.error) &&
                !parseUint64(value, result.options.import.maxInputBytes)) {
                result.error = "invalid --max-input-bytes";
            }
        } else if (argument == "--max-vertices") {
            if (readValue(argc, argv, i, value, result.error) &&
                !parseUint64(value, result.options.import.maxVertices)) {
                result.error = "invalid --max-vertices";
            }
        } else if (argument == "--max-triangles") {
            if (readValue(argc, argv, i, value, result.error) &&
                !parseUint64(value, result.options.import.maxTriangles)) {
                result.error = "invalid --max-triangles";
            }
        } else if (argument == "--max-output-bytes") {
            if (readValue(argc, argv, i, value, result.error) &&
                !parseUint64(value, result.options.import.maxOutputBytes)) {
                result.error = "invalid --max-output-bytes";
            }
        } else {
            result.error = "unknown option: " + argument;
        }
    }

    if (!result.error.empty() || result.options.showHelp)
        return result;
    if (result.options.inputStdin == !result.options.inputPath.empty()) {
        result.error = "choose exactly one of --input or --input-stdin";
    } else if (!result.options.outputStdio) {
        result.error = "--output-stdio is required";
    } else if (result.options.inputStdin && result.options.inputName.empty()) {
        result.error = "--input-name is required with --input-stdin";
    }
    if (result.options.inputName.empty())
        result.options.inputName = result.options.inputPath;
    if (result.options.import.format == ModelFormat::Auto) {
        result.options.import.format = detectModelFormat(result.options.inputName);
        if (result.options.import.format == ModelFormat::Unknown) {
            result.error = "cannot detect model format; pass --format";
        }
    }
    if (lowercase(result.options.import.linearUnit) != "millimeter") {
        result.error = "only millimeter output is currently supported";
    }
    const auto deflectionType = lowercase(result.options.import.linearDeflectionType);
    if (deflectionType != "bounding_box_ratio" && deflectionType != "absolute") {
        result.error = "--linear-deflection-type must be bounding_box_ratio or absolute";
    }
    return result;
}

std::string cliUsage() {
    return "Usage:\n"
           "  rocket-model-import version\n"
           "  rocket-model-import convert --input <path> --format auto --output-stdio [options]\n"
           "  rocket-model-import convert --input-stdin --input-name <name> --format <format> "
           "--output-stdio [options]\n";
}

} // namespace mrp::model_import
