#pragma once

#include "project.hpp"
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <map>
#include <cstdint>

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
    Pixels read(const fs::path& path) const;
    const Texture* texture(const fs::path& path);
    void clear() { textures_.clear(); }
    void writeIndexed(const fs::path& file, const Pixels& image, const Palette& palette,
                      bool alignWidth = false) const;
private:
    ID3D11Device* device_;
    ComPtr<IWICImagingFactory> factory_;
    std::map<fs::path, Texture> textures_;
};

void countColors(Histogram& histogram, const Pixels& image);
Palette makePalette(const Histogram& histogram);
}
