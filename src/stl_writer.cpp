// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "mesh.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace gmesh::detail {
namespace {

void append_float(char *&cursor, const float value)
{
    static_assert(sizeof(float) == 4, "binary STL requires 32-bit floats");
    std::memcpy(cursor, &value, sizeof(value));
    cursor += sizeof(value);
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
    constexpr std::size_t records_per_chunk = 16384;
    std::vector<std::array<char, 50>> records;
    records.reserve(records_per_chunk);
    for (const Triangle &triangle : mesh.triangles) {
        if (triangle[0] >= mesh.vertices.size() || triangle[1] >= mesh.vertices.size() ||
            triangle[2] >= mesh.vertices.size()) {
            error = "mesh contains an out-of-range triangle index";
            return false;
        }
        const Point normal = normal_for(mesh, triangle);
        std::array<char, 50> record{};
        char *cursor = record.data();
        append_float(cursor, static_cast<float>(normal.x));
        append_float(cursor, static_cast<float>(normal.y));
        append_float(cursor, static_cast<float>(normal.z));
        for (const std::uint32_t index : triangle) {
            const Point &point = mesh.vertices[index];
            append_float(cursor, static_cast<float>(point.x));
            append_float(cursor, static_cast<float>(point.y));
            append_float(cursor, static_cast<float>(point.z));
        }
        records.push_back(record);
        if (records.size() == records_per_chunk) {
            output.write(reinterpret_cast<const char *>(records.data()),
                         static_cast<std::streamsize>(records.size() * sizeof(records.front())));
            records.clear();
        }
    }
    if (!records.empty()) {
        output.write(reinterpret_cast<const char *>(records.data()),
                     static_cast<std::streamsize>(records.size() * sizeof(records.front())));
    }
    if (!output) {
        error = "failed while writing output STL";
        return false;
    }
    return true;
}

} // namespace gmesh::detail
