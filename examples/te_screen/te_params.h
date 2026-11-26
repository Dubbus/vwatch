/* Aerobic TE model constants for te_screen.
 *
 * Fitted on the repo author's own runs: least squares against the watch's own
 * session Training Effect on the chronological first 70% of activities (163),
 * held-out last 30% (71): MAE 0.28 TE, r 0.97. Only these six numbers are
 * published, no activity data. Refit on your own FIT files for real use.
 *
 * TE_TAU_DN and TE_A sit on the fit's upper bounds: the data barely constrain
 * the decay constant (the fit is nearly flat in it), and A trades off against B.
 */
#ifndef TE_PARAMS_H
#define TE_PARAMS_H
#define TE_K        0.850058f    /* EPOC target = exp(k * intensity) - 1 */
#define TE_TAU_UP   420.997f     /* s, EPOC rise time constant */
#define TE_TAU_DN   5000.0f      /* s, EPOC decay time constant (bound-limited) */
#define TE_A        12.0f        /* TE = A * ln(1 + B * peak EPOC) (bound-limited) */
#define TE_B        0.433228f
#define TE_HR_FLOOR 78.2996f     /* bpm; HR below this adds no load */
#endif
