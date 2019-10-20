#include "../libberdip/platform.h"

#include <alsa/asoundlib.h>

#include "./platform_sound.h"

#include "./linux_sound.h"
#include "./linux_sound.cpp"

#define MAKE_MAGIC(a, b, c, d)    ((d << 24) | (c << 16) | (b << 8) | a)

#include "wav.h"

#include "../libberdip/std_file.c"

#include "wav.cpp"

internal s32
get_signed32(u8 *data, u32 bitCount)
{
    // TODO(michiel): Handle non-multiples of 8
    i_expect(bitCount);
    i_expect(bitCount <= 32);
    u32 value = 0;
    for (u32 byteIdx = 0; byteIdx < ((bitCount + 7) / 8); ++byteIdx)
    {
        value |= (data[byteIdx] << (byteIdx * 8));
    }
    s32 result = ((s32)value << (32 - bitCount));
    //result >>= (32 - bitCount);
    return result;
}

PlatformSoundErrorString *platform_sound_error_string = linux_sound_error_string;
PlatformSoundInit *platform_sound_init = linux_sound_init;
PlatformSoundWrite *platform_sound_write = linux_sound_write;

s32 main(s32 argc, char **argv)
{
    Buffer wavFile = read_entire_file(static_string("data/PinkFloyd-EmptySpaces.wav"));
    //Buffer wavFile = read_entire_file(static_string("data/11 Info Dump.wav"));
    
    SoundDevice soundDev_ = {};
    SoundDevice *soundDev = &soundDev_;
    
    if (wavFile.size)
    {
        u8 *srcPointer = wavFile.data;
        RiffChunk *chunk = (RiffChunk *)srcPointer;
        srcPointer += sizeof(RiffChunk);
        
        if (chunk->magic == MAKE_MAGIC('R', 'I', 'F', 'F'))
        {
            u32 totalByteCount = chunk->size;
            chunk = (RiffChunk *)srcPointer;
            if (chunk->magic == MAKE_MAGIC('W', 'A', 'V', 'E'))
            {
                srcPointer += 4;
                fprintf(stdout, "Got one (%u)\n", totalByteCount);
                WavFormat *format = 0;
                
                while (srcPointer < (wavFile.data + wavFile.size))
                {
                    chunk = (RiffChunk *)srcPointer;
                    srcPointer += sizeof(RiffChunk);
                    
                    switch (chunk->magic)
                    {
                        case MAKE_MAGIC('f', 'm', 't', ' '):
                        {
                            format = (WavFormat *)chunk;
                            srcPointer += format->chunkSize;
                            
                            soundDev->sampleFrequency = format->sampleRate;
                            soundDev->sampleCount = 4096;
                            soundDev->channelCount = format->channelCount;
                            // TODO(michiel): soundDev->format = format->sampleSize;
                            soundDev->format = SoundFormat_s24;
                            
                            if (platform_sound_init(soundDev))
                            {
                                print_format(format);
                            }
                            else
                            {
                                srcPointer = (u8 *)U64_MAX;
                            }
                        } break;
                        
                        case MAKE_MAGIC('f', 'a', 'c', 't'):
                        {
                            fprintf(stderr, "Unsupported compressed wav file!\n");
                            WavFact *fact = (WavFact *)chunk;
                            srcPointer += chunk->size;
                            print_fact(fact);
                        } break;
                        
                        case MAKE_MAGIC('d', 'a', 't', 'a'):
                        {
                            WavData *data = (WavData *)chunk;
                            srcPointer += (chunk->size + 1) & ~1;
                            
                            i_expect((format->sampleSize % 8) == 0);
                            i_expect(soundDev->sampleFrequency);
                            
                            fprintf(stdout, "Data chunk (%u)\n", chunk->size);
                            
                            //s32 samples[8192];
                            u8 *ptr = data->data;
                            u32 frameByteSize = format->blockAlign;
                            umm frameCount = (srcPointer - ptr) / frameByteSize;
                            
                            umm blocks = frameCount / soundDev->sampleCount;
                            u32 remain = frameCount % soundDev->sampleCount;
                            for (umm blockIdx = 0; blockIdx < blocks; ++blockIdx)
                            {
                                if (platform_sound_write(soundDev, ptr))
                                {
                                    ptr += frameByteSize * soundDev->sampleCount;
                                }
                                else
                                {
                                    break;
                                }
                            }
                            
                            soundDev->sampleCount = remain;
                            if (platform_sound_write(soundDev, ptr))
                            {
                                
                            }
                            else
                            {
                                fprintf(stderr, "Missed ending!\n");
                            }
                            
#if 0                            
                            while (ptr < (srcPointer - 1))
                            {
                                u8 *samples = ptr;
                                ptr += format->blockAlign * soundDev->sampleCount;
                                // TODO(michiel): Fix last block
#if 0                                
                                for (u32 sampleIdx = 0; sampleIdx < soundDev->sampleCount; ++sampleIdx)
                                {
                                    samples[sampleIdx * format->channelCount + 0] = get_signed32(ptr, format->sampleSize);
                                    samples[sampleIdx * format->channelCount + 1] = get_signed32(ptr + format->sampleSize / 8, format->sampleSize);
                                    ptr += format->blockAlign;
                                }
#endif
                                
                                if (platform_sound_write(soundDev, samples))
                                {
                                    // NOTE(michiel): Fine...
                                }
                                else
                                {
                                    break;
                                }
                            }
#endif
                            
#if 0                            
                            s32 left = get_signed32(ptr, format->sampleSize);
                            s32 right = get_signed32(ptr + format->sampleSize / 8, format->sampleSize);
                            ptr += format->blockAlign;
                            fprintf(stdout, "L: %8d | R: %8d\n", left, right);
                            
                            //ptr += 2048 * format->blockAlign;
                            for (u32 x = 0; x < 32; ++x)
                            {
                                left = get_signed32(ptr, format->sampleSize);
                                right = get_signed32(ptr + format->sampleSize / 8, format->sampleSize);
                                ptr += format->blockAlign;
                                fprintf(stdout, "L: %8d | R: %8d\n", left, right);
                            }
#endif
                            
                        } break;
                        
                        INVALID_DEFAULT_CASE;
                    }
                }
            }
        }
    }
    
    return 0;
}