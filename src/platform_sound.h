struct SoundDevice
{
    u32 sampleFrequency;
    u32 sampleCount;
    u32 channelCount;
    void *platform;
};

#define PLATFORM_SOUND_ERROR_STRING(name) String name(SoundDevice *device)
typedef PLATFORM_SOUND_ERROR_STRING(PlatformSoundErrorString);

#define PLATFORM_SOUND_INIT(name) b32 name(SoundDevice *device)
typedef PLATFORM_SOUND_INIT(PlatformSoundInit);

#define PLATFORM_SOUND_WRITE_S16(name) b32 name(SoundDevice *device, s16 *samples)
typedef PLATFORM_SOUND_WRITE_S16(PlatformSoundWriteS16);

#define PLATFORM_SOUND_WRITE_S32(name) b32 name(SoundDevice *device, s32 *samples)
typedef PLATFORM_SOUND_WRITE_S32(PlatformSoundWriteS32);

#define PLATFORM_SOUND_WRITE_F32(name) b32 name(SoundDevice *device, f32 *samples)
typedef PLATFORM_SOUND_WRITE_F32(PlatformSoundWriteF32);
