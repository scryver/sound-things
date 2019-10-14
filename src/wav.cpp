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