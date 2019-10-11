struct FlacInfo
{
    // NOTE(michiel): FLAC specifies a minimum block size of 16 and a maximum block size of 65535, 
    // meaning the bit patterns corresponding to the numbers 0-15 in the minimum blocksize and 
    // maximum blocksize fields are invalid.
    
    u16 minBlockSamples; // NOTE(michiel): <16> 	The minimum block size (in samples) used in the stream.
    u16 maxBlockSamples; // NOTE(michiel): <16> 	The maximum block size (in samples) used in the stream. (Minimum blocksize == maximum blocksize) implies a fixed-blocksize stream.
    u32 minFrameBytes;   // NOTE(michiel): <24> 	The minimum frame size (in bytes) used in the stream. May be 0 to imply the value is not known.
    u32 maxFrameBytes;   // NOTE(michiel): <24> 	The maximum frame size (in bytes) used in the stream. May be 0 to imply the value is not known.
    u32 sampleRate;      // NOTE(michiel): <20> 	Sample rate in Hz. Though 20 bits are available, the maximum sample rate is limited by the structure of frame headers to 655350Hz. Also, a value of 0 is invalid.
    u16 channelCount;    // NOTE(michiel): <3> 	(number of channels)-1. FLAC supports from 1 to 8 channels
    u16 bitsPerSample;   // NOTE(michiel): <5> 	(bits per sample)-1. FLAC supports from 4 to 32 bits per sample. Currently the reference encoder and decoders only support up to 24 bits per sample.
    u64 totalSamples;    // NOTE(michiel): <36> 	Total samples in stream. 'Samples' means inter-channel sample, i.e. one second of 44.1Khz audio will have 44100 samples regardless of the number of channels. A value of zero here means the number of total samples is unknown.
    struct 
    {
        u64 high;
        u64 low;
    } md5signature;      // NOTE(michiel): <128> 	MD5 signature of the unencoded audio data. This allows the decoder to determine if an error exists in the audio data even when the error does not result in an invalid bitstream.
};

struct FlacPadding
{
    u32 count;
};

struct FlacApplication
{
    u32 ID;
    u32 size;
};

struct FlacSeekEntry
{
    u64 firstSample;
    u64 offsetBytes;
    u32 samples;
};
struct FlacSeekTable
{
    u32 count;
    FlacSeekEntry *entries;
};

struct FlacVorbisComments
{
    String vendor;
    
    u32 commentCount;
    String *comments;
};

struct FlacCueSheet
{
    // TODO(michiel): implement
    u32 size;
};

enum FlacPictureType
{
    FlacPicture_Other,
    FlacPicture_32x32pixelsFileIcon, // NOTE(michiel): PNG only
    FlacPicture_OtherFileIcon,
    FlacPicture_CoverFront,
    FlacPicture_CoverBack,
    FlacPicture_LeafletPage,
    FlacPicture_Media, // NOTE(michiel):  (e.g. label side of CD)
    FlacPicture_LeadArtist, // TODO(michiel): or lead performer/soloist
    FlacPicture_ArtistPerformer,
    FlacPicture_Conductor,
    FlacPicture_BandOrchestra,
    FlacPicture_Composer,
    FlacPicture_LyricistTextWriter,
    FlacPicture_RecordingLocation,
    FlacPicture_DuringRecording,
    FlacPicture_DuringPerformance,
    FlacPicture_MovieVideoScreenCapture,
    FlacPicture_ABrightColouredFish,
    FlacPicture_Illustration,
    FlacPicture_BandArtistLogotype,
    FlacPicture_PublisherStudioLogotype,
};

struct FlacPicture
{
    FlacPictureType type;
    String mime;
    String description;
    u32 width;
    u32 height;
    u32 bitsPerPixel;
    u32 indexedColours;
    Buffer image;
};

enum FlacMetadataType
{
    FlacMetadata_StreamInfo,
    FlacMetadata_Padding,
    FlacMetadata_Application,
    FlacMetadata_SeekTable,
    FlacMetadata_VorbisComment,
    FlacMetadata_CueSheet,
    FlacMetadata_Picture,
    // NOTE(michiel): Reserved: 7-126
    FlacMetadata_Invalid = 127,
};

struct FlacMetadata
{
    FlacMetadataType kind;
    u32 totalSize;
    b32 isLast;
    union
    {
        FlacInfo info;
        FlacPadding padding;
        FlacApplication application;
        FlacSeekTable seekTable;
        FlacVorbisComments vorbisComments;
        FlacCueSheet cueSheet;
        FlacPicture picture;
    };
};

//
// NOTE(michiel): Frames
//

enum FlacChannelAssignment
{
    FlacChannel_Mono,
    FlacChannel_LeftRight,
    FlacChannel_LeftRightCenter,
    FlacChannel_FrontLRBackLR,
    FlacChannel_FrontLRCBackLR,
    FlacChannel_FrontLRCSubBackLR,
    FlacChannel_FrontLRCSubBackCLR,
    FlacChannel_FrontLRCSubBackLRSideLR,
    FlacChannel_LeftSide,  // NOTE(michiel): Channel 0 = left,    Channel 1 = left - right
    FlacChannel_RightSide, // NOTE(michiel): Channel 0 = right,   Channel 1 = left - right
    FlacChannel_MidSide,   // NOTE(michiel): Channel 0 = average, Channel 1 = left - right
};

struct FlacFrameHeader
{
    u16 syncCode;
    u16 blockSize;
    u32 sampleRate;
    b8  variableBlocks;
    u8  bitsPerSample;
    u8  channelCount;
    u8  channelAssignment;
    u8  crc8;
    
    union
    {
        u64 sampleNumber;
        u64 frameNumber;
    };
};

enum FlacSubframeType
{
    FlacSubframe_Constant,
    FlacSubframe_Verbatim,
    FlacSubframe_Reserved0,
    FlacSubframe_Reserved1 = 7,
    FlacSubframe_Fixed,
    FlacSubframe_Reserved2 = 16,
    FlacSubframe_Reserved3 = 31,
    FlacSubframe_LPC,
    FlacSubframe_Error = 64,
};

struct FlacSubframeHeader
{
    enum8(FlacSubframeType) type;
    u8 typeOrder;
    u32 wastedBits;
};
