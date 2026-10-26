#!/usr/bin/env python3
"""Build-contract tests for the isolated Pet Base firmware image.

Run after ``tools/pet_firmware_base.sh build``. These intentionally verify
architectural invariants rather than UI text, log output, or implementation
call ordering.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import unittest


parser = argparse.ArgumentParser(add_help=False)
parser.add_argument(
    "--build-dir",
    type=Path,
    default=Path(__file__).resolve().parents[1] / "build-pet-firmware-base",
)
ARGS, UNITTEST_ARGS = parser.parse_known_args()


class PetBaseBuildContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.build_dir = ARGS.build_dir.resolve()
        self.root = Path(__file__).resolve().parents[1]
        self.sdkconfig = (self.build_dir / "sdkconfig").read_text()
        self.compile_commands = json.loads((self.build_dir / "compile_commands.json").read_text())
        self.compiled_files = "\n".join(entry["file"] for entry in self.compile_commands)

    def test_runtime_disabled_build_excludes_optional_services(self) -> None:
        self.assertIn("# CONFIG_STACKCHAN_ENABLE_XIAOZHI_RUNTIME is not set", self.sdkconfig)
        forbidden_sources = (
            "/apps/app_ai_agent/",
            "/apps/app_launcher/",
            "/apps/common/",
            "/hal/audio.cpp",
            "/hal/hal_app_setup.cpp",
            "/hal/hal_ble.cpp",
            "/hal/hal_mcp.cpp",
            "/hal/hal_network.cpp",
            "/hal/utils/",
            "/xiaozhi-esp32/main/application.cc",
            "/xiaozhi-esp32/main/audio/audio_service.cc",
        )
        for source in forbidden_sources:
            with self.subTest(source=source):
                self.assertNotIn(source, self.compiled_files)

    def test_required_board_compatibility_remains_available(self) -> None:
        required_sources = (
            "/hal/pet/pet_hardware.cpp",
            "/hal/pet/pet_hardware_policy.cpp",
            "/pet/face/face.cpp",
            "/pet/face/lvgl_face_renderer.cpp",
            "/hal/board/stackchan_camera.cc",
            "/hal/board/stackchan_display.cc",
        )
        for source in required_sources:
            with self.subTest(source=source):
                self.assertIn(source, self.compiled_files)

    def test_pet_app_does_not_bypass_hardware_gateway(self) -> None:
        app_source = (self.root / "main/apps/app_pet_base/app_pet_base.cpp").read_text()
        forbidden_access = (
            "<hal/hal.h>",
            "<stackchan/stackchan.h>",
            "GetHAL()",
            "GetStackChan()",
            "Board::GetInstance",
        )
        for access in forbidden_access:
            with self.subTest(access=access):
                self.assertNotIn(access, app_source)

    def test_minimal_bringup_keeps_servo_rail_off(self) -> None:
        source = (self.root / "main/hal/hal_io_expander.cpp").read_text()
        servo_setup = source.split("        // RGB", maxsplit=1)[0]

        off_index = servo_setup.index("_io_expander->digitalWrite(0, false);")
        output_index = servo_setup.index("_io_expander->setDirection(0, true);")
        runtime_guard_index = servo_setup.index("#if CONFIG_STACKCHAN_ENABLE_XIAOZHI_RUNTIME")
        on_index = servo_setup.index("GetHAL().setServoPowerEnabled(true);")

        self.assertLess(off_index, output_index)
        self.assertLess(output_index, runtime_guard_index)
        self.assertLess(runtime_guard_index, on_index)

    def test_assistant_asset_partition_is_not_rebuilt_or_flashed(self) -> None:
        self.assertIn("CONFIG_FLASH_NONE_ASSETS=y", self.sdkconfig)
        flash_args = (self.build_dir / "flash_args").read_text()
        self.assertNotIn("generated_assets.bin", flash_args)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *UNITTEST_ARGS])
