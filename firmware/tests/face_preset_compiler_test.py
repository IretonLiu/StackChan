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
            source = directory / "generated_test.cpp"
            source.write_text(f'#include "{header}"\nint main() {{ return 0; }}\n')
            build = subprocess.run(["g++", "-std=c++17", "-I", str(FIRMWARE / "main"), str(source), "-c",
                                    "-o", str(directory / "generated_test.o")], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)

    def test_invalid_scenes_fail_with_actionable_errors(self):
        base = json.loads(VALID.read_text())
        cases = [
            ("unknown-kind", lambda value: value["nodes"][0].update(kind="spline"), "must be circle or rounded_polygon"),
            ("zero-circle", lambda value: value["nodes"][2]["circle"].update(radius=0), "must be positive"),
            ("concave", lambda value: value["nodes"][0]["rounded_polygon"].update(vertices=[[0, 0], [1, 0], [0.2, 0.2], [0, 1]]), "consistent winding"),
            ("unknown-field", lambda value: value.update(unexpected=True), "unknown field"),
            ("unknown-parent", lambda value: value["nodes"][1].update(parent="not-a-node"), "must name an earlier node"),
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


if __name__ == "__main__":
    unittest.main()
