internal void
alsa_set_error(AlsaDevice *device, s32 errorCode, char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    device->lastError = vstring_fmt(sizeof(device->errorBuffer), device->errorBuffer,
                                    fmt, args);
    va_end(args);
    
    device->lastError = append_string(device->lastError, string(snd_strerror(errorCode)), sizeof(device->errorBuffer));
}

internal b32
alsa_hardware_init(AlsaDevice *device, u32 sampleFrequency, u32 sampleCount, u32 channelCount)
{
    b32 result = false;
    
    s32 error = snd_pcm_hw_params_any(device->pcmHandle, device->hwParams);
    if (error >= 0)
    {
        error = snd_pcm_hw_params_set_rate_resample(device->pcmHandle, device->hwParams, 0);
        if (error >= 0)
        {
            error = snd_pcm_hw_params_set_format(device->pcmHandle, device->hwParams, SND_PCM_FORMAT_S16_LE);
            if (error >= 0)
            {
                error = snd_pcm_hw_params_set_channels(device->pcmHandle, device->hwParams, channelCount);
                if (error >= 0)
                {
                    error = snd_pcm_hw_params_set_rate(device->pcmHandle, device->hwParams, sampleFrequency, 0);
                    if (error >= 0)
                    {
                        u32 totalPeriodCount = 4;
                        u32 totalBufferSize = sampleCount * totalPeriodCount;
                        
                        error = snd_pcm_hw_params_set_buffer_size(device->pcmHandle, device->hwParams, totalBufferSize);
                        if (error >= 0)
                        {
                            error = snd_pcm_hw_params_set_period_size(device->pcmHandle, device->hwParams, sampleCount, 0);
                            if (error >= 0)
                            {
                                result = true;
                            }
                            else
                            {
                                alsa_set_error(device, error, "Set period size failed: ");
                            }
                        }
                        else
                        {
                            alsa_set_error(device, error, "Set buffer size failed: ");
                        }
                    }
                    else
                    {
                        alsa_set_error(device, error, "Set sample rate failed: ");
                    }
                }
                else
                {
                    alsa_set_error(device, error, "Set channels failed: ");
                }
            }
            else
            {
                alsa_set_error(device, error, "Set S16_LE format failed: ");
            }
        }
        else
        {
            alsa_set_error(device, error, "Disable resampler failed: ");
        }
    }
    else
    {
        alsa_set_error(device, error, "Get hw params failed: ");
    }
    
    return result;
}

internal b32
alsa_software_init(AlsaDevice *device, u32 sampleCount)
{
    b32 result = false;
    
    s32 error = snd_pcm_sw_params_current(device->pcmHandle, device->swParams);
    if (error >= 0)
    {
        error = snd_pcm_sw_params_set_start_threshold(device->pcmHandle, device->swParams, sampleCount);
        if (error >= 0)
        {
            error = snd_pcm_sw_params_set_avail_min(device->pcmHandle, device->swParams, sampleCount);
            if (error >= 0)
            {
                result = true;
            }
            else
            {
                alsa_set_error(device, error, "Set minimal available samples failed: ");
            }
        }
        else
        {
            alsa_set_error(device, error, "Set start threshold failed: ");
        }
    }
    else
    {
        alsa_set_error(device, error, "Get sw params failed: ");
    }
    
    return result;
}

internal PLATFORM_SOUND_ERROR_STRING(platform_sound_error_string)
{
    String result = {};
    if (device->platform)
    {
        AlsaDevice *alsaDev = (AlsaDevice *)device->platform;
        result = alsaDev->lastError;
    }
    return result;
}

internal PLATFORM_SOUND_INIT(platform_sound_init)
{
    b32 result = false;
    
    u32 sampleFrequency = device->sampleFrequency ? device->sampleFrequency : 44100;
    u32 sampleCount = device->sampleCount ? device->sampleCount : 1024;
    u32 channelCount = device->channelCount ? device->channelCount : 2;
    
    device->platform = allocate_struct(AlsaDevice);
    
    if (device->platform)
    {
        AlsaDevice *alsaDev = (AlsaDevice *)device->platform;
        s32 error = snd_output_stdio_attach(&alsaDev->stdout, stdout, 0);
        if (error >= 0)
        {
            alsaDev->name = "default";
            error = snd_pcm_hw_params_malloc(&alsaDev->hwParams);
            
            if (error >= 0)
            {
                error = snd_pcm_open(&alsaDev->pcmHandle, alsaDev->name, SND_PCM_STREAM_PLAYBACK, 0);
                if (error >= 0)
                {
                    if (alsa_hardware_init(alsaDev, sampleFrequency, sampleCount, channelCount))
                    {
                        error = snd_pcm_hw_params(alsaDev->pcmHandle, alsaDev->hwParams);
                        if (error >= 0)
                        {
                            error = snd_pcm_sw_params_malloc(&alsaDev->swParams);
                            if (error >= 0)
                            {
                                if (alsa_software_init(alsaDev, sampleCount))
                                {
                                    error = snd_pcm_sw_params(alsaDev->pcmHandle, alsaDev->swParams);
                                    if (error >= 0)
                                    {
                                        result = true;
                                    }
                                    else
                                    {
                                        alsa_set_error(alsaDev, error, "Set sw params failed: ");
                                        snd_pcm_sw_params_free(alsaDev->swParams);
                                        snd_pcm_close(alsaDev->pcmHandle);
                                        snd_pcm_hw_params_free(alsaDev->hwParams);
                                    }
                                }
                                else
                                {
                                    snd_pcm_sw_params_free(alsaDev->swParams);
                                    snd_pcm_close(alsaDev->pcmHandle);
                                    snd_pcm_hw_params_free(alsaDev->hwParams);
                                }
                            }
                            else
                            {
                                alsa_set_error(alsaDev, error, "Allocate sw params failed: ");
                                snd_pcm_close(alsaDev->pcmHandle);
                                snd_pcm_hw_params_free(alsaDev->hwParams);
                            }
                        }
                        else
                        {
                            alsa_set_error(alsaDev, error, "Set hw params failed: ");
                            snd_pcm_close(alsaDev->pcmHandle);
                            snd_pcm_hw_params_free(alsaDev->hwParams);
                        }
                    }
                    else
                    {
                        snd_pcm_close(alsaDev->pcmHandle);
                        snd_pcm_hw_params_free(alsaDev->hwParams);
                    }
                }
                else
                {
                    alsa_set_error(alsaDev, error, "Opening PCM stream failed: ");
                    snd_pcm_hw_params_free(alsaDev->hwParams);
                }
            }
            else
            {
                alsa_set_error(alsaDev, error, "Allocate hw params failed: ");
            }
        }
        else
        {
            alsa_set_error(alsaDev, error, "Attaching stdout failed: ");
        }
        
        if (result)
        {
            alsaDev->sampleBuffer = allocate_array(s16, sampleCount * channelCount);
        }
    }
    
    device->sampleFrequency = sampleFrequency;
    device->sampleCount = sampleCount;
    device->channelCount = channelCount;
    
    return result;
}

internal b32
linux_alsa_write(AlsaDevice *device, u32 sampleCount, s16 *buffer)
{
    b32 result = false;
    
    s32 error = snd_pcm_writei(device->pcmHandle, buffer, sampleCount);
    if (error >= 0)
    {
        result = true;
    }
    else if (error == -EPIPE)
    {
        error = snd_pcm_prepare(device->pcmHandle);
        if (error >= 0)
        {
            error = snd_pcm_writei(device->pcmHandle, buffer, sampleCount);
            if (error >= 0)
            {
                result = true;
            }
            else
            {
                alsa_set_error(device, error, "Still can't write after a prepare: ");
            }
        }
        else
        {
            alsa_set_error(device, error, "PCM prepare failed after underrun: ");
        }
    }
    else if (error == -ESTRPIPE)
    {
        error = snd_pcm_resume(device->pcmHandle);
        if (error >= 0)
        {
            error = snd_pcm_prepare(device->pcmHandle);
            if (error >= 0)
            {
                error = snd_pcm_writei(device->pcmHandle, buffer, sampleCount);
                if (error >= 0)
                {
                    result = true;
                }
                else
                {
                    alsa_set_error(device, error, "Still can't write after a resume: ");
                }
            }
            else
            {
                alsa_set_error(device, error, "PCM prepare failed after a suspend: ");
            }
        }
        else
        {
            alsa_set_error(device, error, "PCM resume failed after a suspend: ");
        }
    }
    else
    {
        alsa_set_error(device, error, "Sound write failed: ");
    }
    
    return result;
}

internal PLATFORM_SOUND_WRITE_S16(platform_sound_write_s16)
{
    b32 result = false;
    
    if (device->platform)
    {
        AlsaDevice *alsaDev = (AlsaDevice *)device->platform;
        result = linux_alsa_write(alsaDev, device->sampleCount, samples);
    }
    
    return result;
}

internal PLATFORM_SOUND_WRITE_S32(platform_sound_write_s32)
{
    b32 result = false;
    
    if (device->platform)
    {
        AlsaDevice *alsaDev = (AlsaDevice *)device->platform;
        
        u32 sampleCount = device->sampleCount * device->channelCount;
        // TODO(michiel): Dither
        for (u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
        {
            alsaDev->sampleBuffer[sampleIdx] = samples[sampleIdx] >> 16;
        }
        
        result = linux_alsa_write(alsaDev, device->sampleCount, alsaDev->sampleBuffer);
    }
    
    return result;
}

internal PLATFORM_SOUND_WRITE_F32(platform_sound_write_f32)
{
    b32 result = false;
    
    if (device->platform)
    {
        AlsaDevice *alsaDev = (AlsaDevice *)device->platform;
        
        u32 sampleCount = device->sampleCount * device->channelCount;
        // TODO(michiel): Dither
        for (u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
        {
            alsaDev->sampleBuffer[sampleIdx] = s16_from_f32_round(S16_MAX * samples[sampleIdx]);
        }
        
        result = linux_alsa_write(alsaDev, device->sampleCount, alsaDev->sampleBuffer);
    }
    
    return result;
}

