#!/usr/bin/env bash
# Build or flash Project Kiwi's isolated compatibility configuration.
#
# Examples:
#   ./tools/pet_firmware_base.sh build
#   ./tools/pet_firmware_base.sh -p /dev/ttyACM0 flash monitor
#
# This never writes the project's normal sdkconfig or uses its build directory.
set -euo pipefail

firmware_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${BUILD_DIR:-"${firmware_dir}/build-pet-firmware-base"}
defaults_file="${firmware_dir}/pet_configs/pet_firmware_base.defaults"

if [[ ! -d "${firmware_dir}/xiaozhi-esp32" ]]; then
    echo "Fetching StackChan firmware dependencies..."
    (cd "${firmware_dir}" && python3 fetch_repos.py)
fi

export SDKCONFIG_DEFAULTS="${firmware_dir}/sdkconfig.defaults;${defaults_file}"
sdkconfig="${build_dir}/sdkconfig"
mkdir -p "${build_dir}"

# ESP-IDF only applies SDKCONFIG_DEFAULTS when generating sdkconfig. Regenerate
# it if either defaults file changes; this keeps this image isolated from stock.
stamp_file="${build_dir}/.sdkconfig_defaults.sha256"
defaults_hash=$(cat "${firmware_dir}/sdkconfig.defaults" "${defaults_file}" | sha256sum | cut -d' ' -f1)
if [[ -f "${sdkconfig}" && "$(cat "${stamp_file}" 2>/dev/null)" != "${defaults_hash}" ]]; then
    echo "pet-base defaults changed; regenerating ${sdkconfig}"
    rm -f "${sdkconfig}" "${sdkconfig}.old"
fi
printf '%s\n' "${defaults_hash}" > "${stamp_file}"

cd "${firmware_dir}"
if [[ $# -eq 0 ]]; then
    set -- build
fi
exec idf.py -B "${build_dir}" -D SDKCONFIG="${sdkconfig}" "$@"
