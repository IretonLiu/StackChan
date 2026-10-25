
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

### Host-side tests

The motion coordinate helpers can be tested without ESP-IDF hardware:

```bash
cmake -S tests -B build-host-tests
cmake --build build-host-tests
ctest --test-dir build-host-tests --output-on-failure
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

This is deliberately transitional. The app itself has no XiaoZhi headers, but
hardware bring-up still uses the existing XiaoZhi board component. The next
base-layer milestones extract display, audio, touch, IMU, LEDs, servos, and
NVS behind the custom `hal/` boundary.
