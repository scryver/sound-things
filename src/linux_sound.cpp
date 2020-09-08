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
alsa_hardware_init(AlsaDevice *device, u32 sampleFrequency, u32 sampleCount, u32 channelCount, SoundFormat format)
{
    b32 result = false;
    
    s32 error = snd_pcm_hw_params_any(device->pcmHandle, device->hwParams);
    if (error >= 0)
    {
        error = snd_pcm_hw_params_set_rate_resample(device->pcmHandle, device->hwParams, 0);
        if (error >= 0)
        {
            error = snd_pcm_hw_params_set_access(device->pcmHandle, device->hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
            if (error >= 0)
            {
                switch (format)
                {
                    case SoundFormat_s8:  { device->format = SND_PCM_FORMAT_S8; } break;
                    case SoundFormat_s16: { device->format = SND_PCM_FORMAT_S16_LE; } break;
                    case SoundFormat_s24: { device->format = SND_PCM_FORMAT_S24_3LE; } break;
                    case SoundFormat_s32: { device->format = SND_PCM_FORMAT_S32_LE; } break;
                    case SoundFormat_s64: { device->format = SND_PCM_FORMAT_S32_LE; } break;
                    case SoundFormat_f32: { device->format = SND_PCM_FORMAT_FLOAT_LE; } break;
                    case SoundFormat_f64: { device->format = SND_PCM_FORMAT_FLOAT64_LE; } break;
                    INVALID_DEFAULT_CASE;
                }
                
                error = snd_pcm_hw_params_set_format(device->pcmHandle, device->hwParams, device->format);
                if (error >= 0)
                {
                    error = snd_pcm_hw_params_set_channels(device->pcmHandle, device->hwParams, channelCount);
                    if (error >= 0)
                    {
                        error = snd_pcm_hw_params_set_rate(device->pcmHandle, device->hwParams, sampleFrequency, 0);
                        if (error >= 0)
                        {
                            u32 totalPeriodCount = SOUND_PERIOND_COUNT;
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
                    alsa_set_error(device, error, "Set (%u) format failed: ", device->format);
                }
            }
            else
            {
                alsa_set_error(device, error, "Could not set access to interleaved");
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
        error = snd_pcm_sw_params_set_start_threshold(device->pcmHandle, device->swParams, 2*sampleCount);
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

internal b32
linux_alsa_write(AlsaDevice *device, u32 sampleCount, void *buffer)
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

//
// NOTE(michiel): Platform calls
//

internal PLATFORM_SOUND_ERROR_STRING(linux_sound_error_string)
{
    String result = {};
    if (device->platform)
    {
        AlsaDevice *alsaDev = (AlsaDevice *)device->platform;
        result = alsaDev->lastError;
    }
    return result;
}

internal PLATFORM_SOUND_INIT(linux_sound_init)
{
    b32 result = false;
    
    u32 sampleFrequency = device->sampleFrequency ? device->sampleFrequency : 44100;
    u32 sampleCount = device->sampleCount ? device->sampleCount : 1024;
    u32 channelCount = device->channelCount ? device->channelCount : 2;
    SoundFormat format = device->format ? device->format : SoundFormat_s16;
    
    if (!device->platform)
    {
        device->platform = allocate_struct(allocator, AlsaDevice, default_memory_alloc());
    }
    
    AlsaDevice *alsaDev = (AlsaDevice *)device->platform;
    i_expect(alsaDev);
    alsaDev->allocator = allocator;
    
    s32 error = snd_output_stdio_attach(&alsaDev->stdout, stdout, 0);
    if (error >= 0)
    {
        alsaDev->name = SOUND_HW_NAME;
        error = snd_pcm_hw_params_malloc(&alsaDev->hwParams);
        
        if (error >= 0)
        {
            // TODO(michiel): Blocking or non-blocking??
            error = snd_pcm_open(&alsaDev->pcmHandle, alsaDev->name, SND_PCM_STREAM_PLAYBACK, 0);
            if (error >= 0)
            {
                if (alsa_hardware_init(alsaDev, sampleFrequency, sampleCount, channelCount, format))
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
    
    device->sampleFrequency = sampleFrequency;
    device->sampleCount = sampleCount;
    device->channelCount = channelCount;
    device->format = format;
    
    return result;
}

internal PLATFORM_SOUND_REFORMAT(linux_sound_reformat)
{
    i_expect(device->platform);
    
    b32 result = false;
    
    AlsaDevice *alsaDev = (AlsaDevice *)device->platform;
    snd_pcm_drain(alsaDev->pcmHandle);
    
    snd_pcm_sw_params_free(alsaDev->swParams);
    snd_pcm_close(alsaDev->pcmHandle);
    snd_pcm_hw_params_free(alsaDev->hwParams);
    
    usleep(100000);
    
    result = linux_sound_init(alsaDev->allocator, device);
    
    return result;
}

internal PLATFORM_SOUND_WRITE(linux_sound_write)
{
    // NOTE(michiel): Should this be blocking??
    b32 result = false;
    
    if (device->platform)
    {
        AlsaDevice *alsaDev = (AlsaDevice *)device->platform;
        
#if 0        
        s16 *buffer = alsaDev->sampleBuffer;
        switch (device->format)
        {
            case SoundFormat_s16:
            {
                buffer = (s16 *)samples;
            } break;
            
            case SoundFormat_s32:
            {
                u32 sampleCount = device->sampleCount * device->channelCount;
                s32 *source = (s32 *)samples;
                // TODO(michiel): Dither
                for (u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
                {
                    alsaDev->sampleBuffer[sampleIdx] = source[sampleIdx] >> 16;
                }
            } break;
            
            case SoundFormat_f32:
            {
                u32 sampleCount = device->sampleCount * device->channelCount;
                f32 *source = (f32 *)samples;
                // TODO(michiel): Dither
                for (u32 sampleIdx = 0; sampleIdx < sampleCount; ++sampleIdx)
                {
                    alsaDev->sampleBuffer[sampleIdx] = s16_from_f32_round(S16_MAX * source[sampleIdx]);
                }
            } break;
            
            INVALID_DEFAULT_CASE;
        }
#endif
        
#if 0
        u32 framesToWrite = device->sampleCount;
        result = true;
        u8 *source = (u8 *)samples;
        u32 frameSize = device->channelCount;
        switch (device->format)
        {
            case SoundFormat_s8 : {} break;
            case SoundFormat_s16: { frameSize *= 2; } break;
            case SoundFormat_s24: { frameSize *= 3; } break;
            case SoundFormat_s32: { frameSize *= 4; } break;
            case SoundFormat_s64: { frameSize *= 8; } break;
            case SoundFormat_f32: { frameSize *= 4; } break;
            case SoundFormat_f64: { frameSize *= 8; } break;
            INVALID_DEFAULT_CASE;
        }
        while (result && (framesToWrite > 128))
        {
            result = linux_alsa_write(alsaDev, 128, source);
            framesToWrite -= 128;
            source += 128 * frameSize;
        }
        if (result && framesToWrite)
        {
            result = linux_alsa_write(alsaDev, framesToWrite, source);
        }
#else
        result = linux_alsa_write(alsaDev, device->sampleCount, samples);
#endif
    }
    
    return result;
}

