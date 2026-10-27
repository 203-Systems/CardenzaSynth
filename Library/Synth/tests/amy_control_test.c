// Compare optimized control math against AMY a63d600c's arithmetic, including
// rounding at each float/fixed boundary. This runs against the actual AMY library.
#include "amy.h"
#include <stdio.h>

extern float amp_combine_controls(float *controls, float *coefs);

static float legacy_amp(float *controls, float *coefs) {
    float result = 0;
    for (int i = 0; i < NUM_COMBO_COEFS; ++i) {
        float coef = coefs[i], val = controls[i];
        if (i == COEF_CONST) { val = coef; coef = 1.0f; }
        if (i != COEF_MOD) val = map_60dB_to_01f(MAX(0, val)) - 1.0;
        result += coef * val;
    }
    result = powf(10.0f, 3.0f * result);
    return result <= AMP_THRESH_PLUS ? 0 : result;
}

static SAMPLE legacy_dx7(SAMPLE start, SAMPLE end, uint32_t duration, uint32_t elapsed) {
    start = MIN(F2S(1.0f), start);
    end = MIN(F2S(1.0f), end);
    if (end > start) {
        // Deliberately retain the original double expressions as the reference.
        double l0 = S2F(log2_lut(MAX(F2S(0.0002), start))) + 12.375;
        double l1 = S2F(log2_lut(MAX(F2S(0.0002), end))) + 12.375;
        SAMPLE m0 = F2S(1 - MAX(l0 - 4.25, 0) / 9.375);
        SAMPLE m1 = F2S(1 - MAX(l1 - 4.25, 0) / 9.375);
        float t_const = duration / S2F(log2_lut(m0) - log2_lut(m1));
        float my_t0 = -t_const * S2F(log2_lut(m0));
        return exp2_lut(F2S(4.25 + 9.375 * S2F(F2S(1.0f) -
            exp2_lut(-F2S((my_t0 + elapsed) / t_const))) - 12.375));
    }
    start = MAX(start, F2S(0.0002));
    end = MAX(end, F2S(0.0002));
    SAMPLE rate = F2S(S2F(log2_lut(end) - log2_lut(start)) / (0.001f * duration));
    return MUL4_SS(start, exp2_lut(MUL4_SS(rate, F2S(0.001f * (elapsed - 1)))));
}

static int check_envelopes(void) {
    struct synthinfo oscillator = {0};
    struct synthinfo *pointer = &oscillator;
    synth = &pointer;
    uint32_t times[] = {1, 0, 1};
    float levels[] = {0, 0, 0};
    oscillator.breakpoint_times[0] = times;
    oscillator.breakpoint_values[0] = levels;
    oscillator.max_num_breakpoints[0] = 3;
    oscillator.eg_type[0] = ENVELOPE_DX7;
    oscillator.note_on_clock = 0;
    AMY_UNSET(oscillator.note_off_clock);
    const float amplitudes[] = {0, 0.0002f, 0.001f, 0.01f, 0.1f, 0.3f, 0.75f, 1.0f, 1.25f};
    const uint32_t durations[] = {64, 255, 256, 257, 12000, 220500};
    unsigned checks = 0;
    for (int sign = -1; sign <= 1; sign += 2)
    for (unsigned a = 0; a < sizeof(amplitudes) / sizeof(*amplitudes); ++a)
    for (unsigned b = 0; b < sizeof(amplitudes) / sizeof(*amplitudes); ++b)
    for (unsigned d = 0; d < sizeof(durations) / sizeof(*durations); ++d) {
        // Below the DX7 attack-map floor both mapped endpoints are one. The
        // legacy equation has a zero divisor there and is not a valid oracle.
        if (amplitudes[b] > amplitudes[a] && amplitudes[b] < 0.004f) continue;
        times[1] = durations[d];
        levels[0] = sign * amplitudes[a];
        levels[1] = sign * amplitudes[b];
        for (uint32_t elapsed = 1; elapsed <= times[1]; elapsed += MAX(1, times[1] / 113)) {
            amy_global.total_blocks = (elapsed - 1) / AMY_BLOCK_SIZE;
            uint16_t offset = (elapsed - 1) % AMY_BLOCK_SIZE;
            int expected_sign = (levels[0] < 0 || levels[1] < 0) ? -1 : 1;
            SAMPLE expected = expected_sign * legacy_dx7(F2S(amplitudes[a]), F2S(amplitudes[b]), times[1], elapsed);
            SAMPLE actual = compute_breakpoint_scale(0, 0, offset);
            if (actual != expected) {
                fprintf(stderr, "DX7 mismatch: %g -> %g duration %u elapsed %u: %d != %d\n",
                    levels[0], levels[1], times[1], elapsed, actual, expected);
                return 1;
            }
            ++checks;
        }
    }
    synth = NULL;
    printf("DX7 envelope: %u attack/decay/bipolar comparisons passed\n", checks);
    return 0;
}

static int check_filter_slew(void) {
    struct synthinfo oscillator = {0};
    struct mod_synthinfo mod = {0};
    struct synthinfo *pointer = &oscillator;
    struct mod_synthinfo *mod_pointer = &mod;
    synth = &pointer;
    msynth = &mod_pointer;
    AMY_UNSET(oscillator.midi_note);
    AMY_UNSET(oscillator.mod_source);
    oscillator.amp_coefs[COEF_CONST] = 1;
    // Probe both sides of the slew boundary, especially values whose subtraction
    // rounded differently in double and float. The stored result must match.
    for (int i = -10000; i <= 20000; ++i) {
        float previous = i / 1000.0f;
        float threshold = previous - 2.0f;
        float targets[] = {-10.0f, 0, 10.0f, threshold,
            nextafterf(threshold, -INFINITY), nextafterf(threshold, INFINITY)};
        for (unsigned t = 0; t < sizeof(targets) / sizeof(*targets); ++t) {
            float expected = targets[t];
            if (expected < -2.75) expected = -2.75;
            if (expected < previous - 2.0) expected = previous - 2.0;
            oscillator.filter_logfreq_coefs[COEF_CONST] = targets[t];
            mod.last_filter_logfreq = previous;
            hold_and_modify(0);
            if (mod.filter_logfreq != expected || mod.last_filter_logfreq != expected) {
                fprintf(stderr, "filter slew mismatch: previous %g target %g\n", previous, targets[t]);
                return 1;
            }
        }
    }
    synth = NULL;
    msynth = NULL;
    puts("filter slew: 180006 boundary comparisons passed");
    return 0;
}

int main(void) {
    if (check_envelopes() || check_filter_slew()) return 1;
    uint32_t random = 12345;
    for (int test = 0; test < 30000; ++test) {
        float inputs[NUM_COMBO_COEFS], coefs[NUM_COMBO_COEFS];
        for (int i = 0; i < NUM_COMBO_COEFS; ++i) {
            random = random * 1664525u + 1013904223u;
            inputs[i] = ((int)((random >> 8) % 3001) - 1000) / 1000.0f;
            random = random * 1664525u + 1013904223u;
            coefs[i] = (random & 3) ? 0 : ((int)((random >> 8) % 13) - 4) / 8.0f;
        }
        if (amp_combine_controls(inputs, coefs) != legacy_amp(inputs, coefs)) {
            fprintf(stderr, "amplitude mismatch at case %d\n", test);
            return 2;
        }
    }
    for (int i = -32768; i < 32768; ++i) {
        float phase = i / 65536.0f;
        PHASOR expected = (PHASOR)(phase * 2.0 * (float)(1 << (P_FRAC_BITS - 1)));
        if (F2P(phase) != expected) return 3;
    }
    puts("amplitude: 30000 comparisons; phase: 65536 comparisons passed");
    return 0;
}
