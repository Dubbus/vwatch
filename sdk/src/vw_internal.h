/* SDK-internal declarations (not for apps). */
#ifndef VW_INTERNAL_H
#define VW_INTERNAL_H

#include <stdint.h>
#include "vwatch.h"

typedef struct { int x0, y0, x1, y1; } vw_dirty_t;   /* inclusive; empty if x1 < x0 */

extern uint8_t vw_fb[VW_H][VW_W];
extern vw_dirty_t vw_dirty;
void vw_dirty_reset(void);

/* ---------- port layer: implemented once per target (sdk/port/<target>) ---------- */
void vw_port_init(void);
void vw_port_write(const uint8_t *buf, uint32_t n);   /* blocking */
uint8_t vw_port_read_byte(void);                     /* blocking */
int vw_port_display_round(void);                     /* 1 if the display is round */

#endif
