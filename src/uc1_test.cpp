#include <alsa/asoundlib.h>
#include <stdio.h>
#include <math.h>

#include "synth.h"
#include "synth.cpp"

#define array_count(x)  (sizeof(x) / sizeof(x[0]))

struct SndDevice
{
    char *device;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *hwParams;
};

static char *deviceName = "hw:3,0";
static snd_output_t *sndOutput = NULL;

static SndDevice
create_device(char *name, unsigned int playback = 1)
{
    SndDevice d = {};
    d.device = name;
    snd_pcm_hw_params_malloc(&d.hwParams);
    
    int error = snd_pcm_open(&d.handle, d.device, (playback ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE), 0);
    if (error < 0)
    {
        fprintf(stderr, "%s open error: %s\n", (playback ? "Playback" : "Recording"), snd_strerror(error));
        d.device = 0;
    }
    
    return d;
}

static void
destroy_device(SndDevice *sd)
{
    snd_pcm_hw_params_free(sd->hwParams);
    snd_pcm_close(sd->handle);
    sd->device = 0;
}

static int
set_hw_params_template(SndDevice *sd, unsigned int channels, unsigned int rateCount, unsigned int *rates)
{
    int error = snd_pcm_hw_params_any(sd->handle, sd->hwParams);
    if (error < 0)
    {
        fprintf(stderr, "No configuration available: %s\n", snd_strerror(error));
        return 1;
    }
    
    int resample = 0;
    error = snd_pcm_hw_params_set_rate_resample(sd->handle, sd->hwParams, resample);
    if (error < 0)
    {
        fprintf(stderr, "Resampling setup failed: %s\n", snd_strerror(error));
        return 1;
    }
    
    error = snd_pcm_hw_params_set_access(sd->handle, sd->hwParams, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    if (error < 0)
    {
        fprintf(stderr, "Access type not available: %s\n", snd_strerror(error));
        return 1;
    }
    
#if 0
    snd_pcm_format_t format;
    snd_pcm_hw_params_get_format(sd->hwParams, &format);
    printf("HW Format signed     = %i\n", snd_pcm_format_signed(format));
    printf("HW Format linear     = %i\n", snd_pcm_format_linear(format));
    printf("HW Format ltl-endian = %i\n", snd_pcm_format_little_endian(format));
    printf("HW Format width      = %i\n", snd_pcm_format_width(format));
    printf("HW Format ph-width   = %i\n", snd_pcm_format_physical_width(format));
#endif
    error = snd_pcm_hw_params_set_format(sd->handle, sd->hwParams, SND_PCM_FORMAT_S32_LE);
    if (error < 0)
    {
        fprintf(stderr, "Sample format not available: %s\n", snd_strerror(error));
        return 1;
    }
    
    error = snd_pcm_hw_params_set_channels(sd->handle, sd->hwParams, channels);
    if (error < 0)
    {
        fprintf(stderr, "Channel count (%i) not available: %s\n", channels, snd_strerror(error));
        return 1;
    }
    
    unsigned int rate = rates[0];
    error = snd_pcm_hw_params_set_rate_min(sd->handle, sd->hwParams, &rate, 0);
    if (error < 0)
    {
        fprintf(stderr, "Rate (%i Hz) not available as min: %s\n", rate, snd_strerror(error));
        return 1;
    }
    if (rate != rates[0])
    {
        fprintf(stderr, "Rate (%i Hz) doesn't match requested rate (%i Hz)\n", rate, rates[0]);
        return 1;
    }
    
    rate = rates[rateCount - 1];
    error = snd_pcm_hw_params_set_rate_max(sd->handle, sd->hwParams, &rate, 0);
    if (error < 0)
    {
        fprintf(stderr, "Rate (%i Hz) not available as max: %s\n", rate, snd_strerror(error));
        return 1;
    }
    if (rate != rates[rateCount - 1])
    {
        fprintf(stderr, "Rate (%i Hz) doesn't match requested rate (%i Hz)\n", rate, rates[rateCount - 1]);
        return 1;
    }
    
    return 0;
}

int main(int argc, char *argv[])
{
    int error;
    
    unsigned int rates[] = 
    {
        44100, 48000, 88200, 96000, 176400, 192000,
    };
    unsigned int channels = 10;
    
    unsigned int realRate;
    
    int periods = 2;
    unsigned int periodTime_us = 500; // 500us
    unsigned int bufferTime_us = periods * periodTime_us; // 1ms
    snd_pcm_uframes_t sizeHelper;
    snd_pcm_sframes_t pBufferSize;
    snd_pcm_sframes_t pPeriodSize;
    snd_pcm_sframes_t cBufferSize;
    snd_pcm_sframes_t cPeriodSize;
    int direction;
    
    error = snd_output_stdio_attach(&sndOutput, stdout, 0);
    if (error < 0)
    {
        fprintf(stderr, "Output failed: %s\n", snd_strerror(error));
        return 1;
    }
    
    //
    // NOTE(michiel): Device claiming
    //
    SndDevice pDevice_ = create_device(deviceName);
    SndDevice *pDevice = &pDevice_;
    if (pDevice->device == 0)
    {
        return 1;
    }
    
    if (set_hw_params_template(pDevice, channels, array_count(rates), rates) != 0)
    {
        return 1;
    }
    
    SndDevice cDevice_ = create_device(deviceName, 0);
    SndDevice *cDevice = &cDevice_;
    if (cDevice->device == 0)
    {
        return 1;
    }
    
    if (set_hw_params_template(cDevice, channels, array_count(rates), rates) != 0)
    {
        return 1;
    }
    
    for (unsigned int rateIndex = 0; rateIndex < array_count(rates); ++rateIndex)
    {
        unsigned int rate = rates[rateIndex];
        realRate = rate;
        
        fprintf(stdout, "Testing sample rate %u Hz\n", rate);
        
        int bytesPerSampleRate = rate * channels * 3; // 3 => nr bytes per sample
        
        snd_pcm_drain(pDevice->handle);
        snd_pcm_drop(cDevice->handle);
        
        //
        // NOTE(michiel): Hardware Params
        //
        snd_pcm_hw_params_t *pHwParams;
        snd_pcm_hw_params_alloca(&pHwParams);
        snd_pcm_hw_params_copy(pHwParams, pDevice->hwParams);
        
        error = snd_pcm_hw_params_set_rate_near(pDevice->handle, pHwParams, &realRate, 0);
        if (error < 0)
        {
            fprintf(stderr, "Rate (%i Hz) not available: %s\n", rate, snd_strerror(error));
            return 1;
        }
        if (realRate != rate)
        {
            fprintf(stderr, "Rate (%i Hz) doesn't match requested rate (%i Hz)\n", realRate, rate);
            return 1;
        }
        
        error = snd_pcm_hw_params_set_buffer_time_near(pDevice->handle, pHwParams, &bufferTime_us, &direction);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set buffer time %i usec: %s\n", bufferTime_us, snd_strerror(error));
            return 1;
        }
        error = snd_pcm_hw_params_get_buffer_size(pHwParams, &sizeHelper);
        if (error < 0)
        {
            fprintf(stderr, "Unable to get the buffer size: %s\n", snd_strerror(error));
            return 1;
        }
        pBufferSize = sizeHelper;
        
        error = snd_pcm_hw_params_set_period_time_near(pDevice->handle, pHwParams, &periodTime_us, &direction);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set period time %i usec: %s\n", periodTime_us, snd_strerror(error));
            return 1;
        }
        error = snd_pcm_hw_params_get_period_size(pHwParams, &sizeHelper, &direction);
        if (error < 0)
        {
            fprintf(stderr, "Unable to get the period size: %s\n", snd_strerror(error));
            return 1;
        }
        pPeriodSize = sizeHelper;
        
        error = snd_pcm_hw_params(pDevice->handle, pHwParams);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set hw params: %s\n", snd_strerror(error));
            return 1;
        }
        
        snd_pcm_hw_params_t *cHwParams;
        snd_pcm_hw_params_alloca(&cHwParams);
        snd_pcm_hw_params_copy(cHwParams, cDevice->hwParams);
        
        error = snd_pcm_hw_params_set_rate_near(cDevice->handle, cHwParams, &realRate, 0);
        if (error < 0)
        {
            fprintf(stderr, "Rate (%i Hz) not available: %s\n", rate, snd_strerror(error));
            return 1;
        }
        if (realRate != rate)
        {
            fprintf(stderr, "Rate (%i Hz) doesn't match requested rate (%i Hz)\n", realRate, rate);
            return 1;
        }
        
        error = snd_pcm_hw_params_set_buffer_time_near(cDevice->handle, cHwParams, &bufferTime_us, &direction);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set buffer time %i usec: %s\n", bufferTime_us, snd_strerror(error));
            return 1;
        }
        error = snd_pcm_hw_params_get_buffer_size(cHwParams, &sizeHelper);
        if (error < 0)
        {
            fprintf(stderr, "Unable to get the buffer size: %s\n", snd_strerror(error));
            return 1;
        }
        cBufferSize = sizeHelper;
        
        error = snd_pcm_hw_params_set_period_time_near(cDevice->handle, cHwParams, &periodTime_us, &direction);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set period time %i usec: %s\n", periodTime_us, snd_strerror(error));
            return 1;
        }
        error = snd_pcm_hw_params_get_period_size(cHwParams, &sizeHelper, &direction);
        if (error < 0)
        {
            fprintf(stderr, "Unable to get the period size: %s\n", snd_strerror(error));
            return 1;
        }
        cPeriodSize = sizeHelper;
        
        error = snd_pcm_hw_params(cDevice->handle, cHwParams);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set hw params: %s\n", snd_strerror(error));
            return 1;
        }
        
#if 0
        printf("Period time: %i us\n", periodTime_us);
        printf("Buffer time: %i us\n", bufferTime_us);
        printf("Period size: %li\n", pPeriodSize);
        printf("Buffer size: %li\n", pBufferSize);
#endif

        //
        // NOTE(michiel): Software Params
        //
        snd_pcm_sw_params_t *pSwParams;
        snd_pcm_sw_params_alloca(&pSwParams);
        
        error = snd_pcm_sw_params_current(pDevice->handle, pSwParams);
        if (error < 0)
        {
            fprintf(stderr, "Unable to determine current sw params: %s\n", snd_strerror(error));
            return 1;
        }
        
        error = snd_pcm_sw_params_set_start_threshold(pDevice->handle, pSwParams, (pBufferSize / pPeriodSize) * pPeriodSize);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set start threshold: %s\n", snd_strerror(error));
            return 1;
        }
        
        error = snd_pcm_sw_params_set_avail_min(pDevice->handle, pSwParams, pPeriodSize);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set avail min: %s\n", snd_strerror(error));
            return 1;
        }
        
        error = snd_pcm_sw_params(pDevice->handle, pSwParams);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set sw params: %s\n", snd_strerror(error));
            return 1;
        }
        
        snd_pcm_sw_params_t *cSwParams;
        snd_pcm_sw_params_alloca(&cSwParams);
        
        error = snd_pcm_sw_params_current(cDevice->handle, cSwParams);
        if (error < 0)
        {
            fprintf(stderr, "Unable to determine current sw params: %s\n", snd_strerror(error));
            return 1;
        }
        
        error = snd_pcm_sw_params_set_start_threshold(cDevice->handle, cSwParams, (cBufferSize / cPeriodSize) * cPeriodSize);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set start threshold: %s\n", snd_strerror(error));
            return 1;
        }
        
        error = snd_pcm_sw_params_set_avail_min(cDevice->handle, cSwParams, cPeriodSize);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set avail min: %s\n", snd_strerror(error));
            return 1;
        }
        
        error = snd_pcm_sw_params(cDevice->handle, cSwParams);
        if (error < 0)
        {
            fprintf(stderr, "Unable to set sw params: %s\n", snd_strerror(error));
            return 1;
        }
                
        //
        //
        //
        //snd_pcm_dump(pDevice->handle, sndOutput);
        
        int nrBitsPerSample = 32;
        int nrBytesPerPeriod = pPeriodSize * channels * nrBitsPerSample / 8;
        int *samples = (int *)malloc(nrBytesPerPeriod);
        if (samples == NULL)
        {
            fprintf(stderr, "Not enough memory!\n");
        }
        
        snd_pcm_channel_area_t *areas = (snd_pcm_channel_area_t *)malloc(channels * sizeof(snd_pcm_channel_area_t));
        for (int channel = 0; channel < channels; ++channel)
        {
            areas[channel].addr = samples;
            areas[channel].first = channel * nrBitsPerSample;
            areas[channel].step = channels * nrBitsPerSample;
        }
        
        FreqGen gen = create_frequency_generator(400.0, rate);
        unsigned int maxValue = max_value(nrBitsPerSample);
        
        int *ptr;
        int *readPtr;
        int pptr;
        int cptr;
        
        int playTime_s = 3;
        int playTime_us = 1000000 * playTime_s;
        
        int *recordSamples = (int *)malloc(playTime_s * rate * channels * nrBitsPerSample / 8);
        
        int pError;
        int cError;
        
        for (int i = 0; i < (playTime_us / periodTime_us); ++i)
        {
            for (int p = 0; p < pPeriodSize; ++p)
            {
                double s = next_sample(&gen);
                for (int channel = 0; channel < channels; ++channel)
                {
                    //int sample = sample_to_int(s, (maxValue / (channel * channel + 1)));
                    samples[(p * channels) + channel] = sample_to_int(s, ((channel % 2) == 0) ? maxValue : 0);
                }
            }
            
            ptr = samples;
            readPtr = recordSamples;
            pptr = pPeriodSize;
            cptr = cPeriodSize;
            while (pptr > 0)
            {
                pError = snd_pcm_mmap_writei(pDevice->handle, ptr, pptr);
                cError = snd_pcm_mmap_readi(cDevice->handle, readPtr, pptr);
                if (pError == -EAGAIN)
                {
                    // NOTE(michiel): Overrun
                    continue;
                }
                if (error < 0)
                {
                    fprintf(stderr, "Stream recovery!\n");
                    if (error == -EPIPE)
                    {
                        // NOTE(michiel): Underrun
                        error = snd_pcm_prepare(pDevice->handle);
                        if (error < 0)
                        {
                            fprintf(stderr, "Can't recover from underrun, prepare failed: %s\n", snd_strerror(error));
                        }
                    }
                    else if (error == -ESTRPIPE)
                    {
                        error = snd_pcm_resume(pDevice->handle);
                        while (error == -EAGAIN)
                        {
                            fprintf(stderr, "Waiting for recovery\n");
                            sleep(1);
                            error = snd_pcm_resume(pDevice->handle);
                        }
                        if (error < 0)
                        {
                            error = snd_pcm_prepare(pDevice->handle);
                            if (error < 0)
                            {
                                fprintf(stderr, "Can't recover from suspend, prepare failed: %s\n", snd_strerror(error));
                            }
                        }
                    }
                    else 
                    {
                        fprintf(stderr, "Write error: %s\n", snd_strerror(error));
                        return 1;
                    }
                    // xrun_recovery
                    break;
                }
                
                ptr += error * channels;
                readPtr += x * channels;
                pptr -= error;
            }
        }    
    }
    
    //
    // NOTE(michiel): Device Release
    //
    
    destroy_device(cDevice);
    destroy_device(pDevice);
    
    return 0;
}