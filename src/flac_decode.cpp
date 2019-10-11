#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>

#include "../libberdip/platform.h"

#include "flac.h"

#include "../libberdip/std_file.c"
#include "../libberdip/crc.cpp"

#include "bitstreamer.cpp"
#include "flac.cpp"

int main(int argc, char **argv)
{
    Buffer flacData = read_entire_file(static_string("data/PinkFloyd-EmptySpaces.flac"));
    
    BitStreamer bitStream_ = {};
    BitStreamer *bitStream = &bitStream_;
    bitStream->kind = BitStream_BigEndian;
    bitStream->at = flacData.data;
    bitStream->end = flacData.data + flacData.size;
    
    i_expect(is_flac_file(bitStream));
    bitStream->at += 4;
    
    char *indent = "    ";
    
    u32 maxMetadataEntries = 64;
    u32 metadataEntryCount = 0;
    FlacMetadata *metadataEntries = allocate_array(FlacMetadata, maxMetadataEntries);
    
    for(u32 index = 0; index < maxMetadataEntries; ++index)
    {
        FlacMetadata *metadata = metadataEntries + metadataEntryCount++;
        metadata->isLast = get_bits(bitStream, 1);
        metadata->kind = (FlacMetadataType)get_bits(bitStream, 7);
        metadata->totalSize = get_bits(bitStream, 24);
        i_expect(bitStream->remainingBits == 0);
        
        switch (metadata->kind)
        {
            case FlacMetadata_StreamInfo:
            {
                i_expect(metadata->totalSize == 34);
                metadata->info = parse_info_stream(bitStream);
            } break;
            
            case FlacMetadata_Padding:
            {
                metadata->padding.count = metadata->totalSize;
                void_bytes(bitStream, metadata->totalSize);
            } break;
            
            case FlacMetadata_Application:
            {
                metadata->application.ID = get_bits(bitStream, 32);
                void_bytes(bitStream, metadata->totalSize - 4);
            } break;
            
            case FlacMetadata_SeekTable:
            {
                metadata->seekTable.count = metadata->totalSize / 18;
                i_expect((metadata->totalSize % 18) == 0);
                
                metadata->seekTable.entries =
                    allocate_array(FlacSeekEntry, metadata->seekTable.count);
                
                for (u32 seekTableIndex = 0;
                     seekTableIndex < metadata->seekTable.count;
                     ++seekTableIndex)
                {
                    FlacSeekEntry *entry = metadata->seekTable.entries + seekTableIndex;
                    entry->firstSample = get_bits(bitStream, 64);
                    entry->offsetBytes = get_bits(bitStream, 64);
                    entry->samples = get_bits(bitStream, 16);
                }
            } break;
            
            case FlacMetadata_VorbisComment:
            {
                // TODO(michiel): Handle switch to little endian...
                u32 vendorSize = get_le_u32(bitStream);
                metadata->vorbisComments.vendor = copy_to_string(bitStream, vendorSize);
                
                metadata->vorbisComments.commentCount = get_le_u32(bitStream);
                metadata->vorbisComments.comments =
                    allocate_array(String, metadata->vorbisComments.commentCount);
                
                for (u32 i = 0; i < metadata->vorbisComments.commentCount; ++i)
                {
                    String *comment = metadata->vorbisComments.comments + i;
                    
                    u32 commentSize = get_le_u32(bitStream);
                    *comment = copy_to_string(bitStream, commentSize);
                }
            } break;
            
            case FlacMetadata_CueSheet:
            {
                void_bytes(bitStream, metadata->totalSize);
            } break;
            
            case FlacMetadata_Picture:
            {
                metadata->picture.type = (FlacPictureType)get_bits(bitStream, 32);
                
                u32 mimeSize = get_bits(bitStream, 32);
                metadata->picture.mime = copy_to_string(bitStream, mimeSize);
                
                u32 descSize = get_bits(bitStream, 32);
                metadata->picture.description = copy_to_string(bitStream, descSize);
                
                metadata->picture.width = get_bits(bitStream, 32);
                metadata->picture.height = get_bits(bitStream, 32);
                metadata->picture.bitsPerPixel = get_bits(bitStream, 32);
                metadata->picture.indexedColours = get_bits(bitStream, 32);
                
                u32 imageSize = get_bits(bitStream, 32);
                metadata->picture.image = copy_to_bytes(bitStream, imageSize);
            } break;
            
            case FlacMetadata_Invalid:
            default:
            {
            } break;
        }
        
        if (metadata->isLast)
        {
            break;
        }
    }
    
    for(u32 index = 0; index < metadataEntryCount; ++index)
    {
        FlacMetadata *metadata = metadataEntries + index;
        
        switch (metadata->kind)
        {
            case FlacMetadata_StreamInfo:
            {
                fprintf(stdout, "Stream info metadata (b: %d):\n", metadata->totalSize);
                print_info_stream(&metadata->info, "    ");
            } break;
            
            case FlacMetadata_Padding:
            {
                fprintf(stdout, "Padding metadata (b: %d)\n", metadata->totalSize);
            } break;
            
            case FlacMetadata_Application:
            {
                fprintf(stdout, "Application metadata (b: %d):\n", metadata->totalSize);
                fprintf(stdout, "%sapp id : %08X\n", indent, metadata->application.ID);
            } break;
            
            case FlacMetadata_SeekTable:
            {
                fprintf(stdout, "Seek table metadata (b: %d):\n", metadata->totalSize);
                for (u32 seekTableIndex = 0;
                     seekTableIndex < metadata->seekTable.count;
                     ++seekTableIndex)
                {
                    FlacSeekEntry *entry = metadata->seekTable.entries + seekTableIndex;
                    fprintf(stdout, "%sTable %d:\n", indent, seekTableIndex + 1);
                    fprintf(stdout, "%s%sfirst sample: 0x%016lX\n", indent, indent, entry->firstSample);
                    fprintf(stdout, "%s%soffset bytes: %lu\n", indent, indent, entry->offsetBytes);
                    fprintf(stdout, "%s%ssamples     : %u\n", indent, indent, entry->samples);
                }
            } break;
            
            case FlacMetadata_VorbisComment:
            {
                fprintf(stdout, "Vorbis comment metadata (b: %d):\n", metadata->totalSize);
                fprintf(stdout, "%svendor: %.*s\n", indent,
                        STR_FMT(metadata->vorbisComments.vendor));
                
                for (u32 i = 0; i < metadata->vorbisComments.commentCount; ++i)
                {
                    String *comment = metadata->vorbisComments.comments + i;
                    fprintf(stdout, "%scomment %d: %.*s\n", indent, i + 1, STR_FMT(*comment));
                }
            } break;
            
            case FlacMetadata_CueSheet:
            {
                fprintf(stdout, "Cue sheet metadata (b: %d)\n", metadata->totalSize);
            } break;
            
            case FlacMetadata_Picture:
            {
                fprintf(stdout, "Picture metadata (b: %d):\n", metadata->totalSize);
                fprintf(stdout, "%stype       : %d\n", indent, metadata->picture.type);
                fprintf(stdout, "%smime type  : %.*s\n", indent, STR_FMT(metadata->picture.mime));
                fprintf(stdout, "%sdescription: %.*s\n", indent, STR_FMT(metadata->picture.description));
                fprintf(stdout, "%swidth      : %d\n", indent, metadata->picture.width);
                fprintf(stdout, "%sheight     : %d\n", indent, metadata->picture.height);
                fprintf(stdout, "%sbits/pixel : %d\n", indent, metadata->picture.bitsPerPixel);
                fprintf(stdout, "%sindexed    : %d\n", indent, metadata->picture.indexedColours);
                
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
            } break;
            
            case FlacMetadata_Invalid:
            {
                fprintf(stderr, "Invalid metadata type!\n");
            } break;
            
            default:
            {
                fprintf(stderr, "Unknown metadata type (%d)!\n", metadata->kind);
            } break;
        }
    }
    
    i_expect(metadataEntries[0].kind == FlacMetadata_StreamInfo);
    FlacInfo *info = &metadataEntries[0].info;
    
    while (bitStream->at != bitStream->end)
    {
        u8 *crcStart = bitStream->at;
        FlacFrameHeader frameHeader = parse_frame_header(bitStream, info);
        
        char *channelType = "";
        switch (frameHeader.channelAssignment)
        {
            case FlacChannel_Mono:      { channelType = "M"; } break;
            case FlacChannel_LeftRight: { channelType = "L/R"; } break;
            case FlacChannel_LeftRightCenter: { channelType = "L/R/C"; } break;
            case FlacChannel_FrontLRBackLR: { channelType = "FL/FR/BL/BR"; } break;
            case FlacChannel_FrontLRCBackLR: { channelType = "FL/FR/FC/BL/BR"; } break;
            case FlacChannel_FrontLRCSubBackLR: { channelType = "FL/FR/FC/LFE/BL/BR"; } break;
            case FlacChannel_FrontLRCSubBackCLR: { channelType = "FL/FR/FC/LFE/BC/BL/BR"; } break;
            case FlacChannel_FrontLRCSubBackLRSideLR: { channelType = "FL/FR/FC/LFE/BL/BR/SL/SR"; } break;
            case FlacChannel_LeftSide:  { channelType = "L/S"; } break;
            case FlacChannel_RightSide: { channelType = "R/S"; } break;
            case FlacChannel_MidSide:   { channelType = "M/S"; } break;
            default: break;
        }
        
        fprintf(stdout, "First header:\n");
        fprintf(stdout, "%sblocking          : %s-blocksize stream\n", indent,
                (frameHeader.variableBlocks) ? "variable" : "fixed");
        fprintf(stdout, "%sblock size        : %d samples\n", indent, frameHeader.blockSize);
        fprintf(stdout, "%ssample rate       : %d Hz\n", indent, frameHeader.sampleRate);
        fprintf(stdout, "%schannel count     : %u\n", indent, frameHeader.channelCount);
        fprintf(stdout, "%schannel assignment: %s\n", indent, channelType);
        fprintf(stdout, "%ssample size       : %d bits\n", indent, frameHeader.bitsPerSample);
        
        for (u32 subChannelIndex = 0; subChannelIndex < frameHeader.channelCount; ++subChannelIndex)
        {
            FlacSubframeHeader subframeHeader = parse_subframe_header(bitStream, &frameHeader, subChannelIndex);
            
            char *subframeType = "";
            switch (subframeHeader.type)
            {
                case FlacSubframe_Constant: { subframeType = "constant"; } break;
                case FlacSubframe_Verbatim: { subframeType = "verbatim"; } break;
                case FlacSubframe_Fixed: { subframeType = "fixed"; } break;
                case FlacSubframe_LPC: { subframeType = "lpc"; } break;
                case FlacSubframe_Error: { subframeType = "error"; } break;
                default: { subframeType = "reserved"; } break;
            }
            fprintf(stdout, "Subframe type: %s (%u)\n", subframeType, subframeHeader.typeOrder);
            fprintf(stdout, "Wasted bits: %s (%u)\n", (subframeHeader.wastedBits) ? "true" : "false", subframeHeader.wastedBits);
            
            s32 warmupSamples[32];
            u32 bps = frameHeader.bitsPerSample;
            if ((((frameHeader.channelAssignment == FlacChannel_LeftSide) ||
                  (frameHeader.channelAssignment == FlacChannel_MidSide)) &&
                 (subChannelIndex == 1)) ||
                ((frameHeader.channelAssignment == FlacChannel_RightSide) &&
                 (subChannelIndex == 0)))
            {
                ++bps;
            }
            
            if (subframeHeader.type == FlacSubframe_Constant)
            {
                (void)get_bits(bitStream, frameHeader.bitsPerSample);
            }
            else if (subframeHeader.type == FlacSubframe_Verbatim)
            {
                //umm totalBits = frameHeader.bitsPerSample * frameHeader.blockSize;
                for (u32 x = 0; x < frameHeader.blockSize; ++x)
                {
                    (void)get_bits(bitStream, frameHeader.bitsPerSample);
                }
            }
            else if (subframeHeader.type == FlacSubframe_Fixed)
            {
                u32 fixedOrder = subframeHeader.typeOrder;
                //umm totalBits = frameHeader.bitsPerSample * fixedOrder;
                for (u32 x = 0; x < fixedOrder; ++x)
                {
                    (void)get_bits(bitStream, bps);
                }
                parse_residual_coding(bitStream, fixedOrder, frameHeader.blockSize);
            }
            else if (subframeHeader.type == FlacSubframe_LPC)
            {
                // NOTE(michiel): LPC
                u32 lpcOrder = subframeHeader.typeOrder;
                for (u32 warmupIndex = 0; warmupIndex < lpcOrder; ++warmupIndex)
                {
                    warmupSamples[warmupIndex] = (s32)(get_bits(bitStream, bps) << (32 - bps)) >> (32 - bps);
                }
                u32 linearPredictorCoeffPrecision = get_bits(bitStream, 4) + 1;
                i_expect(linearPredictorCoeffPrecision < 16);
                s32 linearPredictorShift = ((s32)(s8)(get_bits(bitStream, 5) << 3)) >> 3;
                s32 predictorCoeffs[512];
                
                u32 lpcp = linearPredictorCoeffPrecision;
                for (u32 coeffIndex = 0; coeffIndex < lpcOrder; ++coeffIndex)
                {
                    predictorCoeffs[coeffIndex] = (s32)(get_bits(bitStream, lpcp) << (32 - lpcp)) >> (32 - lpcp);
                }
                
                fprintf(stdout, "SubFrame LPC:\n");
                fprintf(stdout, "%slpc order : %d\n", indent, lpcOrder);
                for (u32 warmupIndex = 0; warmupIndex < lpcOrder; ++warmupIndex)
                {
                    fprintf(stdout, "%swarmup(%d): %d\n", indent, warmupIndex, warmupSamples[warmupIndex]);
                }
                fprintf(stdout, "%sprecision : %d\n", indent, linearPredictorCoeffPrecision);
                fprintf(stdout, "%sshift     : %d\n", indent, linearPredictorShift);
                for (u32 coeffIndex = 0; coeffIndex < lpcOrder; ++coeffIndex)
                {
                    fprintf(stdout, "%scoeff(%d) : %d\n", indent, coeffIndex, predictorCoeffs[coeffIndex]);
                }
                
                parse_residual_coding(bitStream, lpcOrder, frameHeader.blockSize);
            }
            else
            {
                fprintf(stderr, "Invalid subframe!\n");
            }
        }
        
        bitStream->remainingBits = 0;
        bitStream->remainingData = 0;
        
        //void_bytes(bitStream, (frameHeader.blockSize * frameHeader.bitsPerSample) / 8);
        u16 crcTable[256];
        crc16_init_table(0x8005, crcTable);
        u16 crcCheck = crc16_calc_crc(crcTable, bitStream->at - crcStart, crcStart);
        u16 crcFile = get_bits(bitStream, 16);
        
        if (crcFile != crcCheck)
        {
            fprintf(stderr, "CRC16 calc: %04X, CRC16 file: %04X\n", crcCheck, crcFile);
        }
        i_expect(crcFile == crcCheck);
    }
    
    return 0;
}
