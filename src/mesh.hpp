#pragma once

#include "images.hpp"

namespace studio {

struct MeshClip {
    std::string name;
    std::string exportName;
    float seconds = 0;
};

struct MeshView {
    int width, height;
    float originX, originY, zoom;
    bool mirrored;
    bool operator==(const MeshView&) const = default;
};

bool validMeshAxes(MeshAxis up, MeshAxis forward);
std::vector<Frame> meshTimingFrames(float seconds);

class GlbModel {
public:
    GlbModel(const fs::path& path, const Images& images);
    ~GlbModel();

    const std::vector<MeshClip>& clips() const;
    std::array<float,3> dimensions(const MeshSettings& settings) const;
    void write(const fs::path& destination, const MeshSettings& settings) const;
    Pixels render(const MeshSettings& settings, int clip, float phase, const MeshView& view) const;

private:
    struct Data;
    std::unique_ptr<Data> data_;
};

}
