/* vwatch SDK: write watch firmware in C, run it in the vwatch emulator.
 *
 * An app implements three callbacks; the runtime owns main() and the event
 * loop, and ships every redrawn screen region to the host after each tick.
 *
 *     #include <vwatch.h>
 *     void app_init(void)                 { ... }   // once, after boot
 *     void app_tick(void)                 { ... }   // every tick (default 1 s)
 *     void app_button(int btn, int down)  { ... }   // bezel buttons
 *
 * Drawing goes into a 240x240 framebuffer with 64 colours (RGB222), the
 * colour depth of transflective (MIP) watch displays. Coordinates outside the
 * screen are clipped. The display is round: corners exist in memory but the
 * host masks them away.
 */
#ifndef VWATCH_H
#define VWATCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- display ---------- */
#define VW_W 240
#define VW_H 240

typedef uint8_t vw_color_t;                  /* 0b00RRGGBB */
#define VW_RGB(r, g, b) ((vw_color_t)((((r) >> 6) << 4) | (((g) >> 6) << 2) | ((b) >> 6)))

#define VW_BLACK   VW_RGB(0, 0, 0)
#define VW_WHITE   VW_RGB(255, 255, 255)
#define VW_GRAY    VW_RGB(170, 170, 170)
#define VW_DKGRAY  VW_RGB(85, 85, 85)
#define VW_RED     VW_RGB(255, 0, 0)
#define VW_GREEN   VW_RGB(0, 255, 0)
#define VW_BLUE    VW_RGB(0, 85, 255)
#define VW_YELLOW  VW_RGB(255, 255, 0)
#define VW_ORANGE  VW_RGB(255, 85, 0)
#define VW_CYAN    VW_RGB(0, 255, 255)
#define VW_MAGENTA VW_RGB(255, 0, 255)

void vw_clear(vw_color_t c);
void vw_pixel(int x, int y, vw_color_t c);
void vw_fill_rect(int x, int y, int w, int h, vw_color_t c);
void vw_rect(int x, int y, int w, int h, vw_color_t c);
void vw_line(int x0, int y0, int x1, int y1, vw_color_t c);
void vw_circle(int cx, int cy, int r, vw_color_t c);
void vw_fill_circle(int cx, int cy, int r, vw_color_t c);
/* Ring segment from angle a0 to a1 in degrees, clockwise, 0 = 12 o'clock. */
void vw_arc(int cx, int cy, int r, int thickness, float a0_deg, float a1_deg, vw_color_t c);

/* Built-in 5x8 bitmap font, scaled by an integer factor (1 = 6 px per char).
 * Returns the x after the last glyph. */
int vw_text(int x, int y, const char *s, vw_color_t c, int scale);
int vw_text_width(const char *s, int scale);
/* Centered horizontally on cx. */
void vw_text_centered(int cx, int y, const char *s, vw_color_t c, int scale);

/* ---------- input ---------- */
enum { VW_BTN_LIGHT = 0, VW_BTN_UP, VW_BTN_DOWN, VW_BTN_START, VW_BTN_BACK, VW_BTN_COUNT };

/* Button events passed to app_button(). A long press delivers PRESSED, then
 * HELD after about a second, then RELEASED. Garmin-style UIs use HELD for
 * secondary actions (menu on UP, controls on LIGHT, reset on BACK). */
enum { VW_RELEASED = 0, VW_PRESSED = 1, VW_HELD = 2 };

/* ---------- backlight ----------
 * Like a real transflective (MIP) watch: the screen is readable without the
 * backlight, and by default any button press lights it for a few seconds.
 * Apps may take manual control. */
void vw_backlight(int on);                     /* on now (resets the timeout) or off */
void vw_backlight_timeout(uint32_t ms);        /* auto-off delay, 0 = never auto-off */
void vw_backlight_auto(int enabled);           /* light on any button press (default 1) */

typedef struct {
    float hr;          /* bpm, 0 if no reading */
    float speed;       /* m/s */
    float cadence;     /* steps/min */
} vw_sensors_t;

const vw_sensors_t *vw_sensors(void);
uint32_t vw_millis(void);          /* simulated ms since boot */

/* ---------- debug ---------- */
/* printf-style message to the emulator's serial console. */
void vw_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* ---------- app callbacks (implemented by the app) ---------- */
extern const char vw_app_name[];   /* define with VW_APP_NAME("...") */
void app_init(void);
void app_tick(void);
void app_button(int button, int event);   /* event: VW_PRESSED / VW_HELD / VW_RELEASED */

#define VW_APP_NAME(s) const char vw_app_name[] = s

#ifdef __cplusplus
}
#endif
#endif
