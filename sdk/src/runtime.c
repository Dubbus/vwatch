/* vwatch runtime: owns main(), speaks the wire protocol, calls the app. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "vwatch.h"
#include "vw_proto.h"
#include "vw_internal.h"

static vw_sensors_t sensors;
static uint32_t now_ms;
static uint8_t rx[VW_MAX_PAYLOAD];

/* backlight: auto-on with any button press, off after a timeout (sim time) */
static int bl_on, bl_auto = 1;
static uint32_t bl_timeout_ms = 8000, bl_off_at;

const vw_sensors_t *vw_sensors(void) { return &sensors; }
uint32_t vw_millis(void) { return now_ms; }

static void send(uint8_t type, const void *payload, uint16_t len) {
    uint8_t hdr[5] = {VW_SYNC0, VW_SYNC1, type, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
    vw_port_write(hdr, sizeof hdr);
    if (len) vw_port_write((const uint8_t *)payload, len);
}

void vw_log(const char *fmt, ...) {
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= (int)sizeof buf) n = sizeof buf - 1;
    send(VW_MSG_LOG, buf, (uint16_t)n);
}

static void set_backlight(int on) {
    if (on == bl_on) return;
    bl_on = on;
    vw_msg_backlight_t m = {(uint8_t)(on ? 100 : 0)};
    send(VW_MSG_BACKLIGHT, &m, sizeof m);
}

void vw_backlight(int on) {
    if (on) bl_off_at = now_ms + bl_timeout_ms;
    set_backlight(on ? 1 : 0);
}

void vw_backlight_timeout(uint32_t ms) { bl_timeout_ms = ms; }
void vw_backlight_auto(int enabled) { bl_auto = enabled ? 1 : 0; }

/* Send a screen region as RECT messages, split by rows so each fits
 * VW_MAX_PAYLOAD. px points at the region's top-left pixel; stride is the
 * source row length in bytes. Returns pixel bytes sent. Used by both display
 * backends. */
uint32_t vw_send_rect(int x, int y, int w, int h, const uint8_t *px, int stride) {
    if (w <= 0 || h <= 0) return 0;
    int rows_per_msg = (VW_MAX_PAYLOAD - (int)sizeof(vw_msg_rect_t)) / w;
    uint32_t sent = 0;
    for (int r0 = 0; r0 < h; r0 += rows_per_msg) {
        int rh = h - r0 < rows_per_msg ? h - r0 : rows_per_msg;
        vw_msg_rect_t r = {(uint16_t)x, (uint16_t)(y + r0), (uint16_t)w, (uint16_t)rh};
        uint16_t len = (uint16_t)(sizeof r + (uint32_t)w * (uint32_t)rh);
        uint8_t hdr[5] = {VW_SYNC0, VW_SYNC1, VW_MSG_RECT, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
        vw_port_write(hdr, sizeof hdr);
        vw_port_write((const uint8_t *)&r, sizeof r);
        for (int i = 0; i < rh; i++) vw_port_write(px + (r0 + i) * stride, (uint32_t)w);
        sent += (uint32_t)(w * rh);
    }
    return sent;
}

/* Display backends. The SDK is linked as a library, so only the backend an app
 * actually uses ends up in its image: gfx.c (the vw_* drawing calls) or
 * vw_lvgl.c (LVGL). Each pushes changed regions; full = resend everything. */
uint32_t vw_gfx_update(int full) __attribute__((weak));
uint32_t vw_lvgl_update(int full) __attribute__((weak));

static uint32_t display_update(int full) {
    uint32_t n = 0;
    if (vw_gfx_update) n += vw_gfx_update(full);
    if (vw_lvgl_update) n += vw_lvgl_update(full);
    return n;
}

/* Read one framed message; returns its type, fills rx/len. Resyncs on garbage. */
static uint8_t recv(uint16_t *len) {
    for (;;) {
        if (vw_port_read_byte() != VW_SYNC0) continue;
        if (vw_port_read_byte() != VW_SYNC1) continue;
        uint8_t type = vw_port_read_byte();
        uint16_t n = vw_port_read_byte();
        n |= (uint16_t)(vw_port_read_byte() << 8);
        if (n > sizeof rx) continue;
        for (uint16_t i = 0; i < n; i++) rx[i] = vw_port_read_byte();
        *len = n;
        return type;
    }
}

int main(void) {
    vw_port_init();
    app_init();
    for (;;) {
        uint16_t len;
        uint8_t type = recv(&len);
        switch (type) {
        case VW_MSG_HELLO: {
            vw_msg_info_t info;
            memset(&info, 0, sizeof info);
            info.proto_version = VW_PROTO_VERSION;
            info.pixel_format = 1;
            info.width = VW_W;
            info.height = VW_H;
            info.round = (uint8_t)vw_port_display_round();
            for (size_t i = 0; i < sizeof info.app_name - 1 && vw_app_name[i]; i++)
                info.app_name[i] = vw_app_name[i];      /* NUL-padded by the memset */
            send(VW_MSG_INFO, &info, sizeof info);
            {                                            /* tell the host the current light state */
                vw_msg_backlight_t m = {(uint8_t)(bl_on ? 100 : 0)};
                send(VW_MSG_BACKLIGHT, &m, sizeof m);
            }
            display_update(1);                           /* host needs a full frame */
            break;
        }
        case VW_MSG_TICK:
            if (len >= sizeof(vw_msg_tick_t)) {
                vw_msg_tick_t t;
                memcpy(&t, rx, sizeof t);
                now_ms = t.t_ms;
                sensors.hr = t.hr;
                sensors.speed = t.speed;
                sensors.cadence = t.cadence;
                if (bl_on && bl_timeout_ms && (int32_t)(now_ms - bl_off_at) >= 0) set_backlight(0);
                app_tick();
                vw_msg_tick_done_t done = {now_ms, display_update(0)};
                send(VW_MSG_TICK_DONE, &done, sizeof done);
            }
            break;
        case VW_MSG_BUTTON:
            if (len >= sizeof(vw_msg_button_t)) {
                vw_msg_button_t b;
                memcpy(&b, rx, sizeof b);
                if (b.button < VW_BTN_COUNT && b.pressed <= VW_HELD) {
                    if (b.pressed == VW_PRESSED && bl_auto) vw_backlight(1);
                    app_button(b.button, b.pressed);
                }
                display_update(0);
            }
            break;
        default:
            break;
        }
    }
}
