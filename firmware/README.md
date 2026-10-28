
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

### Organic-shape geometry preview

The first host-only `soft_polygon` prototype is a normalized, closed cubic
Bézier cat ear. Its semantic landmarks and attachment pivot can be displayed
for geometry review; position/rotation/scale are affine transforms rather than
mutations of the template path:

```bash
./build-host-tests/shape_preview --pose neutral --debug --output neutral-ear.svg
./build-host-tests/shape_preview --pose twitch --tip-bend 0.12 --rotation 8 --debug --output twitch-ear.svg
```

Available static review poses are `neutral`, `perked`, `twitch`, and `droop`.
Run `shape_preview --help` for the bounded parameter overrides.

Kiwi Studio, the GLFW/Dear ImGui face and animation editor, lives in its own
repository:

```bash
git clone --recursive https://github.com/IretonLiu/kiwi-studio.git
```

It consumes this firmware checkout as a git submodule for the shared shape core,
face schema, sample scenes and preset compiler. Build, authoring and headless-check
instructions are in that repository.

This repo keeps the shared contract: `pet_faces/face_schema.json` is the single
source of truth for animatable channels, ranges, preset capacities and polygon
defaults (`python3 tools/generate_face_schema.py` after editing it), and
`pet_faces/pose_contract.json` selects the boot scene. The compiler validates
each scene's `poses` and `animations` blocks before emitting the typed header.

On-device consumers append a sampled `ShapePath` with
`pet::shape::appendToFaceDrawing`; the adapter preserves its commands and
style in the exact `FaceDrawing` model consumed by `LvglFaceRenderer`.
Non-finite shape inputs are rejected rather than converted into geometry.

For Project Kiwi, run the complete suite after exporting ESP-IDF. It runs those
host tests, builds the isolated image, then verifies its build configuration
and compiled-source boundary:

```bash
./tools/test_pet_firmware_base.sh
```

### Flash

```bash
idf.py flash
```

## Project Kiwi firmware image

Project Kiwi uses its own build directory and generated `sdkconfig`.
It boots only `AppPetBase`: no launcher and no XiaoZhi assistant loop.

```bash
./tools/pet_firmware_base.sh build
./tools/pet_firmware_base.sh -p /dev/ttyACM0 flash monitor
```

This is deliberately transitional. Pet code reaches hardware only through
`hal/pet/pet_hardware.*`; the inherited XiaoZhi board component remains behind
that gateway. `CONFIG_STACKCHAN_ENABLE_XIAOZHI_RUNTIME` preserves the optional
XiaoZhi feature set in the checkout, but Project Kiwi sets it to `n`, excluding its
apps, services, audio processing, remote networking, and assistant assets.
The retained display/camera/audio board compatibility is progressively being
moved behind the custom `hal/` boundary.

Measured memory, display and frame-time budgets for this image live in
[docs/pet-base-budgets.md](docs/pet-base-budgets.md) (legacy filename retained for compatibility). Read it before adding
renderers, buffers or tasks: the panel is bandwidth-bound at ~32 fps for a full
frame, and the vector rasterizer needs ~21 KiB of stack.
