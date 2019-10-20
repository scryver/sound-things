enum SoundFormat
{
    SoundFormat_None,
    SoundFormat_s8,
    SoundFormat_s16,
    SoundFormat_s24,
    SoundFormat_s32,
    SoundFormat_s64,
    SoundFormat_f32,
    SoundFormat_f64,
};

struct SoundDevice
{
    u32 sampleFrequency;
    u32 sampleCount; // NOTE(michiel): Samples per channel per write (one write contains channelCount*sampleCount samples)
    u32 channelCount;
    SoundFormat format;
    void *platform;
};

#define PLATFORM_SOUND_ERROR_STRING(name) String name(SoundDevice *device)
typedef PLATFORM_SOUND_ERROR_STRING(PlatformSoundErrorString);

#define PLATFORM_SOUND_INIT(name) b32 name(SoundDevice *device)
typedef PLATFORM_SOUND_INIT(PlatformSoundInit);

#define PLATFORM_SOUND_REFORMAT(name) b32 name(SoundDevice *device)
typedef PLATFORM_SOUND_REFORMAT(PlatformSoundReformat);

#define PLATFORM_SOUND_WRITE(name) b32 name(SoundDevice *device, void *samples)
typedef PLATFORM_SOUND_WRITE(PlatformSoundWrite);
