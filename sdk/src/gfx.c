/* Framebuffer + drawing primitives with dirty-rectangle tracking. */
#include <math.h>
#include <string.h>
#include "vwatch.h"
#include "vw_internal.h"

extern const uint8_t vw_font5x8[95][5];

uint8_t vw_fb[VW_H][VW_W];
vw_dirty_t vw_dirty = {VW_W, VW_H, -1, -1};     /* empty: x0 > x1 */

static inline void mark(int x0, int y0, int x1, int y1) {   /* inclusive, pre-clipped */
    if (x0 < vw_dirty.x0) vw_dirty.x0 = x0;
    if (y0 < vw_dirty.y0) vw_dirty.y0 = y0;
    if (x1 > vw_dirty.x1) vw_dirty.x1 = x1;
    if (y1 > vw_dirty.y1) vw_dirty.y1 = y1;
}

void vw_dirty_reset(void) {
    vw_dirty.x0 = VW_W; vw_dirty.y0 = VW_H; vw_dirty.x1 = -1; vw_dirty.y1 = -1;
}

void vw_clear(vw_color_t c) {
    memset(vw_fb, c, sizeof vw_fb);
    mark(0, 0, VW_W - 1, VW_H - 1);
}

void vw_pixel(int x, int y, vw_color_t c) {
    if ((unsigned)x >= VW_W || (unsigned)y >= VW_H) return;
    vw_fb[y][x] = c;
    mark(x, y, x, y);
}

void vw_fill_rect(int x, int y, int w, int h, vw_color_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > VW_W ? VW_W : x + w, y1 = y + h > VW_H ? VW_H : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) memset(&vw_fb[yy][x0], c, (size_t)(x1 - x0));
    mark(x0, y0, x1 - 1, y1 - 1);
}

void vw_rect(int x, int y, int w, int h, vw_color_t c) {
    vw_fill_rect(x, y, w, 1, c);
    vw_fill_rect(x, y + h - 1, w, 1, c);
    vw_fill_rect(x, y, 1, h, c);
    vw_fill_rect(x + w - 1, y, 1, h, c);
}

void vw_line(int x0, int y0, int x1, int y1, vw_color_t c) {     /* Bresenham */
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        vw_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void vw_circle(int cx, int cy, int r, vw_color_t c) {           /* midpoint */
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        vw_pixel(cx + x, cy + y, c); vw_pixel(cx + y, cy + x, c);
        vw_pixel(cx - y, cy + x, c); vw_pixel(cx - x, cy + y, c);
        vw_pixel(cx - x, cy - y, c); vw_pixel(cx - y, cy - x, c);
        vw_pixel(cx + y, cy - x, c); vw_pixel(cx + x, cy - y, c);
        y++;
        if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
    }
}

void vw_fill_circle(int cx, int cy, int r, vw_color_t c) {
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy));
        vw_fill_rect(cx - dx, cy + dy, 2 * dx + 1, 1, c);
    }
}

void vw_arc(int cx, int cy, int r, int thickness, float a0, float a1, vw_color_t c) {
    if (a1 < a0) { float t = a0; a0 = a1; a1 = t; }
    if (a1 - a0 >= 360.0f) { a0 = 0.0f; a1 = 360.0f; }
    int ri = r - thickness;
    int r2 = r * r, ri2 = ri > 0 ? ri * ri : 0;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 > r2 || d2 < ri2) continue;
            /* clockwise from 12 o'clock, in degrees 0..360 */
            float a = atan2f((float)dx, (float)-dy) * 57.2957795f;
            if (a < 0.0f) a += 360.0f;
            if ((a >= a0 && a <= a1) || (a + 360.0f >= a0 && a + 360.0f <= a1))
                vw_pixel(cx + dx, cy + dy, c);
        }
    }
}

int vw_text(int x, int y, const char *s, vw_color_t c, int scale) {
    if (scale < 1) scale = 1;
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch < 0x20 || ch > 0x7E) ch = '?';
        const uint8_t *g = vw_font5x8[ch - 0x20];
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 8; row++)
                if (g[col] & (1u << row))
                    vw_fill_rect(x + col * scale, y + row * scale, scale, scale, c);
        x += 6 * scale;
    }
    return x;
}

int vw_text_width(const char *s, int scale) {
    int n = (int)strlen(s);
    return n ? (6 * n - 1) * (scale < 1 ? 1 : scale) : 0;
}

void vw_text_centered(int cx, int y, const char *s, vw_color_t c, int scale) {
    vw_text(cx - vw_text_width(s, scale) / 2, y, s, c, scale);
}

/* Display backend hook (see runtime.c): push the dirty region, or everything. */
uint32_t vw_gfx_update(int full) {
    if (full) { vw_dirty.x0 = 0; vw_dirty.y0 = 0; vw_dirty.x1 = VW_W - 1; vw_dirty.y1 = VW_H - 1; }
    if (vw_dirty.x1 < vw_dirty.x0) return 0;
    uint32_t n = vw_send_rect(vw_dirty.x0, vw_dirty.y0, vw_dirty.x1 - vw_dirty.x0 + 1,
                              vw_dirty.y1 - vw_dirty.y0 + 1, &vw_fb[vw_dirty.y0][vw_dirty.x0], VW_W);
    vw_dirty_reset();
    return n;
}
