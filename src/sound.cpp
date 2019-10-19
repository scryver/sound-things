#include "../libberdip/platform.h"

#include <alsa/asoundlib.h>

#include "./platform_sound.h"

#include "./linux_sound.h"
#include "./linux_sound.cpp"

s32 main(s32 argc, char **argv)
{
    SoundDevice soundDev_ = {};
    SoundDevice *soundDev = &soundDev_;
    
    soundDev->sampleFrequency = 192000;
    soundDev->sampleCount = 4096;
    soundDev->channelCount = 2;
    
    if (platform_sound_init(soundDev))
    {
        fprintf(stdout, "Opened sound device:\n");
        fprintf(stdout, "  sample freq   : %u\n", soundDev->sampleFrequency);
        fprintf(stdout, "  sample count  : %u\n", soundDev->sampleCount);
        fprintf(stdout, "  channel count : %u\n", soundDev->channelCount);
        
        f32 frequency = 220.0f;
        f32 step = frequency / soundDev->sampleFrequency;
        f32 *soundBufferF32 = allocate_array(f32, soundDev->sampleCount * soundDev->channelCount);
        s32 *soundBufferS32 = allocate_array(s32, soundDev->sampleCount * soundDev->channelCount);
        s16 *soundBufferS16 = allocate_array(s16, soundDev->sampleCount * soundDev->channelCount);
        
        f32 stepAt = 0.0f;
        while (1)
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
            
            if (platform_sound_write_f32(soundDev, soundBufferF32))
            {
                // NOTE(michiel): Nothing special
            }
            else
            {
                fprintf(stderr, "Sound write failed:\n    ");
                fprintf(stderr, "%.*s\n\n", STR_FMT(platform_sound_error_string(soundDev)));
                break;
            }
            
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
            
            if (platform_sound_write_s32(soundDev, soundBufferS32))
            {
                // NOTE(michiel): Nothing special
            }
            else
            {
                fprintf(stderr, "Sound write failed:\n    ");
                fprintf(stderr, "%.*s\n\n", STR_FMT(platform_sound_error_string(soundDev)));
                break;
            }
            
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
            
            if (platform_sound_write_s16(soundDev, soundBufferS16))
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
        fprintf(stderr, "Sound initialization failed:\n    ");
        fprintf(stderr, "%.*s\n\n", STR_FMT(platform_sound_error_string(soundDev)));
    }
    
    return 0;
}