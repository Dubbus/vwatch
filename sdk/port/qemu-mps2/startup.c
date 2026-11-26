/* Cortex-M4F startup for QEMU mps2-an386: vector table, FPU enable, .bss
 * zeroing, then main(). QEMU's ELF loader places .data at its link address,
 * so no flash-to-RAM copy is needed on this port. */
#include <stdint.h>

extern uint32_t _estack, __bss_start__, __bss_end__;
extern int main(void);
extern void __libc_init_array(void);

#define SCB_CPACR (*(volatile uint32_t *)0xE000ED88u)

void Reset_Handler(void) {
    SCB_CPACR |= (0xFu << 20);                  /* CP10 + CP11 full access (FPU) */
    __asm volatile ("dsb\n\tisb");
    for (uint32_t *p = &__bss_start__; p < &__bss_end__; p++) *p = 0;
    __libc_init_array();
    main();
    for (;;) {}
}

static void Fault_Handler(void) {
    for (;;) {}         /* the host sees a watchdog timeout */
}

__attribute__((section(".isr_vector"), used))
const void *const vw_vectors[16] = {
    &_estack, Reset_Handler,
    Fault_Handler, Fault_Handler, Fault_Handler, Fault_Handler, Fault_Handler,
    0, 0, 0, 0,
    Fault_Handler, Fault_Handler, 0, Fault_Handler, Fault_Handler,
};
