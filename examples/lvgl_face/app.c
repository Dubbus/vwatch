/* lvgl_face: hello_face rebuilt with LVGL, to compare the look.
 *   - clock from simulated time (starts at 10:08:00), seconds ring
 *   - heart rate with a zone bar
 *   - UP/DOWN cycle the accent colour, START toggles a stopwatch,
 *     BACK resets it when stopped, hold BACK resets it any time
 */
#include <vw_lvgl.h>

VW_APP_NAME("lvgl_face");

#define START_S (10 * 3600 + 8 * 60)

static const uint32_t accents[] = {0xFF5500, 0x00AAFF, 0x55FF00, 0xFFFF00, 0xFF00FF};
static int accent;
static int sw_running;
static uint32_t sw_start_ms, sw_elapsed_ms;

static lv_obj_t *ring, *time_lbl, *sec_lbl, *date_lbl, *hr_lbl, *hr_dot, *zone_bar, *sw_lbl;

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, lv_align_t align, int x, int y) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_align(l, align, x, y);
    return l;
}

static void restyle(void) {
    const lv_color_t c = lv_color_hex(accents[accent]);
    lv_obj_set_style_arc_color(ring, c, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(sec_lbl, c, 0);
    lv_obj_set_style_text_color(sw_lbl, sw_running ? c : lv_color_hex(0x888888), 0);
}

static void update(void) {
    const uint32_t ms = vw_millis();
    const uint32_t t = START_S + ms / 1000;
    const int h = (int)(t / 3600 % 24), m = (int)(t / 60 % 60), s = (int)(t % 60);

    lv_label_set_text_fmt(time_lbl, "%02d:%02d", h, m);
    lv_label_set_text_fmt(sec_lbl, "%02d", s);
    lv_arc_set_value(ring, s + 1);

    const float hr = vw_sensors()->hr;
    if (hr > 0) lv_label_set_text_fmt(hr_lbl, "%d", (int)(hr + 0.5f));
    else lv_label_set_text(hr_lbl, "--");
    lv_bar_set_value(zone_bar, hr > 0 ? (int)hr : 0, LV_ANIM_OFF);
    /* zone colour: grey / blue / green / orange / red, like most watch HR gauges */
    static const uint32_t zone_col[] = {0x888888, 0x00AAFF, 0x55FF00, 0xFF5500, 0xFF0000};
    const int zone = hr < 110 ? 0 : hr < 130 ? 1 : hr < 150 ? 2 : hr < 170 ? 3 : 4;
    lv_obj_set_style_bg_color(zone_bar, lv_color_hex(zone_col[zone]), LV_PART_INDICATOR);

    const uint32_t sw = sw_elapsed_ms + (sw_running ? ms - sw_start_ms : 0);
    lv_label_set_text_fmt(sw_lbl, "%s %02lu:%02lu", sw_running ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE,
                          (unsigned long)(sw / 60000), (unsigned long)(sw / 1000 % 60));
}

void app_init(void) {
    vw_lvgl_init();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    ring = lv_arc_create(scr);                               /* seconds ring around the bezel */
    lv_obj_set_size(ring, 236, 236);
    lv_obj_center(ring);
    lv_arc_set_rotation(ring, 270);
    lv_arc_set_bg_angles(ring, 0, 360);
    lv_arc_set_range(ring, 0, 60);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ring, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ring, true, LV_PART_INDICATOR);

    date_lbl = label(scr, &lv_font_montserrat_14, 0xAAAAAA, LV_ALIGN_CENTER, 0, -62);
    lv_label_set_text(date_lbl, "THU NOV 26");
    time_lbl = label(scr, &lv_font_montserrat_48, 0xFFFFFF, LV_ALIGN_CENTER, 0, -22);
    sec_lbl = label(scr, &lv_font_montserrat_20, 0xFF5500, LV_ALIGN_CENTER, 0, 18);

    hr_dot = lv_obj_create(scr);                             /* small red "heart" dot */
    lv_obj_set_size(hr_dot, 10, 10);
    lv_obj_set_style_radius(hr_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hr_dot, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_width(hr_dot, 0, 0);
    lv_obj_align(hr_dot, LV_ALIGN_CENTER, -30, 52);
    hr_lbl = label(scr, &lv_font_montserrat_28, 0xFFFFFF, LV_ALIGN_CENTER, 8, 52);

    zone_bar = lv_bar_create(scr);                           /* HR zone bar */
    lv_obj_set_size(zone_bar, 90, 6);
    lv_obj_align(zone_bar, LV_ALIGN_CENTER, 0, 76);
    lv_bar_set_range(zone_bar, 60, 200);
    lv_obj_set_style_bg_color(zone_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(zone_bar, LV_OPA_COVER, LV_PART_MAIN);

    sw_lbl = label(scr, &lv_font_montserrat_14, 0x888888, LV_ALIGN_CENTER, 0, 96);

    restyle();
    update();
    vw_log("lvgl_face booted");
}

void app_tick(void) { update(); }

void app_button(int button, int event) {
    if (event == VW_HELD && button == VW_BTN_BACK) {
        sw_running = 0;
        sw_elapsed_ms = 0;
        vw_log("stopwatch reset");
    } else if (event == VW_PRESSED) {
        const int n = (int)(sizeof accents / sizeof accents[0]);
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
    }
    restyle();
    update();
}
