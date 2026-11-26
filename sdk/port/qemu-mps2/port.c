/* Port: QEMU mps2-an386 (Cortex-M4F). Wire protocol over CMSDK APB UART0. */
#include <stdint.h>
#include "../../src/vw_internal.h"

#define UART0_BASE 0x40004000u
#define UART_DATA     (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_STATE    (*(volatile uint32_t *)(UART0_BASE + 0x04))
#define UART_CTRL     (*(volatile uint32_t *)(UART0_BASE + 0x08))
#define UART_BAUDDIV  (*(volatile uint32_t *)(UART0_BASE + 0x10))
#define STATE_TX_FULL (1u << 0)
#define STATE_RX_FULL (1u << 1)
#define CTRL_TX_EN    (1u << 0)
#define CTRL_RX_EN    (1u << 1)

void vw_port_init(void) {
    UART_BAUDDIV = 16;                  /* QEMU requires >= 16 to enable the UART */
    UART_CTRL = CTRL_TX_EN | CTRL_RX_EN;
}

void vw_port_write(const uint8_t *buf, uint32_t n) {
    while (n--) {
        while (UART_STATE & STATE_TX_FULL) {}
        UART_DATA = *buf++;
    }
}

uint8_t vw_port_read_byte(void) {
    while (!(UART_STATE & STATE_RX_FULL)) {}
    return (uint8_t)UART_DATA;
}

int vw_port_display_round(void) { return 1; }

/* newlib-nano syscall stub: stdio output isn't wired anywhere on this port
 * (use vw_log), but defining _write keeps the link warning-free. */
int _write(int fd, const char *buf, int len) {
    (void)fd; (void)buf;
    return len;
}
