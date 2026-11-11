#include "images.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <unordered_map>

namespace studio {
namespace {

void check(HRESULT result, const std::string& operation) {
    if (FAILED(result)) throw std::runtime_error(operation);
}

uint32_t rgb(const uint8_t* pixel) {
    return (uint32_t(pixel[0]) << 16) | (uint32_t(pixel[1]) << 8) | pixel[2];
}

int channel(uint32_t color, int index) { return int(color >> (index * 8) & 255); }

struct ColorCount { uint32_t color; uint64_t count; };

struct ColorRange {
    size_t begin, end;
    uint64_t population = 0;
    int axis = 0, spread = 0;
};

ColorRange measure(const std::vector<ColorCount>& colors, size_t begin, size_t end) {
    ColorRange range{begin, end};
    std::array<int,3> low{255,255,255}, high{};
    for (size_t i = begin; i < end; ++i) {
        range.population += colors[i].count;
        for (int c = 0; c < 3; ++c) {
            const auto index = size_t(c);
            const int value = channel(colors[i].color, c);
            low[index] = std::min(low[index], value);
            high[index] = std::max(high[index], value);
        }
    }
    for (int c = 0; c < 3; ++c) {
        const auto index = size_t(c);
        if (high[index] - low[index] > range.spread) {
            range.spread = high[index] - low[index];
            range.axis = c;
        }
    }
    return range;
}

uint8_t closest(const Palette& palette, uint32_t color) {
    int distance = 200000;
    size_t selected = 1;
    for (size_t i = 1; i < palette.size(); ++i) {
        int sum = 0;
        for (int c = 0; c < 3; ++c) {
            const int delta = channel(color, c) - channel(palette[i], c);
            sum += delta * delta;
        }
        if (sum < distance) {
            distance = sum;
            selected = i;
            if (sum == 0) break;
        }
    }
    return uint8_t(selected);
}

} // namespace

Images::Images(ID3D11Device* device) : device_(device) {
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&factory_)), "Cannot initialize Windows Imaging Component.");
}

Pixels Images::read(const fs::path& path) const {
    const std::string error = "Cannot read image " + utf8(path.filename());
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory_->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnDemand, &decoder), error);
    ComPtr<IWICBitmapFrameDecode> source;
    check(decoder->GetFrame(0, &source), error);
    UINT width = 0, height = 0;
    check(source->GetSize(&width, &height), error);
    if (!width || !height || width > 16384 || height > 16384 ||
        uint64_t(width) * height > 40'000'000) throw std::runtime_error("Image dimensions exceed the editor limit.");
    ComPtr<IWICFormatConverter> converter;
    check(factory_->CreateFormatConverter(&converter), error);
    check(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppRGBA,
               WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom), error);
    Pixels output{int(width), int(height), std::vector<uint8_t>(size_t(width) * height * 4)};
    check(converter->CopyPixels(nullptr, width*4, UINT(output.rgba.size()), output.rgba.data()), error);
    return output;
}

const Texture* Images::texture(const fs::path& path) {
    if (path.empty() || !device_) return nullptr;
    const auto found = textures_.find(path);
    if (found != textures_.end()) return found->second.view ? &found->second : nullptr;
    try {
        const auto pixels = read(path);
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = UINT(pixels.width);
        desc.Height = UINT(pixels.height);
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sub{pixels.rgba.data(), desc.Width * 4, 0};
        ComPtr<ID3D11Texture2D> image;
        check(device_->CreateTexture2D(&desc, &sub, &image), "Cannot create texture.");
        Texture tex{pixels.width, pixels.height, {}};
        check(device_->CreateShaderResourceView(image.Get(), nullptr, &tex.view), "Cannot create texture view.");
        return &textures_.emplace(path, std::move(tex)).first->second;
    } catch (const std::exception&) {
        textures_.try_emplace(path);
        return nullptr;
    }
}

void countColors(Histogram& histogram, const Pixels& image) {
    for (size_t i = 0; i < image.rgba.size(); i += 4) {
        const uint8_t* pixel = &image.rgba[i];
        if (pixel[3] >= 128) ++histogram[rgb(pixel)];
    }
}

Palette makePalette(const Histogram& histogram) {
    Palette palette{0};
    if (histogram.size() <= 255) {
        for (const auto& [color, count] : histogram) {
            (void)count;
            palette.push_back(0xff000000 | color);
        }
    } else {
        std::vector<ColorCount> colors;
        for (const auto& [color, count] : histogram) colors.push_back({color, count});
        std::vector<ColorRange> ranges{measure(colors, 0, colors.size())};
        while (ranges.size() < 255) {
            const auto weight = [](const ColorRange& r) { return r.population * uint64_t(r.spread); };
            auto best = std::max_element(ranges.begin(), ranges.end(),
                [&](const ColorRange& a, const ColorRange& b) { return weight(a) < weight(b); });
            if (best->spread == 0) break;
            const ColorRange range = *best;
            std::sort(colors.begin() + ptrdiff_t(range.begin), colors.begin() + ptrdiff_t(range.end),
                      [&](const ColorCount& a, const ColorCount& b) {
                          return channel(a.color, range.axis) < channel(b.color, range.axis);
                      });
            uint64_t total = colors[range.begin].count;
            size_t mid = range.begin + 1;
            while (mid + 1 < range.end && total < range.population / 2) total += colors[mid++].count;
            *best = measure(colors, range.begin, mid);
            ranges.push_back(measure(colors, mid, range.end));
        }
        for (const auto& range : ranges) {
            uint32_t color = 0xff000000;
            for (int c = 0; c < 3; ++c) {
                uint64_t sum = 0;
                for (size_t i = range.begin; i < range.end; ++i) {
                    sum += uint64_t(channel(colors[i].color, c)) * colors[i].count;
                }
                color |= uint32_t(sum / range.population) << (c * 8);
            }
            palette.push_back(color);
        }
    }
    if (palette.size() == 1) palette.push_back(0xff000000);
    return palette;
}

void Images::writeIndexed(const fs::path& file, const Pixels& image, const Palette& palette,
                          bool alignWidth) const {
    const UINT width = UINT(alignWidth ? (image.width + 3) & ~3 : image.width);
    const UINT height = UINT(image.height);
    std::vector<uint8_t> indexed(size_t(width) * height, 0);
    std::unordered_map<uint32_t,uint8_t> lookup;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const uint8_t* pixel = &image.rgba[(size_t(y) * image.width + x) * 4];
            if (pixel[3] < 128) continue;
            const uint32_t color = rgb(pixel);
            const auto [it, inserted] = lookup.try_emplace(color);
            if (inserted) it->second = closest(palette, color);
            indexed[size_t(y) * width + x] = it->second;
        }
    }
    fs::create_directories(file.parent_path());
    const std::string error = "Cannot write OpenBOR PNG " + utf8(file.filename());
    ComPtr<IWICStream> stream;
    check(factory_->CreateStream(&stream), error);
    check(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE), error);
    ComPtr<IWICBitmapEncoder> encoder;
    check(factory_->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), error);
    check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), error);
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    check(encoder->CreateNewFrame(&frame, &properties), error);
    check(frame->Initialize(properties.Get()), error);
    check(frame->SetSize(width, height), error);
    WICPixelFormatGUID format = GUID_WICPixelFormat8bppIndexed;
    check(frame->SetPixelFormat(&format), error);
    if (format != GUID_WICPixelFormat8bppIndexed) throw std::runtime_error("PNG encoder rejected indexed output.");
    Palette padded = palette;
    padded.resize(256, padded.back());
    ComPtr<IWICPalette> nativePalette;
    check(factory_->CreatePalette(&nativePalette), error);
    check(nativePalette->InitializeCustom(padded.data(), UINT(padded.size())), error);
    check(frame->SetPalette(nativePalette.Get()), error);
    check(frame->WritePixels(height, width, UINT(indexed.size()), indexed.data()), error);
    check(frame->Commit(), error);
    check(encoder->Commit(), error);
}
}
