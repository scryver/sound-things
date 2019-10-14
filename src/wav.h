enum WavFormatType
{
    WavFormat_None,
    WavFormat_PCM   = 0x0001,
    WavFormat_Float = 0x0003,
    WavFormat_ALaw  = 0x0006,
    WavFormat_uLaw  = 0x0007,
    WavFormat_Extensible = 0xFFFE, // NOTE(michiel): Defined in subFormat
};

#pragma pack(push, 1)
struct RiffChunk
{
    u32 magic;
    u32 size;
};

struct WavFormat
{
    u32 magic;
    u32 chunkSize;       // NOTE(michiel): 16, 18 or 40
    u16 formatCode;
    u16 channelCount;
    u32 sampleRate;      // NOTE(michiel): Samples/sec
    u32 dataRate;        // NOTE(michiel): Bytes/sec
    u16 blockAlign;      // NOTE(michiel): Data block size
    u16 sampleSize;      // NOTE(michiel): Bits/sample
    // NOTE(michiel): Everything below depends on chunkSize > 16
    u16 extensionCount;  // NOTE(michiel): 0 or 22
    // NOTE(michiel): Everything below depends on extensionCount > 0
    u16 validSampleSize; // NOTE(michiel): Valid bits/sample
    u32 speakerPosMask;
    u8 subFormat[16];
};

struct WavFact
{
    u32 magic;
    u32 chunkSize;    // NOTE(michiel): chunkSize >= 4
    u32 sampleCount;  // NOTE(michiel): Samples/channel
    u8 data[1];       // NOTE(michiel): data[chunkSize - 4]
};

struct WavData
{
    u32 magic;
    u32 chunkSize;    // NOTE(michiel): chunkSize >= 4
    u8 data[2];       // NOTE(michiel): data[(chunkSize + 1) & ~1]
};

#pragma pack(pop)
