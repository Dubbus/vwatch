/* te_screen: live Aerobic Training Effect data screen.
 *   - TE 0.0-5.0 on a 270 degree arc, coloured by benefit zone, with its label
 *   - heart rate, EPOC (model units) and elapsed activity time
 *   - BACK resets the session
 *
 * Time comes from the tick's t_ms. Each tick feeds the model one HR sample per
 * second of activity time since the last tick, so a host that ticks every
 * N seconds (vwhost.py --every N, sending the window's mean HR) still runs
 * the 1 Hz model; with N = 5 the 5 s block means are exact.
 *
 * Every tick logs "te <TE> epoc <EPOC> s <seconds>" for tests and tooling.
 * Only the regions whose text changed are redrawn.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vwatch.h>
#include "te_model.h"

VW_APP_NAME("te_screen");

#ifndef TE_HR_MAX
#define TE_HR_MAX 190
#endif

#define CX 120
#define CY 120
#define ARC_R 116
#define ARC_W 10
#define ARC_A0 225.0f            /* 0.0 at 7:30, 5.0 at 4:30, clockwise */
#define ARC_SPAN 270.0f
#define MAX_CATCHUP_S 86400u     /* ignore time jumps beyond a day */

static te_state_t te;
static int started;

/* what is on screen now; redraw a region only when its text changes */
static char shown_te[8], shown_hr[8], shown_epoc[16], shown_time[12];
static int shown_zone = -1, shown_tenths = -1;

typedef struct { const char *label; vw_color_t color; } zone_t;
static const zone_t zones[] = {
    {"NO BENEFIT", VW_GRAY},           /* 0.0 - 0.9 */
    {"RECOVERY", VW_BLUE},             /* 1.0 - 1.9 */
    {"MAINTAINING", VW_GREEN},         /* 2.0 - 2.9 */
    {"IMPROVING", VW_YELLOW},          /* 3.0 - 3.9 */
    {"HIGHLY IMPROVING", VW_ORANGE},   /* 4.0 - 4.9 */
    {"OVERREACHING", VW_RED},          /* 5.0 */
};

static int zone_of(int tenths) { return tenths >= 50 ? 5 : tenths / 10; }

static void full_redraw(void) {
    vw_clear(VW_BLACK);
    vw_text_centered(CX, 34, "AEROBIC TE", VW_GRAY, 1);
    shown_te[0] = shown_hr[0] = shown_epoc[0] = shown_time[0] = '\0';
    shown_zone = shown_tenths = -1;
}

/* Clear rows y..y+h-1 only inside the ring, so the arc is never cut. */
static void clear_band(int y, int h) {
    int dy0 = y - CY, dy1 = y + h - 1 - CY;
    int dy = dy0 * dy0 > dy1 * dy1 ? dy0 : dy1;
    int ri = ARC_R - ARC_W - 1;
    int half = (int)sqrtf((float)(ri * ri - dy * dy)) - 1;
    vw_fill_rect(CX - half, y, 2 * half + 1, h, VW_BLACK);
}

/* Clear a text band, then draw centred text in it. */
static void band(int y, int h, const char *s, vw_color_t c, int scale) {
    clear_band(y, h);
    vw_text_centered(CX, y, s, c, scale);
}

static void draw(void) {
    float v = te_aerobic(&te);
    int tenths = (int)(v * 10.0f + 0.5f);
    int zone = zone_of(tenths);
    vw_color_t zc = zones[zone].color;
    char buf[24];

    if (tenths != shown_tenths) {
        vw_arc(CX, CY, ARC_R, ARC_W, ARC_A0, ARC_A0 + ARC_SPAN, VW_DKGRAY);
        if (tenths > 0)
            vw_arc(CX, CY, ARC_R, ARC_W, ARC_A0, ARC_A0 + ARC_SPAN * (float)tenths / 50.0f, zc);
        shown_tenths = tenths;
    }

    snprintf(buf, sizeof buf, "%d.%d", tenths / 10, tenths % 10);
    if (strcmp(buf, shown_te) || zone != shown_zone) {
        band(52, 48, buf, VW_WHITE, 6);
        strcpy(shown_te, buf);
    }
    if (zone != shown_zone) {
        band(108, 16, zones[zone].label, zc, 2);
        shown_zone = zone;
    }

    float hr = vw_sensors()->hr;
    if (hr > 0) snprintf(buf, sizeof buf, "%d", (int)(hr + 0.5f));
    else snprintf(buf, sizeof buf, "--");
    if (strcmp(buf, shown_hr)) {
        clear_band(136, 16);
        int x = CX - (vw_text_width(buf, 2) + 22) / 2;
        vw_fill_circle(x + 4, 141, 4, VW_RED);           /* heart */
        vw_fill_circle(x + 12, 141, 4, VW_RED);
        for (int i = 0; i < 7; i++) vw_fill_rect(x + i, 142 + i, 16 - 2 * i, 1, VW_RED);
        vw_text(x + 22, 136, buf, VW_WHITE, 2);
        strcpy(shown_hr, buf);
    }

    long e100 = (long)(te_peak_epoc(&te) * 100.0f + 0.5f);   /* model units, 2 dp */
    snprintf(buf, sizeof buf, "EPOC %ld.%02ld", e100 / 100, e100 % 100);
    if (strcmp(buf, shown_epoc)) {
        band(162, 16, buf, VW_CYAN, 2);
        strcpy(shown_epoc, buf);
    }

    uint32_t s = te.seconds;
    snprintf(buf, sizeof buf, "%lu:%02lu:%02lu", (unsigned long)(s / 3600),
             (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
    if (strcmp(buf, shown_time)) {
        band(190, 16, buf, VW_GRAY, 2);
        strcpy(shown_time, buf);
    }
}

static void reset(void) {
    te_init(&te, (float)(TE_HR_MAX));
    started = 0;
    full_redraw();
    draw();
}

void app_init(void) {
    vw_log("te_screen booted, HRmax %d", (int)TE_HR_MAX);
    reset();
}

void app_tick(void) {
    /* the tick covers activity seconds up to and including t_ms / 1000 */
    uint32_t target = vw_millis() / 1000u + 1u;
    float hr = vw_sensors()->hr;
    if (!started) {
        started = 1;
        if (target > MAX_CATCHUP_S) target = te.seconds + 1;
    }
    if (target > te.seconds + MAX_CATCHUP_S) target = te.seconds + 1;
    while (te.seconds < target) te_push(&te, hr);
    draw();
    /* fixed point: newlib-nano printf has no %f */
    long t100 = (long)(te_aerobic(&te) * 100.0f + 0.5f);
    long e100 = (long)(te_peak_epoc(&te) * 100.0f + 0.5f);
    vw_log("te %ld.%02ld epoc %ld.%02ld s %lu", t100 / 100, t100 % 100, e100 / 100, e100 % 100,
           (unsigned long)te.seconds);
}

/* Hold BACK to reset: like a real watch, a single accidental press during an
 * activity must not throw the session away. */
void app_button(int button, int event) {
    if (event == VW_HELD && button == VW_BTN_BACK) {
        vw_log("session reset");
        reset();
    }
}
