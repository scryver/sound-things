#include "../libberdip/platform.h"
#include "../libberdip/linux_memory.h"

#include <unistd.h>
#include <sys/mman.h>
#include <alsa/asoundlib.h>

#include "./platform_sound.h"

// TODO(michiel): Blah, fugly
#define SOUND_PERIOND_COUNT    4
//#define SOUND_PERIOND_COUNT    2 // Mu
#define SOUND_HW_NAME          "default"
//#define SOUND_HW_NAME          "hw:1,0 // Mu"

#include "./linux_sound.h"
#include "./linux_sound.cpp"

global API api;
global MemoryAPI *gMemoryApi = &api.memory;

#include "../libberdip/memory.cpp"
#include "../libberdip/linux_memory.cpp"

PlatformSoundErrorString *platform_sound_error_string = linux_sound_error_string;
PlatformSoundInit *platform_sound_init = linux_sound_init;
PlatformSoundReformat *platform_sound_reformat = linux_sound_reformat;
PlatformSoundWrite *platform_sound_write = linux_sound_write;

s32 main(s32 argc, char **argv)
{
    linux_memory_api(&api.memory);
    //linux_file_api(&api.file);
    
    MemoryAllocator platformAlloc = {};
    initialize_platform_allocator(0, &platformAlloc);
    
    SoundDevice soundDev_ = {};
    SoundDevice *soundDev = &soundDev_;
    
    soundDev->sampleFrequency = 192000;
    soundDev->sampleCount = 4096;
    soundDev->channelCount = 2;
    
    if (platform_sound_init(&platformAlloc, soundDev))
    {
        fprintf(stdout, "Opened sound device:\n");
        fprintf(stdout, "  sample freq   : %u\n", soundDev->sampleFrequency);
        fprintf(stdout, "  sample count  : %u\n", soundDev->sampleCount);
        fprintf(stdout, "  channel count : %u\n", soundDev->channelCount);
        
        f32 frequency = 220.0f;
        f32 step = frequency / soundDev->sampleFrequency;
        f32 *soundBufferF32 = allocate_array(&platformAlloc, f32, soundDev->sampleCount * soundDev->channelCount, default_memory_alloc());
        s32 *soundBufferS32 = allocate_array(&platformAlloc, s32, soundDev->sampleCount * soundDev->channelCount, default_memory_alloc());
        s16 *soundBufferS16 = allocate_array(&platformAlloc, s16, soundDev->sampleCount * soundDev->channelCount, default_memory_alloc());
        
        u32 stepsPerFormat = 128;
        f32 stepAt = 0.0f;
        while (1)
        {
            soundDev->format = SoundFormat_f32;
            if (platform_sound_reformat(soundDev))
            {
                fprintf(stdout, "Generating float 32bits\n");
                for (u32 x = 0; x < stepsPerFormat; ++x)
                {
                    for (u32 bufIdx = 0; bufIdx < soundDev->sampleCount; ++bufIdx)
                    {
                        f32 sample = sin_f32(stepAt);
                        for (u32 channelIdx = 0; channelIdx < soundDev->channelCount; ++channelIdx)
                        {
                            soundBufferF32[soundDev->channelCount * bufIdx + channelIdx] = 0.01f * sample;
                        }
                        stepAt += step;
                        if (stepAt >= 1.0f)
                        {
                            stepAt -= 1.0f;
                        }
                    }
                    
                    if (platform_sound_write(soundDev, soundBufferF32))
                    {
                        // NOTE(michiel): Nothing special
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
                fprintf(stderr, "Sound reinitialize failed:\n    ");
                fprintf(stderr, "%.*s\n\n", STR_FMT(platform_sound_error_string(soundDev)));
                break;
            }
            
            soundDev->format = SoundFormat_s32;
            if (platform_sound_reformat(soundDev))
            {
                fprintf(stdout, "Generating signed 32bits\n");
                for (u32 x = 0; x < stepsPerFormat; ++x)
                {
                    for (u32 bufIdx = 0; bufIdx < soundDev->sampleCount; ++bufIdx)
                    {
                        f32 sample = sin_f32(stepAt);
                        for (u32 channelIdx = 0; channelIdx < soundDev->channelCount; ++channelIdx)
                        {
                            soundBufferS32[soundDev->channelCount * bufIdx + channelIdx] = s32_from_f32_round(0.01f * S32_MAX * sample);
                        }
                        stepAt += step;
                        if (stepAt >= 1.0f)
                        {
                            stepAt -= 1.0f;
                        }
                    }
                    
                    if (platform_sound_write(soundDev, soundBufferS32))
                    {
                        // NOTE(michiel): Nothing special
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
                fprintf(stderr, "Sound reinitialize failed:\n    ");
                fprintf(stderr, "%.*s\n\n", STR_FMT(platform_sound_error_string(soundDev)));
                break;
            }
            
            soundDev->format = SoundFormat_s16;
            if (platform_sound_reformat(soundDev))
            {
                fprintf(stdout, "Generating signed 16bits\n");
                for (u32 x = 0; x < stepsPerFormat; ++x)
                {
                    for (u32 bufIdx = 0; bufIdx < soundDev->sampleCount; ++bufIdx)
                    {
                        f32 sample = sin_f32(stepAt);
                        for (u32 channelIdx = 0; channelIdx < soundDev->channelCount; ++channelIdx)
                        {
                            soundBufferS16[soundDev->channelCount * bufIdx + channelIdx] = s16_from_f32_round(0.01f * S16_MAX * sample);
                        }
                        stepAt += step;
                        if (stepAt >= 1.0f)
                        {
                            stepAt -= 1.0f;
                        }
                    }
                    
                    if (platform_sound_write(soundDev, soundBufferS16))
                    {
                        // NOTE(michiel): Nothing special
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
                fprintf(stderr, "Sound reinitialize failed:\n    ");
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