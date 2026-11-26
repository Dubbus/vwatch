/* Streaming Aerobic Training Effect (EPOC accumulation), for a microcontroller.
 *
 *   intensity i = (HR - floor) / (HRmax - floor), clipped to 0..1
 *   EPOC target = exp(k * i) - 1
 *   EPOC follows the target with time constant tau_up when rising and
 *   tau_dn when falling, updated once per 5 s block of mean HR
 *   TE = A * ln(1 + B * peak EPOC), clipped to 0..5
 *
 * No heap, fixed-size state, float32, one 1 Hz HR sample per te_push().
 * Clean-room model from published sport-science concepts; the constants in
 * te_params.h were fitted to a watch's reported TE, not taken from any vendor.
 */
#ifndef TE_MODEL_H
#define TE_MODEL_H

#include <stdint.h>

#define TE_BLOCK_S 5

typedef struct {
    float hr_max;          /* user HRmax (bpm) */
    float block_sum;       /* sum of HR over the current 5 s block */
    uint8_t block_n;       /* samples in the current block */
    float epoc;            /* current EPOC (model units) */
    float peak;            /* peak EPOC so far */
    uint32_t seconds;      /* samples consumed */
} te_state_t;

void  te_init(te_state_t *s, float hr_max);
void  te_push(te_state_t *s, float hr_bpm);
float te_peak_epoc(const te_state_t *s);
float te_aerobic(const te_state_t *s);   /* 0.0 .. 5.0, valid at any time */

#endif
