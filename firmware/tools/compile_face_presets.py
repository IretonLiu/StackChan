#!/usr/bin/env python3
"""Validate .petface.json scenes and emit a typed firmware registry header."""
import argparse
import json
import math
import re
import sys
from pathlib import Path

MAX_NODES = 16
MAX_POLYGON_VERTICES = 5
MAX_POSES = 8
MAX_POSE_TARGETS = 32
MAX_CXX_FLOAT = 3.4028234663852886e38
ID = re.compile(r"^[a-z][a-z0-9-]*$")

# Node motion channels that only apply to one primitive kind (mirrors the studio's
# motionPropertyApplies table).
MOTION_PROPERTY_KIND = {6: "circle", 7: "rounded_polygon", 8: "rounded_polygon"}

# channel: (generated enum name, required primitive kind, requires element index, minimum, maximum)
POSE_CHANNELS = {
    "transform.translation_x": ("TransformTranslationX", None, False, -4096, 4096),
    "transform.translation_y": ("TransformTranslationY", None, False, -4096, 4096),
    "transform.rotation_degrees": ("TransformRotationDegrees", None, False, -3600, 3600),
    "transform.scale_x": ("TransformScaleX", None, False, .01, 16),
    "transform.scale_y": ("TransformScaleY", None, False, .01, 16),
    "circle.center_x": ("CircleCenterX", "circle", False, -4096, 4096),
    "circle.center_y": ("CircleCenterY", "circle", False, -4096, 4096),
    "circle.radius": ("CircleRadius", "circle", False, .01, 4096),
    "rounded_polygon.vertex_x": ("PolygonVertexX", "rounded_polygon", True, -4096, 4096),
    "rounded_polygon.vertex_y": ("PolygonVertexY", "rounded_polygon", True, -4096, 4096),
    "rounded_polygon.corner_roundness": ("PolygonCornerRoundness", "rounded_polygon", True, 0, .35),
    "rounded_polygon.corner_cap_tension": ("PolygonCornerCapTension", "rounded_polygon", True, .10, .90),
    "rounded_polygon.edge_bulge": ("PolygonEdgeBulge", "rounded_polygon", True, -.35, .35),
    "rounded_polygon.edge_start": ("PolygonEdgeStart", "rounded_polygon", True, .08, .45),
    "rounded_polygon.edge_end": ("PolygonEdgeEnd", "rounded_polygon", True, .08, .45),
}


class SchemaError(ValueError):
    pass


def fail(where, message):
    raise SchemaError(f"{where}: {message}")


def obj(value, where):
    if not isinstance(value, dict):
        fail(where, "must be an object")
    return value


def number(value, where):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        fail(where, "must be a finite number")
    result = float(value)
    if abs(result) > MAX_CXX_FLOAT:
        fail(where, "must fit a finite C++ float")
    return result


def point(value, where):
    if not isinstance(value, list) or len(value) != 2:
        fail(where, "must be a two-number array")
    return (number(value[0], where + "[0]"), number(value[1], where + "[1]"))


def color(value, where):
    if not isinstance(value, list) or len(value) != 3:
        fail(where, "must be an RGB array")
    output = []
    for index, channel in enumerate(value):
        if isinstance(channel, bool) or not isinstance(channel, int) or not 0 <= channel <= 255:
            fail(f"{where}[{index}]", "must be an integer from 0 to 255")
        output.append(channel)
    return tuple(output)


def require_keys(value, allowed, where):
    unknown = set(value) - set(allowed)
    if unknown:
        fail(where, "unknown field(s): " + ", ".join(sorted(unknown)))


def parse_style(value, where):
    value = {} if value is None else obj(value, where)
    require_keys(value, {"color", "opacity", "stroke_width", "fill", "stroke"}, where)
    result = {"color": color(value.get("color", [248, 245, 237]), where + ".color"),
              "opacity": number(value.get("opacity", 1), where + ".opacity"),
              "stroke_width": number(value.get("stroke_width", 0), where + ".stroke_width"),
              "fill": value.get("fill", True), "stroke": value.get("stroke", False)}
    if not isinstance(result["fill"], bool) or not isinstance(result["stroke"], bool):
        fail(where, "fill and stroke must be booleans")
    if not 0 <= result["opacity"] <= 1 or result["stroke_width"] < 0:
        fail(where, "opacity must be [0, 1] and stroke_width non-negative")
    return result


def parse_transform(value, where):
    value = {} if value is None else obj(value, where)
    require_keys(value, {"translation", "rotation_degrees", "scale", "pivot"}, where)
    result = {"translation": point(value.get("translation", [0, 0]), where + ".translation"),
              "rotation": number(value.get("rotation_degrees", 0), where + ".rotation_degrees"),
              "scale": point(value.get("scale", [1, 1]), where + ".scale"),
              "pivot": point(value.get("pivot", [0, 0]), where + ".pivot")}
    if result["scale"][0] == 0 or result["scale"][1] == 0:
        fail(where + ".scale", "components must be non-zero")
    return result


def cross(a, b, c):
    return (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])



def parse_motion(value, where):
    if value is None:
        return None
    value = obj(value, where)
    require_keys(value, {"enabled", "loop", "use_keyframes", "property", "amplitude", "period_seconds",
                          "phase_radians", "duration_seconds", "easing", "keyframes"}, where)
    result = {}
    enabled = value.get("enabled", False)
    if not isinstance(enabled, bool):
        fail(where + ".enabled", "must be a boolean")
    result["enabled"] = enabled
    loop = value.get("loop", False)
    if not isinstance(loop, bool):
        fail(where + ".loop", "must be a boolean")
    result["loop"] = loop
    use_keyframes = value.get("use_keyframes", False)
    if not isinstance(use_keyframes, bool):
        fail(where + ".use_keyframes", "must be a boolean")
    result["use_keyframes"] = use_keyframes
    prop = value.get("property", 0)
    if not isinstance(prop, int) or not 0 <= prop <= 8:
        fail(where + ".property", "must be an integer 0..8")
    result["property"] = prop
    result["amplitude"] = number(value.get("amplitude", 0), where + ".amplitude")
    result["period_seconds"] = number(value.get("period_seconds", 1), where + ".period_seconds")
    result["phase_radians"] = number(value.get("phase_radians", 0), where + ".phase_radians")
    keyframes = value.get("keyframes", [])
    if not isinstance(keyframes, list):
        fail(where + ".keyframes", "must be an array")
    if len(keyframes) > 8:
        fail(where + ".keyframes", "must contain at most 8 keyframes")
    parsed_keyframes = []
    for index, raw in enumerate(keyframes):
        raw = obj(raw, f"{where}.keyframes[{index}]")
        require_keys(raw, {"time_seconds", "value"}, f"{where}.keyframes[{index}]")
        time = number(raw.get("time_seconds", 0), f"{where}.keyframes[{index}].time_seconds")
        val = number(raw.get("value", 0), f"{where}.keyframes[{index}].value")
        parsed_keyframes.append((time, val))
    result["keyframes"] = parsed_keyframes
    result["duration_seconds"] = number(value.get("duration_seconds", 1), where + ".duration_seconds")
    easing = value.get("easing", "linear")
    if easing not in ("linear", "smoothstep"):
        fail(where + ".easing", "must be linear or smoothstep")
    result["easing"] = easing
    return result

def parse_polygon(value, where):
    value = obj(value, where)
    require_keys(value, {"vertices", "corners", "edges"}, where)
    vertices = value.get("vertices")
    if not isinstance(vertices, list) or not 3 <= len(vertices) <= MAX_POLYGON_VERTICES:
        fail(where + ".vertices", f"must contain 3..{MAX_POLYGON_VERTICES} points")
    vertices = [point(vertex, f"{where}.vertices[{index}]") for index, vertex in enumerate(vertices)]
    turns = [cross(vertices[index - 1], vertices[index], vertices[(index + 1) % len(vertices)])
             for index in range(len(vertices))]
    if any(abs(turn) < 0.0001 for turn in turns) or any(turns[0] * turn <= 0 for turn in turns[1:]):
        fail(where + ".vertices", "must be a non-degenerate convex polygon with consistent winding")
    corners, edges = value.get("corners"), value.get("edges")
    if not isinstance(corners, list) or len(corners) != len(vertices):
        fail(where + ".corners", "must have one entry per vertex")
    if not isinstance(edges, list) or len(edges) != len(vertices):
        fail(where + ".edges", "must have one entry per vertex")
    parsed_corners, parsed_edges = [], []
    for index, raw in enumerate(corners):
        raw = obj(raw, f"{where}.corners[{index}]")
        require_keys(raw, {"roundness", "cap_tension"}, f"{where}.corners[{index}]")
        result = (number(raw.get("roundness", .08), f"{where}.corners[{index}].roundness"),
                  number(raw.get("cap_tension", .55), f"{where}.corners[{index}].cap_tension"))
        if not 0 <= result[0] <= .35 or not .10 <= result[1] <= .90:
            fail(f"{where}.corners[{index}]", "outside firmware-safe range")
        parsed_corners.append(result)
    for index, raw in enumerate(edges):
        raw = obj(raw, f"{where}.edges[{index}]")
        require_keys(raw, {"bulge", "start", "end"}, f"{where}.edges[{index}]")
        result = (number(raw.get("bulge", 0), f"{where}.edges[{index}].bulge"),
                  number(raw.get("start", .28), f"{where}.edges[{index}].start"),
                  number(raw.get("end", .28), f"{where}.edges[{index}].end"))
        if not -.35 <= result[0] <= .35 or not .08 <= result[1] <= .45 or not .08 <= result[2] <= .45:
            fail(f"{where}.edges[{index}]", "outside firmware-safe range")
        parsed_edges.append(result)
    return vertices, parsed_corners, parsed_edges


def parse_response(value, where):
    value = {} if value is None else obj(value, where)
    require_keys(value, {"frequency_hz", "damping_ratio", "maximum_step_seconds"}, where)
    result = {
        "frequency_hz": number(value.get("frequency_hz", 6), where + ".frequency_hz"),
        "damping_ratio": number(value.get("damping_ratio", 1), where + ".damping_ratio"),
        "maximum_step_seconds": number(value.get("maximum_step_seconds", 1 / 120), where + ".maximum_step_seconds"),
    }
    if not .01 <= result["frequency_hz"] <= 30:
        fail(where + ".frequency_hz", "must be from 0.01 to 30")
    if not 0 <= result["damping_ratio"] <= 4:
        fail(where + ".damping_ratio", "must be from 0 to 4")
    if not .001 <= result["maximum_step_seconds"] <= .1:
        fail(where + ".maximum_step_seconds", "must be from 0.001 to 0.1")
    return result


def parse_poses(value, nodes, where):
    if value is None:
        return []
    if not isinstance(value, list) or len(value) > MAX_POSES:
        fail(where, f"must contain at most {MAX_POSES} poses")
    node_indices = {node["id"]: index for index, node in enumerate(nodes)}
    # Base geometry is identical for every pose; build it once and copy per pose so a
    # vertex target in one pose cannot leak into the next pose's convexity check.
    base_polygon_vertices = [[[point[0], point[1]] for point in node.get("polygon", ([], [], []))[0]] for node in nodes]
    seen, poses = set(), []
    for pose_index, raw_pose in enumerate(value):
        pose_where = f"{where}[{pose_index}]"
        raw_pose = obj(raw_pose, pose_where)
        require_keys(raw_pose, {"id", "targets"}, pose_where)
        pose_id = raw_pose.get("id")
        if not isinstance(pose_id, str) or not ID.fullmatch(pose_id) or pose_id in seen:
            fail(pose_where + ".id", "must be a unique lowercase kebab-case id")
        seen.add(pose_id)
        targets = raw_pose.get("targets")
        if not isinstance(targets, list) or len(targets) > MAX_POSE_TARGETS:
            fail(pose_where + ".targets", f"must contain at most {MAX_POSE_TARGETS} targets")
        parsed_targets, seen_channels = [], set()
        polygon_vertices = [[[point[0], point[1]] for point in vertices] for vertices in base_polygon_vertices]
        for target_index, raw_target in enumerate(targets):
            target_where = f"{pose_where}.targets[{target_index}]"
            raw_target = obj(raw_target, target_where)
            require_keys(raw_target, {"node", "channel", "element", "value", "response"}, target_where)
            node_id = raw_target.get("node")
            if not isinstance(node_id, str) or node_id not in node_indices:
                fail(target_where + ".node", "must name a scene node")
            channel_name = raw_target.get("channel")
            if channel_name not in POSE_CHANNELS:
                fail(target_where + ".channel", "must name a supported animatable channel")
            enum_name, expected_kind, needs_element, low, high = POSE_CHANNELS[channel_name]
            node_index = node_indices[node_id]
            node = nodes[node_index]
            if expected_kind is not None and node["kind"] != expected_kind:
                fail(target_where + ".channel", f"requires a {expected_kind} node")
            element = raw_target.get("element", 0)
            if not isinstance(element, int) or isinstance(element, bool) or element < 0:
                fail(target_where + ".element", "must be a non-negative integer")
            if needs_element:
                if "element" not in raw_target:
                    fail(target_where + ".element", "is required for polygon controls")
                if element >= len(node["polygon"][0]):
                    fail(target_where + ".element", "must name an existing polygon vertex")
            elif element != 0:
                fail(target_where + ".element", "must be zero for this channel")
            target_value = number(raw_target.get("value"), target_where + ".value")
            if not low <= target_value <= high:
                fail(target_where + ".value", f"must be from {low} to {high}")
            key = (node_index, channel_name, element)
            if key in seen_channels:
                fail(target_where, "duplicates another target channel in this pose")
            seen_channels.add(key)
            if channel_name == "rounded_polygon.vertex_x":
                polygon_vertices[node_index][element][0] = target_value
            elif channel_name == "rounded_polygon.vertex_y":
                polygon_vertices[node_index][element][1] = target_value
            parsed_targets.append({"node_index": node_index, "enum_name": enum_name, "element": element,
                                   "value": target_value, "response": parse_response(raw_target.get("response"), target_where + ".response")})
        # A pose cannot compile a non-convex end-state; the runtime also checks
        # every interpolated state before it reaches the renderer.
        for node_index, node in enumerate(nodes):
            if node["kind"] != "rounded_polygon":
                continue
            vertices = polygon_vertices[node_index]
            turns = [cross(vertices[index - 1], vertices[index], vertices[(index + 1) % len(vertices)])
                     for index in range(len(vertices))]
            if any(abs(turn) < 0.0001 for turn in turns) or any(turns[0] * turn <= 0 for turn in turns[1:]):
                fail(pose_where + ".targets", "must preserve a non-degenerate convex polygon with consistent winding")
        poses.append({"id": pose_id, "targets": parsed_targets})
    return poses


def parse_scene(path):
    try:
        value = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise SchemaError(f"{path}: {error}") from error
    value = obj(value, str(path)); require_keys(value, {"version", "id", "background", "nodes", "poses"}, str(path))
    if value.get("version") != 1: fail(str(path) + ".version", "must be 1")
    scene_id = value.get("id")
    if not isinstance(scene_id, str) or not ID.fullmatch(scene_id): fail(str(path) + ".id", "must be a lowercase kebab-case id")
    nodes = value.get("nodes")
    if not isinstance(nodes, list) or not nodes or len(nodes) > MAX_NODES: fail(str(path) + ".nodes", f"must contain 1..{MAX_NODES} nodes")
    seen, parsed = set(), []
    for index, raw in enumerate(nodes):
        where = f"{path}.nodes[{index}]"; raw = obj(raw, where)
        require_keys(raw, {"id", "kind", "parent", "style", "transform", "circle", "rounded_polygon", "motion"}, where)
        node_id, kind = raw.get("id"), raw.get("kind")
        if not isinstance(node_id, str) or not ID.fullmatch(node_id) or node_id in seen: fail(where + ".id", "must be a unique lowercase kebab-case id")
        parent = raw.get("parent")
        if parent is not None and (not isinstance(parent, str) or parent not in seen):
            fail(where + ".parent", "must name an earlier node or be omitted")
        parent_index = -1 if parent is None else next(item for item, candidate in enumerate(parsed) if candidate["id"] == parent)
        seen.add(node_id)
        if kind not in ("circle", "rounded_polygon"): fail(where + ".kind", "must be circle or rounded_polygon")
        node = {"id": node_id, "parent_index": parent_index, "kind": kind, "style": parse_style(raw.get("style"), where + ".style"), "transform": parse_transform(raw.get("transform"), where + ".transform")}
        if kind == "circle":
            circle = obj(raw.get("circle"), where + ".circle"); require_keys(circle, {"center", "radius"}, where + ".circle")
            node["circle"] = (point(circle.get("center"), where + ".circle.center"), number(circle.get("radius"), where + ".circle.radius"))
            if node["circle"][1] <= 0: fail(where + ".circle.radius", "must be positive")
        else: node["polygon"] = parse_polygon(raw.get("rounded_polygon"), where + ".rounded_polygon")
        node["motion"] = parse_motion(raw.get("motion"), where + ".motion")
        if node["motion"] is not None:
            required_kind = MOTION_PROPERTY_KIND.get(node["motion"]["property"])
            if required_kind is not None and required_kind != node["kind"]:
                fail(where + ".motion.property", "does not apply to this node kind")
        parsed.append(node)
    poses = parse_poses(value.get("poses"), parsed, str(path) + ".poses")
    return {"id": scene_id, "background": color(value.get("background"), str(path) + ".background"), "nodes": parsed, "poses": poses}


def f(value):
    text = f"{value:.6g}"
    if "." not in text and "e" not in text and "E" not in text:
        text += ".0"
    return text + "f"
def v(value): return "{" + f(value[0]) + ", " + f(value[1]) + "}"
def c(value): return "{" + ", ".join(map(str, value)) + "}"

def emit(scenes, output):
    lines = ["// Generated by tools/compile_face_presets.py. Do not edit.", "#pragma once", "", "#include <pet/preset/face_pose_engine.h>", "", "namespace pet::preset::generated {", ""]
    for scene in scenes:
        name = scene["id"].replace("-", "_")
        lines.append(f"inline constexpr FacePresetNodeDefinition k_{name}_nodes[] = {{")
        for node in scene["nodes"]:
            s, t = node["style"], node["transform"]
            prefix = f'{{ShapeKind::{"Circle" if node["kind"] == "circle" else "RoundedPolygon"}, {{{c(s["color"])}, {f(s["opacity"])}, {f(s["stroke_width"])}, {str(s["fill"]).lower()}, {str(s["stroke"]).lower()}}}, {{{v(t["translation"])}, {f(t["rotation"])}, {v(t["scale"])}, {v(t["pivot"])}}}, {node["parent_index"]}, '
            motion = node.get("motion")
            if motion is None:
                motion_str = "{{}, 0, false, 0.0f}, MotionEasing::Linear"
            else:
                keyframes = motion["keyframes"]
                if keyframes:
                    kf_str = "{" + ", ".join("{" + f(t) + ", " + f(v) + "}" for t, v in keyframes) + "}"
                else:
                    kf_str = "{}"
                motion_str = "{" + kf_str + ", " + str(len(keyframes)) + ", " + str(motion["loop"]).lower() + ", " + f(motion["duration_seconds"]) + "}, MotionEasing::" + motion["easing"].capitalize()

            if node["kind"] == "circle":
                center, radius = node["circle"]
                lines.append(prefix + "{" + v(center) + ", " + f(radius) + "}, {}, " + motion_str + "},")
            else:
                vertices, corners, edges = node["polygon"]
                vs = ", ".join(v(x) for x in vertices)
                cs = ", ".join("{" + f(x[0]) + ", " + f(x[1]) + "}" for x in corners)
                es = ", ".join("{" + f(x[0]) + ", " + f(x[1]) + ", " + f(x[2]) + "}" for x in edges)
                lines.append(prefix + "{}, {" + str(len(vertices)) + ", {{" + vs + "}}, {{" + cs + "}}, {{" + es + "}}}, " + motion_str + "},")
        lines += ["};"]
        poses = scene["poses"]
        for pose in poses:
            target_name = f"k_{name}_{pose['id'].replace('-', '_')}_targets"
            lines.append(f"inline constexpr FacePoseTargetDefinition {target_name}[] = {{")
            for target in pose["targets"]:
                response = target["response"]
                lines.append("    {" + str(target["node_index"]) + ", FacePoseChannel::" + target["enum_name"] + ", " +
                             str(target["element"]) + ", " + f(target["value"]) + ", {" +
                             f(response["frequency_hz"]) + ", " + f(response["damping_ratio"]) + ", " +
                             f(response["maximum_step_seconds"]) + "}},")
            lines.append("};")
        if poses:
            lines.append(f"inline constexpr FacePoseDefinition k_{name}_poses[] = {{")
            for pose in poses:
                target_name = f"k_{name}_{pose['id'].replace('-', '_')}_targets"
                lines.append(f"    {{\"{pose['id']}\", {target_name}, {len(pose['targets'])}}},")
            lines.append("};")
        pose_pointer = f"k_{name}_poses" if poses else "nullptr"
        lines += [f"inline constexpr FacePresetDefinition k_{name}{{\"{scene['id']}\", {c(scene['background'])}, k_{name}_nodes, {len(scene['nodes'])}, {pose_pointer}, {len(poses)}}};", ""]
    lines.append("inline constexpr const FacePresetDefinition* kFacePresets[] = {")
    for scene in scenes:
        lines.append(f"    &k_{scene['id'].replace('-', '_')},")
    lines += ["};", f"inline constexpr uint8_t kFacePresetCount = {len(scenes)};", "", "}  // namespace pet::preset::generated", ""]
    output.parent.mkdir(parents=True, exist_ok=True); output.write_text("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("inputs", nargs="+", type=Path); parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        scenes, seen_ids = [], {}
        for path in args.inputs:
            scene = parse_scene(path)
            if scene["id"] in seen_ids:
                fail(f"{path}.id", f"duplicates scene id '{scene['id']}' already defined by {seen_ids[scene['id']]}")
            seen_ids[scene["id"]] = path
            scenes.append(scene)
        emit(scenes, args.output)
    except SchemaError as error: print(f"preset compiler: {error}", file=sys.stderr); return 2
    return 0

if __name__ == "__main__": sys.exit(main())
