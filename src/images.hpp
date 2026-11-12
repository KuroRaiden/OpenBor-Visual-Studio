#pragma once

#include "project.hpp"
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <map>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace studio {
using Microsoft::WRL::ComPtr;

struct Pixels {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
};

struct Texture {
    int width = 0, height = 0;
    ComPtr<ID3D11ShaderResourceView> view;
};

using Histogram = std::map<uint32_t, uint64_t>;
using Palette = std::vector<WICColor>;

class Images {
public:
    explicit Images(ID3D11Device* device = nullptr);
    ~Images();
    Pixels read(const fs::path& path, bool transparentIndexZero = false) const;
    ImageSize dimensions(const fs::path& path) const;
    Pixels readEmbedded(std::span<const uint8_t> bytes) const;
    const Texture* texture(const fs::path& path, bool stageBackground = false);
    const Texture* preview(const Frame& frame, const Palette& source, const Palette& colors);
    Texture createTexture(const Pixels& pixels);
    const Texture& animatedGif(const fs::path& path, float seconds, float& duration);
    Pixels alphaMask(const Frame& frame, const Pixels& pixels) const;
    void clear();
    void writeIndexed(const fs::path& file, const Pixels& image, const Palette& palette,
                      bool alignWidth = false, const Palette* colors = nullptr) const;
    void writeMask(const fs::path& file, const Pixels& mask) const;
private:
    struct GifPreview;
    ComPtr<IWICBitmapDecoder> openImage(const fs::path& path) const;
    Pixels decode(IWICBitmapDecoder* decoder, const std::string& error, bool transparentIndexZero) const;
    struct Preview {
        fs::path image, mask;
        bool embeddedAlpha;
        Palette source, colors;
        Texture texture;
    };
    void writeIndices(const fs::path& file, UINT width, UINT height, std::vector<uint8_t>& indices,
                      const Palette& palette) const;
    ID3D11Device* device_;
    ComPtr<IWICImagingFactory> factory_;
    std::map<std::pair<fs::path,bool>, Texture> textures_;
    std::optional<Preview> preview_;
    std::unique_ptr<GifPreview> gif_;
};

Pixels starterBackground(bool title);
Pixels screenFontPixels(const ScreenFont& font, const Images& images);
void countColors(Histogram& histogram, const Pixels& image, uint8_t minAlpha = 128);
Palette makePalette(const Histogram& histogram);
Palette makeModelPalette(const Character& model, const Images& images);
}
