#!/usr/bin/env python3
"""Authoring test: the studio must emit a scene the preset compiler accepts."""
import json
import pathlib
import subprocess
import sys

studio, compiler, scene, contract, out_dir = sys.argv[1:6]
out_dir = pathlib.Path(out_dir)
authored = out_dir / "kiwi-idle-authored.petface.json"
header = out_dir / "kiwi-idle-authored.h"

subprocess.run([studio, "--animation-author-selftest", scene, str(authored)], check=True)
data = json.loads(authored.read_text())
goal = data["animations"]["goals"][-1]
assert goal["id"] == "author-goal", goal
assert goal["pose"] == "author-pose", goal
program = goal["programs"][0]
assert program["id"] == "author-program", program
track = program["tracks"][0]
assert track["source"]["kind"] == "sine", track
assert track["node"] == "left-ear", track
assert data["animations"]["default_goal"] == "idle", data["animations"]["default_goal"]

subprocess.run(
    [sys.executable, compiler, str(authored), "--output", str(header), "--contract", contract],
    check=True,
)
text = header.read_text()
assert "author-goal" in text, "compiler did not emit the authored goal"
assert "author-pose" in text, "compiler did not emit the authored pose"
print("studio_animation_author_test: authored scene compiles")
