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

/* Ship the dirty region as one or more RECT messages, rows split so each
 * message fits VW_MAX_PAYLOAD. Returns pixel bytes sent. */
static uint32_t flush_dirty(void) {
    if (vw_dirty.x1 < vw_dirty.x0) return 0;
    int x = vw_dirty.x0, w = vw_dirty.x1 - vw_dirty.x0 + 1;
    int rows_per_msg = (VW_MAX_PAYLOAD - (int)sizeof(vw_msg_rect_t)) / w;
    uint32_t sent = 0;
    for (int y = vw_dirty.y0; y <= vw_dirty.y1; y += rows_per_msg) {
        int h = vw_dirty.y1 - y + 1;
        if (h > rows_per_msg) h = rows_per_msg;
        vw_msg_rect_t r = {(uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h};
        uint16_t len = (uint16_t)(sizeof r + (uint32_t)w * (uint32_t)h);
        uint8_t hdr[5] = {VW_SYNC0, VW_SYNC1, VW_MSG_RECT, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
        vw_port_write(hdr, sizeof hdr);
        vw_port_write((const uint8_t *)&r, sizeof r);
        for (int yy = y; yy < y + h; yy++) vw_port_write(&vw_fb[yy][x], (uint32_t)w);
        sent += (uint32_t)(w * h);
    }
    vw_dirty_reset();
    return sent;
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
    vw_clear(VW_BLACK);
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
            strncpy(info.app_name, vw_app_name, sizeof info.app_name);
            send(VW_MSG_INFO, &info, sizeof info);
            vw_dirty.x0 = 0; vw_dirty.y0 = 0;            /* host needs a full frame */
            vw_dirty.x1 = VW_W - 1; vw_dirty.y1 = VW_H - 1;
            flush_dirty();
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
                app_tick();
                vw_msg_tick_done_t done = {now_ms, flush_dirty()};
                send(VW_MSG_TICK_DONE, &done, sizeof done);
            }
            break;
        case VW_MSG_BUTTON:
            if (len >= sizeof(vw_msg_button_t)) {
                vw_msg_button_t b;
                memcpy(&b, rx, sizeof b);
                if (b.button < VW_BTN_COUNT) app_button(b.button, b.pressed);
                flush_dirty();
            }
            break;
        default:
            break;
        }
    }
}
