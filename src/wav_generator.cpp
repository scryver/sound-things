#include "../libberdip/platform.h"
#include "../libberdip/random.h"
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
#include "truncation.cpp"

s32 main(s32 argc, char **argv)
{
    linux_memory_api(&api.memory);
    linux_file_api(&api.file);
    
    MemoryAllocator platformAlloc = {};
    initialize_platform_allocator(0, &platformAlloc);
    
    //
    // NOTE(michiel): Options
    //

    b32 doSweep = false;
    f64 sweepEndFreq = 0.0;

    WavSettings settings = {};
    settings.channelCount = 2;
    settings.sampleFrequency = 44100;
    settings.sampleResolution = 16;
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
        else if ((arg == string("--resolution")) ||
                 (arg == string("-b")))
        {
            i_expect(index < argc);
            String value = string(argv[index++]);
            settings.sampleResolution = number_from_string(value);
        }
        else if ((arg == string("--sweep")) ||
                 (arg == string("-s")))
        {
            i_expect(index < argc);
            String value = string(argv[index++]);
            sweepEndFreq = float_from_string(value);
            doSweep = true;
        }
        else
        {
            fprintf(stderr, "Unexpected argument: %.*s\n", STR_FMT(arg));
            fprintf(stderr,
                    "Usage: %s [options]\n"
                    "\n"
                    "  -f | --frequency      Generated sine frequency\n"
                    "  -a | --amplitude      Amplitude in dBFS\n"
                    "  -l | --length         Length in seconds\n"
                    "  -r | --samplerate     Sample rate in Hz\n"
                    "  -b | --resolution     Sample resolution in bits\n"
                    "  -s | --sweep          End frequency to sweep to\n",
                    argv[0]);
        }
    }
    i_expect((settings.sampleResolution == 16) || (settings.sampleResolution == 24) || (settings.sampleResolution == 32));

    settings.sampleFrameSize = (settings.channelCount * settings.sampleResolution + 7) / 8;

    //
    // NOTE(michiel): Generator
    //
    i_expect(ampltIndB <= 0.0);
    i_expect(frequency < 0.5 * (f64)settings.sampleFrequency);
    
    u32 sampleCount = lengthInSecs * settings.sampleFrequency;
    f64 amplitude = pow64(10, ampltIndB / 20.0);
    fprintf(stdout, "Amplitude: %f (%a)\n", amplitude, amplitude);

    Buffer sampleData = {};
    sampleData.size = sampleCount * settings.sampleFrameSize;
    sampleData.data = (u8 *)allocate_size(&platformAlloc, sampleData.size, default_memory_alloc());
    
    Buffer fillAt = sampleData;
    f64 at = 0.0;
    f64 atStep = frequency / (f64)settings.sampleFrequency;
    f64 atTotal = 0.0;

    while (fillAt.size)
    {
        f64 sample = amplitude * sin_pi(F64_TAU * at);
        for (u32 channel = 0; channel < settings.channelCount; ++channel)
        {
            switch (settings.sampleResolution)
            {
                case 16: {
                    s32 value = (s32)round64((f64)S16_MAX * sample);
                    *(u16 *)fillAt.data = safe_truncate_to_s16(value);
                    advance(&fillAt, 2);
                } break;

                case 24: {
                    s32 value = (s32)round64((f64)(s32)0x007FFFFF * sample);
                    fillAt.data[0] = ((value >>  0) & 0xFF);
                    fillAt.data[1] = ((value >>  8) & 0xFF);
                    fillAt.data[2] = ((value >> 16) & 0xFF);
                    advance(&fillAt, 3);
                } break;

                case 32: {
                    s32 value = (s32)round64((f64)S32_MAX * sample);
                    *(u32 *)fillAt.data = value;
                    advance(&fillAt, 4);
                } break;

                INVALID_DEFAULT_CASE;
            }
        }

        if (doSweep) {
            f64 fNew = exp64(log64(frequency) * (1.0 - atTotal / lengthInSecs) +
                             log64(sweepEndFreq) * (atTotal / lengthInSecs));
            atStep = fNew / (f64)settings.sampleFrequency;
        }

        at += atStep;
        atTotal += 1.0 / (f64)settings.sampleFrequency;
        if (at >= 1.0) {
            at -= 1.0;
        }
    }
    
    //
    // NOTE(michiel): File write
    //
    u8 nameBuf[4096];
    String filename = {};
    if (doSweep) {
        filename = string_fmt(array_count(nameBuf), nameBuf, "sine_%uHz_%uHz_%.1fdBFS@%uHz_%u_%usec.wav",
                              (u32)frequency, (u32)sweepEndFreq, ampltIndB, settings.sampleFrequency, (u32)settings.sampleResolution, lengthInSecs);
    } else {
        filename = string_fmt(array_count(nameBuf), nameBuf, "sine_%uHz_%.1fdBFS@%uHz_%u_%usec.wav",
                              (u32)frequency, ampltIndB, settings.sampleFrequency, (u32)settings.sampleResolution, lengthInSecs);
    }
    wav_write_file(filename, &settings, sampleData);
    
    return 0;
}
