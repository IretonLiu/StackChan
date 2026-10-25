
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

This is deliberately transitional. Pet code reaches hardware only through
`hal/pet/pet_hardware.*`; the inherited XiaoZhi board component remains behind
that gateway. `CONFIG_STACKCHAN_ENABLE_XIAOZHI_RUNTIME` preserves the optional
XiaoZhi feature set in the checkout, but Pet Base sets it to `n`, excluding its
apps, services, audio processing, remote networking, and assistant assets.
The retained display/camera/audio board compatibility is progressively being
moved behind the custom `hal/` boundary.
