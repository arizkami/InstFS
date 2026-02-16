/*
 * DAUx Stream - Platform-specific audio streaming implementation
 */

#include "stream.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* External error setter */
extern void daux_set_error(const char* msg);

/* ========================================
 * Linux ALSA Implementation
 * ======================================== */

#if DAUX_PLATFORM_LINUX

static void* alsa_thread_func(void* arg) {
    daux_device_t* device = (daux_device_t*)arg;
    snd_pcm_t* pcm = device->pcm_handle;
    
    size_t frame_bytes = device->config.channels * daux_format_size(device->config.format);
    size_t buffer_bytes = device->config.period_frames * frame_bytes;
    void* buffer = malloc(buffer_bytes);
    
    if (!buffer) {
        return NULL;
    }
    
    device->running = 1;
    
    while (!device->stop_requested) {
        /* Call user callback */
        int result = device->callback(device->userdata, buffer, NULL, device->config.period_frames);
        if (result != 0) break;
        
        /* Write to ALSA */
        snd_pcm_sframes_t frames = snd_pcm_writei(pcm, buffer, device->config.period_frames);
        
        if (frames < 0) {
            frames = snd_pcm_recover(pcm, frames, 0);
            if (frames < 0) {
                device->xruns++;
            }
        }
        
        if (frames > 0) {
            device->frames_processed += frames;
        }
    }
    
    free(buffer);
    device->running = 0;
    return NULL;
}

int daux_stream_open(daux_device_t* device, const char* device_name) {
    const char* pcm_name = device_name ? device_name : "default";
    int err;
    
    /* Open PCM device */
    err = snd_pcm_open(&device->pcm_handle, pcm_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        daux_set_error(snd_strerror(err));
        return -1;
    }
    
    /* Set hardware parameters */
    snd_pcm_hw_params_t* hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(device->pcm_handle, hw_params);
    
    snd_pcm_hw_params_set_access(device->pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    
    /* Set format */
    snd_pcm_format_t alsa_format;
    switch (device->config.format) {
        case DAUX_FORMAT_S16LE: alsa_format = SND_PCM_FORMAT_S16_LE; break;
        case DAUX_FORMAT_S24LE: alsa_format = SND_PCM_FORMAT_S24_LE; break;
        case DAUX_FORMAT_S32LE: alsa_format = SND_PCM_FORMAT_S32_LE; break;
        case DAUX_FORMAT_F32LE: alsa_format = SND_PCM_FORMAT_FLOAT_LE; break;
        default: alsa_format = SND_PCM_FORMAT_FLOAT_LE; break;
    }
    snd_pcm_hw_params_set_format(device->pcm_handle, hw_params, alsa_format);
    
    snd_pcm_hw_params_set_channels(device->pcm_handle, hw_params, device->config.channels);
    snd_pcm_hw_params_set_rate_near(device->pcm_handle, hw_params, &device->config.sample_rate, 0);
    snd_pcm_hw_params_set_buffer_size_near(device->pcm_handle, hw_params, (snd_pcm_uframes_t*)&device->config.buffer_frames);
    snd_pcm_hw_params_set_period_size_near(device->pcm_handle, hw_params, (snd_pcm_uframes_t*)&device->config.period_frames, 0);
    
    err = snd_pcm_hw_params(device->pcm_handle, hw_params);
    if (err < 0) {
        daux_set_error(snd_strerror(err));
        snd_pcm_close(device->pcm_handle);
        return -1;
    }
    
    /* Prepare device */
    snd_pcm_prepare(device->pcm_handle);
    
    return 0;
}

void daux_stream_close(daux_device_t* device) {
    if (device->pcm_handle) {
        snd_pcm_close(device->pcm_handle);
        device->pcm_handle = NULL;
    }
}

int daux_stream_start(daux_device_t* device) {
    device->stop_requested = 0;
    
    if (pthread_create(&device->thread, NULL, alsa_thread_func, device) != 0) {
        daux_set_error("Failed to create audio thread");
        return -1;
    }
    
    return 0;
}

int daux_stream_stop(daux_device_t* device) {
    device->stop_requested = 1;
    pthread_join(device->thread, NULL);
    snd_pcm_drop(device->pcm_handle);
    return 0;
}

int daux_list_devices(void (*callback)(int index, const char* name, void* userdata), void* userdata) {
    if (!callback) return 0;
    
    callback(0, "default", userdata);
    callback(1, "hw:0,0", userdata);
    
    return 2;
}

/* ========================================
 * Windows WASAPI Implementation
 * ======================================== */

#elif DAUX_PLATFORM_WINDOWS

static DWORD WINAPI wasapi_thread_func(LPVOID arg) {
    daux_device_t* device = (daux_device_t*)arg;
    HRESULT hr;
    
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    
    UINT32 buffer_frame_count;
    IAudioClient_GetBufferSize(device->audio_client, &buffer_frame_count);
    
    size_t frame_bytes = device->config.channels * daux_format_size(device->config.format);
    size_t buffer_bytes = buffer_frame_count * frame_bytes;
    void* temp_buffer = malloc(buffer_bytes);
    
    if (!temp_buffer) {
        CoUninitialize();
        return 1;
    }
    
    IAudioClient_Start(device->audio_client);
    device->running = 1;
    
    while (!device->stop_requested) {
        WaitForSingleObject(device->event_handle, 2000);
        
        if (device->stop_requested) break;
        
        UINT32 padding;
        IAudioClient_GetCurrentPadding(device->audio_client, &padding);
        UINT32 frames_available = buffer_frame_count - padding;
        
        if (frames_available > 0) {
            BYTE* data;
            hr = IAudioRenderClient_GetBuffer(device->render_client, frames_available, &data);
            
            if (SUCCEEDED(hr)) {
                /* Call user callback */
                device->callback(device->userdata, temp_buffer, NULL, frames_available);
                memcpy(data, temp_buffer, frames_available * frame_bytes);
                
                IAudioRenderClient_ReleaseBuffer(device->render_client, frames_available, 0);
                device->frames_processed += frames_available;
            }
        }
    }
    
    IAudioClient_Stop(device->audio_client);
    free(temp_buffer);
    
    device->running = 0;
    CoUninitialize();
    return 0;
}

int daux_stream_open(daux_device_t* device, const char* device_name) {
    HRESULT hr;
    
    (void)device_name; /* Use default device for now */
    
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    
    /* Create device enumerator */
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                         &IID_IMMDeviceEnumerator, (void**)&device->enumerator);
    if (FAILED(hr)) {
        daux_set_error("Failed to create device enumerator");
        return -1;
    }
    
    /* Get default audio endpoint */
    hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(device->enumerator, eRender, eConsole, &device->device);
    if (FAILED(hr)) {
        daux_set_error("Failed to get default audio device");
        IMMDeviceEnumerator_Release(device->enumerator);
        return -1;
    }
    
    /* Activate audio client */
    hr = IMMDevice_Activate(device->device, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&device->audio_client);
    if (FAILED(hr)) {
        daux_set_error("Failed to activate audio client");
        IMMDevice_Release(device->device);
        IMMDeviceEnumerator_Release(device->enumerator);
        return -1;
    }
    
    /* Set up wave format */
    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx.nChannels = device->config.channels;
    wfx.nSamplesPerSec = device->config.sample_rate;
    wfx.wBitsPerSample = 32;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    
    /* Initialize audio client */
    REFERENCE_TIME buffer_duration = (REFERENCE_TIME)((double)device->config.buffer_frames / device->config.sample_rate * 10000000.0);
    
    device->event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    
    hr = IAudioClient_Initialize(device->audio_client, AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                 buffer_duration, 0, &wfx, NULL);
    if (FAILED(hr)) {
        daux_set_error("Failed to initialize audio client");
        CloseHandle(device->event_handle);
        IAudioClient_Release(device->audio_client);
        IMMDevice_Release(device->device);
        IMMDeviceEnumerator_Release(device->enumerator);
        return -1;
    }
    
    IAudioClient_SetEventHandle(device->audio_client, device->event_handle);
    
    /* Get render client */
    hr = IAudioClient_GetService(device->audio_client, &IID_IAudioRenderClient, (void**)&device->render_client);
    if (FAILED(hr)) {
        daux_set_error("Failed to get render client");
        CloseHandle(device->event_handle);
        IAudioClient_Release(device->audio_client);
        IMMDevice_Release(device->device);
        IMMDeviceEnumerator_Release(device->enumerator);
        return -1;
    }
    
    return 0;
}

void daux_stream_close(daux_device_t* device) {
    if (device->render_client) {
        IAudioRenderClient_Release(device->render_client);
        device->render_client = NULL;
    }
    if (device->audio_client) {
        IAudioClient_Release(device->audio_client);
        device->audio_client = NULL;
    }
    if (device->device) {
        IMMDevice_Release(device->device);
        device->device = NULL;
    }
    if (device->enumerator) {
        IMMDeviceEnumerator_Release(device->enumerator);
        device->enumerator = NULL;
    }
    if (device->event_handle) {
        CloseHandle(device->event_handle);
        device->event_handle = NULL;
    }
    
    CoUninitialize();
}

int daux_stream_start(daux_device_t* device) {
    device->stop_requested = 0;
    device->thread_handle = CreateThread(NULL, 0, wasapi_thread_func, device, 0, &device->thread_id);
    
    if (!device->thread_handle) {
        daux_set_error("Failed to create audio thread");
        return -1;
    }
    
    return 0;
}

int daux_stream_stop(daux_device_t* device) {
    device->stop_requested = 1;
    SetEvent(device->event_handle);
    WaitForSingleObject(device->thread_handle, INFINITE);
    CloseHandle(device->thread_handle);
    device->thread_handle = NULL;
    return 0;
}

int daux_list_devices(void (*callback)(int index, const char* name, void* userdata), void* userdata) {
    if (!callback) return 0;
    callback(0, "Default Audio Device", userdata);
    return 1;
}

/* ========================================
 * BSD OSS Implementation
 * ======================================== */

#elif DAUX_PLATFORM_BSD

static void* oss_thread_func(void* arg) {
    daux_device_t* device = (daux_device_t*)arg;
    
    size_t frame_bytes = device->config.channels * daux_format_size(device->config.format);
    size_t buffer_bytes = device->config.period_frames * frame_bytes;
    void* buffer = malloc(buffer_bytes);
    
    if (!buffer) {
        return NULL;
    }
    
    device->running = 1;
    
    while (!device->stop_requested) {
        /* Call user callback */
        int result = device->callback(device->userdata, buffer, NULL, device->config.period_frames);
        if (result != 0) break;
        
        /* Write to OSS */
        ssize_t written = write(device->fd, buffer, buffer_bytes);
        
        if (written > 0) {
            device->frames_processed += written / frame_bytes;
        } else {
            device->xruns++;
        }
    }
    
    free(buffer);
    device->running = 0;
    return NULL;
}

int daux_stream_open(daux_device_t* device, const char* device_name) {
    const char* dsp_name = device_name ? device_name : "/dev/dsp";
    
    device->fd = open(dsp_name, O_WRONLY);
    if (device->fd < 0) {
        daux_set_error("Failed to open OSS device");
        return -1;
    }
    
    /* Set format */
    int format = AFMT_S16_LE;
    switch (device->config.format) {
        case DAUX_FORMAT_S16LE: format = AFMT_S16_LE; break;
        case DAUX_FORMAT_S32LE: format = AFMT_S32_LE; break;
        default: format = AFMT_S16_LE; break;
    }
    
    if (ioctl(device->fd, SNDCTL_DSP_SETFMT, &format) < 0) {
        daux_set_error("Failed to set format");
        close(device->fd);
        return -1;
    }
    
    /* Set channels */
    int channels = device->config.channels;
    if (ioctl(device->fd, SNDCTL_DSP_CHANNELS, &channels) < 0) {
        daux_set_error("Failed to set channels");
        close(device->fd);
        return -1;
    }
    
    /* Set sample rate */
    int rate = device->config.sample_rate;
    if (ioctl(device->fd, SNDCTL_DSP_SPEED, &rate) < 0) {
        daux_set_error("Failed to set sample rate");
        close(device->fd);
        return -1;
    }
    
    return 0;
}

void daux_stream_close(daux_device_t* device) {
    if (device->fd >= 0) {
        close(device->fd);
        device->fd = -1;
    }
}

int daux_stream_start(daux_device_t* device) {
    device->stop_requested = 0;
    
    if (pthread_create(&device->thread, NULL, oss_thread_func, device) != 0) {
        daux_set_error("Failed to create audio thread");
        return -1;
    }
    
    return 0;
}

int daux_stream_stop(daux_device_t* device) {
    device->stop_requested = 1;
    pthread_join(device->thread, NULL);
    ioctl(device->fd, SNDCTL_DSP_RESET, 0);
    return 0;
}

int daux_list_devices(void (*callback)(int index, const char* name, void* userdata), void* userdata) {
    if (!callback) return 0;
    callback(0, "/dev/dsp", userdata);
    callback(1, "/dev/dsp0", userdata);
    return 2;
}

#endif
