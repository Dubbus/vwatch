# vwatch

A virtual machine for smartwatch firmware. Write a watch face or app in C
against a small SDK, boot it on an emulated Cortex-M4, and watch it draw.

> Status: early. Phase 1 (SDK, runtime, emulator link, headless host) works.
> The desktop app (a VirtualBox-style manager, VM window and IDE with an AI
> assistant) is in progress. See [Roadmap](#roadmap).

![hello_face running in vwatch](docs/hello_face.png)

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

## Quick start

Needs `arm-none-eabi-gcc`, `make` and `qemu-system-arm` (`brew install qemu`).

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
```

## Roadmap

1. ~~SDK, runtime, protocol, QEMU port, headless host~~
2. VM window (C++/Qt): white canvas with the live watch screen, bezel buttons, serial console
3. Manager window in the style of VirtualBox 5: watch list, details pane, New/Settings/Start
4. IDE: editor, build, run, clickable compiler errors, size and cost report
5. AI assistant pane: describe a change, review the diff, apply it ("vibe code" next to hand code)
6. Deploy targets: [PineTime](https://pine64.org/devices/pinetime/) (open hardware, native firmware;
   flashing is at your own risk) and Garmin via a Connect IQ export (sandboxed app, no firmware changes)

vwatch does not modify or flash proprietary watch firmware.

## License

MIT
