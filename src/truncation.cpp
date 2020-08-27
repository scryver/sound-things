// NOTE(michiel): All for 128 -> 24 bit truncations

#if 0
internal void
generate_s128_tpdf(RandomSeriesPCG *series, u32 sampleCount, void *sampleBuffer)
{
    __int128_t *sampleAt = (__int128_t *)sampleBuffer;
    
    for (u32 index = 0; index < sampleCount; ++index)
    {
        __int128_t a = random_next_u32(series);
        a = (a << 32) | random_next_u32(series);
        a = (a << 32) | random_next_u32(series);
        a = (a << 32) | random_next_u32(series);
        
        __int128_t b = random_next_u32(series);
        b = (b << 32) | random_next_u32(series);
        b = (b << 32) | random_next_u32(series);
        b = (b << 32) | random_next_u32(series);
        
        *sampleAt++ = (a + b) >> 23;
    }
}

internal void
s128_from_f32_truncated(u32 sampleCount, f32 *src, void *dest)
{
    __int128_t *sampleAt = (__int128_t *)dest;
    
    for (u32 index = 0; index < sampleCount; ++index)
    {
        f32 source = src[index];
        u32 sourceRaw = *(u32 *)&source;
        b32 negative  = sourceRaw & F32_SIGN_MASK;
        s32 exponent  = (sourceRaw & F32_EXP_MASK) >> 23;
        b32 denormal  = exponent == 0;
        
        s32 mantissa  = (sourceRaw & F32_FRAC_MASK) | (denormal ? 0 : 0x00800000);
        // NOTE(michiel): Sign extend
        mantissa = (mantissa << 8) >> 8;
        
        exponent -= 127;
        i_expect(exponent <= 0);
        
        __int128_t d = (__int128_t)mantissa;
        
        s32 shifts = 104 - exponent;
        if (shifts < 0)
        {
            shifts = -shifts;
            d = d >> shifts;
        }
        else
        {
            d = d << shifts;
        }
        
        *sampleAt++ = negative ? -d : d;
    }
}
#endif

internal __int128_t
generate_s128_tpdf(RandomSeriesPCG *series)
{
    // NOTE(michiel): Make sure upper 24 bits are zero
    // last shift adds some extra randomness to the lower 23:8 bits
    __int128_t a = (u64)random_next_u32(series);
    a = (a << 32) | random_next_u32(series);
    a = (a << 32) | random_next_u32(series);
    a = (a <<  8) ^ random_next_u32(series);
    
    __int128_t b = (u64)random_next_u32(series);
    b = (b << 32) | random_next_u32(series);
    b = (b << 32) | random_next_u32(series);
    b = (b <<  8) ^ random_next_u32(series);
    
    // NOTE(michiel): Basically the same as adding, except we don't need negative random samples
    return a - b;
}

internal __int128_t
generate_s128_dtpdf(RandomSeriesPCG *series)
{
    __int128_t a = generate_s128_tpdf(series);
    __int128_t b = generate_s128_tpdf(series);
    return a - b;
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
        }
        else
        {
            //i_expect(exponent <= 0);
            s32 shifts = 104 + exponent;
            if (shifts < 0)
            {
                shifts = -shifts;
                if (shifts > 24)
                {
                    result = 0;
                }
                else
                {
                    result >>= shifts;
                }
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
s24_from_f32_tpdf(RandomSeriesPCG *series, f32 value)
{
    __int128_t f = s128_from_f32_truncated(value);
    __int128_t n = generate_s128_tpdf(series);
    
    // TODO(michiel): 1 less bit dither and check for overflow
    __int128_t p = f + n;
    s32 result = (p >> 104);
    return result;
}

internal s32
s24_from_f32_dtpdf(RandomSeriesPCG *series, f32 value)
{
    __int128_t f = s128_from_f32_truncated(value);
    __int128_t n = generate_s128_dtpdf(series);
    
    __int128_t p = f + n;
    s32 result = (p >> 104);
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
