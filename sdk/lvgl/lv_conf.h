/* LVGL configuration for vwatch (LVGL v9.2). Anything not set here falls back
 * to LVGL's defaults in lv_conf_internal.h. Tuned for a small watch MCU:
 * 16-bit rendering into partial buffers, converted to the 64-colour display
 * by the SDK glue (sdk/src/vw_lvgl.c). */
#ifndef LV_CONF_H
#define LV_CONF_H

/* ---- colour / rendering ---- */
#define LV_COLOR_DEPTH 16
#define LV_DEF_REFR_PERIOD 33
#define LV_DPI_DEF 200                 /* ~1.2" 240 px round display */

/* ---- memory: LVGL's own heap, no malloc ---- */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN
#define LV_SPRINTF_USE_FLOAT 1         /* lv_label_set_text_fmt("%.1f", te) */
#define LV_MEM_SIZE (48U * 1024U)

/* ---- OS / logging ---- */
#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

/* ---- drawing ---- */
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_COMPLEX 1           /* anti-aliased arcs, rounded corners, shadows */
#define LV_DRAW_BUF_ALIGN 4

/* ---- fonts (Montserrat ships with LVGL, OFL licensed) ---- */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ---- theme ---- */
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

/* ---- widgets: what a watch UI needs ---- */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BUTTON 1
#define LV_USE_IMAGE 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_SCALE 1
#define LV_USE_LED 1
#define LV_USE_CHART 1

#define LV_USE_ANIMIMG 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SLIDER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/* ---- extras off ---- */
#define LV_USE_SNAPSHOT 0
#define LV_USE_SYSMON 0
#define LV_USE_PROFILER 0
#define LV_USE_MONKEY 0
#define LV_USE_GRIDNAV 0
#define LV_USE_FRAGMENT 0
#define LV_USE_IMGFONT 0
#define LV_USE_OBSERVER 0
#define LV_USE_IME_PINYIN 0
#define LV_USE_FILE_EXPLORER 0
#define LV_BUILD_EXAMPLES 0
#define LV_USE_DEMO_WIDGETS 0

#endif
