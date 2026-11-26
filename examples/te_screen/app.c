/* te_screen: live Aerobic Training Effect data screen (LVGL).
 *   - TE 0.0-5.0 on a 270 degree gauge around the bezel, coloured by benefit
 *     zone, with the value and zone name in the middle
 *   - heart rate and EPOC (model units) fields, elapsed activity time
 *   - hold BACK to reset the session
 *
 * Time comes from the tick's t_ms. Each tick feeds the model one HR sample per
 * second of activity time since the last tick, so a host that ticks every
 * N seconds (vwhost.py --every N, sending the window's mean HR) still runs
 * the 1 Hz model; with N = 5 the 5 s block means are exact.
 *
 * Every tick logs "te <TE> epoc <EPOC> s <seconds>" for tests and tooling.
 */
#include <vw_lvgl.h>
#include <vw_fonts.h>
#include "te_model.h"

VW_APP_NAME("te_screen");

#ifndef TE_HR_MAX
#define TE_HR_MAX 190
#endif

#define MAX_CATCHUP_S 86400u     /* ignore time jumps beyond a day */

static te_state_t te;
static int started;

static lv_obj_t *gauge, *te_lbl, *zone_lbl, *hr_lbl, *epoc_lbl, *time_lbl;

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

static lv_obj_t *text(const lv_font_t *font, vw_color_t c, lv_align_t align, int x, int y, const char *s) {
    lv_obj_t *l = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, VW_LV_COLOR(c), 0);
    lv_label_set_text(l, s);
    lv_obj_align(l, align, x, y);
    return l;
}

static void rule(int x0, int y0, int x1, int y1) {
    static lv_point_precise_t pts[2][2];
    static int n;
    lv_point_precise_t *p = pts[n++ % 2];
    p[0].x = x0; p[0].y = y0; p[1].x = x1; p[1].y = y1;
    lv_obj_t *l = lv_line_create(lv_screen_active());
    lv_line_set_points(l, p, 2);
    lv_obj_set_style_line_width(l, 2, 0);
    lv_obj_set_style_line_color(l, VW_LV_COLOR(VW_DKGRAY), 0);
}

static void build_ui(void) {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    gauge = lv_arc_create(scr);                 /* 0.0 at 7:30, 5.0 at 4:30, clockwise */
    lv_obj_set_size(gauge, 232, 232);
    lv_obj_center(gauge);
    lv_arc_set_bg_angles(gauge, 135, 45);
    lv_arc_set_range(gauge, 0, 50);
    lv_obj_remove_style(gauge, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(gauge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(gauge, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(gauge, VW_LV_COLOR(VW_DKGRAY), LV_PART_MAIN);
    lv_obj_set_style_arc_width(gauge, 10, LV_PART_INDICATOR);

    text(&vw_font_label_18, VW_GRAY, LV_ALIGN_TOP_MID, 0, 28, "AEROBIC TE");
    te_lbl = text(&vw_font_num_96, VW_WHITE, LV_ALIGN_CENTER, 0, -38, "0.0");
    zone_lbl = text(&vw_font_label_24, VW_GRAY, LV_ALIGN_CENTER, 0, 18, "");

    rule(30, 150, 210, 150);                    /* fields divider */
    rule(120, 154, 120, 198);
    text(&vw_font_label_18, VW_GRAY, LV_ALIGN_TOP_MID, -46, 150, "HR");
    text(&vw_font_label_18, VW_GRAY, LV_ALIGN_TOP_MID, 46, 150, "EPOC");
    hr_lbl = text(&vw_font_num_36, VW_WHITE, LV_ALIGN_TOP_MID, -46, 162, "--");
    epoc_lbl = text(&vw_font_num_36, VW_CYAN, LV_ALIGN_TOP_MID, 46, 162, "0.00");
    time_lbl = text(&vw_font_label_18, VW_GRAY, LV_ALIGN_BOTTOM_MID, 0, -6, "0:00:00");
}

static void update(void) {
    const int tenths = (int)(te_aerobic(&te) * 10.0f + 0.5f);
    const zone_t *z = &zones[zone_of(tenths)];

    lv_arc_set_value(gauge, tenths);
    lv_obj_set_style_arc_color(gauge, VW_LV_COLOR(z->color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(gauge, tenths > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_label_set_text_fmt(te_lbl, "%d.%d", tenths / 10, tenths % 10);
    lv_label_set_text(zone_lbl, z->label);
    lv_obj_set_style_text_color(zone_lbl, VW_LV_COLOR(z->color), 0);

    const float hr = vw_sensors()->hr;
    if (hr > 0) lv_label_set_text_fmt(hr_lbl, "%d", (int)(hr + 0.5f));
    else lv_label_set_text(hr_lbl, "--");

    const long e100 = (long)(te_peak_epoc(&te) * 100.0f + 0.5f);
    lv_label_set_text_fmt(epoc_lbl, "%ld.%02ld", e100 / 100, e100 % 100);

    const uint32_t s = te.seconds;
    lv_label_set_text_fmt(time_lbl, "%lu:%02lu:%02lu", (unsigned long)(s / 3600),
                          (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
}

static void reset(void) {
    te_init(&te, (float)(TE_HR_MAX));
    started = 0;
    update();
}

void app_init(void) {
    vw_lvgl_init();
    build_ui();
    vw_log("te_screen booted, HRmax %d", (int)TE_HR_MAX);
    reset();
}

void app_tick(void) {
    /* the tick covers activity seconds up to and including t_ms / 1000 */
    uint32_t target = vw_millis() / 1000u + 1u;
    const float hr = vw_sensors()->hr;
    if (!started) {
        started = 1;
        if (target > MAX_CATCHUP_S) target = te.seconds + 1;
    }
    if (target > te.seconds + MAX_CATCHUP_S) target = te.seconds + 1;
    while (te.seconds < target) te_push(&te, hr);
    update();
    /* fixed point: newlib-nano printf has no %f */
    const long t100 = (long)(te_aerobic(&te) * 100.0f + 0.5f);
    const long e100 = (long)(te_peak_epoc(&te) * 100.0f + 0.5f);
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
