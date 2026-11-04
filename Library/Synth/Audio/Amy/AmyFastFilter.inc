// Included into AMY's filters.c by a build-time patch (cmake/CardenzaSynthSourceLists.cmake); AMY's
// submodule stays untouched. Fixed point (AMY_USE_FIXEDPOINT): SAMPLE is s8.23 in an int32_t.
//
// AMY's 24 dB lowpass (FILTER_LPF24, the Juno's): two biquads with b = (1, 2, 1) in AMY's split
// feedback form (a1 = -2 + e, a2 = 1 - f), exactly as dsps_biquad_f32_ansi_split_fb_twice computes it,
// but each product exact in 64 bits instead of top16SMUL's headroom shifts. Those were some 167
// instructions a sample with branches and stack spills, 290 us a voice and block on the board, the
// dearest part of a Juno voice (the stress test's profile of DEMO-2, 2026-11-04). Against a double
// precision reference it is as close as AMY's at the lowest cutoffs (20-90 Hz) and far closer above
// them, and it does not run away at high resonance, where AMY's 15-bit operands did.

// Rounded, not floored: near the lowest cutoffs the poles sit next to 1 and the recursion builds a
// floor's bias up into an offset many times the signal's error.
static inline SAMPLE cardenza_mul_s23(SAMPLE a, SAMPLE b)
{
    return (SAMPLE)(((int64_t)a * (int64_t)b + ((int64_t)1 << (S_FRAC_BITS - 1))) >> S_FRAC_BITS);
}

static SAMPLE cardenza_biquad_lpf24(const SAMPLE *input, SAMPLE *output, int len, const SAMPLE *coef, SAMPLE *w)
{
    SAMPLE x1 = w[0];
    SAMPLE x2 = w[1];
    SAMPLE y1 = w[2];
    SAMPLE y2 = w[3];
    SAMPLE v1 = w[4];
    SAMPLE v2 = w[5];
    const SAMPLE a = coef[0];
    const SAMPLE e = F2S(2.0f) + coef[3];  // so coef[3] = -2 + e
    const SAMPLE f = F2S(1.0f) - coef[4];  // so coef[4] = 1 - f
    SAMPLE max_out = 0;
    for (int i = 0; i < len; i++) {
        const SAMPLE x0 = cardenza_mul_s23(a, input[i]);
        const SAMPLE w0 = x0 + SHIFTL(x1, 1) + x2;
        const SAMPLE v0 = w0 + SHIFTL(v1, 1) - v2 - cardenza_mul_s23(e, v1) + cardenza_mul_s23(f, v2);
        const SAMPLE u0 = cardenza_mul_s23(a, v0 + SHIFTL(v1, 1) + v2);
        const SAMPLE y0 = u0 + SHIFTL(y1, 1) - y2 - cardenza_mul_s23(e, y1) + cardenza_mul_s23(f, y2);
        x2 = x1;
        x1 = x0;
        v2 = v1;
        v1 = v0;
        y2 = y1;
        y1 = y0;
        output[i] = y0;
        const SAMPLE magnitude = y0 < 0 ? -y0 : y0;
        if (magnitude > max_out) max_out = magnitude;
    }
    w[0] = x1;
    w[1] = x2;
    w[2] = y1;
    w[3] = y2;
    w[4] = v1;
    w[5] = v2;
    return max_out;
}
