// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "core/importer_registry.hpp"

#if MODEL_IMPORT_ENABLE_STEP
#include "formats/step/step_importer.hpp"
#endif

namespace mrp::model_import {

std::unique_ptr<IModelImporter> ImporterRegistry::create(ModelFormat format) {
#if MODEL_IMPORT_ENABLE_STEP
    if (format == ModelFormat::Step) {
        return std::make_unique<StepImporter>();
    }
#else
    (void)format;
#endif
    return nullptr;
}

} // namespace mrp::model_import
