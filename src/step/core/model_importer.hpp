// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <functional>
#include <istream>
#include <string>

#include "core/import_options.hpp"
#include "core/import_result.hpp"

namespace mrp::model_import {

using ProgressCallback = std::function<void(const std::string &, int, const std::string &)>;

class IModelImporter {
  public:
    virtual ~IModelImporter() = default;
    virtual ModelFormat format() const = 0;
    virtual ImportOutcome import(std::istream &input, const std::string &sourceName,
                                 const ImportOptions &options,
                                 const ProgressCallback &progress) = 0;
};

} // namespace mrp::model_import
