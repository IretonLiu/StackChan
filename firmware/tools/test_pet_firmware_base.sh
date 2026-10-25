#!/usr/bin/env bash
# Run Pet Base's host policy tests and its firmware build-contract tests.
set -euo pipefail

firmware_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
host_build_dir=${HOST_BUILD_DIR:-"${firmware_dir}/build-host-tests"}
pet_build_dir=${BUILD_DIR:-"${firmware_dir}/build-pet-firmware-base"}

cmake -S "${firmware_dir}/tests" -B "${host_build_dir}"
cmake --build "${host_build_dir}"
ctest --test-dir "${host_build_dir}" --output-on-failure

"${firmware_dir}/tools/pet_firmware_base.sh" build
python3 "${firmware_dir}/tests/pet_base_contract_test.py" --build-dir "${pet_build_dir}"
