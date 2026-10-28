#!/usr/bin/env python3
"""Regression tests for the host-only .petface.json compiler."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

FIRMWARE = Path(__file__).resolve().parents[1]
COMPILER = FIRMWARE / "tools" / "compile_face_presets.py"
VALID = FIRMWARE / "pet_faces" / "idle.petface.json"


def set_circle_radius(scene, radius):
    # The visual default may change primitive kinds; keep scalar validation
    # coverage independent of that authored scene layout.
    scene["nodes"][2] = {
        "id": "test-circle",
        "kind": "circle",
        "circle": {"center": [160, 120], "radius": radius},
    }


def duplicate_pose_target(scene):
    scene["poses"][0]["targets"].append(dict(scene["poses"][0]["targets"][0]))


def make_concave(scene):
    polygon = scene["nodes"][0]["rounded_polygon"]
    polygon["corners"] = [{"position": position} for position in [[0, 0], [1, 0], [0.2, 0.2], [0, 1]]]
    polygon["edges"] = [{}, {}, {}, {}]


class FacePresetCompilerTest(unittest.TestCase):
    def compile(self, source, output):
        return subprocess.run([sys.executable, str(COMPILER), str(source), "--output", str(output)],
                              capture_output=True, text=True)

    def test_valid_scene_generates_compilable_header(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            header = directory / "pet_generated" / "face_presets.generated.h"
            result = self.compile(VALID, header)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("k_idle_cat", header.read_text())
            self.assertIn("k_idle_cat_alert_targets", header.read_text())
            source = directory / "generated_test.cpp"
            source.write_text(f'#include "{header}"\nint main() {{ return 0; }}\n')
            build = subprocess.run(["g++", "-std=c++17", "-I", str(FIRMWARE / "main"), str(source), "-c",
                                    "-o", str(directory / "generated_test.o")], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)

    def test_invalid_scenes_fail_with_actionable_errors(self):
        base = json.loads(VALID.read_text())
        cases = [
            ("unknown-kind", lambda value: value["nodes"][0].update(kind="spline"), "must be circle or rounded_polygon"),
            ("zero-circle", lambda value: set_circle_radius(value, 0), "must be positive"),
            ("concave", make_concave, "consistent winding"),
            ("unknown-field", lambda value: value.update(unexpected=True), "unknown field"),
            ("unknown-parent", lambda value: value["nodes"][1].update(parent="not-a-node"), "must name an earlier node"),
            ("overflow", lambda value: set_circle_radius(value, 1e100), "must fit a finite C++ float"),
            ("invalid-style", lambda value: value["nodes"][0].update(style=[]), "must be an object"),
            ("invalid-pose-channel", lambda value: value["poses"][0]["targets"][0].update(channel="circle.radius"), "requires a circle node"),
            ("duplicate-pose-channel", duplicate_pose_target, "duplicates another target channel"),
        ]
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            for name, mutate, expected in cases:
                scene = json.loads(json.dumps(base)); mutate(scene)
                source, output = directory / f"{name}.petface.json", directory / f"{name}.h"
                source.write_text(json.dumps(scene))
                result = self.compile(source, output)
                self.assertEqual(result.returncode, 2, name)
                self.assertIn(expected, result.stderr, name)

    def test_duplicate_scene_ids_fail_at_schema_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            duplicate = directory / "duplicate.petface.json"
            duplicate.write_text(VALID.read_text())
            result = subprocess.run([sys.executable, str(COMPILER), str(VALID), str(duplicate), "--output", str(directory / "out.h")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn("duplicates scene id 'idle-cat'", result.stderr)


if __name__ == "__main__":
    unittest.main()
