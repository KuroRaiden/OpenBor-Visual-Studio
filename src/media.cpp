#include "media.hpp"

#include <fstream>
#include <stdexcept>

namespace studio {
namespace {
struct Element {
    uint64_t id, end, size;
};

uint64_t vint(std::istream& in, bool id, bool& unknown) {
    const auto first = in.get();
    int mask = 128, count = 1;
    while (mask && !(first & mask)) { mask >>= 1; ++count; }
    if (first == EOF || !mask || (id && count > 4)) throw std::runtime_error("Invalid WebM header.");
    uint64_t value = id ? first : first & (mask-1);
    unknown = !id && value == uint64_t(mask-1);
    for (int i = 1; i < count; ++i) {
        const int byte = in.get();
        if (byte == EOF) throw std::runtime_error("Incomplete WebM header.");
        value = (value << 8) | uint64_t(byte);
        unknown = unknown && byte == 255;
    }
    return value;
}

Element element(std::istream& in, uint64_t limit) {
    bool unknown;
    const auto id = vint(in,true,unknown);
    const auto size = vint(in,false,unknown);
    const auto start = uint64_t(in.tellg());
    if (start > limit || (!unknown && size > limit-start) || (unknown && id != 0x18538067))
        throw std::runtime_error("Invalid WebM element size.");
    return {id,unknown ? limit : start+size,size};
}

std::string field(std::istream& in, uint64_t size) {
    if (size > 64) throw std::runtime_error("Invalid WebM format identifier.");
    std::string value(size,'\0');
    if (!in.read(value.data(),std::streamsize(size))) throw std::runtime_error("Incomplete WebM header.");
    return value;
}

void validateWebm(std::istream& in, uint64_t length) {
    const auto header = element(in,length);
    if (header.id != 0x1a45dfa3) throw std::runtime_error("Choose a WebM container.");
    bool webm = false;
    while (uint64_t(in.tellg()) < header.end) {
        const auto info = element(in,header.end);
        if (info.id == 0x4282) webm = field(in,info.size) == "webm";
        in.seekg(std::streamoff(info.end));
    }
    if (!webm) throw std::runtime_error("Choose a WebM container.");
    while (uint64_t(in.tellg()) < length) {
        const auto segment = element(in,length);
        if (segment.id == 0x18538067) {
            while (uint64_t(in.tellg()) < segment.end) {
                const auto entry = element(in,segment.end);
                if (entry.id == 0x1654ae6b) {
                    bool video = false;
                    while (uint64_t(in.tellg()) < entry.end) {
                        const auto track = element(in,entry.end);
                        if (track.id == 0xae) {
                            std::string type, codec;
                            while (uint64_t(in.tellg()) < track.end) {
                                const auto property = element(in,track.end);
                                if (property.id == 0xd7) type = field(in,property.size);
                                if (property.id == 0x86) codec = field(in,property.size);
                                in.seekg(std::streamoff(property.end));
                            }
                            if (type == "\x01") {
                                if (codec != "V_VP8") throw std::runtime_error("OpenBOR WebM video must use VP8.");
                                video = true;
                            }
                            if (type == "\x02" && codec != "A_VORBIS")
                                throw std::runtime_error("OpenBOR WebM audio must use Vorbis.");
                        }
                        in.seekg(std::streamoff(track.end));
                    }
                    if (!video) throw std::runtime_error("The WebM has no VP8 video track.");
                    return;
                }
                in.seekg(std::streamoff(entry.end));
            }
        }
        in.seekg(std::streamoff(segment.end));
    }
    throw std::runtime_error("The WebM has no tracks.");
}
}

std::string mediaExtension(const fs::path& file) { return lower(utf8(file.extension())); }

void validateAudio(const fs::path& file) {
    std::ifstream in(file,std::ios::binary);
    if (!in) throw std::runtime_error("Could not open " + utf8(file.filename()) + ".");
    const auto extension = mediaExtension(file);
    if (extension == ".wav") {
        char bytes[12];
        if (in.read(bytes,12) && std::string_view(bytes,4) == "RIFF" &&
            std::string_view(bytes+8,4) == "WAVE") return;
    } else if (extension == ".ogg" || extension == ".oga") {
        std::string bytes(65536,'\0');
        in.read(bytes.data(),std::streamsize(bytes.size()));
        bytes.resize(size_t(in.gcount()));
        if (bytes.starts_with("OggS") && bytes.find("\x01vorbis") != std::string::npos) return;
    }
    throw std::runtime_error("Choose a WAV or Ogg Vorbis file: " + utf8(file.filename()));
}

void validateScene(const fs::path& file) {
    std::ifstream in(file,std::ios::binary);
    if (!in) throw std::runtime_error("Could not open " + utf8(file.filename()) + ".");
    const auto extension = mediaExtension(file);
    if (extension == ".webm") {
        validateWebm(in,fs::file_size(file));
        return;
    }
    char bytes[6];
    if (extension == ".gif" && in.read(bytes,6)) {
        const std::string_view magic(bytes,6);
        if (magic == "GIF87a" || magic == "GIF89a") return;
    }
    throw std::runtime_error("Choose an animated GIF or a VP8 WebM.");
}

void copyMedia(const fs::path& source, const fs::path& output) {
    fs::create_directories(output.parent_path());
    fs::copy_file(source,output,fs::copy_options::overwrite_existing);
}

void exportScene(const ScreenScene& scene, const fs::path& root, const std::string& name, const std::string& folder) {
    if (scene.file.empty()) return;
    validateScene(scene.file);
    const auto extension = mediaExtension(scene.file);
    const auto clip = folder+name+extension;
    copyMedia(scene.file,root/pathFromUtf8(clip));
    std::ofstream script(root/pathFromUtf8(folder+name+".txt"),std::ios::binary);
    if (extension == ".gif") {
        script << "animation " << clip << ' ' << scene.position.x << ' ' << scene.position.y
               << " 0 " << int(!scene.skippable) << '\n';
    } else {
        script << "video " << clip << " 0 " << int(!scene.skippable) << '\n';
    }
    script.close();
    if (!script) throw std::runtime_error("Could not write the "+name+" scene.");
}
}
