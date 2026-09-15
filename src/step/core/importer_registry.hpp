// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <memory>

#include "core/model_importer.hpp"

namespace mrp::model_import {

class ImporterRegistry {
  public:
    static std::unique_ptr<IModelImporter> create(ModelFormat format);
};

} // namespace mrp::model_import
