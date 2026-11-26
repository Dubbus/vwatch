/* hello_face: the smallest useful vwatch watch face.
 *   - digital clock from simulated time (starts at 10:08:00)
 *   - seconds ring around the bezel
 *   - live heart rate from the sensor feed
 *   - UP/DOWN cycle the accent colour, START toggles a stopwatch
 */
#include <stdio.h>
#include <vwatch.h>

VW_APP_NAME("hello_face");

#define CX 120
#define CY 120
#define START_S (10 * 3600 + 8 * 60)

static const vw_color_t accents[] = {VW_ORANGE, VW_CYAN, VW_GREEN, VW_YELLOW, VW_MAGENTA};
static int accent;
static int sw_running;
static uint32_t sw_start_ms, sw_elapsed_ms;

static void draw(void) {
    uint32_t ms = vw_millis();
    uint32_t t = START_S + ms / 1000;
    int h = (int)(t / 3600 % 24), m = (int)(t / 60 % 60), s = (int)(t % 60);
    vw_color_t acc = accents[accent];
    char buf[24];

    vw_clear(VW_BLACK);

    /* seconds ring: dim track + bright progress */
    vw_arc(CX, CY, 118, 6, 0, 360, VW_DKGRAY);
    vw_arc(CX, CY, 118, 6, 0, (s + 1) * 6.0f, acc);

    /* clock */
    snprintf(buf, sizeof buf, "%02d:%02d", h, m);
    vw_text_centered(CX, 70, buf, VW_WHITE, 6);
    snprintf(buf, sizeof buf, "%02d", s);
    vw_text_centered(CX, 124, buf, acc, 2);
    vw_text_centered(CX, 50, "THU NOV 26", VW_GRAY, 1);

    /* heart rate */
    float hr = vw_sensors()->hr;
    vw_fill_circle(97, 162, 4, VW_RED);
    vw_fill_circle(105, 162, 4, VW_RED);
    for (int i = 0; i < 7; i++) vw_fill_rect(93 + i, 163 + i, 16 - 2 * i, 1, VW_RED);
    if (hr > 0) snprintf(buf, sizeof buf, "%d", (int)(hr + 0.5f));
    else snprintf(buf, sizeof buf, "--");
    vw_text(116, 156, buf, VW_WHITE, 2);

    /* stopwatch */
    uint32_t sw = sw_elapsed_ms + (sw_running ? ms - sw_start_ms : 0);
    snprintf(buf, sizeof buf, "%s %02lu:%02lu", sw_running ? ">" : "#",
             (unsigned long)(sw / 60000), (unsigned long)(sw / 1000 % 60));
    vw_text_centered(CX, 190, buf, sw_running ? acc : VW_GRAY, 1);
}

void app_init(void) {
    vw_log("hello_face booted");
    draw();
}

void app_tick(void) { draw(); }

void app_button(int button, int pressed) {
    if (!pressed) return;
    int n = (int)(sizeof accents / sizeof accents[0]);
    if (button == VW_BTN_UP) accent = (accent + 1) % n;
    else if (button == VW_BTN_DOWN) accent = (accent + n - 1) % n;
    else if (button == VW_BTN_START) {
        if (sw_running) sw_elapsed_ms += vw_millis() - sw_start_ms;
        else sw_start_ms = vw_millis();
        sw_running = !sw_running;
        vw_log("stopwatch %s", sw_running ? "started" : "stopped");
    } else if (button == VW_BTN_BACK && !sw_running) {
        sw_elapsed_ms = 0;
    }
    draw();
}
