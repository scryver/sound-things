#include "../libberdip/platform.h"

#include "../libberdip/std_file.c"

#include "./mp3.h"

internal void
print_id3v1(Buffer tag)
{
    i_expect(tag.size == 128);
    String tagStr  = { 3, tag.data};
    
    if (tagStr == string(3, "TAG"))
    {
        String title   = {30, tag.data + 3};
        String artist  = {30, tag.data + 33};
        String album   = {30, tag.data + 63};
        String year    = { 4, tag.data + 93};
        String comment = {30, tag.data + 97};
        
        title.size = minimum(string_length((char *)title.data), title.size);
        artist.size = minimum(string_length((char *)artist.data), artist.size);
        album.size = minimum(string_length((char *)album.data), album.size);
        comment.size = minimum(string_length((char *)comment.data), comment.size);
        
        u8 genreIdx    = tag.data[127];
        String genre   = gID3v1Genres[genreIdx];
        fprintf(stdout, "ID3v1 Tag:\n");
        fprintf(stdout, "    Title  : %.*s\n", STR_FMT(title));
        fprintf(stdout, "    Artist : %.*s\n", STR_FMT(artist));
        fprintf(stdout, "    Album  : %.*s\n", STR_FMT(album));
        fprintf(stdout, "    Year   : %.*s\n", STR_FMT(year));
        fprintf(stdout, "    Comment: %.*s\n", STR_FMT(comment));
        fprintf(stdout, "    Genre  : %.*s\n", STR_FMT(genre));
    }
    else
    {
        fprintf(stderr, "Not a valid ID3v1 Tag!\n");
    }
}

internal void
print_ape(Buffer tag)
{
    i_expect(tag.size >= 64);
    String tagStr = {8, tag.data};
    
    if (tagStr == string(8, "APETAGEX"))
    {
        u32 version = *(u32 *)(tag.data + 8);
        u32 tagSize = *(u32 *)(tag.data + 12);
        u32 itemCount = *(u32 *)(tag.data + 16);
        u32 tagFlags = *(u32 *)(tag.data + 20);
        u64 reserved = *(u64 *)(tag.data + 24);
        if (reserved == 0)
        {
            tag.data += 32;
            tag.size -= 32;
            
            fprintf(stdout, "APE Tag v%u:\n", version);
            fprintf(stdout, "    tag size  : %u\n", tagSize);
            fprintf(stdout, "    item count: %u\n", itemCount);
            fprintf(stdout, "    tag flags : 0x%08X\n", tagFlags);
            for (u32 itemIdx = 0; itemIdx < itemCount; ++itemIdx)
            {
                u32 itemSize = *(u32 *)(tag.data + 0);
                u32 itemFlags = *(u32 *)(tag.data + 4);
                String itemKey = string((char *)tag.data + 8);
                String itemValue = string(itemSize, tag.data + itemKey.size + 1 + 8);
                fprintf(stdout, "        %.*s: %.*s (0x%08X)\n", STR_FMT(itemKey), STR_FMT(itemValue),
                        itemFlags);
                
                tag.data += 8 + itemKey.size + 1 + itemSize;
                tag.size -= 8 + itemKey.size + 1 + itemSize;
            }
            
            tagStr.data = tag.data;
            if ((tagStr == string(8, "APETAGEX")) && (tag.size >= 32))
            {
                version = *(u32 *)(tag.data + 8);
                tagSize = *(u32 *)(tag.data + 12);
                itemCount = *(u32 *)(tag.data + 16);
                tagFlags = *(u32 *)(tag.data + 20);
                reserved = *(u64 *)(tag.data + 24);
                if (reserved == 0)
                {
                    // NOTE(michiel): Do something?
                }
                else
                {
                    fprintf(stderr, "Ape footer had a non-zero reserved field!\n");
                }
            }
            else
            {
                fprintf(stderr, "Not a valid APE Tag footer!\n");
            }
        }
        else
        {
            fprintf(stderr, "Ape header had a non-zero reserved field!\n");
        }
    }
    else
    {
        fprintf(stderr, "Not a valid APE Tag header!\n");
    }
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        String inputFile = string(argv[1]);
        fprintf(stdout, "Opening: %.*s.\n", STR_FMT(inputFile));
        
        Buffer inputData = read_entire_file(inputFile);
        if (inputData.size)
        {
            u8 *src = inputData.data;
            String tag = {3, src};
            src += 3;
            
            u8 version = *src++;
            u8 revision = *src++;
            
            u8 flags = *src++;
            b32 unsynchronisation = flags & 0x80;
            b32 extendedHeader = flags & 0x40;
            b32 experimental = flags & 0x20;
            b32 hasFooter = flags & 0x10;
            i_expect((flags & 0x0F) == 0x00);
            
            u32 size = *(u32 *)src;
            src += 4;
            size = (((size << 21) & 0x0FE00000) |
                    ((size <<  6) & 0x001FC000) |
                    ((size >>  9) & 0x00003F80) |
                    ((size >> 24) & 0x0000007F));
            
            fprintf(stdout, "%.*s v%u.%u:\n", STR_FMT(tag), version, revision);
            fprintf(stdout, "    unsynched   : %s\n", unsynchronisation ? "true" : "false");
            fprintf(stdout, "    extended hdr: %s\n", extendedHeader ? "true" : "false");
            fprintf(stdout, "    experimental: %s\n", experimental ? "true" : "false");
            fprintf(stdout, "    footer      : %s\n", hasFooter ? "true" : "false");
            fprintf(stdout, "    size        : %u\n", size);
            
            u32 frameNr = 0;
            u8 *end = src + size;
            while (src < end)
            {
                String frameId = {4, src};
                src += 4;
                u32 frameSize = *(u32 *)src;
                src += 4;
                frameSize = (((frameSize << 21) & 0x0FE00000) |
                             ((frameSize <<  6) & 0x001FC000) |
                             ((frameSize >>  9) & 0x00003F80) |
                             ((frameSize >> 24) & 0x0000007F));
                u16 frameFlags = *src++;
                frameFlags = (frameFlags << 8) | *src++;
                
                // NOTE(michiel): http://id3.org/id3v2.4.0-frames
                if (frameId == string(4, "TIT2"))
                {
                    fprintf(stdout, "Title : %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TPE1"))
                {
                    fprintf(stdout, "Artist: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TALB"))
                {
                    fprintf(stdout, "Album : %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TRCK"))
                {
                    fprintf(stdout, "Track : %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TPOS"))
                {
                    fprintf(stdout, "Disk  : %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TDRC"))
                {
                    fprintf(stdout, "Recorded at %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TCON"))
                {
                    fprintf(stdout, "Content type: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TBPM"))
                {
                    fprintf(stdout, "BPM   : %.*s beats per minute\n", frameSize, src);
                }
                else if (frameId == string(4, "TCOM"))
                {
                    fprintf(stdout, "Composer: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TDOR"))
                {
                    fprintf(stdout, "Original release: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TEXT"))
                {
                    fprintf(stdout, "Text written by: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TMED"))
                {
                    fprintf(stdout, "Original medium: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TPE2"))
                {
                    fprintf(stdout, "Band: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TPUB"))
                {
                    fprintf(stdout, "Publisher: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TSOP"))
                {
                    fprintf(stdout, "Performer sort order: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TLAN"))
                {
                    fprintf(stdout, "Language: %.*s\n", frameSize, src);
                }
                else if (frameId == string(4, "TIPL"))
                {
                    fprintf(stdout, "Involved people:\n");
                    u8 *startP = src;
                    u8 *endP = startP + frameSize;
                    while (startP < endP)
                    {
                        if (((startP[0] == 0xFF) &&
                             (startP[1] == 0xFE)) ||
                            ((startP[0] == 0xFE) &&
                             (startP[1] == 0xFF)))
                        {
                            startP += 2;
                        }
                        
                        String what = string((char *)startP);
                        startP += what.size + 1;
                        String who = string((char *)startP);
                        startP += who.size + 1;
                        fprintf(stdout, "    %.*s: %.*s\n", STR_FMT(what), STR_FMT(who));
                    }
                }
                else if (frameId == string(4, "TMCL"))
                {
                    fprintf(stdout, "Musician credits\n");
                    u8 *startP = src;
                    u8 *endP = startP + frameSize;
                    while (startP < endP)
                    {
                        String what = string((char *)startP);
                        startP += what.size + 1;
                        String who = string((char *)startP);
                        startP += who.size + 1;
                        fprintf(stdout, "    %.*s: %.*s\n", STR_FMT(what), STR_FMT(who));
                    }
                }
                else if (frameId == string(4, "COMM"))
                {
                    u8 encoding = src[0];
                    String language = {3, src + 1};
                    String descript = string((char *)src + 4);
                    String comment  = string(frameSize - descript.size - 5, (char *)src + 4 + descript.size + 1);
                    fprintf(stdout, "Comments (%02X) in %.*s: %.*s ||| %.*s\n", encoding, STR_FMT(language),
                            STR_FMT(descript), STR_FMT(comment));
                }
                else if (frameId == string(4, "PRIV"))
                {
                    // NOTE(michiel): ...
                }
                else if (frameId == string(4, "TXXX"))
                {
                    u8 encoding = src[0];
                    String descript = string((char *)src + 1);
                    String value    = string((char *)src + descript.size + 2);
                    fprintf(stdout, "Text (%02X): %.*s ||| %.*s\n", encoding, STR_FMT(descript), STR_FMT(value));
                }
                else if (frameId == string(4, "UFID"))
                {
                    String owner = string((char *)src);
                    
#if 1
                    String dataBlock = string(frameSize - owner.size - 1, (char *)src + owner.size + 1);
                    fprintf(stdout, "Unique File ID owner: %.*s <== %.*s\n", STR_FMT(owner), STR_FMT(dataBlock));
#else
                    u32 binSize = frameSize - owner.size - 1;
                    u8 *binData = src + owner.size + 1;
                    fprintf(stdout, "Unique File ID owner: %.*s <== 0x", STR_FMT(owner));
                    for (u32 binIdx = 0; binIdx < binSize; ++binIdx)
                    {
                        fprintf(stdout, "%02X", binData[binIdx]);
                    }
                    fprintf(stdout, "\n");
#endif
                    
                }
                else if (frameId == string(4, "USLT"))
                {
                    u8 encoding = src[0];
                    String language = {3, src + 1};
                    String descript = string((char *)src + 4);
                    if (descript.size)
                    {
                        String lyrics  = string(frameSize - descript.size - 5, (char *)src + 4 + descript.size + 1);
                        fprintf(stdout, "Lyrics (%02X) in %.*s: %.*s ||| %.*s\n", encoding, STR_FMT(language),
                                STR_FMT(descript), STR_FMT(lyrics));
                    }
                }
                else
                {
                    fprintf(stdout, "Frame %u:\n", frameNr + 1);
                    fprintf(stdout, "    ID   : %.*s\n", STR_FMT(frameId));
                    fprintf(stdout, "    size : %u\n", frameSize);
                    fprintf(stdout, "    flags: 0x%04X\n", frameFlags);
                    
                    String test = {frameSize, src};
                    fprintf(stdout, "    test : %.*s\n", STR_FMT(test));
                }
                
                src += frameSize;
                if (*src == 0)
                {
                    src = end;
                }
                
                ++frameNr;
            }
            
            Buffer id3v1tag = {128, inputData.data + inputData.size - 128};
            if (string(3, id3v1tag.data) == string(3, "TAG"))
            {
                print_id3v1(id3v1tag);
                inputData.size -= id3v1tag.size;
            }
            
            Buffer testApe = {32, id3v1tag.data - 32};
            if (string(8, testApe.data) == string(8, "APETAGEX"))
            {
                Buffer apeTag = testApe;
                //u32 version = *(u32 *)(testApe.data + 8);
                u32 tagSize = *(u32 *)(testApe.data + 12);
                //u32 itemCount = *(u32 *)(testApe.data + 16);
                //u32 tagFlags = *(u32 *)(testApe.data + 20);
                u64 reserved = *(u64 *)(testApe.data + 24);
                if (reserved == 0)
                {
                    apeTag.data -= tagSize;
                    apeTag.size += tagSize;
                    
                    if (string(8, apeTag.data) == string(8, "APETAGEX"))
                    {
                        print_ape(apeTag);
                        inputData.size -= apeTag.size;
                    }
                }
                else
                {
                    fprintf(stderr, "APE Tag reserved not zero!\n");
                }
            }
            
            end = inputData.data + inputData.size;
            fprintf(stdout, "Remainig: %lu bytes\n", end - src);
            frameNr = 0;
            u8 *lastSearch = src;
            while (src < end)
            {
                b32 endOfFile = false;
                u8 *startSearch = src;
                // NOTE(michiel): Look for sync frame
                while ((src[0] != 0xFF) || ((src[1] & 0xE0) != 0xE0))
                {
                    ++src;
                    if (src >= (end - 1))
                    {
                        endOfFile = true;
                        break;
                    }
                }
                
                if (endOfFile)
                {
                    break;
                }
                
                if (src - startSearch)
                {
                    fprintf(stderr, "Skipped%s %lu bytes searching for sync.\n", frameNr == 0 ? " first" : "", src - startSearch);
                    
                    fprintf(stderr, "Bytes:\n");
                    for (u32 skipIdx = 0; skipIdx < (src - startSearch); ++skipIdx)
                    {
                        fprintf(stderr, "%02X", startSearch[skipIdx]);
                    }
                    fprintf(stderr, "\nString:\n%.*s\n", (s32)(src - startSearch), (char *)startSearch);
                }
                
                u32 frameHeader = *(u32 *)src;
                frameHeader = ((frameHeader >> 24) |
                               ((frameHeader >> 8) & 0x0000FF00) |
                               ((frameHeader << 8) & 0x00FF0000) |
                               (frameHeader << 24));
                
                u16 frameSync = (frameHeader >> 21);
                i_expect(frameSync == 0x07FF); // NOTE(michiel): Guaranteed by the search loop above
#if 0            
                u8 mpegAudioID = (frameHeader >> 19) & 0x3;
                u8 layerDesc = (frameHeader >> 17) & 0x3;
#endif
                MpegVersionLayer versionLayer = (MpegVersionLayer)((frameHeader >> 17) & 0xF);
                switch (versionLayer)
                {
                    case Mpeg25_Layer3:
                    case Mpeg25_Layer2:
                    case Mpeg25_Layer1:
                    case Mpeg2_Layer3:
                    case Mpeg2_Layer2:
                    case Mpeg2_Layer1:
                    case Mpeg1_Layer3:
                    case Mpeg1_Layer2:
                    case Mpeg1_Layer1: {} break;
                    default:
                    {
                        // NOTE(michiel): Invalid frame;
                        fprintf(stderr, "MPEG Version invalid, resyncing!\n");
                        ++src;
                        continue;
                    } break;
                }
                
                
                b32 protection = !(frameHeader & (1 << 16));
                
                u8 bitRateIdx = (frameHeader >> 12) & 0xF;
                if (bitRateIdx == 0xF)
                {
                    // NOTE(michiel): Invalid frame
                    fprintf(stderr, "Bit rate index invalid, resyncing!\n");
                    ++src;
                    continue;
                }
                
                u16 bitRate = 0;
                switch (versionLayer)
                {
                    case Mpeg1_Layer1:
                    {
                        bitRate = gMPEGBitRates[bitRateIdx];
                    } break;
                    
                    case Mpeg1_Layer2:
                    {
                        bitRate = gMPEGBitRates[16 + bitRateIdx];
                    } break;
                    
                    case Mpeg1_Layer3:
                    {
                        bitRate = gMPEGBitRates[32 + bitRateIdx];
                    } break;
                    
                    case Mpeg2_Layer1:
                    case Mpeg25_Layer1:
                    {
                        bitRate = gMPEGBitRates[48 + bitRateIdx];
                    } break;
                    
                    case Mpeg2_Layer2:
                    case Mpeg25_Layer2:
                    case Mpeg2_Layer3:
                    case Mpeg25_Layer3:
                    {
                        bitRate = gMPEGBitRates[64 + bitRateIdx];
                    } break;
                    
                    INVALID_DEFAULT_CASE;
                }
                if (bitRate == 0)
                {
                    fprintf(stderr, "Unsupported variable bitrate!\n");
                }
                
                u8 sampleRateIdx = (frameHeader >> 10) & 0x3;
                if (sampleRateIdx == 0x3)
                {
                    // NOTE(michiel): Invalid frame
                    fprintf(stderr, "Sample rate index invalid, resyncing!\n");
                    ++src;
                    continue;
                }
                
                u32 sampleRate = 0;
                switch (versionLayer)
                {
                    case Mpeg1_Layer1:
                    case Mpeg1_Layer2:
                    case Mpeg1_Layer3:
                    {
                        sampleRate = gMPEGSampleRates[sampleRateIdx];
                    } break;
                    
                    case Mpeg2_Layer1:
                    case Mpeg2_Layer2:
                    case Mpeg2_Layer3:
                    {
                        sampleRate = gMPEGSampleRates[4 + sampleRateIdx];
                    } break;
                    
                    case Mpeg25_Layer1:
                    case Mpeg25_Layer2:
                    case Mpeg25_Layer3:
                    {
                        sampleRate = gMPEGSampleRates[8 + sampleRateIdx];
                    } break;
                    
                    INVALID_DEFAULT_CASE;
                }
                
                b32 framePadded = frameHeader & (1 << 9);
                
                u32 slotSize = 0;
                u32 frameByteCount = 0;
                switch (versionLayer)
                {
                    case Mpeg1_Layer1:
                    case Mpeg2_Layer1:
                    case Mpeg25_Layer1:
                    {
                        slotSize = 4;
                        frameByteCount = (12 * bitRate * 1000 / sampleRate + (framePadded ? 1 : 0)) * 4;
                    } break;
                    
                    case Mpeg1_Layer2:
                    case Mpeg2_Layer2:
                    case Mpeg25_Layer2:
                    case Mpeg1_Layer3:
                    case Mpeg2_Layer3:
                    case Mpeg25_Layer3:
                    {
                        slotSize = 1;
                        frameByteCount = (144 * bitRate * 1000 / sampleRate + (framePadded ? 1 : 0));
                    } break;
                    
                    INVALID_DEFAULT_CASE;
                }
                if (frameByteCount > (end - src))
                {
                    // NOTE(michiel): Invalid frame
                    fprintf(stderr, "Too few bytes remaining in the frame (%lu from the %u), resyncing!\n",
                            end - src, frameByteCount);
                    
                    fprintf(stderr, "Bytes:\n");
                    for (u32 skipIdx = 0; skipIdx < (end - src); ++skipIdx)
                    {
                        fprintf(stderr, "%02X", src[skipIdx]);
                    }
                    fprintf(stderr, "\nString:\n%.*s\n", (s32)(end - src), (char *)src);
                    
                    src = end;
                    continue;
                }
                
                //u8 privateBit = (frameHeader >> 8) & 0x1; // NOTE(michiel): Application-dependent
                MpegChannelModes channelMode = (MpegChannelModes)((frameHeader >> 6) & 0x3);
                u8 modeExtension = 0;
                if (channelMode == MpegChannel_JointStereo)
                {
                    modeExtension = (frameHeader >> 4) & 0x3; // NOTE(michiel): For joint-stereo only
                }
                
                
                MpegEmphasis emphasis = (MpegEmphasis)(frameHeader & 0x3);
                if (emphasis == MpegEmphasis_Reserved)
                {
                    // NOTE(michiel): Invalid frame
                    fprintf(stderr, "Emphasis reserved, resyncing!\n");
                    ++src;
                    continue;
                }
                
#if 0
                b32 copyrighted = frameHeader & (1 << 3);
                b32 original = frameHeader & (1 << 2);
                
                fprintf(stdout, "Data frame %u: 0x%04X\n", frameNr + 1, frameSync);
                switch (versionLayer)
                {
                    case Mpeg25_Layer3: { fprintf(stdout, "MPEG2.5LIII\n"); } break;
                    case Mpeg25_Layer2: { fprintf(stdout, "MPEG2.5LII\n"); } break;
                    case Mpeg25_Layer1: { fprintf(stdout, "MPEG2.5LI\n"); } break;
                    case Mpeg2_Layer3:   { fprintf(stdout, "MPEG2LIII\n"); } break;
                    case Mpeg2_Layer2:   { fprintf(stdout, "MPEG2LII\n"); } break;
                    case Mpeg2_Layer1:   { fprintf(stdout, "MPEG2LI\n"); } break;
                    case Mpeg1_Layer3:   { fprintf(stdout, "MPEG1LIII\n"); } break;
                    case Mpeg1_Layer2:   { fprintf(stdout, "MPEG1LII\n"); } break;
                    case Mpeg1_Layer1:   { fprintf(stdout, "MPEG1LI\n"); } break;
                    
                    INVALID_DEFAULT_CASE;
                }
                
                fprintf(stdout, "%s, %u kbps, %u Hz\n", protection ? "protected" : "unprotected", bitRate, sampleRate);
                
                char *channelModeStr = "";
                switch (channelMode)
                {
                    case MpegChannel_Stereo:        { channelModeStr = "stereo"; } break;
                    case MpegChannel_JointStereo:   { channelModeStr = "joint-stereo"; } break;
                    case MpegChannel_DualChannel:   { channelModeStr = "dual-channel"; } break;
                    case MpegChannel_SingleChannel: { channelModeStr = "single-channel"; } break;
                    INVALID_DEFAULT_CASE;
                }
                fprintf(stdout, "%s, %u bytes, %s, %u\n", framePadded ? "padded" : "unpadded", frameByteCount, channelModeStr, modeExtension);
                
                char *emphasisName = "";
                switch (emphasis)
                {
                    case MpegEmphasis_None:     { emphasisName = "none"; } break;
                    case MpegEmphasis_50_15ms:  { emphasisName = "50/15msec"; } break;
                    case MpegEmphasis_Reserved: { emphasisName = "reserved"; } break;
                    case MpegEmphasis_CCIT_J17: { emphasisName = "CCIT J.17"; } break;
                    INVALID_DEFAULT_CASE;
                }
                fprintf(stdout, "%s, %s, emphasis: %s\n", copyrighted ? "copyright" : "copyleft", original ? "original" : "fake", emphasisName);
#endif
                
                u8 *startOfSideInfo = src + 4;
                if (protection)
                {
                    u16 crc = (src[4] << 8) | src[5];
                    unused(crc);
                    startOfSideInfo += 2;
                }
                
                Buffer sideInfo = {32, startOfSideInfo};
                if (channelMode == MpegChannel_SingleChannel)
                {
                    sideInfo.size = 17;
                }
                s32 offset = *sideInfo.data;
                offset = (offset << 1) | ((sideInfo.data[1] & 0x80) >> 7);
                offset = -offset;
                fprintf(stdout, "Offset: %d\n", offset);
                
                src += frameByteCount;
                lastSearch = src;
                ++frameNr;
            }
        }
    }
    
    return 0;
}