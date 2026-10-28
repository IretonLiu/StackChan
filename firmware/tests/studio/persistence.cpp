/* SPDX-License-Identifier: MIT */
#include "persistence.h"

#include "geometry.h"
#include "motion_model.h"
#include "validate.h"

#include <ArduinoJson.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <string>

namespace kiwi::studio {
namespace {
using namespace pet::shape;
}  // namespace

bool exportSvg(const SceneDoc& document, const char* filename)
{
    std::array<Affine2D, kMaxNodes> worlds{};
    if (!resolveWorlds(document, worlds)) return false;
    std::ofstream file(filename);
    if (!file) return false;
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"320\" height=\"240\" viewBox=\"0 0 320 240\">\n";
    file << "<rect width=\"320\" height=\"240\" fill=\"rgb(" << int(document.background.red) << ','
         << int(document.background.green) << ',' << int(document.background.blue) << ")\"/>\n";
    for (size_t index = 0; index < document.count; ++index) {
        if (!document.nodes[index].visible) continue;
        ShapePath path;
        if (!sampleNode(document.nodes[index], worlds[index], path)) return false;
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
             << "\" stroke-width=\"" << path.style.stroke_width << "\"/>\n";
    }
    file << "</svg>\n";
    file.flush();
    return static_cast<bool>(file);
}

bool save(const SceneDoc& document, const char* filename)
{
    JsonDocument d;
    auto r       = d.to<JsonObject>();
    r["version"] = 1;
    r["id"]      = document.id;
    // Emit preserved compiler pose targets so a round trip does not drop them. The text is
    // already valid JSON captured at load time, so deserialize it into the target member
    // rather than replacing the whole document.
    if (!document.poses_json.empty()) {
        const DeserializationError pose_error = deserializeJson(r["poses"], document.poses_json);
        if (pose_error) return false;
    }
    auto bg = r["background"].to<JsonArray>();
    bg.add(document.background.red);
    bg.add(document.background.green);
    bg.add(document.background.blue);
    auto ns = r["nodes"].to<JsonArray>();
    for (size_t i = 0; i < document.count; i++) {
        auto& o   = document.nodes[i];
        auto n    = ns.add<JsonObject>();
        n["id"]   = o.id;
        n["kind"] = o.kind == Kind::Circle ? "circle" : "rounded_polygon";
        if (o.parent_index >= 0) n["parent"] = document.nodes[o.parent_index].id;
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
        motion["property"]         = static_cast<int>(o.motion.property);
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
        if (!readNumber(object["opacity"], style.opacity)) {
            error = where + ".opacity: must be a finite number";
            return false;
        }
    }
    if (!object["stroke_width"].isNull()) {
        if (!readNumber(object["stroke_width"], style.stroke_width)) {
            error = where + ".stroke_width: must be a finite number";
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
        const int property = object["property"].as<int>();
        if (!motionPropertyValid(property)) {
            error = where + ".property: unknown motion channel";
            return false;
        }
        motion.property = static_cast<MotionProperty>(property);
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

bool load(SceneDoc& out_document, const char* filename, std::string& error)
{
    error.clear();
    SceneDoc candidate{};
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
    if (!rejectUnknown(root, {"version", "id", "background", "nodes", "poses"}, error, file_label)) return false;
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
    if (background.isNull() || !readColor(background, candidate.background)) {
        error = file_label + ".background: must be an RGB array of integers 0..255";
        return false;
    }
    JsonArrayConst nodes = root["nodes"].as<JsonArrayConst>();
    if (nodes.isNull() || nodes.size() == 0 || nodes.size() > kMaxNodes) {
        error = file_label + ".nodes: must contain 1.." + std::to_string(kMaxNodes) + " nodes";
        return false;
    }

    // Pose targets are compiler-owned for now; keep the raw text so saving the scene
    // cannot silently discard animation data the firmware depends on.
    std::string poses_json;
    if (!root["poses"].isNull()) {
        if (!root["poses"].is<JsonArrayConst>() && !root["poses"].is<JsonObjectConst>()) {
            error = file_label + ".poses: must be an array or object";
            return false;
        }
        serializeJson(root["poses"], poses_json);
    }

    candidate.id         = scene_id;
    candidate.poses_json = poses_json;
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
            if (!readNumber(circle["radius"], node.circle.radius)) {
                error = where + ".circle.radius: must be a finite number";
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
                if (!corner["roundness"].isNull() &&
                    !readNumber(corner["roundness"], node.polygon.corners[index].roundness)) {
                    error = corner_where + ".roundness: must be a finite number";
                    return false;
                }
                if (!corner["cap_tension"].isNull() &&
                    !readNumber(corner["cap_tension"], node.polygon.corners[index].cap_tension)) {
                    error = corner_where + ".cap_tension: must be a finite number";
                    return false;
                }
                const std::string edge_where = where + ".rounded_polygon.edges[" + std::to_string(index) + "]";
                if (!edges[index].is<JsonObjectConst>()) {
                    error = edge_where + ": must be an object";
                    return false;
                }
                JsonObjectConst edge = edges[index].as<JsonObjectConst>();
                if (!rejectUnknown(edge, {"bulge", "start", "end"}, error, edge_where)) return false;
                if (!edge["bulge"].isNull() && !readNumber(edge["bulge"], node.polygon.edges[index].bulge)) {
                    error = edge_where + ".bulge: must be a finite number";
                    return false;
                }
                if (!edge["start"].isNull() && !readNumber(edge["start"], node.polygon.edges[index].start)) {
                    error = edge_where + ".start: must be a finite number";
                    return false;
                }
                if (!edge["end"].isNull() && !readNumber(edge["end"], node.polygon.edges[index].end)) {
                    error = edge_where + ".end: must be a finite number";
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

    // All document invariants (ranges, convexity, hierarchy, sampleability, motion
    // applicability) are owned by validateScene(); load only parses JSON shape here.
    Problem problem;
    if (!validateScene(candidate, problem)) {
        error = file_label + ": " + problem.formatted();
        return false;
    }

    out_document = candidate;
    return true;
}

}  // namespace kiwi::studio
