/* vwatch + LVGL: build the watch UI with LVGL (https://lvgl.io).
 *
 *     #include <vw_lvgl.h>
 *     void app_init(void) {
 *         vw_lvgl_init();
 *         lv_obj_t *l = lv_label_create(lv_screen_active());
 *         lv_label_set_text(l, "hello");
 *     }
 *
 * Build with USE_LVGL := 1 in the app Makefile. The runtime redraws after
 * every tick and button event; LVGL's clock is the emulator's sim time.
 * LVGL renders in 16-bit colour; the SDK converts to the 64-colour display.
 * Don't mix LVGL with the vw_* drawing calls in one app.
 */
#ifndef VW_LVGL_H
#define VW_LVGL_H

#include "vwatch.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Call once from app_init(). Returns the display bound to the watch screen. */
lv_display_t *vw_lvgl_init(void);

/* The 64 display colours as LVGL colours (exact matches, no rounding loss). */
#define VW_LV_COLOR(rgb222) lv_color_make((((rgb222) >> 4) & 3) * 85, (((rgb222) >> 2) & 3) * 85, ((rgb222) & 3) * 85)

#ifdef __cplusplus
}
#endif
#endif
