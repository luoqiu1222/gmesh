// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include "protocol/frame_writer.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace mrp::model_import::protocol {
namespace {

void put16(std::array<std::uint8_t, kHeaderBytes> &header, std::size_t offset,
           std::uint16_t value) {
    header[offset] = static_cast<std::uint8_t>(value & 0xffu);
    header[offset + 1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
}

void put32(std::array<std::uint8_t, kHeaderBytes> &header, std::size_t offset,
           std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        header[offset + i] = static_cast<std::uint8_t>((value >> (i * 8u)) & 0xffu);
    }
}

void put64(std::array<std::uint8_t, kHeaderBytes> &header, std::size_t offset,
           std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        header[offset + i] = static_cast<std::uint8_t>((value >> (i * 8u)) & 0xffu);
    }
}

} // namespace

void FrameWriter::write(FrameType type, const void *data, std::size_t bytes) {
    std::array<std::uint8_t, kHeaderBytes> header{};
    put32(header, 0, kMagic);
    put16(header, 4, kVersion);
    put16(header, 6, static_cast<std::uint16_t>(type));
    put32(header, 8, sequence_++);
    put64(header, 16, static_cast<std::uint64_t>(bytes));
    output_.write(reinterpret_cast<const char *>(header.data()),
                  static_cast<std::streamsize>(header.size()));
    if (bytes > 0) {
        output_.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(bytes));
    }
    output_.flush();
    if (!output_)
        throw std::runtime_error("failed to write RMIP frame");
}

void FrameWriter::write(FrameType type, const std::string &payload) {
    write(type, payload.data(), payload.size());
}

void FrameWriter::writeEmpty(FrameType type) {
    write(type, nullptr, 0);
}

void FrameWriter::writeChunked(FrameType type, const void *data, std::size_t bytes,
                               std::size_t chunkBytes) {
    const auto *cursor = static_cast<const std::uint8_t *>(data);
    for (std::size_t offset = 0; offset < bytes; offset += chunkBytes) {
        const std::size_t count = std::min(chunkBytes, bytes - offset);
        write(type, cursor + offset, count);
    }
}

} // namespace mrp::model_import::protocol
