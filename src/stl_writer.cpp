// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "mesh.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace gmesh::detail {
namespace {

void write_float(std::ostream &stream, const float value)
{
    static_assert(sizeof(float) == 4, "binary STL requires 32-bit floats");
    std::array<char, 4> bytes{};
    std::memcpy(bytes.data(), &value, bytes.size());
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

Point normal_for(const Mesh &mesh, const Triangle &triangle)
{
    const Point &a = mesh.vertices[triangle[0]];
    const Point &b = mesh.vertices[triangle[1]];
    const Point &c = mesh.vertices[triangle[2]];
    Point normal{
        (b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y),
        (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z),
        (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x),
    };
    const double length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (length > 1e-15) {
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
    }
    return normal;
}

} // namespace

bool write_binary_stl(const std::string &path, const Mesh &mesh, std::string &error)
{
    if (mesh.triangles.size() > std::numeric_limits<std::uint32_t>::max()) {
        error = "mesh has too many triangles for binary STL";
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "could not open output STL";
        return false;
    }
    std::array<char, 80> header{};
    const char label[] = "gmesh repaired mesh";
    std::copy(label, label + sizeof(label) - 1, header.begin());
    output.write(header.data(), static_cast<std::streamsize>(header.size()));
    const std::uint32_t count = static_cast<std::uint32_t>(mesh.triangles.size());
    output.write(reinterpret_cast<const char *>(&count), sizeof(count));
    for (const Triangle &triangle : mesh.triangles) {
        if (triangle[0] >= mesh.vertices.size() || triangle[1] >= mesh.vertices.size() ||
            triangle[2] >= mesh.vertices.size()) {
            error = "mesh contains an out-of-range triangle index";
            return false;
        }
        const Point normal = normal_for(mesh, triangle);
        write_float(output, static_cast<float>(normal.x));
        write_float(output, static_cast<float>(normal.y));
        write_float(output, static_cast<float>(normal.z));
        for (const std::uint32_t index : triangle) {
            const Point &point = mesh.vertices[index];
            write_float(output, static_cast<float>(point.x));
            write_float(output, static_cast<float>(point.y));
            write_float(output, static_cast<float>(point.z));
        }
        const std::uint16_t attribute = 0;
        output.write(reinterpret_cast<const char *>(&attribute), sizeof(attribute));
    }
    if (!output) {
        error = "failed while writing output STL";
        return false;
    }
    return true;
}

} // namespace gmesh::detail
