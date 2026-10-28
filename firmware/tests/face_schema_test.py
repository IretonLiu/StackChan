#!/usr/bin/env python3
"""Verify the committed C++ face schema header matches its shared JSON source."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

FIRMWARE = Path(__file__).resolve().parents[1]
GENERATOR = FIRMWARE / "tools" / "generate_face_schema.py"
COMMITTED_HEADER = FIRMWARE / "main" / "pet" / "face_schema.generated.h"
SCHEMA = FIRMWARE / "pet_faces" / "face_schema.json"


class FaceSchemaTest(unittest.TestCase):
    def test_committed_header_matches_schema(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "face_schema.generated.h"
            result = subprocess.run(
                [sys.executable, str(GENERATOR), "--schema", str(SCHEMA), "--output", str(output)],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                output.read_text(),
                COMMITTED_HEADER.read_text(),
                "pet_faces/face_schema.json changed; run python3 tools/generate_face_schema.py",
            )

    def test_invalid_schema_fails_loudly(self):
        with tempfile.TemporaryDirectory() as directory:
            schema = Path(directory) / "face_schema.json"
            schema.write_text('{"version": 1}')
            result = subprocess.run(
                [sys.executable, str(GENERATOR), "--schema", str(schema), "--output", str(Path(directory) / "out.h")],
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 2)
            self.assertIn("missing field(s)", result.stderr)


if __name__ == "__main__":
    unittest.main()
