// SPDX-FileCopyrightText: 2026 gmesh contributors
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>

#include "protocol/frame.hpp"

namespace mrp::model_import::protocol {

class FrameWriter {
  public:
    explicit FrameWriter(std::ostream &output) : output_(output) {}

    void write(FrameType type, const void *data, std::size_t bytes);
    void write(FrameType type, const std::string &payload);
    void writeEmpty(FrameType type);
    void writeChunked(FrameType type, const void *data, std::size_t bytes,
                      std::size_t chunkBytes = kDefaultChunkBytes);

  private:
    std::ostream &output_;
    std::uint32_t sequence_ = 1;
};

} // namespace mrp::model_import::protocol
