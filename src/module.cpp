#include "project.hpp"
#include "images.hpp"
#include "media.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace studio {

HudLayout::HudLayout() {
    mp.height = 3;
    enemy.height = 3;
    for (size_t i=0; i<players.size(); ++i) {
        const int x = i%2 ? 200 : 24;
        const int y = i<2 ? 4 : 204;
        players[i] = {{x,y+10},{x,y+18},{x-20,y},{x,y},{x,y+24}};
    }
}

std::string moduleModelsText(const ModuleSettings& module) {
    std::ostringstream out;
    if (module.select.enabled) {
        out << "colourselect " << int(module.select.colorSelect) << "\nspdirection";
        for (const auto& slot : module.select.slots) out << ' ' << int(slot.right);
        out << '\n';
    }
    if (!module.modelsHeader.empty()) out << module.modelsHeader << '\n';
    return out.str();
}

std::string moduleLevelsText(const ModuleSettings& module) {
    std::ostringstream out;
    const auto& hud = module.hud;
    const auto pair = [&](const std::string& command, const ScreenPoint& p) {
        out << command << ' ' << p.x << ' ' << p.y << '\n';
    };
    if (hud.enabled) {
        const auto gauge = [&](const char* command,const Gauge& g) {
            out << command << ' ' << g.width << ' ' << g.height << ' ' << int(g.noBorder)
                << ' ' << int(g.ratio) << ' ' << int(g.vertical) << ' ' << g.border << ' '
                << g.shadow << ' ' << g.fill << ' ' << g.backfill << '\n';
        };
        gauge("lbarsize",hud.life);
        gauge("mpbarsize",hud.mp);
        gauge("olbarsize",hud.enemy);
        out << "timeloc " << hud.timer.x << ' ' << hud.timer.y << ' ' << hud.timerWidth << ' '
            << hud.timerHeight << ' ' << int(!hud.timerBorder) << ' ' << hud.timerFont << '\n';
        if (!hud.overlay.empty()) pair("bgicon data/sprites/studio_hud.png",hud.overlayPosition);
        for (size_t i=0; i<hud.players.size(); ++i) {
            const auto n = std::to_string(i+1);
            const auto p = "p"+n;
            const auto& row = hud.players[i];
            pair(p+"life",row.life);
            pair(p+"mp",row.mp);
            pair(p+"icon",row.icon);
            pair(p+"score",row.score);
            pair("e"+n+"life",row.enemy);
        }
    }
    if (module.select.enabled) {
        for (size_t i=0; i<module.select.slots.size(); ++i) {
            const auto& row = module.select.slots[i];
            out << 'p' << i+1 << "smenu " << row.actor.x << ' ' << row.actor.y << ' '
                << row.ready.x << ' ' << row.ready.y << '\n';
        }
    }
    if (!module.levelsHeader.empty()) out << module.levelsHeader << '\n';
    return out.str();
}

namespace {

void exportImage(Images& images, const fs::path& file, const Pixels& pixels, bool align = false) {
    Histogram colors;
    countColors(colors,pixels);
    images.writeIndexed(file,pixels,makePalette(colors),align);
}

Pixels screenBackground(const Pixels& source) {
    constexpr int width = 320, height = 240;
    if (source.width == width && source.height == height) return source;
    Pixels result{width,height,std::vector<uint8_t>(width*height*4)};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t from = (size_t(y*source.height/height)*source.width+x*source.width/width)*4;
            std::copy_n(source.rgba.data()+from,4,result.rgba.data()+(y*width+x)*4);
        }
    }
    return result;
}

} // namespace

Pixels starterBackground(bool title) {
    Pixels output{320,240,std::vector<uint8_t>(320*240*4)};
    for (int y=0; y<240; ++y) {
        for (int x=0; x<320; ++x) {
            auto* pixel = &output.rgba[(y*320+x)*4];
            const int band = y/16;
            const bool edge = title && (y == 71 || y == 72 || y == 163 || y == 164);
            pixel[0] = uint8_t(edge ? 225 : 17 + band);
            pixel[1] = uint8_t(edge ? 146 : 27 + band);
            pixel[2] = uint8_t(edge ? 72 : 42 + band);
            pixel[3] = 255;
        }
    }
    return output;
}

namespace {

Pixels starterFont() {
    constexpr int cellWidth = 8, cellHeight = 13;
    constexpr int width = 16*cellWidth, height = 16*cellHeight;
    const auto dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bitmap{};
    bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth = width;
    bitmap.bmiHeader.biHeight = -height;
    bitmap.bmiHeader.biPlanes = 1;
    bitmap.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    const auto surface = CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&bits,nullptr,0);
    const auto font = CreateFontW(-12,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,
                                  FIXED_PITCH | FF_MODERN,L"Consolas");
    if (!dc || !surface || !font || !bits) {
        if (font) DeleteObject(font);
        if (surface) DeleteObject(surface);
        if (dc) DeleteDC(dc);
        throw std::runtime_error("Could not generate the OpenBOR starter font.");
    }
    const auto previousSurface = SelectObject(dc,surface);
    const auto previousFont = SelectObject(dc,font);
    PatBlt(dc,0,0,width,height,BLACKNESS);
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,RGB(249,244,231));
    for (int i=33; i<256; ++i) {
        const auto c = wchar_t(i);
        const int x = i%16*cellWidth, y = i/16*cellHeight;
        const RECT cell{x,y,x+cellWidth,y+cellHeight};
        ExtTextOutW(dc,x,y,ETO_CLIPPED,&cell,&c,1,nullptr);
    }
    GdiFlush();
    Pixels result{width,height,std::vector<uint8_t>(width*height*4)};
    const auto* buffer = static_cast<const uint8_t*>(bits);
    for (size_t p=0; p<result.rgba.size(); p+=4) {
        result.rgba[p] = buffer[p+2];
        result.rgba[p+1] = buffer[p+1];
        result.rgba[p+2] = buffer[p];
        result.rgba[p+3] = (buffer[p] || buffer[p+1] || buffer[p+2]) ? 255 : 0;
    }
    // The engine uses glyph zero's palette for all other glyphs.
    result.rgba[(width+1)*4+0] = 249;
    result.rgba[(width+1)*4+1] = 244;
    result.rgba[(width+1)*4+2] = 231;
    result.rgba[(width+1)*4+3] = 255;
    SelectObject(dc,previousFont);
    SelectObject(dc,previousSurface);
    DeleteObject(font);
    DeleteObject(surface);
    DeleteDC(dc);
    return result;
}

} // namespace

Pixels screenFontPixels(const ScreenFont& font, const Images& images) {
    auto pixels = font.bitmap.empty() ? starterFont() : images.read(font.bitmap,true);
    if (pixels.width < 16 || pixels.height < 16 || pixels.width%16 || pixels.height%16 ||
        pixels.width > 1024 || pixels.height > 1024)
        throw std::runtime_error("A font sheet needs a 16 × 16 glyph grid and dimensions up to 1024 pixels.");
    if (font.bitmap.empty() || font.tintBitmap) {
        int brightest = 1;
        for (size_t i = 0; i < pixels.rgba.size(); i+=4) {
            if (pixels.rgba[i+3] >= 128)
                brightest = std::max(brightest,int(std::max({pixels.rgba[i],pixels.rgba[i+1],pixels.rgba[i+2]})));
        }
        for (size_t i = 0; i < pixels.rgba.size(); i+=4) {
            if (pixels.rgba[i+3] < 128) continue;
            const int shade = std::min(brightest,int(std::max({pixels.rgba[i],pixels.rgba[i+1],pixels.rgba[i+2]})));
            for (int channel = 0; channel < 3; ++channel) {
                const int component = (font.color >> (16-8*channel)) & 255;
                pixels.rgba[i+channel] = uint8_t((component*shade+brightest/2)/brightest);
            }
        }
    }
    return pixels;
}

Screens loadScreenTemplate(const fs::path& folder) {
    Screens result;
    const auto require = [&](const fs::path& relative) {
        const auto file = folder/relative;
        if (!fs::is_regular_file(file)) throw std::runtime_error("Missing screen template asset: "+utf8(relative));
        return file;
    };
    for (size_t i=0; i<result.backgrounds.size(); ++i)
        result.backgrounds[i] = require(fs::path("bgs")/(std::string(screenBackgroundNames[i])+".gif"));
    for (size_t i=0; i<result.fonts.size(); ++i)
        result.fonts[i].bitmap = require(fs::path("sprites")/(std::string(screenFontNames[i])+".gif"));
    result.scenes[0].file = require("scenes/senile.gif");
    result.scenes[1].file = require("scenes/newintro.gif");
    result.scenes[2].file = require("scenes/howto.gif");
    result.music = require("music/menu.bor");
    return result;
}

void exportModuleArtwork(const Project& project, const fs::path& root, Images& images, bool playable) {
    const auto& hud = project.module.hud;
    const auto& select = project.module.select;
    if (hud.enabled && !hud.overlay.empty())
        exportImage(images,root/"data/sprites/studio_hud.png",images.read(hud.overlay));
    if (select.enabled && !select.background.empty())
        exportImage(images,root/"data/bgs/select.png",images.read(select.background),true);
    const auto& screens = project.module.screens;
    for (size_t i = 0; i < screens.backgrounds.size(); ++i) {
        const auto& source = screens.backgrounds[i];
        if (source.empty() && !playable) continue;
        exportImage(images,root/"data/bgs"/(std::string(screenBackgroundNames[i])+".png"),
                    source.empty() ? starterBackground(i != 0) : screenBackground(images.read(source)));
    }
    if (playable) {
        const auto back = starterBackground(false);
        for (const char* name : {"select","complete","gameover","hiscore","loading2","unlockbg"}) {
            const auto file = root/"data/bgs"/(std::string(name)+".png");
            if (!fs::exists(file)) exportImage(images,file,back);
        }
    }
    const Screens defaults;
    for (size_t i = 0; i < screens.fonts.size(); ++i) {
        if (playable || screens.fonts[i] != defaults.fonts[i])
            exportImage(images,root/"data/sprites"/(std::string(screenFontNames[i])+".png"),
                        screenFontPixels(screens.fonts[i],images));
    }
    if (!screens.music.empty()) {
        const auto extension = lower(utf8(screens.music.extension()));
        if (extension != ".bor" && extension != ".ogg")
            throw std::runtime_error("Menu music must be an OpenBOR .bor or .ogg file.");
        copyMedia(screens.music,root/"data/music"/("menu"+extension));
    }
    for (size_t i = 0; i < screens.scenes.size(); ++i) {
        exportScene(screens.scenes[i],root,screenSceneNames[i]);
    }
}

fs::path writePlayablePak(const fs::path& root, const std::string& title) {
    std::vector<fs::path> assets;
    for (const auto& item : fs::recursive_directory_iterator(root/"data")) {
        if (item.is_regular_file()) assets.push_back(item.path());
    }
    std::sort(assets.begin(),assets.end());
    const auto token = modelToken(title);
    const auto path = root/"Paks"/pathFromUtf8((token.empty() ? "openbor_studio" : token)+".pak");
    fs::create_directories(path.parent_path());
    std::ofstream pak(path,std::ios::binary);
    uint64_t total = 12;
    for (const auto& asset : assets)
        total += fs::file_size(asset) + 13 + utf8(asset.lexically_relative(root)).size();
    const bool wide = total > uint64_t(std::numeric_limits<int32_t>::max());
    const int fieldWidth = wide ? 8 : 4;
    const auto number = [&](uint64_t n, int bytes) {
        for (int i=0; i<bytes; ++i) pak.put(char(n>>(8*i)));
    };
    pak.write("PACK",4);
    number(wide ? 1 : 0,4);
    struct ArchiveEntry { std::string name; uint64_t offset, size; };
    std::vector<ArchiveEntry> entries;
    uint64_t position = 8;
    for (const auto& asset : assets) {
        const auto size = fs::file_size(asset);
        std::ifstream input(asset,std::ios::binary);
        if (!input) throw std::runtime_error("Could not read "+utf8(asset.filename())+" for the PAK.");
        auto name = utf8(asset.lexically_relative(root));
        std::replace(name.begin(),name.end(),'\\','/');
        entries.push_back({std::move(name),position,size});
        position += size;
        if (size) pak << input.rdbuf();
        if (!pak || uint64_t(pak.tellp()) != position)
            throw std::runtime_error("Could not pack "+utf8(asset.filename())+".");
    }
    const auto table = position;
    for (const auto& entry : entries) {
        number(5+2*fieldWidth+entry.name.size(),4);
        number(entry.offset,fieldWidth);
        number(entry.size,fieldWidth);
        pak.write(entry.name.c_str(),std::streamsize(entry.name.size()+1));
    }
    number(table,fieldWidth);
    pak.close();
    if (!pak) throw std::runtime_error("Could not write "+utf8(path.filename())+".");
    return path;
}

}
