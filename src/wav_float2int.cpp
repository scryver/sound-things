#include "../libberdip/platform.h"
#include "../libberdip/random.h"
#include "../libberdip/linux_memory.h"

#include <sys/mman.h>
#include <unistd.h>
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
    
    RandomSeriesPCG random = random_seed_pcg(986589128046914ULL, 732164587613947ULL);
    
    String inputFile = {};
    String outputFile = {};
    if (argc == 3)
    {
        inputFile = string(argv[1]);
        outputFile = string(argv[2]);
        
        fprintf(stdout, "Converting '%.*s' -> '%.*s'\n", STR_FMT(inputFile), STR_FMT(outputFile));
        
        WavStreamer streamer = wav_open_stream(inputFile);
        
        ApiFile output = api.file.open_file(outputFile, FileOpen_Write);
        
        if (streamer.settings.sampleFrequency)
        {
            RiffChunk chunk = {};
            chunk.magic = MAKE_MAGIC('R', 'I', 'F', 'F');
            chunk.size  = 0; // TODO(michiel): Fill in at the end
            api.file.write_to_file(&output, sizeof(RiffChunk), &chunk);
            
            chunk.magic = MAKE_MAGIC('W', 'A', 'V', 'E');
            api.file.write_to_file(&output, 4, &chunk.magic);
            
            WavFormat format = {};
            format.magic = MAKE_MAGIC('f', 'm', 't', ' ');
            format.chunkSize = 16;
            format.formatCode = 1;
            format.channelCount = 2;
            format.sampleRate = 352800;
            format.blockAlign = 6;
            format.sampleSize = 24;
            format.dataRate = (format.sampleRate * format.channelCount * format.sampleSize) / 8;
            
            print_format(&format);
            api.file.write_to_file(&output, 16 + 8, &format);
            
            fprintf(stdout, "Orig: %uch %uHz %ubit %uframe %uformat\n", streamer.settings.channelCount,
                    streamer.settings.sampleFrequency, streamer.settings.sampleResolution, streamer.settings.sampleFrameSize, streamer.settings.format);
            
            u32 oldDataCount = streamer.dataCount;
            u32 newDataCount = (oldDataCount / 4) * 3;
            
            chunk.magic = MAKE_MAGIC('d', 'a', 't', 'a');
            chunk.size  = newDataCount;
            api.file.write_to_file(&output, sizeof(RiffChunk), &chunk);
            
            PlatformMemoryBlock *readBlock  = gMemoryApi->allocate_memory(kilobytes(4), default_memory_alloc());
            PlatformMemoryBlock *writeBlock = gMemoryApi->allocate_memory(kilobytes(4), default_memory_alloc());
            
            u32 startCount = api.file.get_file_position(&output);
            
            Buffer readBuffer = {readBlock->size, readBlock->base};
            while (wav_read_stream(&streamer, &readBuffer))
            {
                u32 sampleCount = readBuffer.size / streamer.settings.sampleFrameSize;
                
                f32 *source = (f32 *)readBuffer.data;
                u8 *dest = writeBlock->base;
                for (u32 index = 0; index < sampleCount; ++index)
                {
                    f32 left  = *source++;
                    f32 right = *source++;
                    
                    s32 leftDith  = s24_from_f32_dtpdf(&random, left);
                    s32 rightDith = s24_from_f32_dtpdf(&random, right);
                    
                    *dest++ = ((leftDith  >>  0) & 0xFF);
                    *dest++ = ((leftDith  >>  8) & 0xFF);
                    *dest++ = ((leftDith  >> 16) & 0xFF);
                    *dest++ = ((rightDith >>  0) & 0xFF);
                    *dest++ = ((rightDith >>  8) & 0xFF);
                    *dest++ = ((rightDith >> 16) & 0xFF);
                }
                
                api.file.write_to_file(&output, sampleCount * format.blockAlign, writeBlock->base);
            }
            
            u32 endCount = api.file.get_file_position(&output);
            i_expect((endCount - startCount) == newDataCount);
        }
        
        umm filesize  = api.file.get_file_size(&output);
        u32 wavSize = safe_truncate_to_u32(filesize) - 8;
        api.file.set_file_position(&output, 4, FileCursor_StartOfFile);
        api.file.write_to_file(&output, 4, &wavSize);
        
        api.file.close_file(&output);
        
#if 0        
        ApiFile fixup = api.file.open_file(outputFile, FileOpen_Append);
        umm filesize  = api.file.get_file_size(&fixup);
        u32 wavSize = safe_truncate_to_u32(filesize) - 8;
        api.file.set_file_position(&fixup, 4, FileCursor_StartOfFile);
        api.file.write_to_file(&fixup, 4, &wavSize);
        api.file.close_file(&fixup);
#endif
        
    }
    else
    {
        fprintf(stderr, "Usage: %s <input-file> <output-file>\n", argv[0]);
    }
    
    return 0;
}