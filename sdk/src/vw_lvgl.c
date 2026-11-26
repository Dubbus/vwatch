/* LVGL display backend: partial 16-bit rendering, converted to the 64-colour
 * (RGB222) display and shipped region by region. No full framebuffer needed. */
#include "vw_lvgl.h"
#include "vw_internal.h"

#define BUF_LINES 24                                  /* 240 x 24 strip */

static uint16_t draw_buf[VW_W * BUF_LINES] __attribute__((aligned(4)));
static uint8_t out[VW_W * BUF_LINES];
static lv_display_t *disp;
static uint32_t sent;

/* RGB565 -> RGB222 (4 levels per channel: 0, 85, 170, 255). All three
 * channels are quantized from 5 bits with identical thresholds, so any grey
 * stays exactly grey; using green's 6th bit tips anti-aliased grey edges green,
 * and it carries no information at 2 bits of output anyway. */
static inline uint8_t q2(uint32_t v5) { return (uint8_t)((v5 * 3 + 15) / 31); }
static inline uint8_t to_rgb222(uint16_t p) {
    return (uint8_t)((q2((p >> 11) & 0x1F) << 4) | (q2((p >> 6) & 0x1F) << 2) | q2(p & 0x1F));
}

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px_map) {
    const int w = lv_area_get_width(a), h = lv_area_get_height(a);
    const uint16_t *src = (const uint16_t *)px_map;
    for (int i = 0; i < w * h; i++) out[i] = to_rgb222(src[i]);
    sent += vw_send_rect(a->x1, a->y1, w, h, out, w);
    lv_display_flush_ready(d);
}

static uint32_t tick_cb(void) { return vw_millis(); }

lv_display_t *vw_lvgl_init(void) {
    if (disp) return disp;
    lv_init();
    lv_tick_set_cb(tick_cb);
    disp = lv_display_create(VW_W, VW_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof draw_buf, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);
    return disp;
}

/* Display backend hook (see runtime.c). */
uint32_t vw_lvgl_update(int full) {
    if (!disp) return 0;
    sent = 0;
    if (full) lv_obj_invalidate(lv_screen_active());
    lv_timer_handler();                 /* app timers, animations */
    lv_refr_now(disp);                  /* render invalidated areas now (button events don't advance time) */
    return sent;
}
