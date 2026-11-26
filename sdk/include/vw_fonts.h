/* Condensed watch fonts for LVGL apps (Barlow Condensed, OFL), pre-rendered
 * at the sizes watch data screens use. Digits are equal width, so numbers
 * don't shift sideways as they change. Each font is linked only if used.
 *
 *   vw_font_num_96, _56, _36   digits and  space ! " # $ % & ' ( ) * + , - . / :
 *   vw_font_label_24, _18      printable ASCII
 */
#ifndef VW_FONTS_H
#define VW_FONTS_H

#include "lvgl.h"

LV_FONT_DECLARE(vw_font_num_96);
LV_FONT_DECLARE(vw_font_num_56);
LV_FONT_DECLARE(vw_font_num_36);
LV_FONT_DECLARE(vw_font_label_24);
LV_FONT_DECLARE(vw_font_label_18);

#endif
