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

struct RiffHeader
{
    u32 magic;
    u32 size;
    u32 fileType;
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

struct WavSettings
{
    u32 channelCount;
    u32 sampleFrequency;
    u32 sampleResolution;
    u32 sampleFrameSize;  // NOTE(michiel): Size of a single sample frame (so 1 sample for all channels)
    WavFormatType format;
};

struct WavStreamer
{
    ApiFile file;
    
    WavSettings settings;
    
    u32 dataFileOffset; // NOTE(michiel): Start of data chunk in file
    u32 dataCount;      // NOTE(michiel): Number of data bytes total
    u32 dataOffset;     // NOTE(michiel): Current offset in file, updated while reading
};

struct WavReader
{
    Buffer rawData;
    WavSettings settings;
    
    u32 dataOffset; // NOTE(michiel): Start of data in rawData
    u32 dataCount;  // NOTE(michiel): Number of data bytes total
    u32 readOffset; // NOTE(michiel): Current offset in rawData starting at dataOffset
};

// NOTE(michiel): Opens a wav file for streaming input
internal WavStreamer wav_open_stream(String filename);
// NOTE(michiel): Stream from wav file
// `output` should be set to the max read size and a valid data pointer.
// Returns `true` on successful read.
internal b32 wav_read_stream(WavStreamer *streamer, Buffer *output);

internal WavReader wav_load_file(MemoryAllocator *allocator, String filename);
internal Buffer wav_read_chunk(WavReader *reader, u32 byteCount);

internal b32 wav_write_file(String filename, WavSettings *settings, Buffer wavData);


