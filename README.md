# vwatch

A virtual machine for smartwatch firmware. Write a watch face or app in C
against a small SDK, boot it on an emulated Cortex-M4, and watch it draw.

> Status: early, but usable: SDK, emulator, desktop VM window with live
> reload, and a headless host for tests and activity replay.

Bring your own editor: keep VS Code (or anything) on one half of the screen
and the vwatch window on the other. Save a file and vwatch rebuilds and
reboots the watch, like live-reloading an HTML page.

![the vwatch window running hello_face](docs/vwatch_app.png)

## The VM window

```
brew install qemu qtbase cmake         # plus the Arm GNU toolchain (arm-none-eabi-gcc)
cmake -S app -B app/build && cmake --build app/build
app/build/vwatch examples/hello_face   # a project dir (live reload) or a .elf
```

Open the same folder in your editor. On every save vwatch runs `make`, and
on success it reboots the watch with the new firmware. A failed build leaves
the previous firmware running and prints the compiler errors in the serial
console. A firmware that hangs inside `app_tick()` is caught by a 3-second
watchdog.

- **Buttons:** click the bezel buttons, or use L / ↑ / ↓ / Return / Esc.
- **Sensors:** heart rate and speed sliders in the toolbar.
- **Machine menu:** pause, reset, rebuild, speed 1x–300x.

## How it works

```
 your app.c ──► vwatch SDK + runtime ──► Cortex-M4 ELF ──► QEMU mps2-an386
                                                              │  UART0
 host (vwhost.py / desktop app) ◄── screen regions, logs ─────┘
                                 ──► ticks, sensors, buttons ─►
```

- **SDK** (`sdk/include/vwatch.h`): implement `app_init`, `app_tick`,
  `app_button`. Draw into a 240×240 round display with 64 colours (RGB222,
  the depth of transflective watch screens): rects, lines, circles, arcs, a
  bitmap font. Read heart rate, speed and cadence; log to the console.
- **Runtime**: owns `main()`, runs the event loop, and after every tick
  ships only the screen region that changed.
- **Protocol** (`sdk/include/vw_proto.h`): small framed binary messages over
  the emulated UART. Lockstep: the host sends a tick and waits for
  `TICK_DONE`, so pause, speed and replay are deterministic.
- **Port layer** (`sdk/port/<target>/`): UART, startup and linker script per
  target. `qemu-mps2` today; real hardware ports go here.

## Examples

| | | |
|---|---|---|
| ![lvgl_face](docs/lvgl_face.png) | ![hello_face](docs/hello_face.png) | ![te_screen](docs/te_screen.png) |
| `lvgl_face`: watch face built with LVGL | `hello_face`: the same face with the SDK's basic drawing calls | `te_screen`: live Aerobic Training Effect from heart rate |

## LVGL

For real watch UIs, build with [LVGL](https://lvgl.io) (v9.2, MIT), the
standard embedded graphics library and the one open watch firmware such as
PineTime's uses. Anti-aliased fonts, arcs and widgets, rendered in 16-bit
colour and converted to the 64-colour display. LVGL apps don't carry the basic
framebuffer: the SDK links as a library, so each app gets only the display
backend it uses.

```c
#include <vw_lvgl.h>
void app_init(void) {
    vw_lvgl_init();
    lv_obj_t *l = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(l, &lv_font_montserrat_48, 0);
    lv_label_set_text(l, "10:08");
    lv_obj_center(l);
}
```

Set `USE_LVGL := 1` in the app Makefile. LVGL is a git submodule:
`git clone --recursive`, or `git submodule update --init` in an existing clone.
The first LVGL build compiles it once (~10 s) into `sdk/build/`.

## Hardware parity

The emulated watch follows a 240×240 round, 64-colour transflective (MIP)
display: reflective and muted when unlit, brighter when lit. Any button press
lights it for 8 s (`vw_backlight*` to control it). Buttons report press, hold
(after ~1 s) and release, so apps can use long-presses the way watch UIs do.

## Headless (tests, CI, activity replay)

Needs `arm-none-eabi-gcc`, `make` and `qemu-system-arm`.

```
cd examples/hello_face
make            # -> build/hello_face.elf  (~7 KB code)
make run        # boot in QEMU headless, 12 ticks, save build/hello_face.png
```

Minimal app:

```c
#include <vwatch.h>
VW_APP_NAME("my_face");

void app_init(void) {}
void app_tick(void) {
    vw_clear(VW_BLACK);
    vw_text_centered(120, 100, "HELLO", VW_WHITE, 4);
    vw_arc(120, 120, 118, 6, 0, vw_millis() / 1000 % 60 * 6.0f, VW_ORANGE);
}
void app_button(int b, int down) { (void)b; (void)down; }
```

## Tests

```
python3 tests/test_hello_face.py
python3 tests/test_te_screen.py [activity.fit]
python3 tests/test_lvgl_face.py
```

Replay a recorded activity (needs `pip install fitdecode`):

```
python3 host/vwhost.py examples/te_screen/build/te_screen.elf --fit run.fit --every 5 --png out.png
```

## Roadmap

1. ~~SDK, runtime, protocol, QEMU port, headless host~~
2. ~~VM window (C++/Qt): live watch screen, bezel buttons, sensor sliders,
   serial console, live reload on save~~
3. ~~Activity replay: headless (`vwhost.py --fit`) and in the VM window (File > Open Activity), with the watch's own value shown beside your firmware's~~
4. Deploy targets: [PineTime](https://pine64.org/devices/pinetime/) (open hardware, native firmware;
   flashing is at your own risk) and Garmin via a Connect IQ export (sandboxed app, no firmware changes)

vwatch does not modify or flash proprietary watch firmware.

## License

MIT
