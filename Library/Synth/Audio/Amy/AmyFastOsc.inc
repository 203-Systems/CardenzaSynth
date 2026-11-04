// Included into AMY's oscillators.c by a build-time patch (cmake/CardenzaSynthSourceLists.cmake); AMY's
// submodule stays untouched. Fixed point (AMY_USE_FIXEDPOINT): SAMPLE is s8.23, PHASOR a 31-bit turn.
//
// A Juno voice is pulse, saw and sub oscillators read from AMY's band-limited saw tables with cubic
// interpolation (render_lut_cub), a pulse being a saw minus the same saw later in the cycle: two
// passes. Those passes were most of a Juno voice on the board (the stress test's profile of DEMO-2,
// 2026-11-04).
//
// cardenza_render_lut_cub is render_lut_cub to the bit: the same products and shifts on the same
// values. It finds the table index and fraction with a product instead of AMY's two shifts by the
// table's size, so the loop sets no shift amount and keeps its values in registers.
//
// cardenza_render_lut_cub_square is a pulse at a duty of one half (the Juno's sub oscillator, and any
// pulse not modulated off the middle) in one pass: half a turn on is a whole number of table steps,
// so the later saw has the same fraction and the cubic, being linear in the table's values, can run
// once on their differences. Against two passes it differs by AMY's rounding only.

// INTERP_CUBIC (Miller's form) to the bit, from the table's own values: AMY's L2S shifts them up 8
// bits, its MUL0_SS shifts each operand down (8 and 7 bits) before the product, and differences of
// values 8 bits up shift back without loss, so this is the same arithmetic with those shifts taken
// out. f is S_FRAC_OF_P's 23-bit fraction.
static inline __attribute__((always_inline)) SAMPLE cardenza_cubic(int32_t ta, int32_t tb, int32_t tc, int32_t td, int32_t f)
{
    const int32_t f16 = f >> 7;                                                     // MUL0_SS's frac operand
    const int32_t cminusb = tc - tb;
    const int32_t fr = ((td - ta - 3 * cminusb) * f16) >> 8;                        // fr_d_ma_m3cmb
    const int32_t sixth = ((((F2S(1.0f) - f) >> 8) * (F2S(0.16666666666667f) >> 7)) >> 8) >> 7;
    const int32_t next_bit = (((fr >> 8) + td + 2 * ta - 3 * tb) * sixth) >> 8;
    return (tb << 8) + (((cminusb + ((-next_bit) >> 8)) * f16) >> 8);
}

// One block of render_lut_cub, or with `square` of a pulse at half duty, for a table of 2^bits values.
// Called with constant bits and square it compiles to shifts and masks by constants: AMY's loop
// shifted by the table's size, a variable amount, and Xtensa's one shift-amount register and 16
// registers made it set that register twice a sample and keep half its values on the stack (75
// instructions a sample, 61 here).
static inline __attribute__((always_inline)) PHASOR cardenza_lut_cub_block(SAMPLE* buf, uint32_t turn, uint32_t turn_step, SAMPLE current_amp,
                                                                           SAMPLE incremental_amp, const LUTSAMPLE* table, const int bits,
                                                                           const int square, SAMPLE* pmax_value)
{
    const uint32_t mask = ((uint32_t)1 << bits) - 1;
    const uint32_t half = (uint32_t)1 << (bits - 1);
    SAMPLE max_value = 0;
    SAMPLE* const end = buf + AMY_BLOCK_SIZE;
    for (; buf < end; buf++) {
        const uint32_t base = turn >> (32 - bits);                                  // INT_OF_P
        const int32_t f = (int32_t)((turn << bits) >> (32 - S_FRAC_BITS));          // S_FRAC_OF_P
        const uint32_t ia = (base - 1) & mask, ic = (base + 1) & mask, id = (base + 2) & mask;
        int32_t ta = table[ia];
        int32_t tb = table[base];
        int32_t tc = table[ic];
        int32_t td = table[id];
        if (square) {  // half a turn on: the index's top bit flipped
            ta -= table[ia ^ half];
            tb -= table[base ^ half];
            tc -= table[ic ^ half];
            td -= table[id ^ half];
        }
        SAMPLE value = *buf + MULA_SS(cardenza_cubic(ta, tb, tc, td, f), current_amp);
        *buf = value;
        if (value < 0) value = -value;
        if (value > max_value) max_value = value;
        current_amp += incremental_amp;
        turn += turn_step;                                                          // P_WRAPPED_SUM
    }
    *pmax_value = max_value;
    return turn;
}

// render_lut_cub's arguments; square as above. The phase over a whole turn of 32 bits: P_WRAPPED_SUM
// is this sum's wrap, INT_OF_P its top bits and S_FRAC_OF_P the next 23.
static PHASOR cardenza_render_lut_cub(SAMPLE* buf, PHASOR phase, PHASOR step, SAMPLE incoming_amp, SAMPLE ending_amp,
                                      const LUT* lut, int square, SAMPLE* pmax_value)
{
    if (lut == NULL) {  // AMY returns without a maximum, which its callers then read
        *pmax_value = 0;
        return phase;
    }
    const uint32_t turn = (uint32_t)phase << 1;
    const uint32_t turn_step = (uint32_t)step << 1;
    const SAMPLE incremental_amp = SHIFTR(ending_amp - incoming_amp, BLOCK_SIZE_BITS);
    const LUTSAMPLE* table = lut->table;
    uint32_t end;
#define CARDENZA_LUT_CUB_CASE(bits) \
    case bits: \
        end = square ? cardenza_lut_cub_block(buf, turn, turn_step, incoming_amp, incremental_amp, table, bits, 1, pmax_value) \
                     : cardenza_lut_cub_block(buf, turn, turn_step, incoming_amp, incremental_amp, table, bits, 0, pmax_value); \
        break;
    // The saw tables a note from 50 Hz to 2 kHz reads (2048 to 128 values); above, AMY's own loop.
    switch (lut->log_2_table_size) {
        CARDENZA_LUT_CUB_CASE(11)
        CARDENZA_LUT_CUB_CASE(10)
        CARDENZA_LUT_CUB_CASE(9)
        CARDENZA_LUT_CUB_CASE(8)
        CARDENZA_LUT_CUB_CASE(7)
        default: {
            const PHASOR next = render_lut_cub(buf, phase, step, incoming_amp, ending_amp, lut, pmax_value);
            if (square) render_lut_cub(buf, P_WRAPPED_SUM(phase, F2P(0.5f)), step, -incoming_amp, -ending_amp, lut, pmax_value);
            return next;
        }
    }
#undef CARDENZA_LUT_CUB_CASE
    return (PHASOR)(end >> 1);
}

// One block of AMY's linear-interpolating loops (render_lut, render_lut_fm, render_lut_fb and
// render_lut_fm_fb: the DX7's operators), to the bit, for a table of 2^bits values; constant bits,
// use_mod and use_fb select the variant at compile time as above. The modulation and the feedback add
// to the phase before the index is taken, as in RENDER_LUT_GUTS (INT_OF_P drops the turn's carry).
static inline __attribute__((always_inline)) uint32_t cardenza_lut_lin_block(SAMPLE* buf, uint32_t turn, uint32_t turn_step, SAMPLE current_amp,
                                                                             SAMPLE incremental_amp, const LUTSAMPLE* table, const int bits,
                                                                             const SAMPLE* mod, const int use_mod, SAMPLE feedback_level,
                                                                             SAMPLE* last_two, const int use_fb, SAMPLE* pmax_value)
{
    const uint32_t mask = ((uint32_t)1 << bits) - 1;
    SAMPLE max_value = 0;
    SAMPLE sample = 0, past0 = 0, past1 = 0;
    if (use_fb) {
        sample = last_two[0];
        past0 = last_two[1];
    }
    for (int i = 0; i < AMY_BLOCK_SIZE; i++) {
        uint32_t total = turn;
        if (use_mod) total += (uint32_t)S2P(mod[i]) << 1;                                         // MOD_PART_MOD
        if (use_fb) {                                                                             // FEEDBACK_PART_FB
            past1 = past0;
            past0 = sample;
            total += (uint32_t)S2P(MUL4_SS(feedback_level, SHIFTR(past1 + past0, 1))) << 1;
        }
        const uint32_t base = total >> (32 - bits);
        const int32_t f16 = (int32_t)((total << bits) >> 16);                                   // S_FRAC_OF_P >> 7
        const int32_t tb = table[base];
        const int32_t tc = table[(base + 1) & mask];
        sample = (tb << 8) + (((tc - tb) * f16) >> 8);                                            // INTERP_LINEAR
        SAMPLE value = buf[i] + MULA_SS(sample, current_amp);
        buf[i] = value;
        if (value < 0) value = -value;
        if (value > max_value) max_value = value;
        current_amp += incremental_amp;
        turn += turn_step;
    }
    if (use_fb) {
        last_two[0] = sample;
        last_two[1] = past0;
    }
    *pmax_value = max_value;
    return turn;
}

// render_fm_sine's and render_sine's choice of loop, the sine table's 256 values with constant
// shifts; any other table goes to AMY's loops.
static PHASOR cardenza_render_lut_lin(SAMPLE* buf, PHASOR phase, PHASOR step, SAMPLE incoming_amp, SAMPLE ending_amp, const LUT* lut,
                                      SAMPLE* mod, SAMPLE feedback_level, SAMPLE* last_two, SAMPLE* pmax_value)
{
    if (lut == NULL) {
        *pmax_value = 0;
        return phase;
    }
    if (lut->log_2_table_size != 8) {
        if (feedback_level > 0 && mod) return render_lut_fm_fb(buf, phase, step, incoming_amp, ending_amp, lut, mod, feedback_level, last_two, pmax_value);
        if (feedback_level > 0) return render_lut_fb(buf, phase, step, incoming_amp, ending_amp, lut, feedback_level, last_two, pmax_value);
        if (mod) return render_lut_fm(buf, phase, step, incoming_amp, ending_amp, lut, mod, pmax_value);
        return render_lut(buf, phase, step, incoming_amp, ending_amp, lut, pmax_value);
    }
    const uint32_t turn = (uint32_t)phase << 1;
    const uint32_t turn_step = (uint32_t)step << 1;
    const SAMPLE incremental_amp = SHIFTR(ending_amp - incoming_amp, BLOCK_SIZE_BITS);
    const LUTSAMPLE* table = lut->table;
    uint32_t end;
    if (feedback_level > 0 && mod)
        end = cardenza_lut_lin_block(buf, turn, turn_step, incoming_amp, incremental_amp, table, 8, mod, 1, feedback_level, last_two, 1, pmax_value);
    else if (feedback_level > 0)
        end = cardenza_lut_lin_block(buf, turn, turn_step, incoming_amp, incremental_amp, table, 8, NULL, 0, feedback_level, last_two, 1, pmax_value);
    else if (mod)
        end = cardenza_lut_lin_block(buf, turn, turn_step, incoming_amp, incremental_amp, table, 8, mod, 1, 0, NULL, 0, pmax_value);
    else
        end = cardenza_lut_lin_block(buf, turn, turn_step, incoming_amp, incremental_amp, table, 8, NULL, 0, 0, NULL, 0, pmax_value);
    return (PHASOR)(end >> 1);
}
