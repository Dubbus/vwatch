/* Aerobic TE model constants for te_screen.
 *
 * Fitted on the repo author's own runs: least squares against the watch's own
 * session Training Effect on the chronological first 70% of activities (164),
 * held-out last 30% (71): MAE 0.28 TE, r 0.97. Only these six numbers are
 * published, no activity data. Refit on your own FIT files for real use.
 */
#ifndef TE_PARAMS_H
#define TE_PARAMS_H
#define TE_K        0.849876f    /* EPOC target = exp(k * intensity) - 1 */
#define TE_TAU_UP   422.74f      /* s, EPOC rise time constant */
#define TE_TAU_DN   2625.54f     /* s, EPOC decay time constant */
#define TE_A        12.0f        /* TE = A * ln(1 + B * peak EPOC) */
#define TE_B        0.434136f
#define TE_HR_FLOOR 77.8144f     /* bpm; HR below this adds no load */
#endif
