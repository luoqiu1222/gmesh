// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <cstdint>
#include <sstream>
#include <string>

#include "protocol/frame.hpp"
#include "protocol/frame_writer.hpp"

int main() {
    std::ostringstream output(std::ios::binary);
    mrp::model_import::protocol::FrameWriter writer(output);
    writer.write(mrp::model_import::protocol::FrameType::Hello, std::string("{}"));
    writer.writeEmpty(mrp::model_import::protocol::FrameType::End);

    const std::string bytes = output.str();
    if (bytes.size() != mrp::model_import::protocol::kHeaderBytes * 2 + 2)
        return 1;
    const auto *raw = reinterpret_cast<const std::uint8_t *>(bytes.data());
    const std::uint32_t magic =
        static_cast<std::uint32_t>(raw[0]) | (static_cast<std::uint32_t>(raw[1]) << 8u) |
        (static_cast<std::uint32_t>(raw[2]) << 16u) | (static_cast<std::uint32_t>(raw[3]) << 24u);
    if (magic != mrp::model_import::protocol::kMagic)
        return 2;
    if (raw[4] != mrp::model_import::protocol::kVersion)
        return 3;
    if (raw[6] != static_cast<std::uint8_t>(mrp::model_import::protocol::FrameType::Hello))
        return 4;
    if (raw[16] != 2)
        return 5;
    if (bytes[mrp::model_import::protocol::kHeaderBytes] != '{')
        return 6;
    return 0;
}
