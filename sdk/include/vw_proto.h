/* vwatch wire protocol: host app <-> watch firmware, over the emulated UART.
 *
 * Shared by the firmware runtime (C) and the host (C/C++/Python). Lockstep:
 * the host sends one TICK and waits for TICK_DONE before sending the next,
 * so emulation speed, pause and replay are fully host-controlled.
 *
 * Frame: SYNC0 SYNC1 type len_lo len_hi payload[len]   (little endian)
 */
#ifndef VW_PROTO_H
#define VW_PROTO_H

#include <stdint.h>

#define VW_PROTO_VERSION 1
#define VW_SYNC0 0xA5
#define VW_SYNC1 0x5A
#define VW_MAX_PAYLOAD 4096

/* host -> firmware */
enum {
    VW_MSG_HELLO   = 0x01,  /* (empty) -> firmware answers VW_MSG_INFO */
    VW_MSG_TICK    = 0x02,  /* vw_msg_tick_t -> app_tick(), then RECT*, TICK_DONE */
    VW_MSG_BUTTON  = 0x03,  /* vw_msg_button_t -> app_button() */
};

/* firmware -> host */
enum {
    VW_MSG_INFO      = 0x81,  /* vw_msg_info_t */
    VW_MSG_RECT      = 0x82,  /* vw_msg_rect_t header + w*h pixels (1 byte each) */
    VW_MSG_LOG       = 0x83,  /* UTF-8 text, no terminator */
    VW_MSG_TICK_DONE = 0x84,  /* vw_msg_tick_done_t */
};

typedef struct __attribute__((packed)) {
    uint32_t t_ms;        /* simulated time since boot */
    float hr;             /* bpm, 0 = no reading */
    float speed;          /* m/s */
    float cadence;        /* steps/min */
} vw_msg_tick_t;

typedef struct __attribute__((packed)) {
    uint8_t button;       /* VW_BTN_* */
    uint8_t pressed;      /* 1 down, 0 up */
} vw_msg_button_t;

typedef struct __attribute__((packed)) {
    uint8_t proto_version;
    uint8_t pixel_format;  /* 1 = RGB222 in the low 6 bits of each byte */
    uint16_t width, height;
    uint8_t round;         /* 1 = round display (host masks to a circle) */
    char app_name[24];     /* NUL-padded */
} vw_msg_info_t;

typedef struct __attribute__((packed)) {
    uint16_t x, y, w, h;   /* followed by w*h pixel bytes, row-major */
} vw_msg_rect_t;

typedef struct __attribute__((packed)) {
    uint32_t t_ms;
    uint32_t frame_bytes;  /* pixel bytes sent for this tick (0 = no redraw) */
} vw_msg_tick_done_t;

#endif
