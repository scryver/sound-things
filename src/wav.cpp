internal WavStreamer
wav_open_stream(String filename)
{
    WavStreamer result = {};
    
    result.file = api.file.open_file(filename, FileOpen_Read);
    
    if (result.file.fileSize)
    {
        RiffHeader header = {};
        umm fileOffset = 0;
        umm bytesRead = api.file.read_from_file(&result.file,
                                                sizeof(RiffHeader), &header);
        
        if (bytesRead == sizeof(RiffHeader))
        {
            if (header.magic == MAKE_MAGIC('R', 'I', 'F', 'F'))
            {
                fileOffset += sizeof(RiffHeader);
                if ((header.size + sizeof(RiffChunk)) == result.file.fileSize)
                {
                    if (header.fileType == MAKE_MAGIC('W', 'A', 'V', 'E'))
                    {
                        while (fileOffset < result.file.fileSize)
                        {
                            RiffChunk chunk;
                            bytesRead = api.file.read_from_file(&result.file,
                                                                sizeof(RiffChunk), &chunk);
                            if (bytesRead == sizeof(RiffChunk))
                            {
                                fileOffset += sizeof(RiffChunk);
                                
                                switch (chunk.magic)
                                {
                                    case MAKE_MAGIC('f', 'm', 't', ' '):
                                    {
                                        u32 bytesToRead = minimum(chunk.size, sizeof(WavFormat) - sizeof(RiffChunk));
                                        WavFormat format = {};
                                        format.magic = chunk.magic;
                                        format.chunkSize = chunk.size;
                                        bytesRead = api.file.read_from_file(&result.file,
                                                                            bytesToRead,
                                                                            ((u8 *)&format) + sizeof(RiffChunk));
                                        if (bytesRead == bytesToRead)
                                        {
                                            result.settings.channelCount = format.channelCount;
                                            result.settings.sampleFrequency = format.sampleRate;
                                            result.settings.sampleResolution = format.sampleSize;
                                            result.settings.sampleFrameSize = format.blockAlign;
                                            result.settings.format = (WavFormatType)format.formatCode;
                                            if ((format.chunkSize > 16) && format.extensionCount)
                                            {
                                                result.settings.format = (WavFormatType)*(u16 *)format.subFormat;
                                            }
                                        }
                                        else
                                        {
                                            // TODO(michiel): Failed to read all of the fmt
                                        }
                                        
                                        fileOffset += bytesRead;
                                    } break;
                                    
                                    case MAKE_MAGIC('d', 'a', 't', 'a'):
                                    {
                                        result.dataFileOffset = fileOffset;
                                        result.dataCount = chunk.size;
                                        result.dataOffset = fileOffset;
                                        
                                        if (result.settings.sampleFrequency)
                                        {
                                            // NOTE(michiel): Break the search loop, we have all the info
                                            fileOffset = result.file.fileSize;
                                        }
                                        else
                                        {
                                            fileOffset += ((chunk.size + 1) & ~1);
                                            api.file.set_file_position(&result.file, fileOffset, FileCursor_StartOfFile);
                                        }
                                    } break;
                                    
                                    default:
                                    {
                                        // TODO(michiel): Unsupported
                                        fileOffset += chunk.size;
                                        api.file.set_file_position(&result.file, fileOffset, FileCursor_StartOfFile);
                                    } break;
                                }
                            }
                            else
                            {
                                // TODO(michiel): Can't read chunk header
                                break;
                            }
                        }
                    }
                    else
                    {
                        // TODO(michiel): Invalid wave header
                    }
                }
                else
                {
                    // TODO(michiel): Invalid file size in header
                }
            }
            else
            {
                // TODO(michiel): Invalid file header
            }
        }
        else
        {
            // TODO(michiel): Can't read file header
        }
    }
    else
    {
        // TODO(michiel): Can't open file
    }
    
    return result;
}

internal b32
wav_read_stream(WavStreamer *streamer, Buffer *output)
{
    i_expect(output->size);
    i_expect(output->data);
    
    b32 result = false;
    
    u32 readSize = output->size;
    if ((streamer->dataOffset + readSize) > (streamer->dataFileOffset + streamer->dataCount))
    {
        readSize = streamer->dataFileOffset + streamer->dataCount - streamer->dataOffset;
    }
    
    if (readSize)
    {
        api.file.set_file_position(&streamer->file, streamer->dataOffset, FileCursor_StartOfFile);
        u32 bytesRead = api.file.read_from_file(&streamer->file, readSize, output->data);
        if (bytesRead == readSize)
        {
            streamer->dataOffset += bytesRead;
            result = true;
        }
        else
        {
            // TODO(michiel): Should we close the file here?
            //api.file.close_file(&streamer->file);
            readSize = 0;
        }
    }
    else
    {
        // TODO(michiel): Or should we maybe close the file here?
        //api.file.close_file(&streamer->file);
    }
    
    output->size = readSize;
    
    return result;
}

internal WavReader
wav_load_file(MemoryAllocator *allocator, String filename)
{
    WavReader result = {};
    
    result.rawData = api.file.read_entire_file(allocator, filename);
    if (result.rawData.size)
    {
        u8 *src = result.rawData.data;
        RiffHeader *header = (RiffHeader *)src;
        src += sizeof(RiffHeader);
        
        if ((header->magic == MAKE_MAGIC('R', 'I', 'F', 'F')) &&
            (header->fileType == MAKE_MAGIC('W', 'A', 'V', 'E')))
        {
            while (src < (result.rawData.data + result.rawData.size))
            {
                RiffChunk *chunk = (RiffChunk *)src;
                src += sizeof(RiffChunk);
                
                switch (chunk->magic)
                {
                    case MAKE_MAGIC('f', 'm', 't', ' '):
                    {
                        WavFormat *format = (WavFormat *)chunk;
                        src += format->chunkSize;
                        
                        result.settings.channelCount = format->channelCount;
                        result.settings.sampleFrequency = format->sampleRate;
                        result.settings.sampleResolution = format->sampleSize;
                        result.settings.sampleFrameSize = format->blockAlign;
                        result.settings.format = (WavFormatType)format->formatCode;
                        if ((format->formatCode == WavFormat_Extensible) &&
                            (format->chunkSize > 16) &&
                            format->extensionCount)
                        {
                            result.settings.format = (WavFormatType)*(u16 *)format->subFormat;
                        }
                    } break;
                    
                    case MAKE_MAGIC('d', 'a', 't', 'a'):
                    {
                        result.dataOffset = (src - result.rawData.data);
                        result.dataCount = chunk->size;
                        result.readOffset = 0;
                        src += (chunk->size + 1) & ~1;
                    } break;
                    
                    default:
                    {
                        src += chunk->size;
                    } break;
                }
            }
        }
    }
    
    return result;
}

internal Buffer
wav_read_chunk(WavReader *reader, u32 byteCount)
{
    Buffer result = {};
    
    if ((reader->readOffset + byteCount) > reader->dataCount)
    {
        byteCount = reader->dataCount - reader->readOffset;
    }
    
    if (byteCount)
    {
        result.size = byteCount;
        result.data = reader->rawData.data + reader->dataOffset + reader->readOffset;
        reader->readOffset += byteCount;
    }
    
    return result;
}

internal void
print_format(WavFormat *format)
{
    u16 formatCodeTest = format->formatCode;
    if ((format->chunkSize > 16) && format->extensionCount)
    {
        formatCodeTest = *(u16 *)format->subFormat;
    }
    
    char *formatCode = "";
    switch (formatCodeTest)
    {
        case WavFormat_PCM:        { formatCode = "PCM"; } break;
        case WavFormat_Float:      { formatCode = "IEEE Float"; } break;
        case WavFormat_ALaw:       { formatCode = "A-Law"; } break;
        case WavFormat_uLaw:       { formatCode = "mu-Law"; } break;
        //case WavFormat_Extensible: { formatCode = "Extensible"; } break;
        INVALID_DEFAULT_CASE;
    }
    
    fprintf(stdout, "Wav Format  :\n");
    fprintf(stdout, "  Format Code : %s\n", formatCode);
    fprintf(stdout, "  Channels    : %2u\n", format->channelCount);
    fprintf(stdout, "  Sample Rate : %7u Hz\n", format->sampleRate);
    fprintf(stdout, "  Data Rate   : %7u kB/s\n", format->dataRate / 1024);
    fprintf(stdout, "  Block Align : %2u bytes\n", format->blockAlign);
    fprintf(stdout, "  Sample Size : %2u bits\n", format->sampleSize);
    if (format->chunkSize > 16)
    {
        fprintf(stdout, "  Extensions  : %2u bytes\n", format->extensionCount);
        if (format->extensionCount)
        {
            fprintf(stdout, "    Valid Bits  : %u\n", format->validSampleSize);
            fprintf(stdout, "    Speaker Mask: 0x%08X\n", format->speakerPosMask);
            fprintf(stdout, "    GUID        : ");
            fprintf(stdout, "0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
                    format->subFormat[2], format->subFormat[3], format->subFormat[4],format->subFormat[5], format->subFormat[6], format->subFormat[7], format->subFormat[8], format->subFormat[9], format->subFormat[10], format->subFormat[11], format->subFormat[12], format->subFormat[13], format->subFormat[14], format->subFormat[15]);
            fprintf(stdout, "    Expect GUID : 0x000000001000800000AA00389B71\n");
        }
    }
}

internal void
print_fact(WavFact *fact)
{
    fprintf(stdout, "Wav Fact    :\n");
    fprintf(stdout, "  Sample Count : %u samples/channel\n", fact->sampleCount);
}
