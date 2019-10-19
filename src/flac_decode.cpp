#include "../libberdip/platform.h"

#include <alsa/asoundlib.h>

#include "./platform_sound.h"

#include "./linux_sound.h"
#include "./linux_sound.cpp"

#ifndef FLAC_DEBUG_LEVEL
#define FLAC_DEBUG_LEVEL  1
#endif

#include "flac.h"

#include "../libberdip/std_file.c"
#include "../libberdip/crc.cpp"

#include "bitstreamer.cpp"
#include "flac.cpp"

internal s32
get_signed32_left(BitStreamer *bitStream, u32 bitCount)
{
    i_expect(bitCount);
    i_expect(bitCount <= 32);
    s32 result = ((s32)get_bits(bitStream, bitCount) << (32 - bitCount));
    return result;
}

internal s32
get_signed32(BitStreamer *bitStream, u32 bitCount)
{
    i_expect(bitCount);
    i_expect(bitCount <= 32);
    s32 result = ((s32)get_bits(bitStream, bitCount) << (32 - bitCount));
    result >>= (32 - bitCount);
    return result;
}

internal void
process_constant(BitStreamer *bitStream, u32 bitsPerSample,
                 u32 blockCount, s32 *samples)
{
    // NOTE(michiel): expects samples[blockCount]
    s32 constant = get_signed32_left(bitStream, bitsPerSample);
    s32 *dst = samples;
    for (u32 blockIdx = 0; blockIdx < blockCount; ++blockIdx)
    {
        *dst++ = constant;
    }
}

internal void
process_verbatim(BitStreamer *bitStream, u32 bitsPerSample,
                 u32 blockCount, s32 *samples)
{
    // NOTE(michiel): expects samples[blockCount]
    s32 *dst = samples;
    for (u32 blockIdx = 0; blockIdx < blockCount; ++blockIdx)
    {
        s32 source = get_signed32_left(bitStream, bitsPerSample);
        *dst++ = source;
    }
}

internal void
process_fixed(BitStreamer *bitStream, u32 order, u32 bitsPerSample,
              u32 blockCount, s32 *samples)
{
    // NOTE(michiel): expects samples[blockCount]
    for (u32 warmupIdx = 0; warmupIdx < order; ++warmupIdx)
    {
        samples[warmupIdx] = get_signed32(bitStream, bitsPerSample);
    }
    
    Buffer residual;
    residual.size = (blockCount - order) * sizeof(s32);
    residual.data = allocate_array(u8, residual.size);
    parse_residual_coding(bitStream, order, blockCount, &residual);
    
    s32 *res = (s32 *)residual.data;
    
    switch (order)
    {
        case 0:
        {
            for (u32 blockIdx = order; blockIdx < blockCount; ++blockIdx)
            {
                samples[blockIdx] = *res++;
            }
        } break;
        
        case 1:
        {
            for (u32 blockIdx = order; blockIdx < blockCount; ++blockIdx)
            {
                samples[blockIdx] = *res++ + samples[blockIdx - 1];
            }
        } break;
        
        case 2:
        {
            for (u32 blockIdx = order; blockIdx < blockCount; ++blockIdx)
            {
                samples[blockIdx] = *res++ + 2*samples[blockIdx - 1] - samples[blockIdx - 2];
            }
        } break;
        
        case 3:
        {
            for (u32 blockIdx = order; blockIdx < blockCount; ++blockIdx)
            {
                samples[blockIdx] = *res++ + 3*samples[blockIdx - 1] - 3*samples[blockIdx - 2] + samples[blockIdx - 3];
            }
        } break;
        
        case 4:
        {
            for (u32 blockIdx = order; blockIdx < blockCount; ++blockIdx)
            {
                samples[blockIdx] = *res++ + 4*samples[blockIdx - 1] - 6*samples[blockIdx - 2] + 4*samples[blockIdx - 3] - samples[blockIdx - 4];
            }
        } break;
        
        INVALID_DEFAULT_CASE;
    }
    
    for (u32 blockIdx = 0; blockIdx < blockCount; ++blockIdx)
    {
        samples[blockIdx] = samples[blockIdx] << (32 - bitsPerSample);
    }
}

internal void
process_lpc(BitStreamer *bitStream, u32 order, u32 bitsPerSample,
            u32 blockCount, s32 *samples)
{
    // NOTE(michiel): expects samples[blockCount]
    for (u32 warmupIdx = 0; warmupIdx < order; ++warmupIdx)
    {
        samples[warmupIdx] = get_signed32(bitStream, bitsPerSample);
    }
    u32 precision = get_bits(bitStream, 4) + 1;
    s32 quantize  = get_signed32(bitStream, 5);
    
    s32 coefficients[32];
    for (u32 coefIdx = 0; coefIdx < order; ++coefIdx)
    {
        coefficients[coefIdx] = get_signed32(bitStream, precision);
    }
    
    Buffer residual;
    residual.size = (blockCount - order) * sizeof(s32);
    residual.data = allocate_array(u8, residual.size);
    parse_residual_coding(bitStream, order, blockCount, &residual);
    
    s32 *res = (s32 *)residual.data;
    for (u32 blockIdx = order; blockIdx < blockCount; ++blockIdx)
    {
        s64 value = 0;
        for (u32 coef = 0; coef < order; ++coef)
        {
            value += (s64)coefficients[coef] * (s64)samples[blockIdx - coef - 1];
        }
        samples[blockIdx] = *res++ + (s32)(value >> quantize);
    }
    
    for (u32 blockIdx = 0; blockIdx < blockCount; ++blockIdx)
    {
        samples[blockIdx] = samples[blockIdx] << (32 - bitsPerSample);
    }
}

internal void
interleave_samples(u32 channelAssignment, u32 sampleCount, s32 *samplesIn, s32 *samplesOut)
{
    if (channelAssignment > FlacChannel_FrontLRCSubBackLRSideLR)
    {
        // NOTE(michiel): Calc and interleave
        switch (channelAssignment)
        {
            case FlacChannel_LeftSide:
            {
                for (u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
                {
                    s32 left = samplesIn[sampleIdx];
                    s32 diff = samplesIn[sampleIdx + sampleCount];
                    
                    samplesOut[sampleIdx * 2 + 0] = left;
                    samplesOut[sampleIdx * 2 + 1] = ((left >> 1) - diff) << 1;
                }
            } break;
            
            case FlacChannel_SideRight:
            {
                for (u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
                {
                    s32 diff  = samplesIn[sampleIdx];
                    s32 right = samplesIn[sampleIdx + sampleCount];
                    
                    samplesOut[sampleIdx * 2 + 0] = ((right >> 1) - diff) << 1;
                    samplesOut[sampleIdx * 2 + 1] = right;
                }
            } break;
            
            case FlacChannel_MidSide:
            {
                for (u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
                {
                    s32 mid  = samplesIn[sampleIdx];
                    s32 side = samplesIn[sampleIdx + sampleCount];
                    
                    samplesOut[sampleIdx * 2 + 0] = mid + (side >> 1);
                    samplesOut[sampleIdx * 2 + 1] = mid - (side >> 1);
                }
            } break;
            
            INVALID_DEFAULT_CASE;
        }
    }
    else
    {
        // NOTE(michiel): Just interleave
        u32 channelCount = channelAssignment + 1;
        for (u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
        {
            for (u32 channelIdx = 0; channelIdx < channelCount; ++channelIdx)
            {
                samplesOut[sampleIdx * channelCount + channelIdx] = samplesIn[channelIdx * sampleCount + sampleIdx];
            }
        }
    }
}

int main(int argc, char **argv)
{
    Buffer flacData = read_entire_file(static_string("data/PinkFloyd-EmptySpaces.flac"));
    
    BitStreamer bitStream_ = {};
    BitStreamer *bitStream = &bitStream_;
    bitStream->kind = BitStream_BigEndian;
    bitStream->at = flacData.data;
    bitStream->end = flacData.data + flacData.size;
    
    b32 isFlacFile = is_flac_file(bitStream);
    i_expect(isFlacFile);
    bitStream->at += 4;
    
#if FLAC_DEBUG_LEVEL
    char *indent = "    ";
#endif
    
    u32 maxMetadataEntries = 64;
    u32 metadataEntryCount = 0;
    FlacMetadata *metadataEntries = allocate_array(FlacMetadata, maxMetadataEntries);
    
    for(u32 index = 0; index < maxMetadataEntries; ++index)
    {
        FlacMetadata *metadata = metadataEntries + metadataEntryCount++;
        metadata->isLast = get_bits(bitStream, 1);
        metadata->kind = (FlacMetadataType)get_bits(bitStream, 7);
        metadata->totalSize = get_bits(bitStream, 24);
        i_expect(bitStream->remainingBits == 0);
        
        switch (metadata->kind)
        {
            case FlacMetadata_StreamInfo:
            {
                i_expect(metadata->totalSize == 34);
                metadata->info = parse_info_stream(bitStream);
            } break;
            
            case FlacMetadata_Padding:
            {
                metadata->padding.count = metadata->totalSize;
                void_bytes(bitStream, metadata->totalSize);
            } break;
            
            case FlacMetadata_Application:
            {
                metadata->application.ID = get_bits(bitStream, 32);
                void_bytes(bitStream, metadata->totalSize - 4);
            } break;
            
            case FlacMetadata_SeekTable:
            {
                metadata->seekTable.count = metadata->totalSize / 18;
                i_expect((metadata->totalSize % 18) == 0);
                
                metadata->seekTable.entries =
                    allocate_array(FlacSeekEntry, metadata->seekTable.count);
                
                for (u32 seekTableIndex = 0;
                     seekTableIndex < metadata->seekTable.count;
                     ++seekTableIndex)
                {
                    FlacSeekEntry *entry = metadata->seekTable.entries + seekTableIndex;
                    entry->firstSample = get_bits(bitStream, 64);
                    entry->offsetBytes = get_bits(bitStream, 64);
                    entry->samples = get_bits(bitStream, 16);
                }
            } break;
            
            case FlacMetadata_VorbisComment:
            {
                // TODO(michiel): Handle switch to little endian...
                u32 vendorSize = get_le_u32(bitStream);
                metadata->vorbisComments.vendor = copy_to_string(bitStream, vendorSize);
                
                metadata->vorbisComments.commentCount = get_le_u32(bitStream);
                metadata->vorbisComments.comments =
                    allocate_array(String, metadata->vorbisComments.commentCount);
                
                for (u32 i = 0; i < metadata->vorbisComments.commentCount; ++i)
                {
                    String *comment = metadata->vorbisComments.comments + i;
                    
                    u32 commentSize = get_le_u32(bitStream);
                    *comment = copy_to_string(bitStream, commentSize);
                }
            } break;
            
            case FlacMetadata_CueSheet:
            {
                void_bytes(bitStream, metadata->totalSize);
            } break;
            
            case FlacMetadata_Picture:
            {
                metadata->picture.type = (FlacPictureType)get_bits(bitStream, 32);
                
                u32 mimeSize = get_bits(bitStream, 32);
                metadata->picture.mime = copy_to_string(bitStream, mimeSize);
                
                u32 descSize = get_bits(bitStream, 32);
                metadata->picture.description = copy_to_string(bitStream, descSize);
                
                metadata->picture.width = get_bits(bitStream, 32);
                metadata->picture.height = get_bits(bitStream, 32);
                metadata->picture.bitsPerPixel = get_bits(bitStream, 32);
                metadata->picture.indexedColours = get_bits(bitStream, 32);
                
                u32 imageSize = get_bits(bitStream, 32);
                metadata->picture.image = copy_to_bytes(bitStream, imageSize);
            } break;
            
            case FlacMetadata_Invalid:
            default:
            {
            } break;
        }
        
        if (metadata->isLast)
        {
            break;
        }
    }
    
#if FLAC_DEBUG_LEVEL
    for(u32 index = 0; index < metadataEntryCount; ++index)
    {
        FlacMetadata *metadata = metadataEntries + index;
        
        switch (metadata->kind)
        {
            case FlacMetadata_StreamInfo:
            {
                fprintf(stdout, "Stream info metadata (b: %d):\n", metadata->totalSize);
                print_info_stream(&metadata->info, "    ");
            } break;
            
            case FlacMetadata_Padding:
            {
                fprintf(stdout, "Padding metadata (b: %d)\n", metadata->totalSize);
            } break;
            
            case FlacMetadata_Application:
            {
                fprintf(stdout, "Application metadata (b: %d):\n", metadata->totalSize);
                fprintf(stdout, "%sapp id : %08X\n", indent, metadata->application.ID);
            } break;
            
            case FlacMetadata_SeekTable:
            {
                fprintf(stdout, "Seek table metadata (b: %d):\n", metadata->totalSize);
                for (u32 seekTableIndex = 0;
                     seekTableIndex < metadata->seekTable.count;
                     ++seekTableIndex)
                {
                    FlacSeekEntry *entry = metadata->seekTable.entries + seekTableIndex;
                    fprintf(stdout, "%sTable %d:\n", indent, seekTableIndex + 1);
                    fprintf(stdout, "%s%sfirst sample: 0x%016lX\n", indent, indent, entry->firstSample);
                    fprintf(stdout, "%s%soffset bytes: %lu\n", indent, indent, entry->offsetBytes);
                    fprintf(stdout, "%s%ssamples     : %u\n", indent, indent, entry->samples);
                }
            } break;
            
            case FlacMetadata_VorbisComment:
            {
                fprintf(stdout, "Vorbis comment metadata (b: %d):\n", metadata->totalSize);
                fprintf(stdout, "%svendor: %.*s\n", indent,
                        STR_FMT(metadata->vorbisComments.vendor));
                
                for (u32 i = 0; i < metadata->vorbisComments.commentCount; ++i)
                {
                    String *comment = metadata->vorbisComments.comments + i;
                    fprintf(stdout, "%scomment %d: %.*s\n", indent, i + 1, STR_FMT(*comment));
                }
            } break;
            
            case FlacMetadata_CueSheet:
            {
                fprintf(stdout, "Cue sheet metadata (b: %d)\n", metadata->totalSize);
            } break;
            
            case FlacMetadata_Picture:
            {
                fprintf(stdout, "Picture metadata (b: %d):\n", metadata->totalSize);
                fprintf(stdout, "%stype       : %d\n", indent, metadata->picture.type);
                fprintf(stdout, "%smime type  : %.*s\n", indent, STR_FMT(metadata->picture.mime));
                fprintf(stdout, "%sdescription: %.*s\n", indent, STR_FMT(metadata->picture.description));
                fprintf(stdout, "%swidth      : %d\n", indent, metadata->picture.width);
                fprintf(stdout, "%sheight     : %d\n", indent, metadata->picture.height);
                fprintf(stdout, "%sbits/pixel : %d\n", indent, metadata->picture.bitsPerPixel);
                fprintf(stdout, "%sindexed    : %d\n", indent, metadata->picture.indexedColours);
                
#if FLAC_DEBUG_LEVEL > 10
                b32 isJpg = true;
                char *mimeJpg = "image/jpeg";
                Buffer mime = metadata->picture.mime;
                for (u32 i = 0; i < mime.size; ++i)
                {
                    if ((mimeJpg[i] == 0) ||
                        (mimeJpg[i] != mime.data[i]))
                    {
                        isJpg = false;
                        break;
                    }
                }
                
                if (isJpg)
                {
                    FILE *file = fopen("picture.jpg", "wb");
                    if (file)
                    {
                        Buffer picture = metadata->picture.image;
                        fwrite(picture.data, picture.size, 1, file);
                        fclose(file);
                    }
                }
#endif
            } break;
            
            case FlacMetadata_Invalid:
            {
                fprintf(stderr, "Invalid metadata type!\n");
            } break;
            
            default:
            {
                fprintf(stderr, "Unknown metadata type (%d)!\n", metadata->kind);
            } break;
        }
    }
#endif
    
    i_expect(metadataEntries[0].kind == FlacMetadata_StreamInfo);
    FlacInfo *info = &metadataEntries[0].info;
    //FlacFrame *frame = allocate_struct(FlacFrame);
    //init_flac_frame(info);
    
    i_expect(info->minBlockSamples == info->maxBlockSamples);
    u32 totalSampleCount = (u32)info->maxBlockSamples * info->channelCount;
    s32 *testSamples1 = allocate_array(s32, totalSampleCount);
    s32 *testSamples2 = allocate_array(s32, totalSampleCount);
    
    SoundDevice soundDev_ = {};
    SoundDevice *soundDev = &soundDev_;
    soundDev->sampleFrequency = info->sampleRate;
    soundDev->sampleCount = info->maxBlockSamples;
    soundDev->channelCount = info->channelCount;
    
    if (platform_sound_init(soundDev))
    {
        while (bitStream->at != bitStream->end)
        {
            u8 *crcStart = bitStream->at;
            FlacFrameHeader frameHeader = parse_frame_header(bitStream, info);
            //set_flac_frame(frame, info, &frameHeader);
            i_expect(frameHeader.blockSize <= info->maxBlockSamples);
            
#if FLAC_DEBUG_LEVEL
            char *channelType = "";
            switch (frameHeader.channelAssignment)
            {
                case FlacChannel_Mono:      { channelType = "M"; } break;
                case FlacChannel_LeftRight: { channelType = "L/R"; } break;
                case FlacChannel_LeftRightCenter: { channelType = "L/R/C"; } break;
                case FlacChannel_FrontLRBackLR: { channelType = "FL/FR/BL/BR"; } break;
                case FlacChannel_FrontLRCBackLR: { channelType = "FL/FR/FC/BL/BR"; } break;
                case FlacChannel_FrontLRCSubBackLR: { channelType = "FL/FR/FC/LFE/BL/BR"; } break;
                case FlacChannel_FrontLRCSubBackCLR: { channelType = "FL/FR/FC/LFE/BC/BL/BR"; } break;
                case FlacChannel_FrontLRCSubBackLRSideLR: { channelType = "FL/FR/FC/LFE/BL/BR/SL/SR"; } break;
                case FlacChannel_LeftSide:  { channelType = "L/S"; } break;
                case FlacChannel_SideRight: { channelType = "S/R"; } break;
                case FlacChannel_MidSide:   { channelType = "M/S"; } break;
                default: break;
            }
            
            fprintf(stdout, "Frame header %lu:\n", frameHeader.frameNumber);
            fprintf(stdout, "%sblocking          : %s-blocksize stream\n", indent,
                    (frameHeader.variableBlocks) ? "variable" : "fixed");
            fprintf(stdout, "%sblock size        : %d samples\n", indent, frameHeader.blockSize);
            fprintf(stdout, "%ssample rate       : %d Hz\n", indent, frameHeader.sampleRate);
            fprintf(stdout, "%schannel count     : %u\n", indent, frameHeader.channelCount);
            fprintf(stdout, "%schannel assignment: %s\n", indent, channelType);
            fprintf(stdout, "%ssample size       : %d bits\n", indent, frameHeader.bitsPerSample);
#endif
            
            u32 testSampleIndex = 0;
            for (u32 subChannelIndex = 0; subChannelIndex < frameHeader.channelCount; ++subChannelIndex)
            {
                FlacSubframeHeader subframeHeader = parse_subframe_header(bitStream, &frameHeader, subChannelIndex);
                
#if FLAC_DEBUG_LEVEL > 1
                char *subframeType = "";
                switch (subframeHeader.type)
                {
                    case FlacSubframe_Constant: { subframeType = "constant"; } break;
                    case FlacSubframe_Verbatim: { subframeType = "verbatim"; } break;
                    case FlacSubframe_Fixed: { subframeType = "fixed"; } break;
                    case FlacSubframe_LPC: { subframeType = "lpc"; } break;
                    case FlacSubframe_Error: { subframeType = "error"; } break;
                    default: { subframeType = "reserved"; } break;
                }
                fprintf(stdout, "Subframe type: %s (%u)\n", subframeType, subframeHeader.typeOrder);
                fprintf(stdout, "Wasted bits: %s (%u)\n", (subframeHeader.wastedBits) ? "true" : "false", subframeHeader.wastedBits);
#endif
                
                u32 bps = frameHeader.bitsPerSample;
                if ((((frameHeader.channelAssignment == FlacChannel_LeftSide) ||
                      (frameHeader.channelAssignment == FlacChannel_MidSide)) &&
                     (subChannelIndex == 1)) ||
                    ((frameHeader.channelAssignment == FlacChannel_SideRight) &&
                     (subChannelIndex == 0)))
                {
                    // NOTE(michiel): Not in spec, but the side channel (L - R) is 1 bit larger to account for overflows.
                    ++bps;
                }
                
                switch (subframeHeader.type)
                {
                    case FlacSubframe_Constant:
                    {
                        process_constant(bitStream, bps, frameHeader.blockSize, testSamples1 + testSampleIndex);
                    } break;
                    
                    case FlacSubframe_Verbatim:
                    {
                        process_verbatim(bitStream, bps, frameHeader.blockSize, testSamples1 + testSampleIndex);
                    } break;
                    
                    case FlacSubframe_Fixed:
                    {
                        process_fixed(bitStream, subframeHeader.typeOrder, bps,
                                      frameHeader.blockSize, testSamples1 + testSampleIndex);
                    } break;
                    
                    case FlacSubframe_LPC:
                    {
                        process_lpc(bitStream, subframeHeader.typeOrder, bps,
                                    frameHeader.blockSize, testSamples1 + testSampleIndex);
                    } break;
                    
                    INVALID_DEFAULT_CASE;
                }
                
                testSampleIndex += frameHeader.blockSize;
            }
            
            bitStream->remainingBits = 0;
            bitStream->remainingData = 0;
            
            u16 crcTable[256];
            crc16_init_table(0x8005, crcTable);
            u16 crcCheck = crc16_calc_crc(crcTable, bitStream->at - crcStart, crcStart);
            u16 crcFile = get_bits(bitStream, 16);
            
            if (crcFile != crcCheck)
            {
                fprintf(stderr, "CRC16 calc: %04X, CRC16 file: %04X\n", crcCheck, crcFile);
            }
            i_expect(crcFile == crcCheck);
            
            interleave_samples(frameHeader.channelAssignment, frameHeader.blockSize, testSamples1, testSamples2);
            if (platform_sound_write_s32(soundDev, testSamples2))
            {
                // NOTE(michiel): Fine
            }
            else
            {
                fprintf(stderr, "Sound write failed:\n    ");
                fprintf(stderr, "%.*s\n\n", STR_FMT(platform_sound_error_string(soundDev)));
                break;
            }
        }
    }
    else
    {
        fprintf(stderr, "Sound initialization failed:\n    ");
        fprintf(stderr, "%.*s\n\n", STR_FMT(platform_sound_error_string(soundDev)));
    }
    
    
    return 0;
}
