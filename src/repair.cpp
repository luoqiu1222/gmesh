#include "gmesh/repair.hpp"

namespace gmesh {

RepairReport repair_file(const RepairOptions &options)
{
    if (options.input_path.empty())
        return {RepairStatus::invalid_argument, "input path is required", {}, {}};

    if (options.output_path.empty())
        return {RepairStatus::invalid_argument, "output path is required", {}, {}};

    return {
        RepairStatus::not_implemented,
        "mesh repair backend has not been integrated yet",
        {},
        {},
    };
}

std::string_view to_string(const RepairStatus status) noexcept
{
    switch (status) {
    case RepairStatus::success:          return "success";
    case RepairStatus::not_implemented:  return "not_implemented";
    case RepairStatus::invalid_argument: return "invalid_argument";
    case RepairStatus::input_error:      return "input_error";
    case RepairStatus::output_error:     return "output_error";
    case RepairStatus::repair_failed:    return "repair_failed";
    }
    return "unknown";
}

std::string_view version() noexcept
{
    return "0.1.0";
}

} // namespace gmesh
