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
MAX_CXX_FLOAT = 3.4028234663852886e38
ID = re.compile(r"^[a-z][a-z0-9-]*$")


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


def parse_scene(path):
    try:
        value = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise SchemaError(f"{path}: {error}") from error
    value = obj(value, str(path)); require_keys(value, {"version", "id", "background", "nodes"}, str(path))
    if value.get("version") != 1: fail(str(path) + ".version", "must be 1")
    scene_id = value.get("id")
    if not isinstance(scene_id, str) or not ID.fullmatch(scene_id): fail(str(path) + ".id", "must be a lowercase kebab-case id")
    nodes = value.get("nodes")
    if not isinstance(nodes, list) or not nodes or len(nodes) > MAX_NODES: fail(str(path) + ".nodes", f"must contain 1..{MAX_NODES} nodes")
    seen, parsed = set(), []
    for index, raw in enumerate(nodes):
        where = f"{path}.nodes[{index}]"; raw = obj(raw, where)
        require_keys(raw, {"id", "kind", "parent", "style", "transform", "circle", "rounded_polygon"}, where)
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
        parsed.append(node)
    return {"id": scene_id, "background": color(value.get("background"), str(path) + ".background"), "nodes": parsed}


def f(value):
    text = f"{value:.6g}"
    if "." not in text and "e" not in text and "E" not in text:
        text += ".0"
    return text + "f"
def v(value): return "{" + f(value[0]) + ", " + f(value[1]) + "}"
def c(value): return "{" + ", ".join(map(str, value)) + "}"

def emit(scenes, output):
    lines = ["// Generated by tools/compile_face_presets.py. Do not edit.", "#pragma once", "", "#include <pet/preset/face_preset.h>", "", "namespace pet::preset::generated {", ""]
    for scene in scenes:
        name = scene["id"].replace("-", "_")
        lines.append(f"inline constexpr FacePresetNodeDefinition k_{name}_nodes[] = {{")
        for node in scene["nodes"]:
            s, t = node["style"], node["transform"]
            prefix = f'{{ShapeKind::{"Circle" if node["kind"] == "circle" else "RoundedPolygon"}, {{{c(s["color"])}, {f(s["opacity"])}, {f(s["stroke_width"])}, {str(s["fill"]).lower()}, {str(s["stroke"]).lower()}}}, {{{v(t["translation"])}, {f(t["rotation"])}, {v(t["scale"])}, {v(t["pivot"])}}}, {node["parent_index"]}, '
            if node["kind"] == "circle":
                center, radius = node["circle"]; lines.append(prefix + f"{{{v(center)}, {f(radius)}}}, {{}}}},")
            else:
                vertices, corners, edges = node["polygon"]
                vs = ", ".join(v(x) for x in vertices); cs = ", ".join("{" + f(x[0]) + ", " + f(x[1]) + "}" for x in corners); es = ", ".join("{" + f(x[0]) + ", " + f(x[1]) + ", " + f(x[2]) + "}" for x in edges)
                lines.append(prefix + "{{}, 0.0f}, {" + str(len(vertices)) + ", {{" + vs + "}}, {{" + cs + "}}, {{" + es + "}}}},")
        lines += ["};", f"inline constexpr FacePresetDefinition k_{name}{{\"{scene['id']}\", {c(scene['background'])}, k_{name}_nodes, {len(scene['nodes'])}}};", ""]
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
