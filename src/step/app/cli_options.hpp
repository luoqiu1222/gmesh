// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>
#include <string>

#include "core/import_options.hpp"

namespace mrp::model_import {

struct CliOptions {
    bool showHelp = false;
    bool showVersion = false;
    bool inputStdin = false;
    bool outputStdio = false;
    std::string inputPath;
    std::string inputName;
    ImportOptions import;
};

struct CliParseResult {
    CliOptions options;
    std::string error;
    bool ok() const {
        return error.empty();
    }
};

CliParseResult parseCliOptions(int argc, char **argv);
std::string cliUsage();

} // namespace mrp::model_import
