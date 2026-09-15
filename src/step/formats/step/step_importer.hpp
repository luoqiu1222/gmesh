// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include "core/model_importer.hpp"

namespace mrp::model_import {

class StepImporter final : public IModelImporter {
  public:
    ModelFormat format() const override {
        return ModelFormat::Step;
    }
    ImportOutcome import(std::istream &input, const std::string &sourceName,
                         const ImportOptions &options, const ProgressCallback &progress) override;
};

} // namespace mrp::model_import
