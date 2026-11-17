#pragma once

#include <gli/gli.hpp>
#include <gli/core/bc.hpp>
#include <glm/gtc/packing.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

namespace xray::render::ios_gles
{
inline bool requiresDDSConversion(gli::format format)
{
    // These desktop BC families are not part of OpenGL ES 3.0 on iOS.
    return gli::is_s3tc_compressed(format) ||
        (format >= gli::FORMAT_R_ATI1N_UNORM_BLOCK8 && format <= gli::FORMAT_RG_ATI2N_SNORM_BLOCK16);
}

namespace detail
{
template <typename Block, auto Decode>
bool decodeDDS(const gli::texture& source, gli::texture& result, bool opaque, bool signedPixels)
{
    for (std::size_t layer = 0; layer < source.layers(); ++layer)
    for (std::size_t face = 0; face < source.faces(); ++face)
    for (std::size_t mip = 0; mip < source.levels(); ++mip)
    {
        const auto extent = source.extent(mip);
        const std::size_t width = extent.x;
        const std::size_t height = extent.y;
        const std::size_t depth = extent.z;
        const std::size_t blockColumns = (width + 3) / 4;
        const std::size_t blockRows = (height + 3) / 4;
        if (source.size(mip) != blockColumns * blockRows * depth * sizeof(Block))
            return false;
        const auto* input = static_cast<const std::uint8_t*>(source.data(layer, face, mip));
        auto* output = result.data<std::uint32_t>(layer, face, mip);

        for (std::size_t z = 0; z < depth; ++z)
        for (std::size_t y = 0; y < height; y += 4)
        for (std::size_t x = 0; x < width; x += 4)
        {
            Block block;
            std::memcpy(&block, input, sizeof(block));
            input += sizeof(block);
            const auto pixels = Decode(block);
            const auto rows = std::min<std::size_t>(4, height - y);
            const auto columns = std::min<std::size_t>(4, width - x);
            for (std::size_t row = 0; row < rows; ++row)
            for (std::size_t column = 0; column < columns; ++column)
            {
                auto pixel = pixels.Texel[row][column];
                if (opaque)
                    pixel.a = 1.0f;
                output[(z * height + y + row) * width + x + column] =
                    signedPixels ? glm::packSnorm4x8(pixel) : glm::packUnorm4x8(pixel);
            }
        }
    }
    return true;
}
} // namespace detail

inline gli::texture expandDDS(const gli::texture& source)
{
    if (source.empty() || !requiresDDSConversion(source.format()))
        return {};

    // BC1 expands at most eightfold. Limit transient allocation per texture.
    if (source.size() > 16u * 1024u * 1024u)
        return {};

    const auto format = source.format();
    const bool signedPixels = format == gli::FORMAT_R_ATI1N_SNORM_BLOCK8 ||
        format == gli::FORMAT_RG_ATI2N_SNORM_BLOCK16;
    const auto destination = signedPixels ? gli::FORMAT_RGBA8_SNORM_PACK8 :
        gli::is_srgb(format) ? gli::FORMAT_RGBA8_SRGB_PACK8 : gli::FORMAT_RGBA8_UNORM_PACK8;
    gli::texture result(source.target(), destination, source.extent(),
        source.layers(), source.faces(), source.levels(), source.swizzles());

    using namespace gli::detail;
    bool ok = false;
    switch (format)
    {
    case gli::FORMAT_RGB_DXT1_UNORM_BLOCK8:
    case gli::FORMAT_RGB_DXT1_SRGB_BLOCK8:
        ok = detail::decodeDDS<dxt1_block, decompress_dxt1_block>(source, result, true, false);
        break;
    case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:
    case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8:
        ok = detail::decodeDDS<dxt1_block, decompress_dxt1_block>(source, result, false, false);
        break;
    case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16:
    case gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16:
        ok = detail::decodeDDS<dxt3_block, decompress_dxt3_block>(source, result, false, false);
        break;
    case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
    case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16:
        ok = detail::decodeDDS<dxt5_block, decompress_dxt5_block>(source, result, false, false);
        break;
    case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:
        ok = detail::decodeDDS<bc4_block, decompress_bc4unorm_block>(source, result, false, false);
        break;
    case gli::FORMAT_R_ATI1N_SNORM_BLOCK8:
        ok = detail::decodeDDS<bc4_block, decompress_bc4snorm_block>(source, result, false, true);
        break;
    case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:
        ok = detail::decodeDDS<bc5_block, decompress_bc5unorm_block>(source, result, false, false);
        break;
    case gli::FORMAT_RG_ATI2N_SNORM_BLOCK16:
        ok = detail::decodeDDS<bc5_block, decompress_bc5snorm_block>(source, result, false, true);
        break;
    default:
        break;
    }
    return ok ? result : gli::texture{};
}
} // namespace xray::render::ios_gles
