#pragma once

#include <cstddef>

namespace RTechZstd
{
    bool Decompress(const void* src, size_t srcSize, void* dst, size_t dstCapacity, size_t& decodedBytes);

    // Returns 0 on failure, or the real compressed frame size
    size_t FindFrameCompressedSize(const void* src, size_t srcSize);
}
