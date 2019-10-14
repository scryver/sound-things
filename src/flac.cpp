static inline b32
is_flac_file(BitStreamer *bitStream)
{
    b32 result = false;
    if ((bitStream->at[0] == 'f') &&
        (bitStream->at[1] == 'L') &&
        (bitStream->at[2] == 'a') &&
        (bitStream->at[3] == 'C'))
    {
        result = true;
    }
    return result;
}

static inline FlacInfo
parse_info_stream(BitStreamer *bitStream)
{
    FlacInfo info = {};
    info.minBlockSamples   = get_bits(bitStream, 16);
    info.maxBlockSamples   = get_bits(bitStream, 16);
    info.minFrameBytes     = get_bits(bitStream, 24);
    info.maxFrameBytes     = get_bits(bitStream, 24);
    info.sampleRate        = get_bits(bitStream, 20);
    info.channelCount      = get_bits(bitStream, 3) + 1;
    info.bitsPerSample     = get_bits(bitStream, 5) + 1;
    info.totalSamples      = get_bits(bitStream, 36);
    info.md5signature.high = get_bits(bitStream, 64);
    info.md5signature.low  = get_bits(bitStream, 64);
    return info;
}

static FlacFrameHeader
parse_frame_header(BitStreamer *bitStream, FlacInfo *info)
{
    i_expect(bitStream->remainingBits == 0);
    
    FlacFrameHeader result = {};
    u8 *crcCheck = bitStream->at;
    u32 dataBlock = get_bits(bitStream, 32);
    result.syncCode = dataBlock >> 18;
    if (result.syncCode != 0x3FFE)
    {
        fprintf(stderr, "Synccode mismatch: 0x%04X vs 0x3FFE\n", result.syncCode);
    }
    i_expect(result.syncCode == 0x3FFE);
    i_expect((dataBlock & 0x00020000) == 0);
    result.variableBlocks = (dataBlock & 0x00010000) ? 1 : 0;
    
    u32 blockSize = (dataBlock >> 12) & 0x0F;
    i_expect(blockSize);
    
    u32 sampleRate = (dataBlock >> 8) & 0x0F;
    
    result.channelAssignment = (dataBlock >> 4) & 0x0F;
    i_expect(result.channelAssignment <= FlacChannel_MidSide);
    if (result.channelAssignment < FlacChannel_LeftSide)
    {
        result.channelCount = result.channelAssignment + 1;
    }
    else
    {
        result.channelCount = 2;
    }
    
    u32 sampleSize = (dataBlock >> 1) & 0x07;
    switch (sampleSize)
    {
        case 0:  { result.bitsPerSample = info->bitsPerSample; } break;
        case 1:  { result.bitsPerSample =  8; } break;
        case 2:  { result.bitsPerSample = 12; } break;
        case 4:  { result.bitsPerSample = 16; } break;
        case 5:  { result.bitsPerSample = 20; } break;
        case 6:  { result.bitsPerSample = 24; } break;
        case 3:
        case 7:
        default: { i_expect(0); } break;
    }
    
    i_expect((dataBlock & 0x01) == 0);
    i_expect(bitStream->remainingBits == 0);
    
    u32 numberByteCount = 0;
    u64 number = 0;
    u8 numberByte = get_bits(bitStream, 8);
    
    if (result.variableBlocks &&
        (numberByte == 0xFE))
    {
        number = 0;
        numberByteCount = 6;
    }
    else if ((numberByte & 0xFE) == 0xFC)
    {
        number = numberByte & 0x01;
        numberByteCount = 5;
    }
    else if ((numberByte & 0xFC) == 0xF8)
    {
        number = numberByte & 0x03;
        numberByteCount = 4;
    }
    else if ((numberByte & 0xF8) == 0xF0)
    {
        number = numberByte & 0x07;
        numberByteCount = 3;
    }
    else if ((numberByte & 0xF0) == 0xE0)
    {
        number = numberByte & 0x0F;
        numberByteCount = 2;
    }
    else if ((numberByte & 0xE0) == 0xC0)
    {
        number = numberByte & 0x1F;
        numberByteCount = 1;
    }
    else if (!(numberByte & 0x80))
    {
        number = numberByte;
        numberByteCount = 0;
    }
    else
    {
        fprintf(stderr, "Number byte not properly encoded: %lu\n", number);
    }
    for (u32 byteIndex = 0; byteIndex < numberByteCount; ++byteIndex)
    {
        numberByte = get_bits(bitStream, 8);
        if ((numberByte & 0xC0) == 0x80)
        {
            number <<= 6;
            number |= (numberByte & 0x3F);
        }
    }
    
    result.frameNumber = number;
    //i_expect(result.frames.frameCount == result.blocks.sampleCount);
    
    if (blockSize == 0x1)
    {
        result.blockSize = 192;
    }
    else if ((0x2 <= blockSize) && (blockSize <= 0x5))
    {
        result.blockSize = 576 * (1 << (blockSize - 2));
    }
    else if (blockSize == 0x6)
    {
        result.blockSize = get_bits(bitStream, 8) + 1;
    }
    else if (blockSize == 0x7)
    {
        result.blockSize = get_bits(bitStream, 16) + 1;
    }
    else
    {
        result.blockSize = 256 * (1 << (blockSize - 8));
    }
    
    switch (sampleRate)
    {
        case 0x0: { result.sampleRate = info->sampleRate; } break;
        case 0x1: { result.sampleRate =  88200; } break;
        case 0x2: { result.sampleRate = 176400; } break;
        case 0x3: { result.sampleRate = 192000; } break;
        case 0x4: { result.sampleRate =   8000; } break;
        case 0x5: { result.sampleRate =  16000; } break;
        case 0x6: { result.sampleRate =  22050; } break;
        case 0x7: { result.sampleRate =  24000; } break;
        case 0x8: { result.sampleRate =  32000; } break;
        case 0x9: { result.sampleRate =  44100; } break;
        case 0xA: { result.sampleRate =  48000; } break;
        case 0xB: { result.sampleRate =  96000; } break;
        case 0xC:
        {
            result.sampleRate = get_bits(bitStream, 8) * 1000;
        } break;
        
        case 0xD:
        {
            result.sampleRate = get_bits(bitStream, 16);
        } break;
        
        case 0xE:
        {
            result.sampleRate = get_bits(bitStream, 16) * 10;
        } break;
        
        case 0xF:
        default:
        {
            i_expect(0);
        } break;
    }
    
    u8 crcTable[256];
    crc8_init_table(0x07, crcTable);
    u8 crc8calc = crc8_calc_crc(crcTable, bitStream->at - crcCheck, crcCheck);
    
    result.crc8 = get_bits(bitStream, 8); // (polynomial = x^8 + x^2 + x^1 + x^0, initialized with 0)
    if (result.crc8 != crc8calc)
    {
        fprintf(stderr, "CRC8 calc: %02X, CRC8 file: %02X\n", crc8calc, result.crc8);
    }
    i_expect(result.crc8 == crc8calc);
    
    return result;
}

internal FlacSubframeHeader
parse_subframe_header(BitStreamer *bitStream, FlacFrameHeader *frameHeader, u32 channelIndex)
{
    FlacSubframeHeader result;
    u32 testBits = get_bits(bitStream, 8);
    u8 testBit = (testBits & 0x80) >> 7;
    u8 subframeType = (testBits & 0xFE) >> 1;
    b8 hasWastedBits = testBits & 0x01;
    i_expect(testBit == 0);
    
    if (subframeType < FlacSubframe_Reserved0)
    {
        result.type = subframeType;
        result.typeOrder = 0;
    }
    else if ((subframeType > FlacSubframe_Reserved1) &&
             (subframeType < FlacSubframe_Reserved2))
    {
        result.type = FlacSubframe_Fixed;
        if ((subframeType & 0x07) <= 4)
        {
            result.typeOrder = (subframeType & 0x07);
        }
        else
        {
            result.type = FlacSubframe_Error;
        }
    }
    else if ((subframeType > FlacSubframe_Reserved2) &&
             (subframeType < FlacSubframe_Error))
    {
        result.type = FlacSubframe_LPC;
        result.typeOrder = (subframeType & 0x1F) + 1;
    }
    else
    {
        result.type = FlacSubframe_Error;
        result.typeOrder = 0;
    }
    
    result.wastedBits = 0;
    if (hasWastedBits)
    {
        result.wastedBits = 1;
        while (!get_bits(bitStream, 1))
        {
            ++result.wastedBits;
        }
    }
    
    return result;
}

internal void
parse_residual_coding(BitStreamer *bitStream, u32 order, u32 blockSize,
                      Buffer *residual)
{
    u8 residualEncoding = get_bits(bitStream, 2);
    i_expect(residualEncoding < 2);
    u32 partitionOrder = get_bits(bitStream, 4);
    u32 partitionCount = 1 << partitionOrder;
    
#if FLAC_DEBUG_LEVEL > 1
    char *indent = "    ";
    fprintf(stdout, "Rice:\n");
    fprintf(stdout, "%sencoding : %d\n", indent, residualEncoding);
    fprintf(stdout, "%sorder    : %d\n", indent, partitionOrder);
#endif
    
    u32 nrBitsPerRice = residualEncoding == 0 ? 4 : 5;
    u32 riceEscape = residualEncoding == 0 ? 0xF : 0x1F;
    
    u32 nrSamples = ((partitionOrder > 0) ? (blockSize >> partitionOrder) : (blockSize - order));
    
    s32 *dest = (s32 *)residual->data;
    for (u32 partitionIndex = 0; partitionIndex < partitionCount; ++partitionIndex)
    {
        u32 riceParameter = get_bits(bitStream, nrBitsPerRice);
        
        u32 partitionSamples = ((partitionOrder == 0) || (partitionIndex > 0)) ? nrSamples : nrSamples - order;
#if FLAC_DEBUG_LEVEL > 1
        fprintf(stdout, "%ssamples  : %d\n", indent, partitionSamples);
        fprintf(stdout, "%sparameter: %d\n", indent, riceParameter);
#endif
        
        if (riceParameter == riceEscape)
        {
            u32 bitsPerSample = get_bits(bitStream, 5);
            for (u32 i = 0; i < partitionSamples; ++i)
            {
                *dest++ = (s32)(get_bits(bitStream, bitsPerSample) >> (32 - bitsPerSample)) << (32 - bitsPerSample);
            }
        }
        else
        {
            for (u32 i = 0; i < partitionSamples; ++i)
            {
                u32 q = 0;
                // TODO(michiel): Optimize check 0
                while (!get_bits(bitStream, 1))
                {
                    ++q;
                }
                u32 x = q << riceParameter;
                x |= get_bits(bitStream, riceParameter);
                s32 value = (s32)(x >> 1) ^ -(s32)(x & 1);
                *dest++ = value;
                
#if FLAC_DEBUG_LEVEL > 2
                fprintf(stdout, "%s%srice %u: %d (q %u r %u)\n", indent, indent, i, value, q, riceParameter);
#endif
            }
        }
    }
}

static void
print_info_stream(FlacInfo *info, char *indent = "")
{
    fprintf(stdout, "%smin block size: %d samples\n", indent, info->minBlockSamples);
    fprintf(stdout, "%smax block size: %d samples\n", indent, info->maxBlockSamples);
    fprintf(stdout, "%smin frame size: %d bytes\n", indent, info->minFrameBytes);
    fprintf(stdout, "%smax frame size: %d bytes\n", indent, info->maxFrameBytes);
    fprintf(stdout, "%ssample rate   : %d Hz\n", indent, info->sampleRate);
    fprintf(stdout, "%schannel count : %d\n", indent, info->channelCount);
    fprintf(stdout, "%ssample depth  : %d bits\n", indent, info->bitsPerSample);
    fprintf(stdout, "%stotal samples : %lu\n", indent, info->totalSamples);
    fprintf(stdout, "%sMD5 signature : 0x%016lX%016lX\n", indent,
            info->md5signature.high, info->md5signature.low);
}