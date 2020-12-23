#include "../libberdip/platform.h"
#include "../libberdip/linux_memory.h"

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>

#include "wav.h"

global API api;
global MemoryAPI *gMemoryApi = &api.memory;

#include "../libberdip/memory.cpp"
#include "../libberdip/linux_memory.cpp"
#include "../libberdip/linux_file.c"

#include "wav.cpp"

s32 main(s32 argc, char **argv)
{
    linux_memory_api(&api.memory);
    linux_file_api(&api.file);
    
    MemoryAllocator platformAlloc = {};
    initialize_platform_allocator(0, &platformAlloc);
    
    //
    // NOTE(michiel): Options
    //
    
    WavSettings settings = {};
    settings.channelCount = 2;
    settings.sampleFrequency = 44100;
    settings.sampleResolution = 16;
    settings.sampleFrameSize = 4; // (settings.channelCount * settings.sampleResolution + 7) / 8;
    settings.format = WavFormat_PCM;
    
    u32 lengthInSecs = 60;
    f64 frequency = 600.0; // Hz
    f64 ampltIndB = -60.0; // dBFS
    
    s32 index = 1;
    while (index < argc)
    {
        String arg = string(argv[index++]);
        if ((arg == string("--frequency")) ||
            (arg == string("-f")))
        {
            i_expect(index < argc);
            String value = string(argv[index++]);
            frequency = float_from_string(value);
        }
        else if ((arg == string("--amplitude")) ||
                 (arg == string("-a")))
        {
            i_expect(index < argc);
            String value = string(argv[index++]);
            ampltIndB = float_from_string(value);
        }
        else if ((arg == string("--length")) ||
                 (arg == string("-l")))
        {
            i_expect(index < argc);
            String value = string(argv[index++]);
            lengthInSecs = number_from_string(value);
        }
        else if ((arg == string("--samplerate")) ||
                 (arg == string("-r")))
        {
            i_expect(index < argc);
            String value = string(argv[index++]);
            settings.sampleFrequency = number_from_string(value);
        }
#if 0
        else if ((arg == string("--resolution")) ||
                 (arg == string("-b")))
        {
            i_expect(index < argc);
            String value = string(argv[index++]);
            settings.sampleResolution = number_from_string(value);
        }
#endif
        else
        {
            fprintf(stderr, "Unexpected argument: %.*s\n", STR_FMT(arg));
            fprintf(stderr,
                    "Usage: %s [options]\n"
                    "\n"
                    "  -f | --frequency      Generated sine frequency\n"
                    "  -a | --amplitude      Amplitude in dBFS\n"
                    "  -l | --length         Length in seconds\n"
                    "  -r | --samplerate     Sample rate in Hz\n",
                    argv[0]);
        }
    }
    
    //
    // NOTE(michiel): Generator
    //
    i_expect(ampltIndB <= 0.0);
    i_expect(frequency < 0.5 * (f64)settings.sampleFrequency);
    
    u32 sampleCount = lengthInSecs * settings.sampleFrequency;
    f64 amplitude = pow(10, ampltIndB / 20.0);
    
    Buffer sampleData = {};
    sampleData.size = sampleCount * settings.sampleFrameSize;
    sampleData.data = (u8 *)allocate_size(&platformAlloc, sampleData.size, default_memory_alloc());
    
    Buffer fillAt = sampleData;
    f64 at = 0.0;
    f64 atStep = frequency / (f64)settings.sampleFrequency;
    while (fillAt.size)
    {
        f64 sample = amplitude * sin_pi(F64_PI * at);
        for (u32 channel = 0; channel < settings.channelCount; ++channel)
        {
            // NOTE(michiel): 16bit
            s16 value = (s16)round64((f64)S16_MAX * sample);
            *(u16 *)fillAt.data = value;
            advance(&fillAt, 2);
        }
        at += atStep;
        if (atStep >= 1.0) {
            atStep -= 1.0;
        }
    }
    
    //
    // NOTE(michiel): File write
    //
    u8 nameBuf[4096];
    String filename = string_fmt(array_count(nameBuf), nameBuf, "sine_%uHz_%ddBFS@%uHz_%usec.wav",
                                 (u32)frequency, (s32)ampltIndB, settings.sampleFrequency, lengthInSecs);
    wav_write_file(filename, &settings, sampleData);
    
    return 0;
}
