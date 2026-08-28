#include "gmesh/repair.hpp"

#include <iostream>

int main()
{
    const gmesh::RepairReport empty_report = gmesh::repair_file({});
    if (empty_report.status != gmesh::RepairStatus::invalid_argument) {
        std::cerr << "empty options must be rejected\n";
        return 1;
    }

    gmesh::RepairOptions options;
    options.input_path  = "input.stl";
    options.output_path = "output.stl";
    const gmesh::RepairReport report = gmesh::repair_file(options);
    if (report.status != gmesh::RepairStatus::not_implemented) {
        std::cerr << "skeleton must report a missing repair backend\n";
        return 1;
    }
    return 0;
}
