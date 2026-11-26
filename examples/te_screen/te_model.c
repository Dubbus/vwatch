#include "te_model.h"
#include "te_params.h"
#include <math.h>

/* Per-block exponential update factors depend only on the constants, so they
 * are computed once here instead of on every block (saves two expf calls). */
static float a_up, a_dn;
static int factors_ready;

static void init_factors(void) {
    a_up = 1.0f - expf(-(float)TE_BLOCK_S / TE_TAU_UP);
    a_dn = 1.0f - expf(-(float)TE_BLOCK_S / TE_TAU_DN);
    factors_ready = 1;
}

void te_init(te_state_t *s, float hr_max) {
    if (!factors_ready) init_factors();
    s->hr_max = hr_max;
    s->block_sum = 0.0f;
    s->block_n = 0;
    s->epoc = 0.0f;
    s->peak = 0.0f;
    s->seconds = 0;
}

static void update_block(te_state_t *s, float hr) {
    float span = s->hr_max - TE_HR_FLOOR;
    float i = span > 0.0f ? (hr - TE_HR_FLOOR) / span : 0.0f;
    if (i < 0.0f) i = 0.0f;
    if (i > 1.0f) i = 1.0f;
    float target = expf(TE_K * i) - 1.0f;
    s->epoc += (target - s->epoc) * (target > s->epoc ? a_up : a_dn);
    if (s->epoc > s->peak) s->peak = s->epoc;
}

void te_push(te_state_t *s, float hr_bpm) {
    s->seconds++;
    s->block_sum += hr_bpm;
    if (++s->block_n == TE_BLOCK_S) {
        update_block(s, s->block_sum / TE_BLOCK_S);
        s->block_sum = 0.0f;
        s->block_n = 0;
    }
}

float te_peak_epoc(const te_state_t *s) { return s->peak; }

float te_aerobic(const te_state_t *s) {
    float te = TE_A * logf(1.0f + TE_B * s->peak);
    if (te < 0.0f) te = 0.0f;
    if (te > 5.0f) te = 5.0f;
    return te;
}
