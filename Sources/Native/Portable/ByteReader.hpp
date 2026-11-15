#pragma once
// Portable, bounded binary reader for future X-Ray format parsers.
// This is NOT an X-Ray archive decoder or an implementation of xrCore.
#include <cstddef>
#include <cstdint>

namespace stalker_port {
class ByteReader {
public:
    ByteReader(const std::uint8_t* data, std::size_t size)
        : data_(data), size_(size), position_(0) {}

    bool read_u32_le(std::uint32_t& out) {
        if (position_ > size_ || size_ - position_ < 4 || data_ == nullptr)
            return false;
        const auto* p = data_ + position_;
        out = static_cast<std::uint32_t>(p[0]) |
              (static_cast<std::uint32_t>(p[1]) << 8) |
              (static_cast<std::uint32_t>(p[2]) << 16) |
              (static_cast<std::uint32_t>(p[3]) << 24);
        position_ += 4;
        return true;
    }

    bool read_u16_le(std::uint16_t& out) {
        if (position_ > size_ || size_ - position_ < 2 || data_ == nullptr) return false;
        const auto* p = data_ + position_;
        out = static_cast<std::uint16_t>(p[0]) |
              static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8);
        position_ += 2;
        return true;
    }

    bool read_bytes(std::uint8_t* out, std::size_t count) {
        if (position_ > size_ || count > size_ - position_ ||
            (count && (out == nullptr || data_ == nullptr))) return false;
        for (std::size_t i = 0; i < count; ++i) out[i] = data_[position_ + i];
        position_ += count;
        return true;
    }

    bool skip(std::size_t count) {
        if (position_ > size_ || count > size_ - position_ ||
            (count != 0 && data_ == nullptr))
            return false;
        position_ += count;
        return true;
    }

    std::size_t position() const { return position_; }
    std::size_t remaining() const {
        return position_ <= size_ ? size_ - position_ : 0;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t position_;
};
} // namespace stalker_port
