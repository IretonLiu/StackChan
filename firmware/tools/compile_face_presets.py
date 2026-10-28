#!/usr/bin/env python3
"""Validate .petface.json scenes and emit a typed firmware registry header."""
import argparse
import json
import math
import re
import sys
from pathlib import Path

from face_schema import SCHEMA_PATH as DEFAULT_SCHEMA, SchemaError, load_schema

SCHEMA = load_schema(DEFAULT_SCHEMA)
MAX_NODES = SCHEMA["limits"]["max_nodes"]
MAX_POLYGON_CORNERS = SCHEMA["limits"]["max_polygon_corners"]
MAX_POSES = SCHEMA["limits"]["max_poses"]
MAX_POSE_TARGETS = SCHEMA["limits"]["max_pose_targets"]
MAX_ANIMATION_GOALS = SCHEMA["limits"]["max_animation_goals"]
MAX_MOTION_PROGRAMS = SCHEMA["limits"]["max_motion_programs"]
MAX_MOTION_TRACKS = SCHEMA["limits"]["max_motion_tracks"]
MAX_MOTION_KEYFRAMES = SCHEMA["limits"]["max_motion_keyframes"]
# The global contract selects the boot scene only. Animation policy belongs to
# each preset and compiles into the generic animation runtime.
DEFAULT_CONTRACT = Path(__file__).resolve().parents[1] / "pet_faces" / "pose_contract.json"
MAX_CXX_FLOAT = 3.4028234663852886e38
ID = re.compile(r"^[a-z][a-z0-9-]*$")

# channel: (generated enum name, required primitive kind, requires element index, minimum, maximum)
POSE_CHANNELS = {
    channel["id"]: (
        channel["enum"],
        channel["kind"],
        channel["element"],
        SCHEMA["ranges"][channel["range"]]["minimum"],
        SCHEMA["ranges"][channel["range"]]["maximum"],
    )
    for channel in SCHEMA["channels"]
}

POLYGON_DEFAULTS = SCHEMA["defaults"]
CORNER_ROUNDNESS_RANGE = SCHEMA["ranges"]["corner_roundness"]
CORNER_CAP_TENSION_RANGE = SCHEMA["ranges"]["corner_cap_tension"]
EDGE_BULGE_RANGE = SCHEMA["ranges"]["edge_bulge"]
EDGE_HANDLE_RANGE = SCHEMA["ranges"]["edge_handle"]


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


def parse_contract(path):
    try:
        value = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise SchemaError(f"{path}: {error}") from error
    value = obj(value, str(path))
    require_keys(value, {"version", "default_scene"}, str(path))
    if value.get("version") != 1:
        fail(str(path) + ".version", "must be 1")
    default_scene = value.get("default_scene")
    if not isinstance(default_scene, str) or not ID.fullmatch(default_scene):
        fail(str(path) + ".default_scene", "must be a lowercase kebab-case id")
    return {"default_scene": default_scene}


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
    """A rounded polygon is a ring of corners joined by edges.

    A corner is the smallest primitive: it carries its joint position plus the
    rounding and cap tension of the patch. Vertices are derived from the joint
    positions and are not authored. The edge at index i joins corner i to corner
    (i + 1) % count.
    """
    value = obj(value, where)
    require_keys(value, {"corners", "edges"}, where)
    corners = value.get("corners")
    if not isinstance(corners, list) or not 3 <= len(corners) <= MAX_POLYGON_CORNERS:
        fail(where + ".corners", f"must contain 3..{MAX_POLYGON_CORNERS} corners")
    positions, parsed_corners = [], []
    for index, raw in enumerate(corners):
        corner_where = f"{where}.corners[{index}]"
        raw = obj(raw, corner_where)
        require_keys(raw, {"position", "roundness", "cap_tension"}, corner_where)
        if raw.get("position") is None:
            fail(corner_where + ".position", "is required")
        positions.append(point(raw.get("position"), corner_where + ".position"))
        result = (number(raw.get("roundness", POLYGON_DEFAULTS["corner_roundness"]), corner_where + ".roundness"),
                  number(raw.get("cap_tension", POLYGON_DEFAULTS["corner_cap_tension"]),
                         corner_where + ".cap_tension"))
        if not CORNER_ROUNDNESS_RANGE["minimum"] <= result[0] <= CORNER_ROUNDNESS_RANGE["maximum"] or \
                not CORNER_CAP_TENSION_RANGE["minimum"] <= result[1] <= CORNER_CAP_TENSION_RANGE["maximum"]:
            fail(corner_where, "outside firmware-safe range")
        parsed_corners.append(result)
    turns = [cross(positions[index - 1], positions[index], positions[(index + 1) % len(positions)])
             for index in range(len(positions))]
    if any(abs(turn) < 0.0001 for turn in turns) or any(turns[0] * turn <= 0 for turn in turns[1:]):
        fail(where + ".corners", "positions must form a non-degenerate convex polygon with consistent winding")
    edges = value.get("edges")
    if not isinstance(edges, list) or len(edges) != len(positions):
        fail(where + ".edges", "must have one entry per corner")
    parsed_edges = []
    for index, raw in enumerate(edges):
        raw = obj(raw, f"{where}.edges[{index}]")
        require_keys(raw, {"bulge", "start", "end"}, f"{where}.edges[{index}]")
        result = (number(raw.get("bulge", POLYGON_DEFAULTS["edge_bulge"]), f"{where}.edges[{index}].bulge"),
                  number(raw.get("start", POLYGON_DEFAULTS["edge_handle"]), f"{where}.edges[{index}].start"),
                  number(raw.get("end", POLYGON_DEFAULTS["edge_handle"]), f"{where}.edges[{index}].end"))
        if not EDGE_BULGE_RANGE["minimum"] <= result[0] <= EDGE_BULGE_RANGE["maximum"] or \
                not EDGE_HANDLE_RANGE["minimum"] <= result[1] <= EDGE_HANDLE_RANGE["maximum"] or \
                not EDGE_HANDLE_RANGE["minimum"] <= result[2] <= EDGE_HANDLE_RANGE["maximum"]:
            fail(f"{where}.edges[{index}]", "outside firmware-safe range")
        parsed_edges.append(result)
    return positions, parsed_corners, parsed_edges


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
        value = []
    if not isinstance(value, list) or len(value) > MAX_POSES:
        fail(where, f"must contain at most {MAX_POSES} poses")
    node_indices = {node["id"]: index for index, node in enumerate(nodes)}
    # Base geometry is identical for every pose; build it once and copy per pose so a
    # corner-joint target in one pose cannot leak into the next pose's convexity check.
    base_polygon_positions = [[[point[0], point[1]] for point in node.get("polygon", ([], [], []))[0]] for node in nodes]
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
        polygon_positions = [[[point[0], point[1]] for point in positions] for positions in base_polygon_positions]
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
                    fail(target_where + ".element", "must name an existing polygon corner")
            elif element != 0:
                fail(target_where + ".element", "must be zero for this channel")
            target_value = number(raw_target.get("value"), target_where + ".value")
            if not low <= target_value <= high:
                fail(target_where + ".value", f"must be from {low} to {high}")
            key = (node_index, channel_name, element)
            if key in seen_channels:
                fail(target_where, "duplicates another target channel in this pose")
            seen_channels.add(key)
            if channel_name == "rounded_polygon.corner_x":
                polygon_positions[node_index][element][0] = target_value
            elif channel_name == "rounded_polygon.corner_y":
                polygon_positions[node_index][element][1] = target_value
            parsed_targets.append({"node_index": node_index, "enum_name": enum_name, "element": element,
                                   "value": target_value, "response": parse_response(raw_target.get("response"), target_where + ".response")})
        # A pose cannot compile a non-convex end-state; the runtime also checks
        # every interpolated state before it reaches the renderer.
        for node_index, node in enumerate(nodes):
            if node["kind"] != "rounded_polygon":
                continue
            positions = polygon_positions[node_index]
            turns = [cross(positions[index - 1], positions[index], positions[(index + 1) % len(positions)])
                     for index in range(len(positions))]
            if any(abs(turn) < 0.0001 for turn in turns) or any(turns[0] * turn <= 0 for turn in turns[1:]):
                fail(pose_where + ".targets", "must preserve a non-degenerate convex polygon with consistent winding")
        poses.append({"id": pose_id, "targets": parsed_targets})
    return poses


def parse_animations(value, nodes, poses, where):
    value = obj(value, where)
    require_keys(value, {"default_goal", "goals"}, where)
    default_goal = value.get("default_goal")
    if not isinstance(default_goal, str) or not ID.fullmatch(default_goal):
        fail(where + ".default_goal", "must be a lowercase kebab-case id")
    goals = value.get("goals")
    if not isinstance(goals, list) or not 1 <= len(goals) <= MAX_ANIMATION_GOALS:
        fail(where + ".goals", f"must contain 1..{MAX_ANIMATION_GOALS} goals")
    node_indices = {node["id"]: index for index, node in enumerate(nodes)}
    pose_channels = {
        pose["id"]: {(target["node_index"], target["enum_name"], target["element"]) for target in pose["targets"]}
        for pose in poses
    }
    events = {"pet": 1, "shake": 2}
    parsed, seen = [], set()
    for goal_index, raw_goal in enumerate(goals):
        goal_where = f"{where}.goals[{goal_index}]"
        raw_goal = obj(raw_goal, goal_where)
        require_keys(raw_goal, {"id", "pose", "events", "priority", "duration_ms", "programs"}, goal_where)
        goal_id = raw_goal.get("id")
        if not isinstance(goal_id, str) or not ID.fullmatch(goal_id) or goal_id in seen:
            fail(goal_where + ".id", "must be a unique lowercase kebab-case id")
        seen.add(goal_id)
        pose_id = raw_goal.get("pose")
        if not isinstance(pose_id, str) or pose_id not in pose_channels:
            fail(goal_where + ".pose", "must name a defined pose")
        raw_events = raw_goal.get("events")
        if not isinstance(raw_events, list):
            fail(goal_where + ".events", "must be an array")
        event_mask = 0
        for event_index, event in enumerate(raw_events):
            if not isinstance(event, str) or event not in events:
                fail(f"{goal_where}.events[{event_index}]", "must be pet or shake")
            if event_mask & events[event]:
                fail(f"{goal_where}.events[{event_index}]", "duplicates another event")
            event_mask |= events[event]
        goal_priority = raw_goal.get("priority", 0)
        if isinstance(goal_priority, bool) or not isinstance(goal_priority, int) or not 0 <= goal_priority <= 255:
            fail(goal_where + ".priority", "must be an integer from 0 to 255")
        duration_ms = raw_goal.get("duration_ms")
        if isinstance(duration_ms, bool) or not isinstance(duration_ms, int) or duration_ms < 0:
            fail(goal_where + ".duration_ms", "must be a non-negative integer")
        if event_mask and duration_ms == 0:
            fail(goal_where + ".duration_ms", "must be positive for an event goal")
        if not event_mask and duration_ms != 0:
            fail(goal_where + ".duration_ms", "must be zero when no event activates the goal")
        programs = raw_goal.get("programs")
        if not isinstance(programs, list) or len(programs) > MAX_MOTION_PROGRAMS:
            fail(goal_where + ".programs", f"must contain at most {MAX_MOTION_PROGRAMS} programs")
        parsed_programs, program_ids = [], set()
        for program_index, raw_program in enumerate(programs):
            program_where = f"{goal_where}.programs[{program_index}]"
            raw_program = obj(raw_program, program_where)
            require_keys(raw_program, {"id", "duration_seconds", "loop", "priority", "tracks"}, program_where)
            program_id = raw_program.get("id")
            if not isinstance(program_id, str) or not ID.fullmatch(program_id) or program_id in program_ids:
                fail(program_where + ".id", "must be a unique lowercase kebab-case id")
            program_ids.add(program_id)
            duration = number(raw_program.get("duration_seconds"), program_where + ".duration_seconds")
            if not .001 <= duration <= 600:
                fail(program_where + ".duration_seconds", "must be from 0.001 to 600")
            loop = raw_program.get("loop")
            if not isinstance(loop, bool):
                fail(program_where + ".loop", "must be a boolean")
            priority = raw_program.get("priority", 0)
            if isinstance(priority, bool) or not isinstance(priority, int) or not 0 <= priority <= 255:
                fail(program_where + ".priority", "must be an integer from 0 to 255")
            tracks = raw_program.get("tracks")
            if not isinstance(tracks, list) or not 1 <= len(tracks) <= MAX_MOTION_TRACKS:
                fail(program_where + ".tracks", f"must contain 1..{MAX_MOTION_TRACKS} tracks")
            parsed_tracks = []
            seen_channels = set()
            for track_index, raw_track in enumerate(tracks):
                track_where = f"{program_where}.tracks[{track_index}]"
                raw_track = obj(raw_track, track_where)
                require_keys(raw_track, {"node", "channel", "element", "priority", "response", "source"}, track_where)
                node_id = raw_track.get("node")
                if not isinstance(node_id, str) or node_id not in node_indices:
                    fail(track_where + ".node", "must name a scene node")
                channel_name = raw_track.get("channel")
                if channel_name not in POSE_CHANNELS:
                    fail(track_where + ".channel", "must name a supported animatable channel")
                enum_name, expected_kind, needs_element, low, high = POSE_CHANNELS[channel_name]
                node_index = node_indices[node_id]
                if expected_kind is not None and nodes[node_index]["kind"] != expected_kind:
                    fail(track_where + ".channel", f"requires a {expected_kind} node")
                element = raw_track.get("element", 0)
                if not isinstance(element, int) or isinstance(element, bool) or element < 0:
                    fail(track_where + ".element", "must be a non-negative integer")
                if needs_element:
                    if "element" not in raw_track or element >= len(nodes[node_index]["polygon"][0]):
                        fail(track_where + ".element", "must name an existing polygon vertex")
                elif element != 0:
                    fail(track_where + ".element", "must be zero for this channel")
                channel_key = (node_index, enum_name, element)
                if channel_key in seen_channels:
                    fail(track_where, "duplicates another track channel in this program")
                seen_channels.add(channel_key)
                if channel_key not in pose_channels[pose_id]:
                    fail(track_where, "must target a channel declared by the goal pose")
                track_priority = raw_track.get("priority", 0)
                if isinstance(track_priority, bool) or not isinstance(track_priority, int) or not 0 <= track_priority <= 255:
                    fail(track_where + ".priority", "must be an integer from 0 to 255")
                source = obj(raw_track.get("source"), track_where + ".source")
                source_kind = source.get("kind")
                if source_kind == "keyframes":
                    require_keys(source, {"kind", "keyframes", "easing"}, track_where + ".source")
                    keyframes = source.get("keyframes")
                    if not isinstance(keyframes, list) or not 2 <= len(keyframes) <= MAX_MOTION_KEYFRAMES:
                        fail(track_where + ".source.keyframes", f"must contain 2..{MAX_MOTION_KEYFRAMES} keyframes")
                    parsed_keyframes, previous_time = [], -1.0
                    for frame_index, frame in enumerate(keyframes):
                        frame_where = f"{track_where}.source.keyframes[{frame_index}]"
                        frame = obj(frame, frame_where)
                        require_keys(frame, {"time_seconds", "value"}, frame_where)
                        time = number(frame.get("time_seconds"), frame_where + ".time_seconds")
                        frame_value = number(frame.get("value"), frame_where + ".value")
                        if time < 0 or time <= previous_time or time > duration or not low <= frame_value <= high:
                            fail(frame_where, "must be ordered, within program duration, and inside the channel range")
                        previous_time = time
                        parsed_keyframes.append((time, frame_value))
                    if parsed_keyframes[0][0] != 0:
                        fail(track_where + ".source.keyframes[0].time_seconds", "must be zero")
                    easing = source.get("easing", "linear")
                    if easing not in ("linear", "smoothstep"):
                        fail(track_where + ".source.easing", "must be linear or smoothstep")
                    parsed_source = {"kind": "keyframes", "keyframes": parsed_keyframes, "easing": easing}
                elif source_kind == "sine":
                    require_keys(source, {"kind", "center", "amplitude", "period_seconds", "phase_radians"}, track_where + ".source")
                    center = number(source.get("center"), track_where + ".source.center")
                    amplitude = number(source.get("amplitude"), track_where + ".source.amplitude")
                    period = number(source.get("period_seconds"), track_where + ".source.period_seconds")
                    phase = number(source.get("phase_radians", 0), track_where + ".source.phase_radians")
                    if amplitude < 0 or period <= 0 or center - amplitude < low or center + amplitude > high:
                        fail(track_where + ".source", "must remain inside the channel range with a positive period")
                    parsed_source = {"kind": "sine", "center": center, "amplitude": amplitude, "period_seconds": period, "phase_radians": phase}
                else:
                    fail(track_where + ".source.kind", "must be keyframes or sine")
                parsed_tracks.append({"node_index": node_index, "enum_name": enum_name, "element": element,
                                      "priority": track_priority, "response": parse_response(raw_track.get("response"), track_where + ".response"),
                                      "source": parsed_source})
            parsed_programs.append({"id": program_id, "duration_seconds": duration, "loop": loop,
                                    "priority": priority, "tracks": parsed_tracks})
        parsed.append({"id": goal_id, "pose": pose_id, "event_mask": event_mask, "priority": goal_priority,
                       "duration_ms": duration_ms, "programs": parsed_programs})
    if default_goal not in seen:
        fail(where + ".default_goal", "must name a defined goal")
    return {"default_goal": default_goal, "goals": parsed}


def parse_scene(path):
    try:
        value = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as error:
        raise SchemaError(f"{path}: {error}") from error
    value = obj(value, str(path)); require_keys(value, {"version", "id", "background", "nodes", "poses", "animations"}, str(path))
    # Version 2 dropped nodes[].motion; version 1 scenes without that block remain valid.
    if value.get("version") not in (1, 2): fail(str(path) + ".version", "must be 1 or 2")
    scene_id = value.get("id")
    if not isinstance(scene_id, str) or not ID.fullmatch(scene_id): fail(str(path) + ".id", "must be a lowercase kebab-case id")
    nodes = value.get("nodes")
    if not isinstance(nodes, list) or not nodes or len(nodes) > MAX_NODES: fail(str(path) + ".nodes", f"must contain 1..{MAX_NODES} nodes")
    seen, parsed = set(), []
    for index, raw in enumerate(nodes):
        where = f"{path}.nodes[{index}]"; raw = obj(raw, where)
        if "motion" in raw:
            fail(where + ".motion", "per-node motion was removed in scene version 2; express it as an animation track")
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
    poses = parse_poses(value.get("poses"), parsed, str(path) + ".poses")
    animations = parse_animations(value.get("animations"), parsed, poses, str(path) + ".animations")
    return {"id": scene_id, "background": color(value.get("background"), str(path) + ".background"), "nodes": parsed, "poses": poses, "animations": animations}


def f(value):
    text = f"{value:.6g}"
    if "." not in text and "e" not in text and "E" not in text:
        text += ".0"
    return text + "f"
def v(value): return "{" + f(value[0]) + ", " + f(value[1]) + "}"
def c(value): return "{" + ", ".join(map(str, value)) + "}"

def emit(scenes, output, contract):
    lines = ["// Generated by tools/compile_face_presets.py. Do not edit.", "#pragma once", "",
             "#include <pet/preset/face_animation_engine.h>", "#include <cstdint>", "",
             "namespace pet::preset::generated {", "",
             f'inline constexpr const char* kDefaultFacePresetId = "{contract["default_scene"]}";',
             ""]
    for scene in scenes:
        name = scene["id"].replace("-", "_")
        lines.append(f"inline constexpr FacePresetNodeDefinition k_{name}_nodes[] = {{")
        for node in scene["nodes"]:
            s, t = node["style"], node["transform"]
            prefix = f'{{ShapeKind::{"Circle" if node["kind"] == "circle" else "RoundedPolygon"}, {{{c(s["color"])}, {f(s["opacity"])}, {f(s["stroke_width"])}, {str(s["fill"]).lower()}, {str(s["stroke"]).lower()}}}, {{{v(t["translation"])}, {f(t["rotation"])}, {v(t["scale"])}, {v(t["pivot"])}}}, {node["parent_index"]}, '
            if node["kind"] == "circle":
                center, radius = node["circle"]
                lines.append(prefix + "{" + v(center) + ", " + f(radius) + "}, {}},")
            else:
                positions, corners, edges = node["polygon"]
                ps = ", ".join(v(x) for x in positions)
                cs = ", ".join("{" + f(x[0]) + ", " + f(x[1]) + "}" for x in corners)
                es = ", ".join("{" + f(x[0]) + ", " + f(x[1]) + ", " + f(x[2]) + "}" for x in edges)
                lines.append(prefix + "{}, {" + str(len(positions)) + ", {{" + ps + "}}, {{" + cs + "}}, {{" + es + "}}}},")
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
        animations = scene["animations"]
        for goal in animations["goals"]:
            goal_name = f"k_{name}_{goal['id'].replace('-', '_')}"
            for program in goal["programs"]:
                program_name = f"{goal_name}_{program['id'].replace('-', '_')}"
                for track_index, track in enumerate(program["tracks"]):
                    if track["source"]["kind"] != "keyframes":
                        continue
                    keyframe_name = f"{program_name}_track_{track_index}_keyframes"
                    frames = ", ".join("{" + f(time) + ", " + f(value) + "}" for time, value in track["source"]["keyframes"])
                    lines.append(f"inline constexpr FaceMotionKeyframe {keyframe_name}[] = {{{frames}}};")
                lines.append(f"inline constexpr FaceMotionTrackDefinition {program_name}_tracks[] = {{")
                for track_index, track in enumerate(program["tracks"]):
                    response = track["response"]
                    source = track["source"]
                    if source["kind"] == "keyframes":
                        keyframes = f"{program_name}_track_{track_index}_keyframes"
                        source_text = (f"FaceMotionSource::Keyframes, {keyframes}, {len(source['keyframes'])}, "
                                       f"MotionEasing::{source['easing'].capitalize()}, 0.0f, 0.0f, 1.0f, 0.0f")
                    else:
                        source_text = (f"FaceMotionSource::Sine, nullptr, 0, MotionEasing::Linear, {f(source['center'])}, "
                                       f"{f(source['amplitude'])}, {f(source['period_seconds'])}, {f(source['phase_radians'])}")
                    lines.append("    {" + str(track["node_index"]) + ", FacePoseChannel::" + track["enum_name"] + ", " +
                                 str(track["element"]) + ", " + str(track["priority"]) + ", {" +
                                 f(response["frequency_hz"]) + ", " + f(response["damping_ratio"]) + ", " +
                                 f(response["maximum_step_seconds"]) + "}, " + source_text + "},")
                lines.append("};")
            if goal["programs"]:
                lines.append(f"inline constexpr FaceMotionProgramDefinition {goal_name}_programs[] = {{")
                for program in goal["programs"]:
                    program_name = f"{goal_name}_{program['id'].replace('-', '_')}"
                    lines.append(f"    {{\"{program['id']}\", {program_name}_tracks, {len(program['tracks'])}, {f(program['duration_seconds'])}, {str(program['loop']).lower()}, {program['priority']}}},")
                lines.append("};")
        lines.append(f"inline constexpr FaceAnimationGoalDefinition k_{name}_animation_goals[] = {{")
        for goal in animations["goals"]:
            goal_name = f"k_{name}_{goal['id'].replace('-', '_')}"
            program_pointer = f"{goal_name}_programs" if goal["programs"] else "nullptr"
            lines.append(f"    {{\"{goal['id']}\", \"{goal['pose']}\", static_cast<FaceAnimationEvent>({goal['event_mask']}), {goal['priority']}, {goal['duration_ms']}, {program_pointer}, {len(goal['programs'])}}},")
        lines.append("};")
        lines += [f"inline constexpr FacePresetDefinition k_{name}{{\"{scene['id']}\", {c(scene['background'])}, k_{name}_nodes, {len(scene['nodes'])}, {pose_pointer}, {len(poses)}, k_{name}_animation_goals, {len(animations['goals'])}, \"{animations['default_goal']}\"}};", ""]
    lines.append("inline constexpr const FacePresetDefinition* kFacePresets[] = {")
    for scene in scenes:
        lines.append(f"    &k_{scene['id'].replace('-', '_')},")
    lines += ["};", f"inline constexpr uint8_t kFacePresetCount = {len(scenes)};", "", "}  // namespace pet::preset::generated", ""]
    output.parent.mkdir(parents=True, exist_ok=True); output.write_text("\n".join(lines))


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("inputs", nargs="+", type=Path); parser.add_argument("--output", required=True, type=Path); parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    args = parser.parse_args()
    try:
        contract = parse_contract(args.contract)
        scenes, seen_ids = [], {}
        for path in args.inputs:
            scene = parse_scene(path)
            if scene["id"] in seen_ids:
                fail(f"{path}.id", f"duplicates scene id '{scene['id']}' already defined by {seen_ids[scene['id']]}")
            seen_ids[scene["id"]] = path
            scenes.append(scene)
        if contract["default_scene"] not in seen_ids:
            fail(str(args.contract) + ".default_scene", f"must name a compiled scene, but no scene with id '{contract['default_scene']}' was given")
        emit(scenes, args.output, contract)
    except SchemaError as error: print(f"preset compiler: {error}", file=sys.stderr); return 2
    return 0

if __name__ == "__main__": sys.exit(main())
