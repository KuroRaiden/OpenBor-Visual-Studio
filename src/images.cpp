#include "images.hpp"
#include <gdiplus.h>
#include <shlwapi.h>
#include <fstream>

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

struct Images::GifPreview {
    ULONG_PTR startup = 0;
    fs::path path;
    ComPtr<IStream> stream;
    std::unique_ptr<Gdiplus::Image> image;
    std::vector<float> frameEnds;
    UINT width = 0, height = 0, frame = UINT_MAX;
    Texture texture;

    ~GifPreview() {
        image.reset();
        stream.Reset();
        if (startup) Gdiplus::GdiplusShutdown(startup);
    }
};

Images::Images(ID3D11Device* device) : device_(device) {
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&factory_)), "Cannot initialize Windows Imaging Component.");
}

Images::~Images() = default;

void Images::clear() {
    textures_.clear();
    preview_.reset();
    gif_.reset();
}

ComPtr<IWICBitmapDecoder> Images::openImage(const fs::path& path) const {
    const std::string error = "Cannot read image " + utf8(path.filename());
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory_->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnDemand, &decoder), error);
    return decoder;
}

Pixels Images::read(const fs::path& path, bool transparentIndexZero) const {
    return decode(openImage(path).Get(),"Cannot read image "+utf8(path.filename()),transparentIndexZero);
}

ImageSize Images::dimensions(const fs::path& path) const {
    const auto decoder = openImage(path);
    const std::string error = "Cannot read image dimensions for "+utf8(path.filename());
    ComPtr<IWICBitmapFrameDecode> frame;
    check(decoder->GetFrame(0,&frame),error);
    UINT width = 0, height = 0;
    check(frame->GetSize(&width,&height),error);
    if (!width || !height || width > 16384 || height > 16384 || uint64_t(width)*height > 40'000'000)
        throw std::runtime_error("Image dimensions exceed the editor limit.");
    return {int(width),int(height)};
}

Pixels Images::readEmbedded(std::span<const uint8_t> bytes) const {
    const std::string error = "Cannot decode embedded GLB texture.";
    if (bytes.empty() || bytes.size() > UINT_MAX) throw std::runtime_error(error);
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(bytes.data(),UINT(bytes.size())));
    if (!stream) throw std::runtime_error(error);
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory_->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnDemand,&decoder),error);
    return decode(decoder.Get(),error,false);
}

Pixels Images::decode(IWICBitmapDecoder* decoder, const std::string& error, bool transparentIndexZero) const {
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
    if (transparentIndexZero) {
        WICPixelFormatGUID format{};
        check(source->GetPixelFormat(&format),error);
        const UINT depth = format == GUID_WICPixelFormat8bppIndexed ? 8 :
                           format == GUID_WICPixelFormat4bppIndexed ? 4 :
                           format == GUID_WICPixelFormat2bppIndexed ? 2 :
                           format == GUID_WICPixelFormat1bppIndexed ? 1 : 0;
        if (depth) {
            const UINT stride = (width*depth+7)/8;
            std::vector<uint8_t> indices(size_t(stride)*height);
            check(source->CopyPixels(nullptr,stride,UINT(indices.size()),indices.data()),error);
            for (UINT y = 0; y < height; ++y) {
                for (UINT x = 0; x < width; ++x) {
                    const UINT bit = x*depth;
                    const UINT index = (indices[size_t(y)*stride+bit/8] >> (8-depth-bit%8)) & ((1u<<depth)-1);
                    output.rgba[(size_t(y)*width+x)*4+3] = index ? 255 : 0;
                }
            }
        }
    }
    return output;
}

const Texture& Images::animatedGif(const fs::path& path, float seconds, float& duration) {
    const std::string error = "Cannot preview GIF " + utf8(path.filename());
    if (!gif_ || gif_->path != path) {
        auto next = std::make_unique<GifPreview>();
        const auto size = fs::file_size(path);
        if (!size || size > 80'000'000) throw std::runtime_error(error);
        std::ifstream file(path,std::ios::binary);
        std::vector<BYTE> bytes(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))
            throw std::runtime_error(error);
        next->stream.Attach(SHCreateMemStream(bytes.data(),UINT(bytes.size())));
        const Gdiplus::GdiplusStartupInput input;
        if (!next->stream || Gdiplus::GdiplusStartup(&next->startup,&input,nullptr) != Gdiplus::Ok)
            throw std::runtime_error(error);
        next->image.reset(Gdiplus::Image::FromStream(next->stream.Get()));
        GUID format{};
        if (!next->image || next->image->GetRawFormat(&format) != Gdiplus::Ok ||
            format != Gdiplus::ImageFormatGIF) throw std::runtime_error(error);
        next->width = next->image->GetWidth();
        next->height = next->image->GetHeight();
        const UINT count = std::max(1u,next->image->GetFrameCount(&Gdiplus::FrameDimensionTime));
        if (!next->width || !next->height || next->width > 4096 || next->height > 4096 || count > 100'000)
            throw std::runtime_error("GIF dimensions or frame count exceed the preview limit.");
        const auto propertySize = next->image->GetPropertyItemSize(PropertyTagFrameDelay);
        std::vector<uint8_t> property(propertySize);
        const auto item = reinterpret_cast<Gdiplus::PropertyItem*>(property.data());
        const bool hasDelays = propertySize >= sizeof(*item) &&
            next->image->GetPropertyItem(PropertyTagFrameDelay,propertySize,item) == Gdiplus::Ok &&
            item->type == PropertyTagTypeLong;
        const auto delays = hasDelays ? static_cast<const ULONG*>(item->value) : nullptr;
        float end = 0;
        for (UINT i = 0; i < count; ++i) {
            const auto delay = delays && i < item->length/sizeof(ULONG) ? delays[i] : 10;
            end += float(std::max(1ul,delay))*.01f;
            next->frameEnds.push_back(end);
        }
        next->path = path;
        gif_ = std::move(next);
    }
    auto& gif = *gif_;
    duration = gif.frameEnds.size() > 1 ? gif.frameEnds.back() : 0;
    const auto nextFrame = std::upper_bound(gif.frameEnds.begin(),gif.frameEnds.end(),seconds);
    const auto frame = UINT(std::min<size_t>(nextFrame-gif.frameEnds.begin(),gif.frameEnds.size()-1));
    if (gif.frame != frame) {
        if (gif.frameEnds.size() > 1 &&
            gif.image->SelectActiveFrame(&Gdiplus::FrameDimensionTime,frame) != Gdiplus::Ok)
            throw std::runtime_error(error);
        Gdiplus::Bitmap bitmap(INT(gif.width),INT(gif.height),PixelFormat32bppARGB);
        {
            Gdiplus::Graphics graphics(&bitmap);
            graphics.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            if (graphics.DrawImage(gif.image.get(),0,0,INT(gif.width),INT(gif.height)) != Gdiplus::Ok)
                throw std::runtime_error(error);
        }
        Pixels pixels{int(gif.width),int(gif.height),std::vector<uint8_t>(size_t(gif.width)*gif.height*4)};
        Gdiplus::Rect bounds(0,0,pixels.width,pixels.height);
        Gdiplus::BitmapData data{};
        if (bitmap.LockBits(&bounds,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data) != Gdiplus::Ok)
            throw std::runtime_error(error);
        for (UINT y = 0; y < gif.height; ++y) {
            const auto* row = static_cast<const uint8_t*>(data.Scan0)+ptrdiff_t(y)*data.Stride;
            auto* dest = &pixels.rgba[size_t(y)*gif.width*4];
            for (UINT x = 0; x < gif.width; ++x, row+=4, dest+=4) {
                dest[0] = row[2];
                dest[1] = row[1];
                dest[2] = row[0];
                dest[3] = row[3];
            }
        }
        bitmap.UnlockBits(&data);
        gif.texture = createTexture(pixels);
        gif.frame = frame;
    }
    return gif.texture;
}

const Texture* Images::texture(const fs::path& path, bool stageBackground) {
    if (path.empty() || !device_) return nullptr;
    const auto key = std::pair{path,stageBackground};
    const auto found = textures_.find(key);
    if (found != textures_.end()) return found->second.view ? &found->second : nullptr;
    try {
        auto pixels = read(path);
        if (stageBackground) {
            const int width = (pixels.width+3)&~3;
            Pixels padded{width,pixels.height,std::vector<uint8_t>(size_t(width)*pixels.height*4)};
            for (int y = 0; y < pixels.height; ++y) {
                for (int x = 0; x < width; ++x) {
                    auto* pixel = &padded.rgba[(size_t(y)*width+x)*4];
                    if (x < pixels.width) {
                        const auto* source = &pixels.rgba[(size_t(y)*pixels.width+x)*4];
                        if (source[3] >= 128) std::copy_n(source,3,pixel);
                    }
                    pixel[3] = 255;
                }
            }
            pixels = std::move(padded);
        }
        return &textures_.emplace(key,createTexture(pixels)).first->second;
    } catch (const std::exception&) {
        textures_.try_emplace(key);
        return nullptr;
    }
}

Texture Images::createTexture(const Pixels& pixels) {
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
    check(device_->CreateTexture2D(&desc,&sub,&image), "Cannot create texture.");
    Texture tex{pixels.width,pixels.height,{}};
    check(device_->CreateShaderResourceView(image.Get(),nullptr,&tex.view), "Cannot create texture view.");
    return tex;
}

Pixels Images::alphaMask(const Frame& frame, const Pixels& pixels) const {
    const bool external = !frame.effects.mask.empty();
    auto mask = external ? read(frame.effects.mask) : pixels;
    const auto description = utf8(frame.image.filename());
    if (mask.width != pixels.width || mask.height != pixels.height)
        throw std::runtime_error("Alpha mask dimensions must match " + description + ".");
    for (size_t i=0; i<mask.rgba.size(); i+=4) {
        auto* p = &mask.rgba[i];
        if (external && p[3] && (p[0] != p[1] || p[0] != p[2]))
            throw std::runtime_error("Alpha mask for " + description + " must be grayscale.");
        const auto alpha = external ? (p[3] ? p[0] : uint8_t(0)) : pixels.rgba[i+3];
        if (bool(alpha) != bool(pixels.rgba[i+3]))
            throw std::runtime_error("Alpha mask transparency must match the silhouette of " + description + ".");
        p[0] = p[1] = p[2] = alpha;
        p[3] = 255;
    }
    return mask;
}

const Texture* Images::preview(const Frame& frame, const Palette& source, const Palette& colors) {
    const auto& effects = frame.effects;
    const bool masked = effects.embeddedAlpha || !effects.mask.empty();
    if (!device_ || (!masked && source.empty())) return texture(frame.image);
    if (preview_ && preview_->image == frame.image && preview_->mask == effects.mask &&
        preview_->embeddedAlpha == effects.embeddedAlpha && preview_->source == source &&
        preview_->colors == colors) return &preview_->texture;
    auto pixels = read(frame.image);
    if (masked) {
        const auto mask = alphaMask(frame,pixels);
        for (size_t i=0; i<pixels.rgba.size(); i+=4) pixels.rgba[i+3] = mask.rgba[i];
    }
    if (source.size() > 1 && colors.size() == source.size()) {
        for (size_t i=0; i<pixels.rgba.size(); i+=4) {
            auto* p = &pixels.rgba[i];
            if (!p[3]) continue;
            const auto color = colors[closest(source,rgb(p))];
            p[0] = uint8_t(color >> 16);
            p[1] = uint8_t(color >> 8);
            p[2] = uint8_t(color);
        }
    }
    preview_ = Preview{frame.image,effects.mask,effects.embeddedAlpha,source,colors,createTexture(pixels)};
    return &preview_->texture;
}

void countColors(Histogram& histogram, const Pixels& image, uint8_t minAlpha) {
    for (size_t i = 0; i < image.rgba.size(); i += 4) {
        const uint8_t* pixel = &image.rgba[i];
        if (pixel[3] >= minAlpha) ++histogram[rgb(pixel)];
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

Palette makeModelPalette(const Character& model, const Images& images) {
    std::map<fs::path,bool> assets;
    for (const auto& animation : model.animations) {
        for (const auto& frame : animation.frames)
            if (!frame.image.empty()) assets[frame.image] |= frame.effects.embeddedAlpha || !frame.effects.mask.empty();
    }
    Histogram histogram;
    for (const auto& [path,masked] : assets) countColors(histogram,images.read(path),masked ? 1 : 128);
    return makePalette(histogram);
}

void Images::writeIndexed(const fs::path& file, const Pixels& image, const Palette& palette,
                          bool alignWidth, const Palette* colors) const {
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
    writeIndices(file,width,height,indexed,colors ? *colors : palette);
}

void Images::writeMask(const fs::path& file, const Pixels& mask) const {
    Palette shades;
    std::vector<uint8_t> indices(size_t(mask.width)*mask.height);
    for (unsigned i=0; i<256; ++i) shades.push_back((i ? 0xff000000u : 0u) | i*0x010101u);
    for (size_t i=0; i<indices.size(); ++i) indices[i] = mask.rgba[i*4];
    writeIndices(file,UINT(mask.width),UINT(mask.height),indices,shades);
}

void Images::writeIndices(const fs::path& file, UINT width, UINT height, std::vector<uint8_t>& indexed,
                          const Palette& palette) const {
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
