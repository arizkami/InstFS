/*
 * DAUx Stream - Internal audio streaming interface
 * Platform-specific implementations
 */

#ifndef DAUX_STREAM_H
#define DAUX_STREAM_H

#include "DAUx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
    #define DAUX_PLATFORM_WINDOWS 1
    #include <windows.h>
    #include <audioclient.h>
    #include <mmdeviceapi.h>
#elif defined(__linux__)
    #define DAUX_PLATFORM_LINUX 1
    #include <alsa/asoundlib.h>
    #include <pthread.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #define DAUX_PLATFORM_BSD 1
    #include <sys/soundcard.h>
    #include <pthread.h>
#else
    #error "Unsupported platform"
#endif

/* ========================================
 * Platform-Specific Device Structure
 * ======================================== */

struct daux_device_t {
    daux_config_t config;
    daux_callback_t callback;
    void* userdata;
    
    int running;
    uint64_t frames_processed;
    uint32_t xruns;
    
#if DAUX_PLATFORM_WINDOWS
    /* Windows WASAPI */
    IMMDeviceEnumerator* enumerator;
    IMMDevice* device;
    IAudioClient* audio_client;
    IAudioRenderClient* render_client;
    IAudioCaptureClient* capture_client;
    HANDLE event_handle;
    HANDLE thread_handle;
    DWORD thread_id;
    volatile int stop_requested;
    
#elif DAUX_PLATFORM_LINUX
    /* Linux ALSA */
    snd_pcm_t* pcm_handle;
    pthread_t thread;
    volatile int stop_requested;
    
#elif DAUX_PLATFORM_BSD
    /* BSD OSS */
    int fd;
    pthread_t thread;
    volatile int stop_requested;
#endif
};

/* ========================================
 * Internal Stream Functions
 * ======================================== */

/**
 * Platform-specific device open
 */
int daux_stream_open(daux_device_t* device, const char* device_name);

/**
 * Platform-specific device close
 */
void daux_stream_close(daux_device_t* device);

/**
 * Platform-specific stream start
 */
int daux_stream_start(daux_device_t* device);

/**
 * Platform-specific stream stop
 */
int daux_stream_stop(daux_device_t* device);

/**
 * Get format size in bytes
 */
static inline size_t daux_format_size(daux_format_t format) {
    switch (format) {
        case DAUX_FORMAT_S16LE: return 2;
        case DAUX_FORMAT_S24LE: return 3;
        case DAUX_FORMAT_S32LE: return 4;
        case DAUX_FORMAT_F32LE: return 4;
        case DAUX_FORMAT_F64LE: return 8;
        default: return 0;
    }
}

/**
 * Calculate buffer size in bytes
 */
static inline size_t daux_buffer_bytes(const daux_config_t* config) {
    return config->buffer_frames * config->channels * daux_format_size(config->format);
}

#ifdef __cplusplus
}
#endif

#endif /* DAUX_STREAM_H */
