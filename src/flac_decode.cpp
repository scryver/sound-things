#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>

#define i_expect(x) assert(x)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef int8_t   b8;
typedef int16_t  b16;
typedef int32_t  b32;
typedef int64_t  b64;

typedef size_t   umm;

struct Buffer
{
    u32 size;
     u8 *data;
};

typedef Buffer String;

#pragma pack(push, 1)
#pragma pack(pop)

enum FlacMetadataType
{
    FlacMetadata_StreamInfo,
    FlacMetadata_Padding,
    FlacMetadata_Application,
    FlacMetadata_Seektable,
    FlacMetadata_VorbisComment,
    FlacMetadata_CueSheet,
    FlacMetadata_Picture,
    // NOTE(michiel): Reserved: 7-126
    FlacMetadata_Invalid = 127,
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

#define allocate_struct(type) (type *)allocate_size(sizeof(type))
#define allocate_array(type, size) (type *)allocate_size(size)
static inline void *
allocate_size(umm size)
{
    return malloc(size);
}

struct BitStreamer
{
    u8 remainingBits;
    u8 remainingData;
    u8 *at;
    u8 *end;
};

static inline u32
get_bits(BitStreamer *stream, u32 nrBits)
{
    // NOTE(michiel): This gets the big endian integer specified by nrBits
    i_expect(nrBits <= 32);
    i_expect(nrBits <= ((stream->end - stream->at) << 3));
    
    u32 result = stream->remainingData;
    if (nrBits == stream->remainingBits)
    {
        stream->remainingBits = 0;
        stream->remainingData = 0;
    }
    else if (nrBits < stream->remainingBits)
    {
        stream->remainingBits -= nrBits;
        result >>= stream->remainingBits;
        stream->remainingData &= (1 << stream->remainingBits) - 1;
    }
    else
    {
        u32 bytes = nrBits >> 3;
        u32 remaining = nrBits & 0x7;
        
        if (remaining == stream->remainingBits)
        {
            stream->remainingBits = 0;
            stream->remainingData = 0;
            remaining = 0;
        }
        else if (remaining < stream->remainingBits)
        {
            i_expect(bytes > 0);
            bytes -= 1;
            remaining = (remaining + 8) - stream->remainingBits;
            stream->remainingBits = 0;
            stream->remainingData = 0;
        }
        else
        {
            i_expect(remaining > stream->remainingBits);
            stream->remainingBits = 0;
            stream->remainingData = 0;
            remaining -= stream->remainingBits;
        }
        
        for (u32 byte = 0; byte < bytes; ++byte)
        {
            result <<= 8;
            result |= *stream->at++;
        }
        
        if (remaining)
        {
            result <<= remaining;
            stream->remainingData = *stream->at++;
            stream->remainingBits = 8 - remaining;
            result |= stream->remainingData >> stream->remainingBits;
            stream->remainingData &= (1 << stream->remainingBits) - 1;
        }
    }
    
    return result;
}

static inline u64
get_64bits(BitStreamer *stream)
{
    u64 result;
    result = ((u64)get_bits(stream, 32) << 32) | (u64)get_bits(stream, 32);
    return result;
}

static inline u16
get_u16_bigendian(u8 *data)
{
    u16 result = *(u16 *)data;
    result = (((result >> 8) & 0x00FF) |
              ((result << 8) & 0xFF00));
    return result;
}

static inline u32
get_u24_bigendian(u8 *data)
{
    u32 result = *(u32 *)data;
    result = (((result >> 16) & 0x000000FF) |
              ( result        & 0x0000FF00) |
              ((result << 16) & 0x00FF0000));
    return result;
}

static inline u32
get_u32_bigendian(u8 *data)
{
    u32 result = *(u32 *)data;
    result = (((result >> 24) & 0x000000FF) |
              ((result >>  8) & 0x0000FF00) |
              ((result <<  8) & 0x00FF0000) |
              ((result << 24) & 0xFF000000));
    return result;
}

static inline u64
get_u64_bigendian(u8 *data)
{
    u64 result = *(u64 *)data;
    result = (((result >> 56) & 0x00000000000000FF) |
                    ((result >> 40) & 0x000000000000FF00) |
                    ((result >> 24) & 0x0000000000FF0000) |
                    ((result >>  8) & 0x00000000FF000000) |
                    ((result <<  8) & 0x000000FF00000000) |
                    ((result << 24) & 0x0000FF0000000000) |
                    ((result << 40) & 0x00FF000000000000) |
                    ((result << 56) & 0xFF00000000000000));
    return result;
}

static u8 *
read_entire_file(char *filename)
{
     u8 *result = 0;
    
    FILE *file = fopen(filename, "rb");
    if(file)
    {
        fseek(file, 0, SEEK_END);
         umm fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        result = allocate_array(u8, fileSize);
        fread(result, fileSize, 1, file);
        
        fclose(file);
    }
    
    return(result);
}


int main(int argc, char **argv)
{
    u8 *flacData = read_entire_file("data/PinkFloyd-EmptySpaces.flac");
    
    u8 *at = flacData;
    
    i_expect(flacData[0] == 'f');
    i_expect(flacData[1] == 'L');
    i_expect(flacData[2] == 'a');
    i_expect(flacData[3] == 'C');
    
    at += 4;
    
    char *indent = "    ";
    
    u32 header = *(u32 *)at;
    for (;;)
    {
        // TODO(michiel): struct BitStreamer with data/size/at and current byte mask or
        // something, advance at if mask is zero and that sort of stuff...
        u32 metadataKind = header & ((1 << 7) - 1);
        u32 metadataLength = (((header >> 24) & 0x0000FF) |
                              ((header >>  8) & 0x00FF00) |
                           ((header <<  8) & 0xFF0000));
                           
        at += 4; // NOTE(michiel): Skip header
        switch (metadataKind)
        {
            case FlacMetadata_StreamInfo:
            {
                fprintf(stdout, "Stream info metadata (b: %d):\n", metadataLength);
                /*
<16> 	The minimum block size (in samples) used in the stream.
<16> 	The maximum block size (in samples) used in the stream. (Minimum blocksize == maximum blocksize) implies a fixed-blocksize stream.
<24> 	The minimum frame size (in bytes) used in the stream. May be 0 to imply the value is not known.
<24> 	The maximum frame size (in bytes) used in the stream. May be 0 to imply the value is not known.
<20> 	Sample rate in Hz. Though 20 bits are available, the maximum sample rate is limited by the structure of frame headers to 655350Hz. Also, a value of 0 is invalid.
<3> 	(number of{

} channels)-1. FLAC supports from 1 to 8 channels
<5> 	(bits per sample)-1. FLAC supports from 4 to 32 bits per sample. Currently the reference encoder and decoders only support up to 24 bits per sample.
<36> 	Total samples in stream. 'Samples' means inter-channel sample, i.e. one second of 44.1Khz audio will have 44100 samples regardless of the number of channels. A value of zero here means the number of total samples is unknown.
<128> 	MD5 signature of the unencoded audio data. This allows the decoder to determine if an error exists in the audio data even when the error does not result in an invalid bitstream.
 NOTES
 
    FLAC specifies a minimum block size of 16 and a maximum block size of 65535, meaning the bit patterns corresponding to the numbers 0-15 in the minimum blocksize and maximum blocksize fields are invalid.
    
                */
                u8 *streamAt = at;
                u32 minBlockSize = get_u16_bigendian(streamAt);
                streamAt += 2;
                u32 maxBlockSize = get_u16_bigendian(streamAt);
                streamAt += 2;
                u32 minFrameSize = get_u24_bigendian(streamAt);
                streamAt += 3;
                u32 maxFrameSize = get_u24_bigendian(streamAt);
                streamAt += 3;
                u32 temp = get_u32_bigendian(streamAt);
                streamAt += 4;
                
                u32 sampleRate = temp >> 12;
                u32 channelCount = ((temp >> 9) & 0x7) + 1;
                u32 bitsPerSample = ((temp >> 4) & 0x1F) + 1;
                
                u64 totalSamples = get_u32_bigendian(streamAt) | (((u64)temp & 0xF) << 32);
                streamAt += 4;
                
                u64 md5signHigh = get_u64_bigendian(streamAt);
                streamAt += 8;
                u64 md5signLow = get_u64_bigendian(streamAt);
                streamAt += 8;
                
                i_expect(streamAt == (at + metadataLength));
                
                fprintf(stdout, "%smin block size: %d samples\n", indent, minBlockSize);
                fprintf(stdout, "%smax block size: %d samples\n", indent, maxBlockSize);
                fprintf(stdout, "%smin frame size: %d bytes\n", indent, minFrameSize);
                fprintf(stdout, "%smax frame size: %d bytes\n", indent, maxFrameSize);
                fprintf(stdout, "%ssample rate   : %d Hz\n", indent, sampleRate);
                fprintf(stdout, "%schannel count : %d\n", indent, channelCount);
                fprintf(stdout, "%ssample depth  : %d bits\n", indent, bitsPerSample);
                fprintf(stdout, "%stotal samples : %lu\n", indent, totalSamples);
                fprintf(stdout, "%sMD5 signature : 0x%016lX%016lX\n", indent,
                        md5signHigh, md5signLow);
            } break;
            
            case FlacMetadata_Padding:
            {
                fprintf(stdout, "Padding metadata (b: %d)\n", metadataLength);
                //i_expect((metadataLength & 0x7) == 0);
            } break;
            
            case FlacMetadata_Application:
            {
                fprintf(stdout, "Application metadata (b: %d):\n", metadataLength);
                String appIdStr = {};
                appIdStr.size = 4;
                appIdStr.data = at;
                u32 appId = get_u32_bigendian(at);
                fprintf(stdout, "%sapp id : %08X (%.*s)\n", indent, appId, 
                        appIdStr.size, appIdStr.data);
            } break;
            
            case FlacMetadata_Seektable:
            {
                fprintf(stdout, "Seek table metadata (b: %d):\n", metadataLength);

                u32 seekTableCount = metadataLength / 18;
                seekTableCount = 1;
                //i_expect((metadataLength % 18) == 0);
                
                u8 *seekAt = at;
                for (u32 seekTableIndex = 0; seekTableIndex < seekTableCount; ++seekTableIndex)
                {
                    u64 firstSample = get_u64_bigendian(seekAt);
                    seekAt += 8;
                    u64 offsetBytes = get_u64_bigendian(seekAt);
                    seekAt += 8;
                    u32 samples = get_u16_bigendian(seekAt);
                    seekAt += 2;
                    
                    fprintf(stdout, "%sTable %d:\n", indent, seekTableIndex + 1);
                    fprintf(stdout, "%s%sfirst sample: 0x%016lX\n", indent, indent, firstSample);
                    fprintf(stdout, "%s%soffset bytes: %lu\n", indent, indent, offsetBytes);
                    fprintf(stdout, "%s%ssamples     : %u\n", indent, indent, samples);
                }
            } break;
            
            case FlacMetadata_VorbisComment:
            {
                fprintf(stdout, "Vorbis comment metadata (b: %d):\n", metadataLength);
                
                u8 *commentAt = at;
                String vendor = {};
                vendor.size = *(u32 *)commentAt;
                commentAt+= 4;
                vendor.data = commentAt;
                commentAt+= vendor.size;
                
                fprintf(stdout, "%svendor: %.*s\n", indent, vendor.size, vendor.data);

                u32 commentListCount = *(u32 *)commentAt;
                commentAt += 4;
                for (u32 i = 0; i < commentListCount; ++i)
                {
                    String comment = {};
                    comment.size = *(u32 *)commentAt;
                    commentAt += 4;
                    comment.data = commentAt;
                    commentAt += comment.size;
                    
                    fprintf(stdout, "%scomment %d: %.*s\n", indent, i + 1, comment.size, comment.data);
                }
            } break;
            
            case FlacMetadata_CueSheet:
            {
                fprintf(stdout, "Cue sheet metadata (b: %d)\n", metadataLength);
            } break;
            
            case FlacMetadata_Picture:
            {
                fprintf(stdout, "Picture metadata (b: %d):\n", metadataLength);
                
                u8 *pictureAt = at;
                
                u32 type = get_u32_bigendian(pictureAt);
                pictureAt += 4;
                
                String mime = {};
                mime.size = get_u32_bigendian(pictureAt);
                pictureAt += 4;
                mime.data = pictureAt;
                pictureAt += mime.size;
                
                String description = {};
                description.size = get_u32_bigendian(pictureAt);
                pictureAt += 4;
                description.data = pictureAt;
                pictureAt += description.size;
                
                u32 width = get_u32_bigendian(pictureAt);
                pictureAt += 4;
                u32 height = get_u32_bigendian(pictureAt);
                pictureAt += 4;
                u32 bitsPerPixel = get_u32_bigendian(pictureAt);
                pictureAt += 4;
                
                u32 indexedColours = get_u32_bigendian(pictureAt);
                pictureAt += 4;
                
                Buffer picture = {};
                picture.size = get_u32_bigendian(pictureAt);
                pictureAt += 4;
                picture.data = pictureAt;
                pictureAt += picture.size;
                
                fprintf(stdout, "%stype       : %d\n", indent, type);
                fprintf(stdout, "%smime type  : %.*s\n", indent, mime.size, mime.data);
                fprintf(stdout, "%sdescription: %.*s\n", indent, 
                        description.size, description.data);
                fprintf(stdout, "%swidth      : %d\n", indent, width);
                fprintf(stdout, "%sheight     : %d\n", indent, height);
                fprintf(stdout, "%sbits/pixel : %d\n", indent, bitsPerPixel);
                fprintf(stdout, "%sindexed    : %d\n", indent, indexedColours);

#if 0
#if 0
                b32 isJpg = true;
                char *mimeJpg = "image/jpeg";
                for (u32 i = 0; i < mime.size; ++i)
                {
                    if ((mimeJpg[i] == 0) ||
                        (mimeJpg[i] != mime.data[i]))
                    {
                        isJpg = false;
                        break;
                    }
                }
                #endif

                if (isJpg)
                {
                FILE *file = fopen("picture.jpg", "wb");
                if (file)
                {
                    fwrite(picture.data, picture.size, 1, file);
                    fclose(file);
                }
                }
                #endif

                i_expect(pictureAt == (at + metadataLength));
            } break;
            
            case FlacMetadata_Invalid:
            {
                fprintf(stderr, "Invalid metadata type!\n");
            } break;
            
            default:
                {
                fprintf(stderr, "Unknown metadata type (%d)!\n", metadataKind);
                } break;
            }
        
        at += metadataLength; // NOTE(michiel): Skip metadata
        
        if ((header & 0x80))
        {
            break;
        }
        header = *(u32 *)at;
    }
    
    u32 frameHeaderBaseSyncBlock = get_u16_bigendian(at);
    at += 2;
    u32 frameHeaderBlockSizeSampleRate = *at++;
    u32 frameHeaderChannelSampleSize = *at++;
    
    u32 syncCode = frameHeaderBaseSyncBlock >> 2;
    i_expect((frameHeaderBaseSyncBlock & 0x2) == 0);
    u32 blocking = frameHeaderBaseSyncBlock & 0x1;
    u32 blockSize = frameHeaderBlockSizeSampleRate >> 4;
    u32 sampleRate = frameHeaderBlockSizeSampleRate & 0xF;
    u32 channelAssignment = frameHeaderChannelSampleSize >> 4;
    u32 sampleSize = (frameHeaderChannelSampleSize >> 1) & 0x7;
    i_expect((frameHeaderChannelSampleSize & 0x1) == 0);
    
    if (blocking)
    {
        // NOTE(michiel): Variable blocksize
        
    }
    
    fprintf(stdout, "First header:\n");
    fprintf(stdout, "%ssync code         : 0x%04X\n", indent, syncCode);
    fprintf(stdout, "%sblocking          : %s-blocksize stream\n", indent,
            (blocking) ? "variable" : "fixed");
    fprintf(stdout, "%sblock size        : 0x%X\n", indent, blockSize);
    fprintf(stdout, "%ssample rate       : 0x%X\n", indent, sampleRate);
    fprintf(stdout, "%schannel assignment: 0x%X\n", indent, channelAssignment);
    fprintf(stdout, "%ssample size       : 0x%X\n", indent, sampleSize);
    
    return 0;
}
