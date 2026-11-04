// Included into AMY's amy.c by a build-time patch (cmake/CardenzaSynthSourceLists.cmake); AMY's
// submodule stays untouched.
//
// hold_and_modify works out every oscillator's amplitude twice a block (now and at the block's end),
// each time through up to four log2f and a powf(10, x): library calls of some hundreds of cycles on
// this chip, for each oscillator of each voice, every block. These are polynomials on the float
// unit instead: log2 within 3e-5 (the amplitude within 2e-5, some 93 dB down), 2^x within 3e-7.

static inline float cardenza_log2f(float x)  // x > 0
{
    union { float f; uint32_t u; } v;
    v.f = x;
    const float exponent = (float)((int32_t)((v.u >> 23) & 0xFFu) - 127);
    v.u = (v.u & 0x007FFFFFu) | 0x3F800000u;  // the mantissa, in [1, 2)
    const float m = v.f;
    return exponent + (((((0.043428363f * m - 0.404862309f) * m + 1.593884548f) * m - 3.492466043f) * m + 5.046852936f) * m - 2.786805564f);
}

static inline float cardenza_exp2f(float x)
{
    if (x < -126.0f) return 0.0f;
    if (x > 127.0f) x = 127.0f;
    int whole = (int)x;
    if ((float)whole > x) --whole;  // floor
    const float f = x - (float)whole;
    union { float f; uint32_t u; } v;
    v.f = ((((0.001894379f * f + 0.008940578f) * f + 0.055876569f) * f + 0.240131684f) * f + 0.693156779f) * f + 0.999999769f;
    v.u += (uint32_t)whole << 23;  // times 2^whole: the polynomial is in [1, 2)
    return v.f;
}

// map_60dB_to_01f: .001 to 0 and 1 to 1, logarithmically.
static inline float cardenza_map_60dB_to_01f(float lin)
{
    if (lin == 0) return -10.0f;
    return 1.0f + 0.10034333188799373f * cardenza_log2f(lin);
}
