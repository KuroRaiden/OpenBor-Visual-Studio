#include "mesh.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <numbers>
#include <set>
#include <stdexcept>
#include <nlohmann/json.hpp>

#pragma warning(push, 0)
#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>
#pragma warning(pop)

namespace studio {
namespace {

constexpr size_t maxGlbBytes = 32*1024*1024, maxRigNodes = 1024, maxTriangles = 100'000;
using Matrix = std::array<float,16>;
constexpr Matrix identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
using Point = std::array<float,3>;

void require(bool ok, const char* error) {
    if (!ok) throw std::runtime_error(error);
}

Point axis(MeshAxis value) {
    Point result{};
    const int i = int(value);
    if (i >= 0 && i < 6) result[i%3] = i < 3 ? 1.f : -1.f;
    return result;
}

Point cross(Point a, Point b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}

float dot(Point a, Point b) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}

Point transform(const Matrix& m, Point p) {
    return {m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],
            m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],
            m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]};
}

Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix result{};
    for (size_t c = 0; c < 16; c+=4)
        for (size_t r = 0; r < 4; ++r)
            for (size_t j = 0; j < 4; ++j)
                result[c+r] += a[j*4+r]*b[c+j];
    return result;
}

Matrix orientation(const MeshSettings& settings, bool withYaw) {
    require(validMeshAxes(settings.up,settings.forward),"Choose different up and forward axes.");
    const auto up = axis(settings.up), front = axis(settings.forward), right = cross(up,front);
    constexpr float rad = std::numbers::pi_v<float>/180;
    const float p = settings.pitch*rad, r = settings.roll*rad, y = settings.yaw*rad;
    const float sp = std::sin(p), cp = std::cos(p), sr = std::sin(r), cr = std::cos(r);
    const Matrix basis{right[0],up[0],front[0],0, right[1],up[1],front[1],0, right[2],up[2],front[2],0, 0,0,0,1};
    const Matrix pitch{1,0,0,0, 0,cp,sp,0, 0,-sp,cp,0, 0,0,0,1};
    const Matrix roll{cr,sr,0,0, -sr,cr,0,0, 0,0,1,0, 0,0,0,1};
    const auto baked = multiply(roll,multiply(pitch,basis));
    if (!withYaw) return baked;
    const float sy = std::sin(y), cy = std::cos(y);
    const Matrix yaw{cy,0,-sy,0, 0,1,0,0, sy,0,cy,0, 0,0,0,1};
    return multiply(yaw,baked);
}

bool clipToken(std::string_view name) {
    return !name.empty() && name.size() <= 120 && std::all_of(name.begin(),name.end(),[](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-' || c == '.';
    });
}

struct Vertex {
    Point position{};
    std::array<float,2> uv{};
    std::array<unsigned,4> joints{};
    std::array<float,4> weights{};
};

struct Part {
    const cgltf_skin* skin = nullptr;
    std::vector<Vertex> vertices;
    std::vector<unsigned> indices;
    std::vector<Matrix> bind;
    const Pixels* texture = nullptr;
    cgltf_wrap_mode wrapS = cgltf_wrap_mode_repeat, wrapT = cgltf_wrap_mode_repeat;
    std::array<float,4> tint{1,1,1,1};
    float cutoff = 0;
};

bool activeNode(const cgltf_node& node, const cgltf_scene& scene) {
    const auto* root = &node;
    while (root->parent) root = root->parent;
    return std::any_of(scene.nodes,scene.nodes+scene.nodes_count,[&](auto* entry) { return entry == root; });
}

void poseChannel(const cgltf_animation_channel& channel, cgltf_node& node, float time) {
    const auto& sampler = *channel.sampler;
    const auto keyTime = [&](size_t i) {
        float value = 0;
        cgltf_accessor_read_float(sampler.input,i,&value,1);
        return value;
    };
    size_t begin = 0, end = sampler.input->count;
    while (begin < end) {
        const size_t middle = begin+(end-begin)/2;
        if (keyTime(middle) <= time) begin = middle+1;
        else end = middle;
    }
    const size_t last = std::min(begin,sampler.input->count-1), first = begin ? begin-1 : 0;
    const float t0 = keyTime(first), t1 = keyTime(last);
    const float weight = sampler.interpolation == cgltf_interpolation_type_linear && t1 > t0
        ? std::clamp((time-t0)/(t1-t0),0.f,1.f) : 0.f;
    const bool rotation = channel.target_path == cgltf_animation_path_type_rotation;
    const size_t count = rotation ? 4 : 3;
    std::array<float,4> a{}, b{};
    if (!cgltf_accessor_read_float(sampler.output,first,a.data(),count) ||
        !cgltf_accessor_read_float(sampler.output,last,b.data(),count)) return;
    if (rotation && a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3] < 0)
        for (auto& value : b) value = -value;
    for (size_t i = 0; i < count; ++i) a[i] += (b[i]-a[i])*weight;
    if (rotation) {
        const float length = std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]+a[3]*a[3]);
        if (length > 0) for (auto& value : a) value /= length;
        std::copy_n(a.begin(),4,node.rotation);
        node.has_rotation = true;
    } else if (channel.target_path == cgltf_animation_path_type_translation) {
        std::copy_n(a.begin(),3,node.translation);
        node.has_translation = true;
    } else {
        std::copy_n(a.begin(),3,node.scale);
        node.has_scale = true;
    }
}

struct Projected { float x, y, z, u, v; };

float edge(Projected a, Projected b, float x, float y) {
    return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);
}

int texel(float coordinate, int size, cgltf_wrap_mode wrap) {
    if (wrap == cgltf_wrap_mode_clamp_to_edge) coordinate = std::clamp(coordinate,0.f,1.f);
    else if (wrap == cgltf_wrap_mode_mirrored_repeat) {
        const float t = coordinate-2*std::floor(coordinate/2);
        coordinate = t > 1 ? 2-t : t;
    } else coordinate -= std::floor(coordinate);
    return std::clamp(int(coordinate*size),0,size-1);
}

void drawTriangle(Pixels& image, std::vector<float>& depth, const Part& part,
                  Projected a, Projected b, Projected c) {
    const float area = edge(a,b,c.x,c.y);
    if (!std::isfinite(area) || std::abs(area) < .001f) return;
    const auto bounds = [](float lo, float hi, int size) {
        return std::pair{int(std::clamp(std::floor(lo),0.f,float(size))),
                         int(std::clamp(std::ceil(hi),0.f,float(size)))};
    };
    const auto [left,right] = bounds(std::min({a.x,b.x,c.x}),std::max({a.x,b.x,c.x}),image.width);
    const auto [top,bottom] = bounds(std::min({a.y,b.y,c.y}),std::max({a.y,b.y,c.y}),image.height);
    static constexpr uint8_t white[]{255,255,255,255};
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const float u = edge(b,c,x+.5f,y+.5f)/area, v = edge(c,a,x+.5f,y+.5f)/area, w = 1-u-v;
            if (u < 0 || v < 0 || w < 0) continue;
            const float z = a.z*u+b.z*v+c.z*w;
            const size_t index = size_t(y)*image.width+x;
            if (z <= depth[index]) continue;
            const auto* color = white;
            if (part.texture) {
                const auto& tex = *part.texture;
                const int tx = texel(a.u*u+b.u*v+c.u*w,tex.width,part.wrapS);
                const int ty = texel(a.v*u+b.v*v+c.v*w,tex.height,part.wrapT);
                color = &tex.rgba[(size_t(ty)*tex.width+tx)*4];
            }
            if (part.cutoff > 0 && color[3]*part.tint[3] < 255*part.cutoff) continue;
            depth[index] = z;
            auto* pixel = &image.rgba[index*4];
            for (size_t i = 0; i < 3; ++i) pixel[i] = uint8_t(std::clamp(color[i]*part.tint[i],0.f,255.f));
            pixel[3] = 255;
        }
    }
}

}

bool validMeshAxes(MeshAxis up, MeshAxis forward) {
    return int(up) >= 0 && int(up) < 6 && int(forward) >= 0 && int(forward) < 6 && int(up)%3 != int(forward)%3;
}

std::vector<Frame> meshTimingFrames(float seconds) {
    const int duration = std::clamp(int(std::lround(std::clamp(seconds,0.f,120.f)*100)),2,12000);
    const int count = std::clamp((duration+4)/5,2,240);
    std::vector<Frame> frames(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        frames[size_t(i)].ticks = (i+1)*duration/count-i*duration/count;
        frames[size_t(i)].originX = meshFrameWidth/2;
        frames[size_t(i)].originY = meshFrameHeight-1;
    }
    return frames;
}

struct GlbModel::Data {
    std::vector<uint8_t> bytes;
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> gltf{nullptr,cgltf_free};
    std::vector<std::optional<Pixels>> textures;
    std::vector<MeshClip> clips;
    std::vector<Part> parts;
    std::vector<Point> rest;

    std::vector<Matrix> worldTransforms(int clip, float phase) const {
        std::vector<cgltf_node> nodes(gltf->nodes,gltf->nodes+gltf->nodes_count);
        for (auto& node : nodes) if (node.parent) node.parent = &nodes[size_t(node.parent-gltf->nodes)];
        if (clip >= 0 && size_t(clip) < clips.size()) {
            const auto& motion = gltf->animations[clip];
            const float time = clips[size_t(clip)].seconds*std::clamp(phase,0.f,1.f);
            for (size_t i = 0; i < motion.channels_count; ++i) {
                const auto& channel = motion.channels[i];
                poseChannel(channel,nodes[size_t(channel.target_node-gltf->nodes)],time);
            }
        }
        std::vector<Matrix> world(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i)
            cgltf_node_transform_world(&nodes[i],world[i].data());
        return world;
    }

    std::vector<Point> skin(const Part& part, const std::vector<Matrix>& world) const {
        std::vector<Matrix> joints(part.bind.size());
        for (size_t i = 0; i < joints.size(); ++i)
            joints[i] = multiply(world[size_t(part.skin->joints[i]-gltf->nodes)],part.bind[i]);
        std::vector<Point> points(part.vertices.size());
        for (size_t i = 0; i < points.size(); ++i) {
            const auto& vertex = part.vertices[i];
            for (size_t j = 0; j < vertex.weights.size(); ++j) {
                if (vertex.weights[j] <= 0) continue;
                const auto p = transform(joints[vertex.joints[j]],vertex.position);
                for (size_t k = 0; k < p.size(); ++k) points[i][k] += p[k]*vertex.weights[j];
            }
        }
        return points;
    }
};

GlbModel::GlbModel(const fs::path& path, const Images& images) : data_(std::make_unique<Data>()) {
    auto& asset = *data_;
    const auto size = fs::file_size(path);
    require(size >= 28 && size <= maxGlbBytes,"OpenBOR3D requires a GLB smaller than 32 MiB.");
    asset.bytes.resize(size_t(size));
    std::ifstream input(path,std::ios::binary);
    require(bool(input.read(reinterpret_cast<char*>(asset.bytes.data()),std::streamsize(size))),"Could not read GLB.");
    cgltf_options options{};
    options.type = cgltf_file_type_glb;
    cgltf_data* parsed = nullptr;
    const auto status = cgltf_parse(&options,asset.bytes.data(),asset.bytes.size(),&parsed);
    asset.gltf.reset(parsed);
    require(status == cgltf_result_success && parsed && parsed->file_type == cgltf_file_type_glb &&
            parsed->buffers_count == 1 && !parsed->buffers[0].uri && parsed->bin &&
            cgltf_load_buffers(&options,parsed,nullptr) == cgltf_result_success &&
            cgltf_validate(parsed) == cgltf_result_success,"Expected a self-contained glTF 2.0 binary.");
    const auto* scene = parsed->scene ? parsed->scene : parsed->scenes;
    require(scene && scene->nodes_count && parsed->nodes_count <= maxRigNodes && parsed->animations_count &&
            parsed->skins_count && !parsed->extensions_required_count,
            "OpenBOR3D needs an animated skinned GLB with a scene, at most 1024 nodes, and no required extensions.");

    std::set<std::string> reserved;
    for (size_t i = 0; i < parsed->animations_count; ++i)
        if (parsed->animations[i].name && clipToken(parsed->animations[i].name)) reserved.emplace(parsed->animations[i].name);
    std::set<std::string> used;
    for (size_t i = 0; i < parsed->animations_count; ++i) {
        const auto& animation = parsed->animations[i];
        const auto name = animation.name ? std::string(animation.name) : std::string{};
        std::string alias = name;
        if (!clipToken(name) || !used.insert(name).second) {
            alias = "studio_clip_"+std::to_string(i+1);
            while (reserved.contains(alias)) alias += '_';
            reserved.insert(alias);
        }
        MeshClip clip{name,alias};
        for (size_t j = 0; j < animation.channels_count; ++j) {
            const auto& channel = animation.channels[j];
            const auto& sampler = *channel.sampler;
            require(channel.target_node && !channel.target_node->has_matrix &&
                    (channel.target_path == cgltf_animation_path_type_translation ||
                     channel.target_path == cgltf_animation_path_type_rotation ||
                     channel.target_path == cgltf_animation_path_type_scale) &&
                    sampler.input && sampler.output && sampler.input->count &&
                    sampler.input->count == sampler.output->count &&
                    (sampler.interpolation == cgltf_interpolation_type_linear ||
                     sampler.interpolation == cgltf_interpolation_type_step),
                    "OpenBOR3D supports LINEAR or STEP joint transforms and cannot use morph animation channels.");
            float last = 0;
            require(cgltf_accessor_read_float(sampler.input,sampler.input->count-1,&last,1) &&
                    std::isfinite(last) && last >= 0,"Invalid GLB animation duration.");
            clip.seconds = std::max(clip.seconds,last);
        }
        asset.clips.push_back(std::move(clip));
    }

    asset.textures.resize(parsed->images_count);
    size_t texturePixels = 0, triangles = 0, vertices = 0;
    for (size_t i = 0; i < parsed->nodes_count; ++i) {
        const auto& node = parsed->nodes[i];
        if (!node.mesh || !node.skin || !activeNode(node,*scene)) continue;
        for (size_t j = 0; j < node.mesh->primitives_count; ++j) {
            const auto& primitive = node.mesh->primitives[j];
            const auto* pos = cgltf_find_accessor(&primitive,cgltf_attribute_type_position,0);
            const auto* joints = cgltf_find_accessor(&primitive,cgltf_attribute_type_joints,0);
            const auto* weights = cgltf_find_accessor(&primitive,cgltf_attribute_type_weights,0);
            require(primitive.type == cgltf_primitive_type_triangles && !primitive.has_draco_mesh_compression &&
                    !primitive.targets_count && pos && pos->type == cgltf_type_vec3 && joints && weights &&
                    joints->type == cgltf_type_vec4 && weights->type == cgltf_type_vec4 &&
                    joints->count == pos->count && weights->count == pos->count &&
                    !cgltf_find_accessor(&primitive,cgltf_attribute_type_joints,1),
                    "OpenBOR3D needs skinned triangles with up to four weights per vertex and no morph targets.");
            const size_t count = primitive.indices ? primitive.indices->count : pos->count;
            require(count && count%3 == 0 && count/3 <= maxTriangles-triangles &&
                    pos->count && pos->count <= maxTriangles*3-vertices && asset.parts.size() < maxRigNodes,
                    "The GLB exceeds the fork's geometry limits.");
            triangles += count/3;
            vertices += pos->count;
            auto& part = asset.parts.emplace_back();
            part.skin = node.skin;
            require(node.skin->joints_count && node.skin->joints_count <= parsed->nodes_count,"Invalid GLB skin joints.");
            part.bind.resize(node.skin->joints_count,identity);
            if (const auto* inverse = node.skin->inverse_bind_matrices) {
                require(inverse->type == cgltf_type_mat4 && inverse->count == part.bind.size(),"Invalid inverse bind matrices.");
                for (size_t k = 0; k < part.bind.size(); ++k)
                    require(cgltf_accessor_read_float(inverse,k,part.bind[k].data(),16),"Invalid inverse bind matrix.");
            }
            const cgltf_accessor* uv = nullptr;
            if (const auto* material = primitive.material) {
                require(material->alpha_mode != cgltf_alpha_mode_blend,"Use opaque or alpha-mask GLB materials.");
                if (material->alpha_mode == cgltf_alpha_mode_mask) part.cutoff = material->alpha_cutoff;
                if (material->has_pbr_metallic_roughness) {
                    const auto& pbr = material->pbr_metallic_roughness;
                    std::copy_n(pbr.base_color_factor,4,part.tint.begin());
                    const auto& base = pbr.base_color_texture;
                    if (base.texture) {
                        const auto* image = base.texture->image;
                        uv = cgltf_find_accessor(&primitive,cgltf_attribute_type_texcoord,0);
                        require(uv && uv->type == cgltf_type_vec2 && uv->count == pos->count &&
                                !base.texcoord && !base.has_transform && image && image->buffer_view && image->mime_type &&
                                std::string_view(image->mime_type) == "image/png",
                                "GLB base colors need embedded PNG textures with untransformed TEXCOORD_0.");
                        auto& texture = asset.textures[size_t(image-parsed->images)];
                        if (!texture) {
                            const auto* view = image->buffer_view;
                            auto pixels = images.readEmbedded({cgltf_buffer_view_data(view),view->size});
                            texturePixels += size_t(pixels.width)*pixels.height;
                            require(pixels.width <= 2048 && pixels.height <= 2048 && texturePixels <= 8*1024*1024,
                                    "GLB texture size exceeds the OpenBOR3D limit.");
                            texture = std::move(pixels);
                        }
                        part.texture = &*texture;
                        if (const auto* sampler = base.texture->sampler) {
                            part.wrapS = sampler->wrap_s;
                            part.wrapT = sampler->wrap_t;
                        }
                    }
                }
            }
            part.vertices.resize(pos->count);
            const auto finite = [](float n) { return std::isfinite(n); };
            for (size_t k = 0; k < pos->count; ++k) {
                auto& vertex = part.vertices[k];
                require(cgltf_accessor_read_float(pos,k,vertex.position.data(),3) &&
                        cgltf_accessor_read_uint(joints,k,vertex.joints.data(),4) &&
                        cgltf_accessor_read_float(weights,k,vertex.weights.data(),4) &&
                        (!uv || cgltf_accessor_read_float(uv,k,vertex.uv.data(),2)) &&
                        std::all_of(vertex.position.begin(),vertex.position.end(),finite) &&
                        std::all_of(vertex.uv.begin(),vertex.uv.end(),finite),"Invalid GLB skin vertex.");
                float total = 0;
                for (size_t w = 0; w < vertex.weights.size(); ++w) {
                    require(vertex.joints[w] < part.bind.size() && finite(vertex.weights[w]) && vertex.weights[w] >= 0,
                            "Invalid GLB skin weight or joint.");
                    total += vertex.weights[w];
                }
                require(finite(total) && total > 0,"Invalid GLB skin weights.");
                for (auto& weight : vertex.weights) weight /= total;
            }
            for (size_t k = 0; k < count; ++k) {
                const auto index = primitive.indices ? cgltf_accessor_read_index(primitive.indices,k) : k;
                require(index < part.vertices.size(),"Invalid GLB triangle index.");
                part.indices.push_back(unsigned(index));
            }
        }
    }
    require(!asset.parts.empty(),"The selected GLB scene has no skinned geometry.");
    const auto world = asset.worldTransforms(-1,0);
    for (const auto& part : asset.parts) {
        const auto points = asset.skin(part,world);
        asset.rest.insert(asset.rest.end(),points.begin(),points.end());
    }
}

GlbModel::~GlbModel() = default;

const std::vector<MeshClip>& GlbModel::clips() const { return data_->clips; }

std::array<float,3> GlbModel::dimensions(const MeshSettings& settings) const {
    const auto matrix = orientation(settings,true);
    const float inf = std::numeric_limits<float>::infinity();
    Point lo{inf,inf,inf}, hi{-inf,-inf,-inf};
    for (const auto& vertex : data_->rest) {
        const auto p = transform(matrix,vertex);
        for (size_t i = 0; i < p.size(); ++i) {
            lo[i] = std::min(lo[i],p[i]);
            hi[i] = std::max(hi[i],p[i]);
        }
    }
    return {(hi[0]-lo[0])*settings.scale,(hi[1]-lo[1])*settings.scale,(hi[2]-lo[2])*settings.scale};
}

void GlbModel::write(const fs::path& destination, const MeshSettings& settings) const {
    const auto& asset = *data_;
    const auto matrix = orientation(settings,false);
    const bool rename = std::any_of(asset.clips.begin(),asset.clips.end(),[](const MeshClip& clip) {
        return clip.exportName != clip.name;
    });
    fs::create_directories(destination.parent_path());
    std::ofstream output(destination,std::ios::binary);
    if (matrix == identity && !rename) {
        output.write(reinterpret_cast<const char*>(asset.bytes.data()),std::streamsize(asset.bytes.size()));
    } else {
        auto json = nlohmann::json::parse(asset.gltf->json,asset.gltf->json+asset.gltf->json_size);
        if (rename)
            for (size_t i = 0; i < asset.clips.size(); ++i) json["animations"][i]["name"] = asset.clips[i].exportName;
        if (matrix != identity) {
            auto& nodes = json["nodes"];
            std::map<size_t,size_t> wrappers;
            for (auto& scene : json["scenes"]) {
                if (!scene.contains("nodes")) continue;
                for (auto& root : scene["nodes"]) {
                    const auto child = root.get<size_t>();
                    const auto [entry,created] = wrappers.try_emplace(child,nodes.size());
                    if (created) nodes.push_back({{"matrix",matrix},{"children",std::array{child}}});
                    root = entry->second;
                }
            }
            require(nodes.size() <= maxRigNodes,"The axis correction exceeds the fork's 1024-node rig limit.");
        }
        auto encoded = json.dump();
        encoded.append((4-encoded.size()%4)%4,' ');
        const size_t tail = 20+asset.gltf->json_size;
        require(tail <= asset.bytes.size(),"Invalid GLB chunk length.");
        const auto total = 20+encoded.size()+asset.bytes.size()-tail;
        require(total <= maxGlbBytes,"The converted GLB exceeds 32 MiB.");
        const auto write32 = [&](size_t value) {
            for (size_t i = 0; i < 4; ++i) output.put(char((value>>(i*8))&255));
        };
        output.write("glTF",4);
        write32(2);
        write32(total);
        write32(encoded.size());
        output.write("JSON",4);
        output.write(encoded.data(),std::streamsize(encoded.size()));
        output.write(reinterpret_cast<const char*>(asset.bytes.data()+tail),std::streamsize(asset.bytes.size()-tail));
    }
    output.close();
    require(bool(output),"Could not write the OpenBOR3D GLB.");
}

Pixels GlbModel::render(const MeshSettings& settings, int clip, float phase, const MeshView& view) const {
    Pixels result{view.width,view.height,std::vector<uint8_t>(size_t(view.width)*view.height*4)};
    std::vector<float> depth(size_t(view.width)*view.height,-std::numeric_limits<float>::infinity());
    const auto world = data_->worldTransforms(clip,phase);
    const auto orient = orientation(settings,true);
    const float scale = view.zoom*settings.scale, xScale = view.mirrored ? -scale : scale;
    for (const auto& part : data_->parts) {
        const auto points = data_->skin(part,world);
        std::vector<Projected> projected(points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            const auto p = transform(orient,points[i]);
            projected[i] = {view.originX+p[0]*xScale,
                            view.originY+((settings.planar ? 0.f : p[2])-p[1])*scale,
                            p[2],part.vertices[i].uv[0],part.vertices[i].uv[1]};
        }
        for (size_t i = 0; i < part.indices.size(); i+=3)
            drawTriangle(result,depth,part,projected[part.indices[i]],projected[part.indices[i+1]],projected[part.indices[i+2]]);
    }
    return result;
}

}
