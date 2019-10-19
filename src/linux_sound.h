
struct AlsaDevice
{
    char *name;
    snd_pcm_t *pcmHandle;
    snd_pcm_hw_params_t *hwParams;
    snd_pcm_sw_params_t *swParams;
    
    s16 *sampleBuffer;
    
    snd_output_t *stdout;
    
    u8 errorBuffer[256];
    String lastError;
};

