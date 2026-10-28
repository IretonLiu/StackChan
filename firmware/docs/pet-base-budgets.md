# Pet Base resource budgets

Measured on the StackChan body (ESP32-S3, 8 MB PSRAM, 16 MB flash) with the
`build-pet-firmware-base` image. Keep these numbers current when the renderer,
the display path, or the task layout changes: they are the constants behind a
class of bug that is otherwise very hard to read from a backtrace.

## The bug this document exists for

The vector face renderer boot-looped the device. The panic pointed at a bad
`free()` inside the heap allocator, but the cause was a **stack overflow**: the
LVGL/ThorVG software rasterizer needs about 21 KiB of stack per face frame and
the pet app renders from the default 8 KiB `main` task. The overflow ran past
the stack bottom into DRAM and corrupted the ESP heap region list, so the next
allocation crashed somewhere unrelated.

Diagnostic rule: if `free()` dies inside `find_containing_heap`, or a heap
integrity check passes and a free still faults, suspect a stack overflow from a
heavy C++ pipeline before suspecting the allocator.

## Internal SRAM (the scarce pool)

Data-bus window only, measured with `heap_caps_print_heap_info()`:

| Region | Size | Free | Content |
|---|---|---|---|
| `0x3FCA5218` | 273.2 KiB | 142.2 KiB | main task stack (32 KiB), LVGL task stack (7 KiB), drivers, audio, std heap |
| `0x3FCE9710` | 21.8 KiB | ~0 | early boot and driver allocations, effectively full |
| `0x3FCF0000` | 32.0 KiB | 32.0 KiB | untouched, still in the heap |
| `0x3FCB1B08` | 64.0 KiB | 63.2 KiB | DMA pool reserved by PSRAM support for internal-cap DMA |
| `0x600FE000` | 8.0 KiB | 7.8 KiB | RTC RAM |

Runtime snapshot after opening the procedural face: **246,731 B free**; the
minimum observed during animated rendering was **241,999 B**. The largest
contiguous internal block remained **135,168 B**.

Not in the heap, from `idf.py size`:

| Section | Bytes |
|---|---|
| DIRAM `.text` (code in the shared IRAM window) | 89,135 |
| `.data` | 14,512 |
| `.bss` | 14,440 |
| `.vectors` | 1,028 |
| Flash `.text` | 846,836 |
| Flash `.rodata` | 581,804 |

DIRAM total is 341,760 B; static usage is 35% of it.

## PSRAM (the plentiful pool)

One pool at `0x3C16_0000` (it starts after the flash DROM mapping window):
8 MiB, **7,737,160 B free** (largest block 7,733,248 B). The minimum observed
during animated rendering was 7,724,856 B; no progressive loss was observed.

- Face canvas: 300 KiB, ARGB8888 320x240, allocated once and reused.
- LVGL image cache: 2 MiB reserved by the display bring-up, committed lazily.

Anything large (frame buffers, caches, future sprite atlases) belongs here, not
in internal SRAM.

## Flash

| Offset | Partition | Size | Notes |
|---|---|---|---|
| `0x000000` | bootloader | 23.8 KiB | 27% free |
| `0x008000` | partition table | 3 KiB | |
| `0x009000` | nvs | 16 KiB | |
| `0x00D000` | otadata | 8 KiB | |
| `0x00F000` | phy_init | 4 KiB | |
| `0x020000` | ota_0 | 4.94 MiB | app 1,533,411 B, **70% free** |
| `0x510000` | ota_1 | 4.94 MiB | empty OTA slot |
| `0xA00000` | assets | 4 MiB | unused while `CONFIG_FLASH_NONE_ASSETS=y` |
| `0xE00000` | coredump | 64 KiB | |
| `0xE10000` | unpartitioned | ~1.94 MiB | |

Storage is never the constraint for the pet image today. The 4 MiB `assets`
partition is dead space unless we ever bake sprites.

## Display

The panel is an **ILI9342E** (falls back to ILI9342C): 320x240, RGB565, 16 bpp
BGR, colour inverted, driven from `firmware/main/hal/board/stackchan.cc`:

```
bus:           SPI3, MOSI only (no quad lines)
clock:         io_config.pclk_hz = 40 MHz, spi_mode 2
DMA:           trans_queue_depth = 10, max_transfer_sz = full framebuffer
TE / vsync:    not wired
```

This panel has **no refresh rate**. It holds an internal GRAM; pixels persist
until written. There is no scan-out oscillator, no field rate and no blanking
interval, so nothing throttles or tears against a hardware refresh. The only
hardware rate is write bandwidth:

$$f_{\max} = \frac{40\ \text{Mbit/s} \times 1\ \text{line}}{320 \times 240 \times 16\ \text{bit}} = \frac{5.0\ \text{MB/s}}{153{,}600\ \text{B}} \approx \frac{1}{30.7\ \text{ms}} \approx 32.5\ \text{fps}$$

Consequences:

- A full-screen repaint costs 30.7 ms of wire time, so **~32 fps is the hard
  ceiling** for full-frame animation, regardless of CPU or PSRAM headroom.
- Partial updates scale linearly: the current face ink box is 194x50 = 9,700 px
  = 19.4 KB = **3.9 ms** of wire time, which leaves plenty of headroom.
- Raising `io_config.pclk_hz` to 80 MHz would double the ceiling to ~65 fps. It
  is a signal-integrity experiment on a single-ended SPI bus with a flex cable,
  not a free win.

## Frame budget

Measured on-device with the visible blink plus ±8° procedural ear sway. The
app's rate-limited telemetry uses `esp_timer_get_time()` around each phase:

| Step | Average | Maximum | Notes |
|---|---:|---:|---|
| Preset evaluation | 0.319 ms | 0.471 ms | Copies the immutable base scene. |
| Animation advance | 0.814 ms | 1.027 ms | Keyframe/sine resolution and response functions. |
| Drawing build | 0.704 ms | 1.009 ms | Converts evaluated geometry to renderer paths. |
| Display-lock wait | 29.9–31.0 ms | 53.5 ms | Waiting for LVGL/display ownership. |
| Renderer while locked | 12.7 ms | 13.2 ms | Direct canvas update, vector rasterization, and flush. |
| Face tick | 73 ms | 100 ms | 68 frames in 5.0 s, approximately 13.5 fps. |

The procedural engine costs about 1.1 ms before drawing; it is not the current
rate limiter. Display ownership plus rendering costs roughly 43–44 ms, and the
main app's scheduling cadence accounts for the remaining time to the 73 ms
face tick. The older 6.5 ms dirty-rect figure measured renderer work only and
must not be treated as end-to-end animation throughput.

Budget rules:

- Never render vectors on a task with less than 24 KiB of stack.
- Never fill the whole canvas per frame: a full 320x240 ARGB clear alone costs
  ~20 ms of PSRAM writes.
- Keep one vector layer per frame, not one per path.
- LVGL's refresh tick is the software ceiling: `CONFIG_LV_DEF_REFR_PERIOD = 33`
  means a **30 fps display update**, below what the panel could accept.

## Levers, in order of value

1. Lower `CONFIG_LV_DEF_REFR_PERIOD` (software cap) if smoother motion is ever
   needed; the panel has headroom for the current ink-only updates.
2. Drive the bundled ThorVG canvas directly instead of going through LVGL's
   vector draw task, so the canvas is created once instead of per frame. Should
   take the 4.4 ms fixed cost to ~1-2 ms and keeps everything procedural.
3. Run rendering on a Pet-owned task with a PSRAM-backed stack
   (`xTaskCreateWithCaps` + `MALLOC_CAP_SPIRAM`) to return `main` to 8 KiB and
   take the 21 KiB out of internal SRAM. Good fit: a pure render task needs no
   cache-disabled execution.
4. Raise the SPI clock to 80 MHz if full-screen animation is ever required.
5. Bake sprites only if ThorVG must leave the image entirely. It costs the
   procedural rig and needs a build-time bake pipeline; storage is not a
   blocker (4 MiB `assets` is unused).
