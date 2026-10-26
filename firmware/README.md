
## Build

### Fetch Dependencies

```bash
python3 ./fetch_repos.py
```

### Tool Chains

[ESP-IDF v5.5.4](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/index.html)

### Build

```bash
idf.py build
```

### Tests

The native host suite checks hardware-policy invariants and motion helpers
without ESP-IDF hardware:

```bash
cmake -S tests -B build-host-tests
cmake --build build-host-tests
ctest --test-dir build-host-tests --output-on-failure
```

### Local face previews

The procedural face core can be reviewed without ESP-IDF or a connected
StackChan. The preview tool writes an SVG still or a directory of SVG animation
frames with an `index.html` contact sheet:

```bash
# After building the host suite above:
./build-host-tests/face_preview --expression happy --output happy.svg
./build-host-tests/face_preview \
  --from idle --to excited --duration-ms 220 --frames 12 --output face-transition
```

Open the SVG or `face-transition/index.html` in a browser before flashing.
Use `--help` for every available semantic expression and option.

For Pet Base, run the complete suite after exporting ESP-IDF. It runs those
host tests, builds the isolated image, then verifies its build configuration
and compiled-source boundary:

```bash
./tools/test_pet_firmware_base.sh
```

### Flash

```bash
idf.py flash
```

## Custom Pet Base image

The custom pet work uses its own build directory and generated `sdkconfig`.
It boots only `AppPetBase`: no launcher and no XiaoZhi assistant loop.

```bash
./tools/pet_firmware_base.sh build
./tools/pet_firmware_base.sh -p /dev/ttyACM0 flash monitor
```

This is deliberately transitional. Pet code reaches hardware only through
`hal/pet/pet_hardware.*`; the inherited XiaoZhi board component remains behind
that gateway. `CONFIG_STACKCHAN_ENABLE_XIAOZHI_RUNTIME` preserves the optional
XiaoZhi feature set in the checkout, but Pet Base sets it to `n`, excluding its
apps, services, audio processing, remote networking, and assistant assets.
The retained display/camera/audio board compatibility is progressively being
moved behind the custom `hal/` boundary.
