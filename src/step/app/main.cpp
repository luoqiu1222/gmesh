// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "app/cli_options.hpp"
#include "core/importer_registry.hpp"
#include "protocol/frame_writer.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

namespace {

std::string jsonEscape(const std::string &value) {
    std::ostringstream output;
    for (const unsigned char c : value) {
        switch (c) {
        case '\\':
            output << "\\\\";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (c < 0x20) {
                const char *hex = "0123456789abcdef";
                output << "\\u00" << hex[(c >> 4u) & 0xfu] << hex[c & 0xfu];
            } else {
                output << static_cast<char>(c);
            }
        }
    }
    return output.str();
}

std::string progressJson(const std::string &stage, int percent, const std::string &message) {
    return "{\"stage\":\"" + jsonEscape(stage) + "\",\"percent\":" + std::to_string(percent) +
           ",\"message\":\"" + jsonEscape(message) + "\"}";
}

std::string groupsJson(const mrp::model_import::ImportResult &result) {
    std::ostringstream output;
    output << '[';
    for (std::size_t i = 0; i < result.groups.size(); ++i) {
        if (i > 0)
            output << ',';
        const auto &group = result.groups[i];
        output << "{\"start\":" << group.start << ",\"count\":" << group.count
               << ",\"sourceIndex\":" << group.sourceIndex << ",\"sourceFormat\":\""
               << jsonEscape(group.sourceFormat) << "\",\"name\":\"" << jsonEscape(group.name)
               << "\"}";
    }
    output << ']';
    return output.str();
}

std::string warningsJson(const mrp::model_import::ImportResult &result) {
    std::string output = "[";
    for (std::size_t i = 0; i < result.warnings.size(); ++i) {
        if (i)
            output += ",";
        output += "\"" + jsonEscape(result.warnings[i]) + "\"";
    }
    return output + "]";
}

std::string reportJson(const mrp::model_import::ImportResult &result) {
    return "{\"ok\":true,\"sourceFormat\":\"" + jsonEscape(result.sourceFormat) +
           "\",\"sourceUnit\":\"" + jsonEscape(result.sourceUnit) +
           "\",\"vertexCount\":" + std::to_string(result.vertexCount()) +
           ",\"triangleCount\":" + std::to_string(result.triangleCount()) +
           ",\"groupCount\":" + std::to_string(result.groups.size()) +
           ",\"faceCount\":" + std::to_string(result.faceCount) +
           ",\"tessellatedFaceCount\":" + std::to_string(result.tessellatedFaceCount) +
           ",\"recoveredFaceCount\":" + std::to_string(result.recoveredFaceCount) +
           ",\"degenerateFaceCount\":" + std::to_string(result.degenerateFaceCount) +
           ",\"reorientedTriangleCount\":" + std::to_string(result.reorientedTriangleCount) +
           ",\"warnings\":" + warningsJson(result) + "}";
}

int exitCode(mrp::model_import::ImportErrorCode code) {
    using mrp::model_import::ImportErrorCode;
    switch (code) {
    case ImportErrorCode::ReadFailed:
        return 3;
    case ImportErrorCode::TransferFailed:
        return 4;
    case ImportErrorCode::TessellationFailed:
        return 5;
    case ImportErrorCode::ResourceLimit:
        return 7;
    case ImportErrorCode::UnsupportedFormat:
        return 2;
    default:
        return 6;
    }
}

} // namespace

int run_step_convert(int argc, char **argv) {
    const auto parsed = mrp::model_import::parseCliOptions(argc, argv);
    if (!parsed.ok()) {
        std::cerr << parsed.error << '\n' << mrp::model_import::cliUsage();
        return 2;
    }
    if (parsed.options.showHelp) {
        std::cout << mrp::model_import::cliUsage();
        return 0;
    }
    if (parsed.options.showVersion) {
        std::cout << "rocket-model-import 0.1.0\n";
        return 0;
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    std::ifstream file;
    std::istringstream stdinBuffer;
    std::istream *input = &std::cin;
    if (parsed.options.inputStdin) {
        std::vector<char> bytes;
        constexpr std::size_t kReadChunkBytes = 1024 * 1024;
        std::vector<char> chunk(kReadChunkBytes);
        while (std::cin) {
            std::cin.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
            const auto count = static_cast<std::size_t>(std::cin.gcount());
            if (count == 0)
                break;
            if (count > parsed.options.import.maxInputBytes ||
                bytes.size() > parsed.options.import.maxInputBytes - count) {
                std::cerr << "stdin input exceeds --max-input-bytes\n";
                return 7;
            }
            bytes.insert(bytes.end(), chunk.data(), chunk.data() + count);
        }
        stdinBuffer.str(std::string(bytes.begin(), bytes.end()));
        input = &stdinBuffer;
    } else {
        std::error_code sizeError;
        const auto inputBytes = std::filesystem::file_size(
            std::filesystem::u8path(parsed.options.inputPath), sizeError);
        if (!sizeError && inputBytes > parsed.options.import.maxInputBytes) {
            std::cerr << "input file exceeds --max-input-bytes\n";
            return 7;
        }
        file.open(std::filesystem::u8path(parsed.options.inputPath), std::ios::binary);
        if (!file) {
            std::cerr << "failed to open input file\n";
            return 3;
        }
        input = &file;
    }

    mrp::model_import::protocol::FrameWriter writer(std::cout);
    try {
        writer.write(mrp::model_import::protocol::FrameType::Hello,
                     "{\"protocol\":\"RMIP\",\"version\":1}");
        auto importer = mrp::model_import::ImporterRegistry::create(parsed.options.import.format);
        if (!importer) {
            writer.write(
                mrp::model_import::protocol::FrameType::Error,
                "{\"code\":\"UNSUPPORTED_FORMAT\",\"message\":\"no importer registered\"}");
            return 2;
        }
        const auto outcome = importer->import(
            *input, parsed.options.inputName, parsed.options.import,
            [&writer](const std::string &stage, int percent, const std::string &message) {
                writer.write(mrp::model_import::protocol::FrameType::Progress,
                             progressJson(stage, percent, message));
            });
        if (!outcome.ok()) {
            writer.write(mrp::model_import::protocol::FrameType::Error,
                         "{\"code\":\"IMPORT_FAILED\",\"message\":\"" + jsonEscape(outcome.error) +
                             "\"}");
            return exitCode(outcome.errorCode);
        }

        const auto &result = outcome.result;
        writer.write(mrp::model_import::protocol::FrameType::MeshInfo,
                     "{\"vertexCount\":" + std::to_string(result.vertexCount()) +
                         ",\"indexCount\":" + std::to_string(result.indices.size()) +
                         ",\"triangleCount\":" + std::to_string(result.triangleCount()) +
                         ",\"groupCount\":" + std::to_string(result.groups.size()) + "}");
        writer.writeChunked(mrp::model_import::protocol::FrameType::PositionChunk,
                            result.positions.data(), result.positions.size() * sizeof(float));
        if (!result.normals.empty()) {
            writer.writeChunked(mrp::model_import::protocol::FrameType::NormalChunk,
                                result.normals.data(), result.normals.size() * sizeof(float));
        }
        writer.writeChunked(mrp::model_import::protocol::FrameType::IndexChunk,
                            result.indices.data(), result.indices.size() * sizeof(std::uint32_t));
        writer.write(mrp::model_import::protocol::FrameType::GroupTable, groupsJson(result));
        writer.write(mrp::model_import::protocol::FrameType::Report, reportJson(result));
        writer.writeEmpty(mrp::model_import::protocol::FrameType::End);
        return 0;
    } catch (const std::exception &error) {
        try {
            writer.write(mrp::model_import::protocol::FrameType::Error,
                         "{\"code\":\"OUTPUT_FAILED\",\"message\":\"" + jsonEscape(error.what()) +
                             "\"}");
        } catch (...) {
        }
        std::cerr << error.what() << '\n';
        return 6;
    }
}
