/* SPDX-License-Identifier: MIT */
#include <pet/preset/face_motion.h>
#include <pet/shape/circle.h>
#include <pet/shape/rounded_polygon.h>
#include <ArduinoJson.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using namespace pet::shape;
constexpr size_t kMaxNodes     = 16;
constexpr float kDisplayWidth  = 320.0f;
constexpr float kDisplayHeight = 240.0f;
enum class Kind { Circle, Polygon };
struct MotionPreview {
    bool enabled       = false;
    bool playing       = false;
    bool loop          = true;
    bool use_keyframes = false;
    int property = 2;  // 0=Translate X, 1=Translate Y, 2=Rotation, 3=Scale X, 4=Scale Y, 5=Opacity, 6=Circle radius,
                       // 7=Corner roundness, 8=Edge bulge
    float amplitude        = 8.0f;
    float period_seconds   = 2.0f;
    float phase_radians    = 0.0f;
    float time_seconds     = 0.0f;
    float duration_seconds = 2.0f;
    pet::preset::KeyframeMotion keyframes{};
    pet::preset::MotionEasing easing = pet::preset::MotionEasing::Linear;
};
struct Node {
    std::string id;
    Kind kind = Kind::Circle;
    PathStyle style{};
    Transform2D transform{};
    CircleParameters circle{{160, 120}, 20};
    RoundedPolygonParameters polygon{};
    int parent_index = -1;
    bool visible     = true;
    MotionPreview motion{};
};

struct Scene {
    std::string id{"kiwi-scene"};
    Color background{7, 16, 24};
    std::array<Node, kMaxNodes> nodes{};
    size_t count = 0;
    int selected = 0;
    std::string file_path{"kiwi-face.petface.json"};
    std::string status;
    bool dirty = false;
};

struct View {
    float zoom = 1.0f;
    ImVec2 pan{};
    bool frame_requested = false;
    bool reset_requested = false;
    bool show_bounds     = false;
    bool show_guides     = false;
    bool show_vertices   = true;
    MotionPreview motion{};
};

struct History {
    static constexpr size_t kMaxEntries = 64;
    std::vector<Scene> undo;
    std::vector<Scene> redo;

    void record(const Scene& scene)
    {
        if (undo.size() == kMaxEntries) undo.erase(undo.begin());
        undo.push_back(scene);
        redo.clear();
    }

    bool undoScene(Scene& scene)
    {
        if (undo.empty()) return false;
        redo.push_back(scene);
        scene = undo.back();
        undo.pop_back();
        return true;
    }

    bool redoScene(Scene& scene)
    {
        if (redo.empty()) return false;
        undo.push_back(scene);
        scene = redo.back();
        redo.pop_back();
        return true;
    }
};

Node makeNode(Kind kind, int number)
{
    Node n;
    n.id                   = (kind == Kind::Circle ? "circle-" : "polygon-") + std::to_string(number);
    n.kind                 = kind;
    n.polygon.vertex_count = 3;
    n.polygon.vertices     = {{{-30, 20}, {0, -35}, {30, 20}}};
    return n;
}
void reset(Scene& s)
{
    s                                = {};
    s.nodes[0]                       = makeNode(Kind::Polygon, 1);
    s.nodes[0].transform.translation = {108, 78};
    s.nodes[1]                       = makeNode(Kind::Polygon, 2);
    s.nodes[1].transform.translation = {212, 78};
    s.nodes[2]                       = makeNode(Kind::Circle, 1);
    s.nodes[2].circle                = {{118, 138}, 12};
    s.nodes[3]                       = makeNode(Kind::Circle, 2);
    s.nodes[3].circle                = {{202, 138}, 12};
    s.count                          = 4;
    s.dirty                          = false;
}

void newScene(Scene& s)
{
    const std::string path = s.file_path;
    s                      = {};
    s.file_path            = path;
    s.selected             = -1;
    s.dirty                = false;
}

std::string uniqueCopyId(const Scene& scene, const std::string& id)
{
    for (int suffix = 1;; ++suffix) {
        const std::string candidate = id + "-copy" + (suffix == 1 ? "" : "-" + std::to_string(suffix));
        bool exists                 = false;
        for (size_t index = 0; index < scene.count; ++index) exists |= scene.nodes[index].id == candidate;
        if (!exists) return candidate;
    }
}

bool duplicateSelected(Scene& scene)
{
    if (scene.selected < 0 || scene.selected >= static_cast<int>(scene.count) || scene.count == kMaxNodes) return false;
    Node copy = scene.nodes[scene.selected];
    copy.id   = uniqueCopyId(scene, copy.id);
    copy.transform.translation.x += 8.0f;
    copy.transform.translation.y += 8.0f;
    scene.nodes[scene.count] = copy;
    scene.selected           = static_cast<int>(scene.count++);
    return true;
}
Vec2 cubic(Vec2 a, Vec2 b, Vec2 c, Vec2 d, float t)
{
    float u = 1 - t;
    return {u * u * u * a.x + 3 * u * u * t * b.x + 3 * u * t * t * c.x + t * t * t * d.x,
            u * u * u * a.y + 3 * u * u * t * b.y + 3 * u * t * t * c.y + t * t * t * d.y};
}
ImU32 rgba(Color c, float a = 1)
{
    return IM_COL32(c.red, c.green, c.blue, int(std::clamp(a, 0.f, 1.f) * 255));
}
std::vector<ImVec2> flatten(const ShapePath& p, ImVec2 o, float z)
{
    std::vector<ImVec2> out;
    Vec2 cur{};
    for (uint8_t i = 0; i < p.command_count; i++) {
        auto& q = p.commands[i];
        if (q.type == PathCommandType::Move) {
            cur = q.first;
            out.push_back({o.x + cur.x * z, o.y + cur.y * z});
        } else if (q.type == PathCommandType::Cubic) {
            for (int j = 1; j <= 16; j++) {
                cur = cubic(cur, q.first, q.second, q.end, float(j) / 16);
                out.push_back({o.x + cur.x * z, o.y + cur.y * z});
            }
        }
    }
    return out;
}
bool sample(const Node& n, const Affine2D& world, ShapePath& p)
{
    if (n.kind == Kind::Circle) {
        CircleSample x;
        if (!sampleCircle(n.circle, world, x, n.style)) return false;
        p = x.path;
        return true;
    }
    RoundedPolygonSample x;
    if (!sampleRoundedPolygon(n.polygon, world, x, n.style)) return false;
    p = x.path;
    return true;
}
bool resolveWorlds(const Scene& scene, std::array<Affine2D, kMaxNodes>& worlds)
{
    for (size_t index = 0; index < scene.count; ++index) {
        const Node& node = scene.nodes[index];
        if (node.parent_index < -1 || node.parent_index >= static_cast<int>(index)) return false;
        const Affine2D local = makeAffine(node.transform);
        worlds[index]        = node.parent_index < 0 ? local : compose(worlds[node.parent_index], local);
    }
    return true;
}

const char* shapeProblem(const Node& node, const Affine2D& world)
{
    if (node.kind == Kind::Circle) {
        CircleSample sample;
        if (sampleCircle(node.circle, world, sample, node.style)) return nullptr;
        switch (sample.error) {
            case CircleSampleError::InvalidRadius:
                return "radius must be greater than zero";
            case CircleSampleError::NonFiniteParameters:
                return "circle center or radius is not finite";
            case CircleSampleError::NonFiniteTransform:
                return "transform produces a singular shape";
            case CircleSampleError::NonFiniteStyle:
                return "style values are not finite";
            default:
                return "circle geometry is invalid";
        }
    }
    RoundedPolygonSample sample;
    if (sampleRoundedPolygon(node.polygon, world, sample, node.style)) return nullptr;
    switch (sample.error) {
        case RoundedPolygonSampleError::InvalidVertexCount:
            return "vertex count must be between 3 and 5";
        case RoundedPolygonSampleError::DegenerateOrNonConvex:
            return "vertices must form a convex polygon";
        case RoundedPolygonSampleError::NonFiniteParameters:
            return "a vertex or edge value is not finite";
        case RoundedPolygonSampleError::NonFiniteTransform:
            return "transform produces a singular shape";
        case RoundedPolygonSampleError::NonFiniteStyle:
            return "style values are not finite";
        default:
            return "polygon geometry is invalid";
    }
}

float* motionTarget(Node& node, int property, pet::preset::MotionRange& range)
{
    switch (property) {
        case 0:
            range = {-4096.0f, 4096.0f};
            return &node.transform.translation.x;
        case 1:
            range = {-4096.0f, 4096.0f};
            return &node.transform.translation.y;
        case 2:
            range = {-180.0f, 180.0f};
            return &node.transform.rotation_degrees;
        case 3:
            range = {.05f, 240.0f};
            return &node.transform.scale.x;
        case 4:
            range = {.05f, 240.0f};
            return &node.transform.scale.y;
        case 5:
            range = {0.0f, 1.0f};
            return &node.style.opacity;
        case 6:
            if (node.kind != Kind::Circle) return nullptr;
            range = {.01f, 240.0f};
            return &node.circle.radius;
        case 7:
            if (node.kind != Kind::Polygon || node.polygon.vertex_count == 0) return nullptr;
            range = {0.0f, .35f};
            return &node.polygon.corners[0].roundness;
        case 8:
            if (node.kind != Kind::Polygon || node.polygon.vertex_count == 0) return nullptr;
            range = {-.35f, .35f};
            return &node.polygon.edges[0].bulge;
        default:
            return nullptr;
    }
}

bool applyMotion(Node& node, const MotionPreview& motion, uint32_t elapsed_ms)
{
    if (!motion.enabled) return false;
    pet::preset::MotionRange range{};
    float* target = motionTarget(node, motion.property, range);
    if (target == nullptr) return false;
    if (motion.use_keyframes)
        return pet::preset::evaluateKeyframeMotion(motion.keyframes, elapsed_ms, range, motion.easing, *target);
    pet::preset::LoopMotion loop;
    loop.amplitude      = motion.amplitude;
    loop.period_seconds = motion.period_seconds;
    loop.phase_radians  = motion.phase_radians;
    return pet::preset::evaluateLoopMotion(*target, loop, elapsed_ms, range, *target);
}

bool canvas(Scene& s, View& view)
{
    ImGui::BeginChild("canvas", {}, true);
    Scene display = s;
    if (view.motion.playing) {
        const uint32_t elapsed_ms = static_cast<uint32_t>(std::max(0.0f, view.motion.time_seconds) * 1000.0f);
        for (size_t index = 0; index < display.count; ++index) {
            MotionPreview preview = display.nodes[index].motion;
            preview.playing       = true;
            preview.time_seconds  = view.motion.time_seconds;
            if (preview.enabled) applyMotion(display.nodes[index], preview, elapsed_ms);
        }
    }
    auto a          = ImGui::GetContentRegionAvail();
    const float fit = std::max(.1f, std::min((a.x - 10) / kDisplayWidth, (a.y - 10) / kDisplayHeight));
    bool changed    = false;
    std::array<Affine2D, kMaxNodes> worlds{};
    const bool hierarchy_valid = resolveWorlds(display, worlds);

    if (view.reset_requested) {
        view.reset_requested = false;
        view.zoom            = 1.0f;
        view.pan             = {};
    }
    if (view.frame_requested) {
        view.frame_requested = false;
        Bounds2D content{};
        if (hierarchy_valid)
            for (size_t index = 0; index < s.count; ++index) {
                if (!display.nodes[index].visible) continue;
                ShapePath path;
                if (!sample(display.nodes[index], worlds[index], path) || !path.bounds.valid) continue;
                include(content, path.bounds.minimum);
                include(content, path.bounds.maximum);
            }
        if (content.valid) {
            const float width  = std::max(1.0f, content.maximum.x - content.minimum.x);
            const float height = std::max(1.0f, content.maximum.y - content.minimum.y);
            view.zoom          = std::clamp(std::min(kDisplayWidth / width, kDisplayHeight / height) * .9f, .25f, 6.0f);
            const float z      = fit * view.zoom;
            view.pan.x         = a.x * .5f - (content.minimum.x + content.maximum.x) * .5f * z;
            view.pan.y         = a.y * .5f - (content.minimum.y + content.maximum.y) * .5f * z;
        } else {
            view.zoom = 1.0f;
            view.pan  = {};
        }
    }
    if (ImGui::IsWindowHovered()) {
        if (ImGui::GetIO().MouseWheel != 0.0f)
            view.zoom = std::clamp(view.zoom * std::pow(1.15f, ImGui::GetIO().MouseWheel), 0.25f, 6.0f);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            view.pan.x += ImGui::GetIO().MouseDelta.x;
            view.pan.y += ImGui::GetIO().MouseDelta.y;
        }
    }
    const float z       = fit * view.zoom;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 o{origin.x + view.pan.x, origin.y + view.pan.y};
    auto* d = ImGui::GetWindowDrawList();
    d->AddRectFilled(o, {o.x + kDisplayWidth * z, o.y + kDisplayHeight * z}, rgba(s.background));
    if (view.show_guides) {
        const ImU32 guide = IM_COL32(90, 120, 140, 160);
        d->AddLine({o.x + kDisplayWidth * .5f * z, o.y}, {o.x + kDisplayWidth * .5f * z, o.y + kDisplayHeight * z},
                   guide);
        d->AddLine({o.x, o.y + kDisplayHeight * .5f * z}, {o.x + kDisplayWidth * z, o.y + kDisplayHeight * .5f * z},
                   guide);
    }
    d->AddRect(o, {o.x + kDisplayWidth * z, o.y + kDisplayHeight * z}, IM_COL32(110, 130, 145, 255));
    if (!hierarchy_valid) {
        d->AddText({o.x + 4, o.y + 4}, IM_COL32(255, 120, 120, 255), "Invalid parent ordering");
    }
    for (size_t i = 0; hierarchy_valid && i < display.count; i++) {
        const Node& node = display.nodes[i];
        if (!node.visible) continue;
        ShapePath p;
        if (!sample(node, worlds[i], p)) continue;
        auto v = flatten(p, o, z);
        if (v.size() > 2 && p.style.fill)
            d->AddConcavePolyFilled(v.data(), int(v.size()), rgba(p.style.color, p.style.opacity));
        if (v.size() > 1 && p.style.stroke)
            d->AddPolyline(v.data(), int(v.size()), rgba(p.style.color, p.style.opacity), ImDrawFlags_Closed,
                           p.style.stroke_width * z);
        if (view.show_bounds)
            d->AddRect({o.x + p.bounds.minimum.x * z, o.y + p.bounds.minimum.y * z},
                       {o.x + p.bounds.maximum.x * z, o.y + p.bounds.maximum.y * z}, IM_COL32(90, 120, 140, 180));
        if (int(i) == s.selected) {
            d->AddRect({o.x + p.bounds.minimum.x * z, o.y + p.bounds.minimum.y * z},
                       {o.x + p.bounds.maximum.x * z, o.y + p.bounds.maximum.y * z}, IM_COL32(132, 204, 22, 255));
            const Vec2 pivot = transformPoint(makeAffine(node.transform), node.transform.pivot);
            d->AddCircle({o.x + pivot.x * z, o.y + pivot.y * z}, 3.0f, IM_COL32(255, 200, 60, 255));
            if (view.show_vertices && node.kind == Kind::Polygon) {
                RoundedPolygonSample handles;
                if (sampleRoundedPolygon(node.polygon, worlds[i], handles, node.style))
                    for (const Vec2& point : handles.landmarks)
                        d->AddCircle({o.x + point.x * z, o.y + point.y * z}, 2.5f, IM_COL32(120, 200, 255, 255));
            }
        }
    }
    if (hierarchy_valid && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const Vec2 point{(mouse.x - o.x) / z, (mouse.y - o.y) / z};
        for (int index = int(display.count) - 1; index >= 0; --index) {
            ShapePath path;
            if (display.nodes[index].visible && sample(display.nodes[index], worlds[index], path) &&
                path.bounds.valid && point.x >= path.bounds.minimum.x && point.x <= path.bounds.maximum.x &&
                point.y >= path.bounds.minimum.y && point.y <= path.bounds.maximum.y) {
                s.selected = index;
                break;
            }
        }
    }
    char zoom_label[32];
    snprintf(zoom_label, sizeof(zoom_label), "%.0f%%", view.zoom * 100.0f);
    ImGui::Dummy(a);
    ImGui::EndChild();
    ImGui::TextDisabled("320 x 240 device space · %s · middle-drag pan, wheel zoom", zoom_label);
    return changed;
}
bool exportSvg(const Scene& scene, const char* filename)
{
    std::array<Affine2D, kMaxNodes> worlds{};
    if (!resolveWorlds(scene, worlds)) return false;
    std::ofstream file(filename);
    if (!file) return false;
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"320\" height=\"240\" viewBox=\"0 0 320 240\">\\n";
    file << "<rect width=\"320\" height=\"240\" fill=\"rgb(" << int(scene.background.red) << ','
         << int(scene.background.green) << ',' << int(scene.background.blue) << ")\"/>\\n";
    for (size_t index = 0; index < scene.count; ++index) {
        if (!scene.nodes[index].visible) continue;
        ShapePath path;
        if (!sample(scene.nodes[index], worlds[index], path)) return false;
        file << "<path d=\"";
        for (uint8_t command = 0; command < path.command_count; ++command) {
            const PathCommand& item = path.commands[command];
            if (item.type == PathCommandType::Move)
                file << "M " << item.first.x << ' ' << item.first.y << ' ';
            else if (item.type == PathCommandType::Cubic)
                file << "C " << item.first.x << ' ' << item.first.y << ' ' << item.second.x << ' ' << item.second.y
                     << ' ' << item.end.x << ' ' << item.end.y << ' ';
            else
                file << "Z ";
        }
        file << "\" fill=\""
             << (path.style.fill
                     ? "rgb(" + std::to_string(path.style.color.red) + "," + std::to_string(path.style.color.green) +
                           "," + std::to_string(path.style.color.blue) + ")"
                     : "none")
             << "\" fill-opacity=\"" << path.style.opacity << "\" stroke=\""
             << (path.style.stroke
                     ? "rgb(" + std::to_string(path.style.color.red) + "," + std::to_string(path.style.color.green) +
                           "," + std::to_string(path.style.color.blue) + ")"
                     : "none")
             << "\" stroke-width=\"" << path.style.stroke_width << "\"/>\\n";
    }
    file << "</svg>\\n";
    file.flush();
    return static_cast<bool>(file);
}

std::string shellQuote(const std::string& value)
{
    std::string quoted{"'"};
    for (char character : value) quoted += character == '\'' ? "'\\\"'\\\"'" : std::string(1, character);
    return quoted + "'";
}

bool validateWithCompiler(const std::string& filename)
{
    const std::string command = "python3 tools/compile_face_presets.py " + shellQuote(filename) +
                                " --output /tmp/kiwi-studio-preset.h > /tmp/kiwi-studio-validation.log 2>&1";
    const int result          = std::system(command.c_str());
    return result == 0;
}

std::string compilerMessage()
{
    std::ifstream file("/tmp/kiwi-studio-validation.log");
    std::string line;
    std::getline(file, line);
    return line.empty() ? "Compiler rejected the scene; see /tmp/kiwi-studio-validation.log" : line;
}

bool save(const Scene& s, const char* filename)
{
    JsonDocument d;
    auto r       = d.to<JsonObject>();
    r["version"] = 1;
    r["id"]      = s.id;
    auto bg      = r["background"].to<JsonArray>();
    bg.add(s.background.red);
    bg.add(s.background.green);
    bg.add(s.background.blue);
    auto ns = r["nodes"].to<JsonArray>();
    for (size_t i = 0; i < s.count; i++) {
        auto& o   = s.nodes[i];
        auto n    = ns.add<JsonObject>();
        n["id"]   = o.id;
        n["kind"] = o.kind == Kind::Circle ? "circle" : "rounded_polygon";
        if (o.parent_index >= 0) n["parent"] = s.nodes[o.parent_index].id;
        auto st = n["style"].to<JsonObject>();
        auto co = st["color"].to<JsonArray>();
        co.add(o.style.color.red);
        co.add(o.style.color.green);
        co.add(o.style.color.blue);
        st["opacity"]      = o.style.opacity;
        st["stroke_width"] = o.style.stroke_width;
        st["fill"]         = o.style.fill;
        st["stroke"]       = o.style.stroke;
        auto tr            = n["transform"].to<JsonObject>();
        auto tt            = tr["translation"].to<JsonArray>();
        tt.add(o.transform.translation.x);
        tt.add(o.transform.translation.y);
        tr["rotation_degrees"] = o.transform.rotation_degrees;
        auto ts                = tr["scale"].to<JsonArray>();
        ts.add(o.transform.scale.x);
        ts.add(o.transform.scale.y);
        auto tp = tr["pivot"].to<JsonArray>();
        tp.add(o.transform.pivot.x);
        tp.add(o.transform.pivot.y);
        if (o.kind == Kind::Circle) {
            auto x = n["circle"].to<JsonObject>();
            auto c = x["center"].to<JsonArray>();
            c.add(o.circle.center.x);
            c.add(o.circle.center.y);
            x["radius"] = o.circle.radius;
        } else {
            auto x  = n["rounded_polygon"].to<JsonObject>();
            auto vs = x["vertices"].to<JsonArray>();
            auto cs = x["corners"].to<JsonArray>();
            auto es = x["edges"].to<JsonArray>();
            for (uint8_t j = 0; j < o.polygon.vertex_count; j++) {
                auto v = vs.add<JsonArray>();
                v.add(o.polygon.vertices[j].x);
                v.add(o.polygon.vertices[j].y);
                auto c           = cs.add<JsonObject>();
                c["roundness"]   = o.polygon.corners[j].roundness;
                c["cap_tension"] = o.polygon.corners[j].cap_tension;
                auto e           = es.add<JsonObject>();
                e["bulge"]       = o.polygon.edges[j].bulge;
                e["start"]       = o.polygon.edges[j].start;
                e["end"]         = o.polygon.edges[j].end;
            }
        }
        auto motion                = n["motion"].to<JsonObject>();
        motion["enabled"]          = o.motion.enabled;
        motion["loop"]             = o.motion.loop;
        motion["use_keyframes"]    = o.motion.use_keyframes;
        motion["property"]         = o.motion.property;
        motion["amplitude"]        = o.motion.amplitude;
        motion["period_seconds"]   = o.motion.period_seconds;
        motion["phase_radians"]    = o.motion.phase_radians;
        motion["duration_seconds"] = o.motion.duration_seconds;
        motion["easing"]           = (o.motion.easing == pet::preset::MotionEasing::Linear) ? "linear" : "smoothstep";
        if (o.motion.keyframes.count > 0) {
            auto kfs = motion["keyframes"].to<JsonArray>();
            for (int k = 0; k < o.motion.keyframes.count; ++k) {
                auto kf            = kfs.add<JsonObject>();
                kf["time_seconds"] = o.motion.keyframes.keyframes[k].time_seconds;
                kf["value"]        = o.motion.keyframes.keyframes[k].value;
            }
        }
    }
    std::ofstream f(filename);
    if (!f) return false;
    if (serializeJsonPretty(d, f) == 0) return false;
    f.flush();
    return static_cast<bool>(f);
}
bool validId(const char* text)
{
    if (text == nullptr || *text < 'a' || *text > 'z') return false;
    for (const char* character = text; *character != '\0'; ++character)
        if (!((*character >= 'a' && *character <= 'z') || (*character >= '0' && *character <= '9') ||
              *character == '-'))
            return false;
    return true;
}

bool rejectUnknown(JsonObjectConst object, std::initializer_list<const char*> allowed, std::string& error,
                   const std::string& where)
{
    for (JsonPairConst pair : object) {
        const std::string key = pair.key().c_str();
        bool known            = false;
        for (const char* candidate : allowed) known |= key == candidate;
        if (!known) {
            error = where + ": unknown field '" + key + "'";
            return false;
        }
    }
    return true;
}

bool readInteger(JsonVariantConst value, int minimum, int maximum, int& output)
{
    if (!value.is<int>()) return false;
    const int candidate = value.as<int>();
    if (candidate < minimum || candidate > maximum) return false;
    output = candidate;
    return true;
}

bool readNumber(JsonVariantConst value, float& output)
{
    if (!value.is<float>()) return false;
    output = value.as<float>();
    return std::isfinite(output);
}

bool readColor(JsonVariantConst value, Color& output)
{
    if (!value.is<JsonArrayConst>()) return false;
    JsonArrayConst array = value.as<JsonArrayConst>();
    if (array.size() != 3) return false;
    int channels[3];
    for (int index = 0; index < 3; ++index)
        if (!readInteger(array[index], 0, 255, channels[index])) return false;
    output = {uint8_t(channels[0]), uint8_t(channels[1]), uint8_t(channels[2])};
    return true;
}

bool readPoint(JsonVariantConst value, Vec2& point)
{
    if (!value.is<JsonArrayConst>()) return false;
    JsonArrayConst array = value.as<JsonArrayConst>();
    if (array.size() != 2) return false;
    if (!readNumber(array[0], point.x) || !readNumber(array[1], point.y)) return false;
    return true;
}

bool readFinite(JsonVariantConst value, float& output)
{
    return readNumber(value, output);
}

float cross(Vec2 a, Vec2 b, Vec2 c)
{
    return (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
}

bool readStyle(JsonVariantConst value, PathStyle& style, std::string& error, const std::string& where)
{
    if (value.isNull()) return true;
    if (!value.is<JsonObjectConst>()) {
        error = where + ": style must be an object";
        return false;
    }
    JsonObjectConst object = value.as<JsonObjectConst>();
    if (!rejectUnknown(object, {"color", "opacity", "stroke_width", "fill", "stroke"}, error, where)) return false;
    if (!object["color"].isNull() && !readColor(object["color"], style.color)) {
        error = where + ".color: must be an RGB array of integers 0..255";
        return false;
    }
    if (!object["opacity"].isNull()) {
        if (!readNumber(object["opacity"], style.opacity) || style.opacity < 0.0f || style.opacity > 1.0f) {
            error = where + ".opacity: must be a finite number in [0, 1]";
            return false;
        }
    }
    if (!object["stroke_width"].isNull()) {
        if (!readNumber(object["stroke_width"], style.stroke_width) || style.stroke_width < 0.0f) {
            error = where + ".stroke_width: must be a finite non-negative number";
            return false;
        }
    }
    if (!object["fill"].isNull()) {
        if (!object["fill"].is<bool>()) {
            error = where + ".fill: must be a boolean";
            return false;
        }
        style.fill = object["fill"].as<bool>();
    }
    if (!object["stroke"].isNull()) {
        if (!object["stroke"].is<bool>()) {
            error = where + ".stroke: must be a boolean";
            return false;
        }
        style.stroke = object["stroke"].as<bool>();
    }
    return true;
}

bool readTransform(JsonVariantConst value, Transform2D& transform, std::string& error, const std::string& where)
{
    if (value.isNull()) return true;
    if (!value.is<JsonObjectConst>()) {
        error = where + ": transform must be an object";
        return false;
    }
    JsonObjectConst object = value.as<JsonObjectConst>();
    if (!rejectUnknown(object, {"translation", "rotation_degrees", "scale", "pivot"}, error, where)) return false;
    if (!object["translation"].isNull() && !readPoint(object["translation"], transform.translation)) {
        error = where + ".translation: must be a two-number array";
        return false;
    }
    if (!object["rotation_degrees"].isNull() && !readNumber(object["rotation_degrees"], transform.rotation_degrees)) {
        error = where + ".rotation_degrees: must be a finite number";
        return false;
    }
    if (!object["scale"].isNull() && !readPoint(object["scale"], transform.scale)) {
        error = where + ".scale: must be a two-number array";
        return false;
    }
    if (transform.scale.x == 0.0f || transform.scale.y == 0.0f) {
        error = where + ".scale: components must be non-zero";
        return false;
    }
    if (!object["pivot"].isNull() && !readPoint(object["pivot"], transform.pivot)) {
        error = where + ".pivot: must be a two-number array";
        return false;
    }
    return true;
}

bool readMotion(JsonVariantConst value, MotionPreview& motion, std::string& error, const std::string& where)
{
    if (value.isNull()) return true;
    if (!value.is<JsonObjectConst>()) {
        error = where + ": must be an object";
        return false;
    }
    JsonObjectConst object = value.as<JsonObjectConst>();
    if (!rejectUnknown(object,
                       {"enabled", "loop", "use_keyframes", "property", "amplitude", "period_seconds", "phase_radians",
                        "duration_seconds", "easing", "keyframes"},
                       error, where))
        return false;
    if (!object["enabled"].isNull()) {
        if (!object["enabled"].is<bool>()) {
            error = where + ".enabled: must be a boolean";
            return false;
        }
        motion.enabled = object["enabled"].as<bool>();
    }
    if (!object["loop"].isNull()) {
        if (!object["loop"].is<bool>()) {
            error = where + ".loop: must be a boolean";
            return false;
        }
        motion.loop = object["loop"].as<bool>();
    }
    if (!object["use_keyframes"].isNull()) {
        if (!object["use_keyframes"].is<bool>()) {
            error = where + ".use_keyframes: must be a boolean";
            return false;
        }
        motion.use_keyframes = object["use_keyframes"].as<bool>();
    }
    if (!object["property"].isNull()) {
        if (!object["property"].is<int>()) {
            error = where + ".property: must be an integer";
            return false;
        }
        motion.property = object["property"].as<int>();
    }
    if (!object["amplitude"].isNull() && !readNumber(object["amplitude"], motion.amplitude)) {
        error = where + ".amplitude: must be a finite number";
        return false;
    }
    if (!object["period_seconds"].isNull() && !readNumber(object["period_seconds"], motion.period_seconds)) {
        error = where + ".period_seconds: must be a finite number";
        return false;
    }
    if (!object["phase_radians"].isNull() && !readNumber(object["phase_radians"], motion.phase_radians)) {
        error = where + ".phase_radians: must be a finite number";
        return false;
    }
    if (!object["duration_seconds"].isNull() && !readNumber(object["duration_seconds"], motion.duration_seconds)) {
        error = where + ".duration_seconds: must be a finite number";
        return false;
    }
    if (!object["easing"].isNull()) {
        if (!object["easing"].is<const char*>()) {
            error = where + ".easing: must be linear or smoothstep";
            return false;
        }
        const char* easing = object["easing"];
        if (strcmp(easing, "linear") == 0)
            motion.easing = pet::preset::MotionEasing::Linear;
        else if (strcmp(easing, "smoothstep") == 0)
            motion.easing = pet::preset::MotionEasing::Smoothstep;
        else {
            error = where + ".easing: must be linear or smoothstep";
            return false;
        }
    }
    JsonArrayConst keyframes = object["keyframes"].as<JsonArrayConst>();
    if (!keyframes.isNull()) {
        motion.keyframes.count = 0;
        for (JsonVariantConst keyframe : keyframes) {
            if (motion.keyframes.count >= pet::preset::kMaxMotionKeyframes) {
                error = where + ".keyframes: contains too many keyframes";
                return false;
            }
            if (!keyframe.is<JsonObjectConst>()) {
                error = where + ".keyframes: must contain objects";
                return false;
            }
            JsonObjectConst keyframe_object = keyframe.as<JsonObjectConst>();
            if (!rejectUnknown(keyframe_object, {"time_seconds", "value"}, error, where + ".keyframes")) return false;
            auto& destination = motion.keyframes.keyframes[motion.keyframes.count];
            if (!readNumber(keyframe_object["time_seconds"], destination.time_seconds)) {
                error = where + ".keyframes.time_seconds: must be a finite number";
                return false;
            }
            if (!readNumber(keyframe_object["value"], destination.value)) {
                error = where + ".keyframes.value: must be a finite number";
                return false;
            }
            ++motion.keyframes.count;
        }
    }
    return true;
}

bool load(Scene& scene, const char* filename, std::string& error)
{
    error.clear();
    std::ifstream file(filename);
    if (!file) {
        error = std::string(filename) + ": cannot open file";
        return false;
    }
    JsonDocument document;
    const DeserializationError parse_error = deserializeJson(document, file);
    if (parse_error) {
        error = std::string(filename) + ": " + parse_error.c_str();
        return false;
    }
    if (document.overflowed()) {
        error = std::string(filename) + ": JSON document exceeded its memory pool";
        return false;
    }
    if (!document.is<JsonObjectConst>()) {
        error = std::string(filename) + ": root must be a JSON object";
        return false;
    }
    JsonObjectConst root = document.as<JsonObjectConst>();
    const std::string file_label{filename};
    if (!rejectUnknown(root, {"version", "id", "background", "nodes"}, error, file_label)) return false;
    if (!root["version"].is<int>() || root["version"].as<int>() != 1) {
        error = file_label + ".version: must be 1";
        return false;
    }
    const char* scene_id = root["id"];
    if (!root["id"].is<const char*>() || !validId(scene_id)) {
        error = file_label + ".id: must be a lowercase kebab-case id";
        return false;
    }
    JsonArrayConst background = root["background"].as<JsonArrayConst>();
    if (background.isNull() || !readColor(background, scene.background)) {
        error = file_label + ".background: must be an RGB array of integers 0..255";
        return false;
    }
    JsonArrayConst nodes = root["nodes"].as<JsonArrayConst>();
    if (nodes.isNull() || nodes.size() == 0 || nodes.size() > kMaxNodes) {
        error = file_label + ".nodes: must contain 1.." + std::to_string(kMaxNodes) + " nodes";
        return false;
    }

    Scene candidate{};
    candidate.id        = scene_id;
    candidate.file_path = filename;
    for (size_t node_index = 0; node_index < nodes.size(); ++node_index) {
        JsonObjectConst source  = nodes[node_index].as<JsonObjectConst>();
        const std::string where = file_label + ".nodes[" + std::to_string(node_index) + "]";
        if (source.isNull()) {
            error = where + ": must be an object";
            return false;
        }
        if (!rejectUnknown(source,
                           {"id", "kind", "parent", "style", "transform", "circle", "rounded_polygon", "motion"}, error,
                           where))
            return false;
        const char* id = source["id"];
        if (!source["id"].is<const char*>() || !validId(id)) {
            error = where + ".id: must be a lowercase kebab-case id";
            return false;
        }
        for (size_t existing = 0; existing < candidate.count; ++existing)
            if (candidate.nodes[existing].id == id) {
                error = where + ".id: duplicates node id '" + std::string(id) + "'";
                return false;
            }
        const char* kind = source["kind"];
        if (!source["kind"].is<const char*>()) {
            error = where + ".kind: must be circle or rounded_polygon";
            return false;
        }

        Node node;
        node.id = id;
        if (std::string(kind) == "circle") {
            node.kind = Kind::Circle;
            if (!source["circle"].is<JsonObjectConst>()) {
                error = where + ".circle: must be an object";
                return false;
            }
            JsonObjectConst circle = source["circle"].as<JsonObjectConst>();
            if (!rejectUnknown(circle, {"center", "radius"}, error, where + ".circle")) return false;
            if (!readPoint(circle["center"], node.circle.center)) {
                error = where + ".circle.center: must be a two-number array";
                return false;
            }
            if (!readNumber(circle["radius"], node.circle.radius) || node.circle.radius <= 0.0f) {
                error = where + ".circle.radius: must be a finite positive number";
                return false;
            }
        } else if (std::string(kind) == "rounded_polygon") {
            node.kind = Kind::Polygon;
            if (!source["rounded_polygon"].is<JsonObjectConst>()) {
                error = where + ".rounded_polygon: must be an object";
                return false;
            }
            JsonObjectConst polygon = source["rounded_polygon"].as<JsonObjectConst>();
            if (!rejectUnknown(polygon, {"vertices", "corners", "edges"}, error, where + ".rounded_polygon"))
                return false;
            JsonArrayConst vertices = polygon["vertices"].as<JsonArrayConst>();
            if (vertices.isNull() || vertices.size() < 3 || vertices.size() > kMaxRoundedPolygonVertices) {
                error = where + ".rounded_polygon.vertices: must contain 3.." +
                        std::to_string(kMaxRoundedPolygonVertices) + " points";
                return false;
            }
            JsonArrayConst corners = polygon["corners"].as<JsonArrayConst>();
            JsonArrayConst edges   = polygon["edges"].as<JsonArrayConst>();
            if (corners.isNull() || corners.size() != vertices.size()) {
                error = where + ".rounded_polygon.corners: must have one entry per vertex";
                return false;
            }
            if (edges.isNull() || edges.size() != vertices.size()) {
                error = where + ".rounded_polygon.edges: must have one entry per vertex";
                return false;
            }
            node.polygon.vertex_count = uint8_t(vertices.size());
            for (uint8_t index = 0; index < node.polygon.vertex_count; ++index) {
                const std::string vertex_where = where + ".rounded_polygon.vertices[" + std::to_string(index) + "]";
                if (!readPoint(vertices[index], node.polygon.vertices[index])) {
                    error = vertex_where + ": must be a two-number array";
                    return false;
                }
                const std::string corner_where = where + ".rounded_polygon.corners[" + std::to_string(index) + "]";
                if (!corners[index].is<JsonObjectConst>()) {
                    error = corner_where + ": must be an object";
                    return false;
                }
                JsonObjectConst corner = corners[index].as<JsonObjectConst>();
                if (!rejectUnknown(corner, {"roundness", "cap_tension"}, error, corner_where)) return false;
                if (!corner["roundness"].isNull()) {
                    if (!readNumber(corner["roundness"], node.polygon.corners[index].roundness) ||
                        node.polygon.corners[index].roundness < 0.0f || node.polygon.corners[index].roundness > .35f) {
                        error = corner_where + ".roundness: must be a finite number in [0, 0.35]";
                        return false;
                    }
                }
                if (!corner["cap_tension"].isNull()) {
                    if (!readNumber(corner["cap_tension"], node.polygon.corners[index].cap_tension) ||
                        node.polygon.corners[index].cap_tension < .10f ||
                        node.polygon.corners[index].cap_tension > .90f) {
                        error = corner_where + ".cap_tension: must be a finite number in [0.10, 0.90]";
                        return false;
                    }
                }
                const std::string edge_where = where + ".rounded_polygon.edges[" + std::to_string(index) + "]";
                if (!edges[index].is<JsonObjectConst>()) {
                    error = edge_where + ": must be an object";
                    return false;
                }
                JsonObjectConst edge = edges[index].as<JsonObjectConst>();
                if (!rejectUnknown(edge, {"bulge", "start", "end"}, error, edge_where)) return false;
                if (!edge["bulge"].isNull()) {
                    if (!readNumber(edge["bulge"], node.polygon.edges[index].bulge) ||
                        node.polygon.edges[index].bulge < -.35f || node.polygon.edges[index].bulge > .35f) {
                        error = edge_where + ".bulge: must be a finite number in [-0.35, 0.35]";
                        return false;
                    }
                }
                if (!edge["start"].isNull()) {
                    if (!readNumber(edge["start"], node.polygon.edges[index].start) ||
                        node.polygon.edges[index].start < .08f || node.polygon.edges[index].start > .45f) {
                        error = edge_where + ".start: must be a finite number in [0.08, 0.45]";
                        return false;
                    }
                }
                if (!edge["end"].isNull()) {
                    if (!readNumber(edge["end"], node.polygon.edges[index].end) ||
                        node.polygon.edges[index].end < .08f || node.polygon.edges[index].end > .45f) {
                        error = edge_where + ".end: must be a finite number in [0.08, 0.45]";
                        return false;
                    }
                }
            }
            const auto& vertices_out   = node.polygon.vertices;
            const uint8_t vertex_count = node.polygon.vertex_count;
            float first_turn           = 0.0f;
            for (uint8_t index = 0; index < vertex_count; ++index) {
                const float turn = cross(vertices_out[(index + vertex_count - 1) % vertex_count], vertices_out[index],
                                         vertices_out[(index + 1) % vertex_count]);
                if (std::fabs(turn) < .0001f) {
                    error = where + ".rounded_polygon.vertices: must be non-degenerate";
                    return false;
                }
                if (index == 0)
                    first_turn = turn;
                else if (first_turn * turn <= 0.0f) {
                    error = where + ".rounded_polygon.vertices: must be convex with consistent winding";
                    return false;
                }
            }
        } else {
            error = where + ".kind: must be circle or rounded_polygon";
            return false;
        }

        if (!readStyle(source["style"], node.style, error, where + ".style")) return false;
        if (!readTransform(source["transform"], node.transform, error, where + ".transform")) return false;
        if (!readMotion(source["motion"], node.motion, error, where + ".motion")) return false;

        JsonVariantConst parent_value = source["parent"];
        if (!parent_value.isNull()) {
            if (!parent_value.is<const char*>()) {
                error = where + ".parent: must be a node id or omitted";
                return false;
            }
            const std::string parent = parent_value.as<const char*>();
            int parent_index         = -1;
            for (size_t existing = 0; existing < candidate.count; ++existing)
                if (candidate.nodes[existing].id == parent) parent_index = static_cast<int>(existing);
            if (parent_index < 0) {
                error = where + ".parent: must name an earlier node or be omitted";
                return false;
            }
            node.parent_index = parent_index;
        }
        candidate.nodes[candidate.count++] = node;
    }

    std::array<Affine2D, kMaxNodes> worlds{};
    if (!resolveWorlds(candidate, worlds)) {
        error = file_label + ": invalid parent hierarchy";
        return false;
    }
    for (size_t index = 0; index < candidate.count; ++index) {
        const char* problem = shapeProblem(candidate.nodes[index], worlds[index]);
        if (problem != nullptr) {
            error = file_label + ".nodes[" + std::to_string(index) + "]: " + problem;
            return false;
        }
    }

    candidate.selected = candidate.count == 0 ? -1 : 0;
    candidate.dirty    = false;
    scene              = candidate;
    return true;
}

bool moveNode(Scene& scene, int target)
{
    const int source = scene.selected;
    if (target < 0 || target >= static_cast<int>(scene.count)) return false;
    Scene before = scene;
    std::swap(scene.nodes[source], scene.nodes[target]);
    for (size_t index = 0; index < scene.count; ++index) {
        int& parent = scene.nodes[index].parent_index;
        if (parent == source)
            parent = target;
        else if (parent == target)
            parent = source;
    }
    scene.selected = target;
    std::array<Affine2D, kMaxNodes> worlds{};
    if (resolveWorlds(scene, worlds)) return true;
    scene = before;
    return false;
}

bool deleteNode(Scene& scene)
{
    const int removed = scene.selected;
    for (size_t index = 0; index < scene.count; ++index)
        if (scene.nodes[index].parent_index == removed) return false;
    for (size_t index = removed; index + 1 < scene.count; ++index) scene.nodes[index] = scene.nodes[index + 1];
    --scene.count;
    for (size_t index = 0; index < scene.count; ++index)
        if (scene.nodes[index].parent_index > removed) --scene.nodes[index].parent_index;
    scene.selected = std::min<int>(removed, int(scene.count) - 1);
    return true;
}

bool deleteSelected(Scene& scene)
{
    if (scene.count <= 1 || scene.selected < 0) return false;
    return deleteNode(scene);
}

bool resetPopup(const char* label, const char* text)
{
    const std::string id = std::string("##reset-") + label;
    if (!ImGui::BeginPopupContextItem(id.c_str())) return false;
    const bool reset = ImGui::MenuItem(text);
    ImGui::EndPopup();
    return reset;
}

bool floatField(const char* label, float& value, float minimum, float maximum, float default_value, float speed,
                const char* format = "%.2f")
{
    bool changed      = false;
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(std::max(80.0f, width - 116.0f));
    changed |= ImGui::DragFloat(label, &value, speed, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(76.0f);
    changed |= ImGui::InputFloat((std::string("##exact-") + label).c_str(), &value, 0.0f, 0.0f, format);
    if (!std::isfinite(value)) value = default_value;
    value = std::clamp(value, minimum, maximum);
    if (ImGui::IsItemHovered()) {
        char hint[96];
        snprintf(hint, sizeof(hint), "Safe range %.3g to %.3g · right-click to reset", minimum, maximum);
        ImGui::SetTooltip("%s", hint);
    }
    char reset_text[64];
    snprintf(reset_text, sizeof(reset_text), "Reset to %.3g", default_value);
    changed |= resetPopup(label, reset_text);
    return changed;
}

bool pointField(const char* label, Vec2& value, Vec2 default_value, float speed, float minimum = -4096.0f,
                float maximum = 4096.0f)
{
    bool changed = ImGui::DragFloat2(label, &value.x, speed, minimum, maximum, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) value = default_value;
    value.x = std::clamp(value.x, minimum, maximum);
    value.y = std::clamp(value.y, minimum, maximum);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Ctrl+click a value to type it exactly");
    char reset_text[96];
    snprintf(reset_text, sizeof(reset_text), "Reset to {%.3g, %.3g}", default_value.x, default_value.y);
    changed |= resetPopup(label, reset_text);
    return changed;
}

bool validateScene(const Scene& scene, std::string& error);

bool saveScene(Scene& s)
{
    std::string error;
    if (!validateScene(s, error)) {
        s.status = error;
        return false;
    }
    if (!save(s, s.file_path.c_str())) {
        s.status = "Could not write " + s.file_path;
        return false;
    }
    s.dirty  = false;
    s.status = "Saved " + s.file_path;
    return true;
}

bool loadScene(Scene& s)
{
    std::string error;
    if (!load(s, s.file_path.c_str(), error)) {
        s.status = error;
        return false;
    }
    s.status = "Loaded " + s.file_path;
    return true;
}

std::string compilerLog()
{
    std::ifstream file("/tmp/kiwi-studio-validation.log");
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    return text.empty() ? "Preset compiler produced no output." : text;
}

bool validateScene(const Scene& scene, std::string& error)
{
    error.clear();
    if (!validId(scene.id.c_str())) {
        error = "Scene id must be a lowercase kebab-case id";
        return false;
    }
    if (scene.count == 0 || scene.count > kMaxNodes) {
        error = "Scene must contain 1.." + std::to_string(kMaxNodes) + " nodes";
        return false;
    }
    for (size_t index = 0; index < scene.count; ++index) {
        const Node& node        = scene.nodes[index];
        const std::string where = "nodes[" + std::to_string(index) + "]";
        if (!validId(node.id.c_str())) {
            error = where + ": id must be a lowercase kebab-case id";
            return false;
        }
        for (size_t earlier = 0; earlier < index; ++earlier)
            if (scene.nodes[earlier].id == node.id) {
                error = where + ": duplicates node id '" + node.id + "'";
                return false;
            }
        if (node.parent_index < -1 || node.parent_index >= static_cast<int>(index)) {
            error = where + ": parent must name an earlier node or be omitted";
            return false;
        }
        if (!std::isfinite(node.style.opacity) || node.style.opacity < 0.0f || node.style.opacity > 1.0f) {
            error = where + ".style.opacity: must be a finite number in [0, 1]";
            return false;
        }
        if (!std::isfinite(node.style.stroke_width) || node.style.stroke_width < 0.0f) {
            error = where + ".style.stroke_width: must be a finite non-negative number";
            return false;
        }
        if (!std::isfinite(node.transform.rotation_degrees) || !std::isfinite(node.transform.translation.x) ||
            !std::isfinite(node.transform.translation.y) || !std::isfinite(node.transform.scale.x) ||
            !std::isfinite(node.transform.scale.y) || !std::isfinite(node.transform.pivot.x) ||
            !std::isfinite(node.transform.pivot.y)) {
            error = where + ".transform: values must be finite";
            return false;
        }
        if (node.transform.scale.x == 0.0f || node.transform.scale.y == 0.0f) {
            error = where + ".transform.scale: components must be non-zero";
            return false;
        }
        if (node.kind == Kind::Circle) {
            if (!std::isfinite(node.circle.center.x) || !std::isfinite(node.circle.center.y) ||
                !std::isfinite(node.circle.radius) || node.circle.radius <= 0.0f) {
                error = where + ".circle: center and positive radius must be finite";
                return false;
            }
        } else {
            const uint8_t count = node.polygon.vertex_count;
            if (count < 3 || count > kMaxRoundedPolygonVertices) {
                error = where + ".rounded_polygon.vertices: must contain 3.." +
                        std::to_string(kMaxRoundedPolygonVertices) + " points";
                return false;
            }
            float first_turn = 0.0f;
            for (uint8_t vertex = 0; vertex < count; ++vertex) {
                const Vec2& point = node.polygon.vertices[vertex];
                if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                    error = where + ".rounded_polygon.vertices: points must be finite";
                    return false;
                }
                const float turn = cross(node.polygon.vertices[(vertex + count - 1) % count], point,
                                         node.polygon.vertices[(vertex + 1) % count]);
                if (std::fabs(turn) < .0001f) {
                    error = where + ".rounded_polygon.vertices: must be non-degenerate";
                    return false;
                }
                if (vertex == 0)
                    first_turn = turn;
                else if (first_turn * turn <= 0.0f) {
                    error = where + ".rounded_polygon.vertices: must be convex with consistent winding";
                    return false;
                }
                if (!std::isfinite(node.polygon.corners[vertex].roundness) ||
                    node.polygon.corners[vertex].roundness < 0.0f || node.polygon.corners[vertex].roundness > .35f ||
                    !std::isfinite(node.polygon.corners[vertex].cap_tension) ||
                    node.polygon.corners[vertex].cap_tension < .10f ||
                    node.polygon.corners[vertex].cap_tension > .90f) {
                    error = where + ".rounded_polygon.corners[" + std::to_string(vertex) + "]: outside safe range";
                    return false;
                }
                if (!std::isfinite(node.polygon.edges[vertex].bulge) || node.polygon.edges[vertex].bulge < -.35f ||
                    node.polygon.edges[vertex].bulge > .35f || !std::isfinite(node.polygon.edges[vertex].start) ||
                    node.polygon.edges[vertex].start < .08f || node.polygon.edges[vertex].start > .45f ||
                    !std::isfinite(node.polygon.edges[vertex].end) || node.polygon.edges[vertex].end < .08f ||
                    node.polygon.edges[vertex].end > .45f) {
                    error = where + ".rounded_polygon.edges[" + std::to_string(vertex) + "]: outside safe range";
                    return false;
                }
            }
        }
    }
    std::array<Affine2D, kMaxNodes> worlds{};
    if (!resolveWorlds(scene, worlds)) {
        error = "Invalid parent hierarchy";
        return false;
    }
    for (size_t index = 0; index < scene.count; ++index) {
        const char* problem = shapeProblem(scene.nodes[index], worlds[index]);
        if (problem != nullptr) {
            error = "nodes[" + std::to_string(index) + "]: " + problem;
            return false;
        }
    }
    return true;
}

enum class Pending { None, New, Reset, Load, Quit };

int nodeDepth(const Scene& scene, int index)
{
    int depth = 0;
    for (int parent = scene.nodes[index].parent_index; parent >= 0 && depth < 8;
         parent     = scene.nodes[parent].parent_index)
        ++depth;
    return depth;
}

void toolbar(Scene& s, View& view, History& history, Pending& pending, bool& validate_requested, bool& replaced)
{
    if (ImGui::Button("New")) {
        if (s.dirty)
            pending = Pending::New;
        else {
            newScene(s);
            replaced = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        if (s.dirty)
            pending = Pending::Reset;
        else {
            reset(s);
            replaced = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        if (s.dirty)
            pending = Pending::Load;
        else if (loadScene(s))
            replaced = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) saveScene(s);
    ImGui::SameLine();
    if (ImGui::Button("Validate")) validate_requested = true;
    ImGui::SameLine();
    if (ImGui::Button("Export SVG"))
        s.status = exportSvg(s, "kiwi-face.svg") ? "Wrote kiwi-face.svg at StackChan LCD dimensions"
                                                 : "Could not export kiwi-face.svg";
    ImGui::SameLine();
    ImGui::Dummy({8.0f, 0.0f});
    ImGui::SameLine();
    ImGui::BeginDisabled(history.undo.empty());
    if (ImGui::Button("Undo")) {
        history.undoScene(s);
        s.dirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(history.redo.empty());
    if (ImGui::Button("Redo")) {
        history.redoScene(s);
        s.dirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("|  Ctrl+S save  Ctrl+Z/Y undo/redo  Ctrl+D duplicate  Del delete  F frame  R reset view");

    char path[256];
    snprintf(path, sizeof(path), "%s", s.file_path.c_str());
    ImGui::SetNextItemWidth(420.0f);
    if (ImGui::InputText("Scene file", path, sizeof(path))) s.file_path = path;
    ImGui::SameLine();
    if (ImGui::Button("Frame all")) view.frame_requested = true;
    ImGui::SameLine();
    if (ImGui::Button("Reset view")) view.reset_requested = true;
    ImGui::SameLine();
    ImGui::Checkbox("Bounds", &view.show_bounds);
    ImGui::SameLine();
    ImGui::Checkbox("Guides", &view.show_guides);
    ImGui::SameLine();
    ImGui::Checkbox("Vertices", &view.show_vertices);
}

bool scenePanel(Scene& s)
{
    bool changed = false;
    ImGui::SeparatorText("Scene");
    char scene_id[48];
    snprintf(scene_id, sizeof(scene_id), "%s", s.id.c_str());
    if (ImGui::InputText("Id", scene_id, sizeof(scene_id))) s.id = scene_id, changed = true;
    float background[3] = {s.background.red / 255.0f, s.background.green / 255.0f, s.background.blue / 255.0f};
    if (ImGui::ColorEdit3("Background", background)) {
        s.background = {uint8_t(background[0] * 255), uint8_t(background[1] * 255), uint8_t(background[2] * 255)};
        changed      = true;
    }

    ImGui::SeparatorText("Nodes");
    if (ImGui::Button("Add circle") && s.count < kMaxNodes) {
        s.nodes[s.count] = makeNode(Kind::Circle, int(s.count) + 1);
        s.selected       = int(s.count++);
        changed          = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add polygon") && s.count < kMaxNodes) {
        s.nodes[s.count] = makeNode(Kind::Polygon, int(s.count) + 1);
        s.selected       = int(s.count++);
        changed          = true;
    }
    ImGui::BeginChild("node-list", {0, std::max(120.0f, ImGui::GetContentRegionAvail().y - 90.0f)}, true);
    for (size_t index = 0; index < s.count; ++index) {
        Node& node = s.nodes[index];
        ImGui::PushID(int(index));
        changed |= ImGui::Checkbox("##visible", &node.visible);
        ImGui::SameLine();
        if (nodeDepth(s, int(index)) > 0) ImGui::Indent(12.0f * nodeDepth(s, int(index)));
        if (ImGui::Selectable(node.id.c_str(), s.selected == int(index))) s.selected = int(index);
        if (nodeDepth(s, int(index)) > 0) ImGui::Unindent(12.0f * nodeDepth(s, int(index)));
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::BeginDisabled(s.selected < 0 || s.selected >= int(s.count));
    if (ImGui::Button("Duplicate")) changed |= duplicateSelected(s);
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        if (deleteSelected(s))
            changed = true;
        else
            s.status = "Cannot delete a node while it has children";
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(s.selected <= 0 || s.selected >= int(s.count));
    if (ImGui::Button("Move up")) {
        if (moveNode(s, s.selected - 1))
            changed = true;
        else
            s.status = "Cannot move a child before its parent";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(s.selected < 0 || size_t(s.selected + 1) >= s.count);
    if (ImGui::Button("Move down")) {
        if (moveNode(s, s.selected + 1))
            changed = true;
        else
            s.status = "Cannot move a parent after its child";
    }
    ImGui::EndDisabled();
    return changed;
}

bool inspector(Scene& s, View& view)
{
    if (s.selected < 0 || size_t(s.selected) >= s.count) {
        ImGui::TextDisabled("No node selected");
        return false;
    }
    bool changed = false;
    auto& n      = s.nodes[s.selected];
    std::array<Affine2D, kMaxNodes> worlds{};
    const bool valid_hierarchy = resolveWorlds(s, worlds);
    const char* problem        = valid_hierarchy ? shapeProblem(n, worlds[s.selected]) : "invalid parent ordering";

    ImGui::Text("%s · %s", n.id.c_str(), n.kind == Kind::Circle ? "circle" : "rounded polygon");
    if (problem != nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 140, 120, 255));
        ImGui::TextWrapped("Invalid: %s", problem);
        ImGui::PopStyleColor();
    }

    ImGui::SeparatorText("Node");
    char id[48];
    snprintf(id, sizeof(id), "%s", n.id.c_str());
    if (ImGui::InputText("Name", id, sizeof(id))) n.id = id, changed = true;
    changed |= ImGui::Checkbox("Visible", &n.visible);

    ImGui::SeparatorText("Shape");
    if (n.kind == Kind::Circle) {
        changed |= pointField("Center", n.circle.center, {160.0f, 120.0f}, .5f);
        changed |= floatField("Radius", n.circle.radius, 1.0f, 160.0f, 20.0f, .2f);
    } else {
        int count = n.polygon.vertex_count;
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::SliderInt("Vertices", &count, 3, int(kMaxRoundedPolygonVertices))) {
            n.polygon.vertex_count = uint8_t(count);
            changed                = true;
        }
        if (ImGui::BeginCombo("Ear preset", "Apply to shape")) {
            if (ImGui::Selectable("Neutral")) {
                for (int i = 0; i < n.polygon.vertex_count; ++i) {
                    n.polygon.corners[i].roundness   = .08f;
                    n.polygon.corners[i].cap_tension = .55f;
                    n.polygon.edges[i].bulge         = 0.0f;
                    n.polygon.edges[i].start         = .28f;
                    n.polygon.edges[i].end           = .28f;
                }
                changed = true;
            }
            if (ImGui::Selectable("Pointed")) {
                for (int i = 0; i < n.polygon.vertex_count; ++i) {
                    n.polygon.corners[i].roundness   = 0.0f;
                    n.polygon.corners[i].cap_tension = .70f;
                }
                changed = true;
            }
            if (ImGui::Selectable("Rounded")) {
                for (int i = 0; i < n.polygon.vertex_count; ++i) {
                    n.polygon.corners[i].roundness   = .30f;
                    n.polygon.corners[i].cap_tension = .45f;
                }
                changed = true;
            }
            ImGui::EndCombo();
        }
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            ImGui::SeparatorText(("Vertex " + std::to_string(i)).c_str());
            changed |= pointField("Position", n.polygon.vertices[i], {}, .5f);
            changed |= floatField("Roundness", n.polygon.corners[i].roundness, 0.0f, .35f, .08f, .005f, "%.3f");
            changed |= floatField("Cap tension", n.polygon.corners[i].cap_tension, .10f, .90f, .55f, .005f, "%.3f");
            changed |= floatField("Edge bulge", n.polygon.edges[i].bulge, -.35f, .35f, 0.0f, .005f, "%.3f");
            changed |= floatField("Edge in", n.polygon.edges[i].start, .08f, .45f, .28f, .005f, "%.3f");
            changed |= floatField("Edge out", n.polygon.edges[i].end, .08f, .45f, .28f, .005f, "%.3f");
            ImGui::PopID();
        }
    }

    ImGui::SeparatorText("Style");
    float color_value[3] = {n.style.color.red / 255.f, n.style.color.green / 255.f, n.style.color.blue / 255.f};
    if (ImGui::ColorEdit3("Color", color_value)) {
        n.style.color = {uint8_t(color_value[0] * 255), uint8_t(color_value[1] * 255), uint8_t(color_value[2] * 255)};
        changed       = true;
    }
    changed |= floatField("Opacity", n.style.opacity, 0.0f, 1.0f, 1.0f, .01f);
    changed |= ImGui::Checkbox("Fill", &n.style.fill);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Stroke", &n.style.stroke);
    if (n.style.stroke) changed |= floatField("Stroke width", n.style.stroke_width, 0.0f, 24.0f, 0.0f, .1f);

    ImGui::SeparatorText("Transform");
    const char* parent_label = n.parent_index < 0 ? "World" : s.nodes[n.parent_index].id.c_str();
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("Parent", parent_label)) {
        if (ImGui::Selectable("World", n.parent_index < 0)) n.parent_index = -1, changed = true;
        for (int index = 0; index < s.selected; ++index)
            if (ImGui::Selectable(s.nodes[index].id.c_str(), n.parent_index == index))
                n.parent_index = index, changed = true;
        ImGui::EndCombo();
    }
    changed |= pointField("Translate", n.transform.translation, {}, .5f);
    changed |= floatField("Rotation", n.transform.rotation_degrees, -180.0f, 180.0f, 0.0f, .5f);
    changed |= pointField("Scale", n.transform.scale, {1.0f, 1.0f}, .01f, -240.0f, 240.0f);
    changed |= pointField("Pivot", n.transform.pivot, {}, .5f);

    ImGui::SeparatorText("Motion");
    MotionPreview& motion = n.motion;
    changed |= ImGui::Checkbox("Enabled", &motion.enabled);
    ImGui::SameLine();
    ImGui::Checkbox("Preview playing", &view.motion.playing);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Loop", &motion.loop);
    const char* properties[] = {"Translate X", "Translate Y", "Rotation", "Scale X",   "Scale Y",
                                "Opacity",     "Circle R",    "Corner R", "Edge bulge"};
    ImGui::SetNextItemWidth(160.0f);
    changed |= ImGui::Combo("Property", &motion.property, properties, 9);
    ImGui::SetNextItemWidth(160.0f);
    changed |= ImGui::Combo("Type", reinterpret_cast<int*>(&motion.use_keyframes), "Loop\0Keyframes\0");
    if (!motion.use_keyframes) {
        ImGui::SetNextItemWidth(160.0f);
        changed |= ImGui::DragFloat("Amplitude", &motion.amplitude, .1f, -4096.0f, 4096.0f, "%.2f");
        ImGui::SetNextItemWidth(160.0f);
        changed |= ImGui::DragFloat("Period", &motion.period_seconds, .01f, .1f, 10.0f, "%.2f s");
        ImGui::SetNextItemWidth(160.0f);
        changed |= ImGui::DragFloat("Phase", &motion.phase_radians, .01f, -3.14159f, 3.14159f, "%.2f rad");
    } else {
        ImGui::SetNextItemWidth(160.0f);
        changed |=
            ImGui::DragFloat("Keyframe duration", &motion.keyframes.duration_seconds, .01f, .1f, 10.0f, "%.2f s");
        changed |= ImGui::Checkbox("Loop keyframes", &motion.keyframes.loop);
        for (int index = 0; index < motion.keyframes.count; ++index) {
            ImGui::PushID(index);
            ImGui::SetNextItemWidth(100.0f);
            changed |= ImGui::DragFloat("Time", &motion.keyframes.keyframes[index].time_seconds, .01f, 0.0f,
                                        motion.keyframes.duration_seconds, "%.2f s");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            changed |=
                ImGui::DragFloat("Value", &motion.keyframes.keyframes[index].value, .1f, -4096.0f, 4096.0f, "%.2f");
            ImGui::PopID();
        }
        if (motion.keyframes.count < pet::preset::kMaxMotionKeyframes && ImGui::Button("Add keyframe")) {
            pet::preset::MotionRange range{};
            float* target = motionTarget(n, motion.property, range);
            if (target != nullptr) {
                auto& frame            = motion.keyframes.keyframes[motion.keyframes.count];
                frame.time_seconds     = view.motion.time_seconds;
                frame.value            = *target;
                motion.keyframes.count = uint8_t(motion.keyframes.count + 1);
                changed                = true;
            }
        }
        if (motion.keyframes.count > 0 && ImGui::Button("Remove last keyframe")) {
            --motion.keyframes.count;
            changed = true;
        }
    }
    ImGui::SetNextItemWidth(160.0f);
    int easing_index = motion.easing == pet::preset::MotionEasing::Smoothstep ? 1 : 0;
    if (ImGui::Combo("Easing", &easing_index, "Linear\0Smoothstep\0")) {
        motion.easing = easing_index == 1 ? pet::preset::MotionEasing::Smoothstep : pet::preset::MotionEasing::Linear;
        changed       = true;
    }
    ImGui::SetNextItemWidth(160.0f);
    ImGui::DragFloat("Preview duration", &view.motion.duration_seconds, .01f, .1f, 10.0f, "%.2f s");
    ImGui::SetNextItemWidth(160.0f);
    ImGui::DragFloat("Preview time", &view.motion.time_seconds, .01f, 0.0f, view.motion.duration_seconds, "%.2f s");
    if (ImGui::Button("Restart preview")) view.motion.time_seconds = 0.0f;
    ImGui::SameLine();
    if (ImGui::Button("Blink (scale Y)")) {
        motion.property                   = 4;
        motion.use_keyframes              = true;
        motion.keyframes                  = {};
        motion.keyframes.loop             = true;
        motion.keyframes.duration_seconds = 2.5f;
        motion.keyframes.count            = 4;
        motion.keyframes.keyframes[0]     = {0.0f, 1.0f};
        motion.keyframes.keyframes[1]     = {0.08f, 0.12f};
        motion.keyframes.keyframes[2]     = {0.16f, 1.0f};
        motion.keyframes.keyframes[3]     = {1.0f, 1.0f};
        motion.enabled                    = true;
        view.motion.playing               = true;
        view.motion.time_seconds          = 0.0f;
        changed                           = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Ear twitch (rotation)")) {
        motion.property                   = 2;
        motion.use_keyframes              = true;
        motion.keyframes                  = {};
        motion.keyframes.loop             = true;
        motion.keyframes.duration_seconds = 3.0f;
        motion.keyframes.count            = 4;
        motion.keyframes.keyframes[0]     = {0.0f, 0.0f};
        motion.keyframes.keyframes[1]     = {0.12f, 8.0f};
        motion.keyframes.keyframes[2]     = {0.24f, 0.0f};
        motion.keyframes.keyframes[3]     = {1.0f, 0.0f};
        motion.enabled                    = true;
        view.motion.playing               = true;
        view.motion.time_seconds          = 0.0f;
        changed                           = true;
    }
    return changed;
}
int runCli(int argc, char** argv)
{
    if (argc < 3) return -1;
    const std::string mode = argv[1];
    if (mode != "--validate" && mode != "--roundtrip") return -1;
    Scene scene;
    std::string error;
    if (!load(scene, argv[2], error)) {
        fprintf(stderr, "kiwi_studio: %s\n", error.c_str());
        return 2;
    }
    printf("kiwi_studio: loaded %s (%zu nodes)\n", argv[2], scene.count);
    if (mode == "--roundtrip") {
        const char* output = argc >= 4 ? argv[3] : "/tmp/kiwi-studio-roundtrip.petface.json";
        if (!save(scene, output)) {
            fprintf(stderr, "kiwi_studio: could not write %s\n", output);
            return 2;
        }
        printf("kiwi_studio: wrote %s\n", output);
        if (!validateWithCompiler(output)) {
            fprintf(stderr, "kiwi_studio: %s", compilerLog().c_str());
            return 2;
        }
        printf("kiwi_studio: compiler accepted %s\n", output);
    }
    return 0;
}
}  // namespace
int main(int argc, char** argv)
{
    const int cli = runCli(argc, argv);
    if (cli >= 0) return cli;
    if (!glfwInit()) return 1;
    GLFWwindow* w = glfwCreateWindow(1200, 780, "Kiwi Studio", nullptr, nullptr);
    if (!w) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(w);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(w, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    Scene s;
    View view;
    History history;
    Scene transaction_start;
    bool transaction_active    = false;
    Pending pending            = Pending::None;
    bool pending_modal_open    = false;
    bool validate_requested    = false;
    bool validation_failed     = false;
    bool validation_modal_open = false;
    std::string validation_log;
    reset(s);
    bool running = true;
    while (running) {
        glfwPollEvents();
        if (glfwWindowShouldClose(w)) {
            glfwSetWindowShouldClose(w, GLFW_FALSE);
            if (s.dirty) {
                if (pending == Pending::None) pending = Pending::Quit;
            } else {
                running = false;
                break;
            }
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        Scene before          = s;
        bool shortcut_changed = false;
        const ImGuiIO& io     = ImGui::GetIO();
        const bool text_input = io.WantTextInput;
        if (text_input) {
            // Text fields consume their own keys; shortcuts stay inactive.
        } else {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                if (history.undoScene(s)) s.dirty = true;
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
                if (history.redoScene(s)) s.dirty = true;
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) saveScene(s);
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && duplicateSelected(s)) shortcut_changed = true;
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                if (deleteSelected(s))
                    shortcut_changed = true;
                else
                    s.status = "Cannot delete the last node or a node with children";
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F)) view.frame_requested = true;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) view.reset_requested = true;
        }
        ImGui::Begin("Kiwi Studio", nullptr, ImGuiWindowFlags_NoDecoration);
        if (view.motion.playing) {
            view.motion.time_seconds += ImGui::GetIO().DeltaTime;
            if (view.motion.time_seconds > view.motion.duration_seconds) {
                if (view.motion.loop) {
                    view.motion.time_seconds =
                        std::fmod(view.motion.time_seconds, std::max(0.1f, view.motion.duration_seconds));
                } else {
                    view.motion.time_seconds = view.motion.duration_seconds;
                    view.motion.playing      = false;
                }
            }
        }
        ImGui::Text("Kiwi Studio — %s%s", s.file_path.c_str(), s.dirty ? " *" : "");
        bool changed  = shortcut_changed;
        bool replaced = false;
        toolbar(s, view, history, pending, validate_requested, replaced);
        if (replaced) {
            history.undo.clear();
            history.redo.clear();
            transaction_active = false;
            s.dirty            = false;
        }
        ImGui::Separator();

        const ImVec2 available   = ImGui::GetContentRegionAvail();
        const float left_width   = 250.0f;
        const float right_width  = 340.0f;
        const float panel_height = std::max(240.0f, available.y - 34.0f);
        const float center_width = std::max(280.0f, available.x - left_width - right_width - 26.0f);

        ImGui::BeginChild("parts", {left_width, panel_height}, true);
        changed |= scenePanel(s);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("viewport", {center_width, panel_height}, false);
        canvas(s, view);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("inspector", {right_width, panel_height}, true);
        changed |= inspector(s, view);
        ImGui::EndChild();

        if (changed) s.dirty = true;
        if (validate_requested) {
            validate_requested = false;
            const bool saved   = !s.dirty || saveScene(s);
            if (saved) {
                if (validateWithCompiler(s.file_path)) {
                    s.status          = "Preset compiler accepted " + s.file_path;
                    validation_failed = false;
                } else {
                    validation_log    = compilerLog();
                    validation_failed = true;
                    s.status          = compilerMessage();
                }
            }
        }

        if (!s.status.empty()) ImGui::TextWrapped("%s", s.status.c_str());

        if (pending != Pending::None && !pending_modal_open) {
            ImGui::OpenPopup("Discard unsaved changes?");
            pending_modal_open = true;
        }
        if (ImGui::BeginPopupModal("Discard unsaved changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("The current scene has unsaved changes.");
            if (ImGui::Button("Discard")) {
                const Pending action = pending;
                pending              = Pending::None;
                pending_modal_open   = false;
                bool replaced_action = false;
                if (action == Pending::New) {
                    newScene(s);
                    replaced_action = true;
                } else if (action == Pending::Reset) {
                    reset(s);
                    replaced_action = true;
                } else if (action == Pending::Load) {
                    replaced_action = loadScene(s);
                } else if (action == Pending::Quit) {
                    running = false;
                }
                if (replaced_action) {
                    history.undo.clear();
                    history.redo.clear();
                    transaction_active = false;
                    s.dirty            = false;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                pending            = Pending::None;
                pending_modal_open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else if (pending != Pending::None && !ImGui::IsPopupOpen("Discard unsaved changes?")) {
            pending            = Pending::None;
            pending_modal_open = false;
        }

        if (validation_failed && !validation_modal_open) {
            ImGui::OpenPopup("Preset compiler rejected the scene");
            validation_modal_open = true;
        }
        if (ImGui::BeginPopupModal("Preset compiler rejected the scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("The preset compiler rejected the saved scene.");
            ImGui::BeginChild("validation-log", {620.0f, 260.0f}, true);
            ImGui::TextUnformatted(validation_log.c_str());
            ImGui::EndChild();
            if (ImGui::Button("Close")) {
                validation_failed     = false;
                validation_modal_open = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        } else if (!ImGui::IsPopupOpen("Preset compiler rejected the scene")) {
            validation_modal_open = false;
        }

        if (changed && !transaction_active) {
            transaction_start  = before;
            transaction_active = true;
        }
        if (transaction_active && !ImGui::IsAnyItemActive()) {
            history.record(transaction_start);
            transaction_active = false;
        }
        ImGui::End();
        ImGui::Render();
        int x, y;
        glfwGetFramebufferSize(w, &x, &y);
        glViewport(0, 0, x, y);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(w);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(w);
    glfwTerminate();
}
