// NOTE(michiel): All for f32 -> s128 -> s24 bit truncations

#ifndef DITHER_OUTPUT_BITS
#define DITHER_OUTPUT_BITS    24
#endif
#ifndef DITHER_HEADROOM_BITS
#define DITHER_HEADROOM_BITS   2
#endif

#define DITHER_TOTAL_BITS      (DITHER_OUTPUT_BITS + DITHER_HEADROOM_BITS)
#define DITHER_LAST_TPDF_SHIFT (32 - DITHER_TOTAL_BITS)

compile_expect(DITHER_TOTAL_BITS < 32);

global u32 gPrintFlags = U32_MAX;

internal __int128_t
generate_s128_tpdf(RandomSeriesPCG *series)
{
    // TODO(michiel): Maybe use simd random numbers?
    
    // NOTE(michiel): Make sure upper 24 bits are zero
    // last shift adds some extra randomness to the lower 23:8 bits
    __int128_t a = (u64)random_next_u32(series);
    a = (a << 32) | random_next_u32(series);
    a = (a << 32) | random_next_u32(series);
    a = (a << DITHER_LAST_TPDF_SHIFT) ^ random_next_u32(series);
    
    __int128_t b = (u64)random_next_u32(series);
    b = (b << 32) | random_next_u32(series);
    b = (b << 32) | random_next_u32(series);
    b = (b << DITHER_LAST_TPDF_SHIFT) ^ random_next_u32(series);
    
    // NOTE(michiel): Basically the same as adding, except we don't need negative random samples
    __int128_t result = a - b;
    if (gPrintFlags & 0x1)
    {
        fprintf(stdout, "TPDF a(0x%016lX%016lX) - b(0x%016lX%016lX) = 0x%016lX%016lX\n",
                (u64)(a >> 64), (u64)a, (u64)(b >> 64), (u64)b, (u64)(result >> 64), (u64)result);
        gPrintFlags &= ~0x1;
    }
    return result;
}

internal __int128_t
generate_s128_dtpdf(RandomSeriesPCG *series)
{
    __int128_t a = generate_s128_tpdf(series);
    __int128_t b = generate_s128_tpdf(series);
    __int128_t result = a - b;
    if (gPrintFlags & 0x2)
    {
        fprintf(stdout, "DTPDF a(0x%016lX%016lX) - b(0x%016lX%016lX) = 0x%016lX%016lX\n",
                (u64)(a >> 64), (u64)a, (u64)(b >> 64), (u64)b, (u64)(result >> 64), (u64)result);
        gPrintFlags &= ~0x2;
    }
    return result;
}

internal __int128_t
s128_from_f32_truncated(f32 value)
{
    u32 sourceRaw = *(u32 *)&value;
    b32 negative  = sourceRaw & F32_SIGN_MASK;
    s32 exponent  = (sourceRaw & F32_EXP_MASK) >> 23;
    b32 denormal  = exponent == 0;
    
    s32 mantissa  = (sourceRaw & F32_FRAC_MASK) | (denormal ? 0 : 0x00800000);
    
    __int128_t result = (__int128_t)mantissa;
    
    if (exponent && mantissa)
    {
        exponent -= 127;
        if (exponent > 0)
        {
            fprintf(stderr, "Overflow: %f\n", value);
            if (negative)
            {
                result = ((__int128_t)0x8000000000000000LL) << 64;
            }
            else
            {
                result = (((__int128_t)0x7FFFFFFFFFFFFFFFLL) << 64) | 0xFFFFFFFFFFFFFFFFLL;
            }
#if DITHER_HEADROOM_BITS
            result >>= DITHER_HEADROOM_BITS;
#endif
        }
        else
        {
            //i_expect(exponent <= 0);
            // NOTE(michiel): Minus mantissa+headroom = 24 + 2
            s32 shifts = (128 - (24 + DITHER_HEADROOM_BITS)) + exponent;
            
            if (gPrintFlags & 0x4)
            {
                fprintf(stdout, "Shifts %d\n", shifts);
                gPrintFlags &= ~0x4;
            }
            
            if (shifts < 0)
            {
                shifts = -shifts;
                result >>= shifts;
            }
            else
            {
                result <<= shifts;
            }
            
            if (negative)
            {
                result = -result;
            }
        }
    }
    
    return result;
}

internal s32
s24_from_f32_truncated(f32 value)
{
    u32 sourceRaw = *(u32 *)&value;
    b32 negative  = sourceRaw & F32_SIGN_MASK;
    s32 exponent  = (sourceRaw & F32_EXP_MASK) >> 23;
    b32 denormal  = exponent == 0;
    
    s32 mantissa  = (sourceRaw & F32_FRAC_MASK) | (denormal ? 0 : 0x00800000);
    
    s32 result = 0;
    if (exponent && mantissa)
    {
        exponent -= 127;
        if (exponent > 0)
        {
            fprintf(stderr, "Overflow: %f\n", value);
            if (negative) {
                result = 0x80000000;
            } else {
                result = 0x7FFFFFFF;
            }
        }
        else
        {
            //i_expect(exponent <= 0);
            s32 shifts = 8 + exponent;
            if (shifts < 0)
            {
                shifts = -shifts;
                if (shifts > 24)
                {
                    result = 0;
                }
                else
                {
                    result = mantissa >> shifts;
                }
            }
            else
            {
                result = mantissa << shifts;
            }
            
            if (negative)
            {
                result = -result;
            }
        }
    }
    
    result = result >> 8;
    return result;
}

internal s32
dither_saturate(__int128_t p)
{
    s32 result = 0;
#if DITHER_HEADROOM_BITS == 2
    if (((p >> 126) & 0x3) != ((p >> 125) & 0x3))
    {
        if (p >> 127)
        {
            result = 0x80000000;
        }
        else
        {
            result = 0x7FFFFFFF;
        }
        result >>= (32 - DITHER_OUTPUT_BITS);
    }
    else
    {
        result = p >> (126 - DITHER_OUTPUT_BITS);
    }
#elif DITHER_HEADROOM_BITS == 1
    if (((p >> 127) & 1) != ((p >> 126) & 1))
    {
        if (p >> 127)
        {
            result = 0x80000000;
        }
        else
        {
            result = 0x7FFFFFFF;
        }
        result >>= (32 - DITHER_OUTPUT_BITS);
    }
    else
    {
        result = p >> (127 - DITHER_OUTPUT_BITS);
    }
#elif DITHER_HEADROOM_BITS == 0
    result = p >> (128 - DITHER_OUTPUT_BITS);
#else
#error Don't know how to process this
#endif
    if (gPrintFlags & 0x8)
    {
        fprintf(stdout, "Dithered 0x%016lX%016lX => 0x%08X\n", (u64)(p >> 64), (u64)p, result);
        gPrintFlags &= ~0x8;
    }
    
    return result;
}

internal s32
s24_from_f32_tpdf(RandomSeriesPCG *series, f32 value)
{
    __int128_t f = s128_from_f32_truncated(value);
    __int128_t n = generate_s128_tpdf(series);
    
    __int128_t p = f + n;
    s32 result = dither_saturate(p);
    return result;
}

internal s32
s24_from_f32_dtpdf(RandomSeriesPCG *series, f32 value)
{
    __int128_t f = s128_from_f32_truncated(value);
    __int128_t n = generate_s128_dtpdf(series);
    
    __int128_t p = f + n;
    s32 result = dither_saturate(p);
    return result;
}

internal void
s24_from_f32_tpdf(RandomSeriesPCG *series, u32 sampleCount, f32 *src, s32 *dest)
{
    for (u32 index = 0; index < sampleCount; ++index)
    {
        *dest++ = s24_from_f32_tpdf(series, *src++);
    }
}

internal void
s24_from_f32_dtpdf(RandomSeriesPCG *series, u32 sampleCount, f32 *src, s32 *dest)
{
    for (u32 index = 0; index < sampleCount; ++index)
    {
        *dest++ = s24_from_f32_dtpdf(series, *src++);
    }
}

internal f32
f32_from_s24(s32 value)
{
    i_expect(((value & 0xFF800000) == 0xFF800000) ||
             ((value & 0xFF800000) == 0x00000000));
    
    u32 sign = 0;
    if (value < 0)
    {
        sign = F32_SIGN_MASK;
        value = -value;
    }
    
    s32 exponent = 0;
    u32 mantissa = value & 0x00FFFFFF;
    BitScanResult msb = find_most_significant_set_bit(mantissa);
    if (msb.found)
    {
        i_expect(msb.index < 24);
        s32 shifts = 23 - msb.index;
        exponent += 127 - shifts;
        mantissa = (mantissa << shifts) & 0x007FFFFF;
        exponent = (exponent & 0xFF) << 23;
    }
    
    u32 builded = sign | exponent | mantissa;
    f32 result = *(f32 *)&builded;
    return result;
}
