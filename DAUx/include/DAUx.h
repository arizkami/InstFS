/*
 * DAUx - Direct Audio Engine
 * Cross-platform low-latency audio interface
 * 
 * Linux: ALSA-based
 * Windows: Kernel Streaming (WASAPI)
 * BSD: OSS (Open Sound System)
 */

#ifndef DAUX_H
#define DAUX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================
 * Audio Format Definitions
 * ======================================== */

typedef enum {
    DAUX_FORMAT_S16LE = 0,    /* Signed 16-bit little-endian */
    DAUX_FORMAT_S24LE,        /* Signed 24-bit little-endian */
    DAUX_FORMAT_S32LE,        /* Signed 32-bit little-endian */
    DAUX_FORMAT_F32LE,        /* Float 32-bit little-endian */
    DAUX_FORMAT_F64LE         /* Float 64-bit little-endian */
} daux_format_t;

typedef enum {
    DAUX_MODE_PLAYBACK = 0,
    DAUX_MODE_CAPTURE,
    DAUX_MODE_DUPLEX
} daux_mode_t;

/* ========================================
 * Audio Configuration
 * ======================================== */

typedef struct {
    uint32_t sample_rate;     /* Sample rate in Hz (e.g., 44100, 48000) */
    uint16_t channels;        /* Number of channels (1=mono, 2=stereo) */
    daux_format_t format;     /* Sample format */
    uint32_t buffer_frames;   /* Buffer size in frames */
    uint32_t period_frames;   /* Period size in frames (for low latency) */
    daux_mode_t mode;         /* Playback, capture, or duplex */
} daux_config_t;

/* ========================================
 * Audio Device Handle
 * ======================================== */

typedef struct daux_device_t daux_device_t;

/* ========================================
 * Audio Callback
 * ======================================== */

typedef int (*daux_callback_t)(
    void* userdata,
    void* output_buffer,      /* Output buffer (playback) */
    void* input_buffer,       /* Input buffer (capture) */
    uint32_t frames           /* Number of frames to process */
);

/* ========================================
 * Device Management
 * ======================================== */

/**
 * Initialize DAUx library
 * Returns 0 on success, negative on error
 */
int daux_init(void);

/**
 * Shutdown DAUx library
 */
void daux_shutdown(void);

/**
 * Get default audio configuration
 */
daux_config_t daux_default_config(void);

/**
 * Open audio device
 * 
 * @param device_name Device name (NULL for default device)
 * @param config Audio configuration
 * @param callback Audio processing callback
 * @param userdata User data passed to callback
 * @return Device handle or NULL on error
 */
daux_device_t* daux_open(
    const char* device_name,
    const daux_config_t* config,
    daux_callback_t callback,
    void* userdata
);

/**
 * Close audio device
 */
void daux_close(daux_device_t* device);

/**
 * Start audio stream
 * Returns 0 on success, negative on error
 */
int daux_start(daux_device_t* device);

/**
 * Stop audio stream
 * Returns 0 on success, negative on error
 */
int daux_stop(daux_device_t* device);

/**
 * Check if device is running
 * Returns 1 if running, 0 if stopped
 */
int daux_is_running(daux_device_t* device);

/* ========================================
 * Device Information
 * ======================================== */

/**
 * Get actual sample rate (may differ from requested)
 */
uint32_t daux_get_sample_rate(daux_device_t* device);

/**
 * Get actual buffer size in frames
 */
uint32_t daux_get_buffer_frames(daux_device_t* device);

/**
 * Get current latency in microseconds
 */
uint64_t daux_get_latency_us(daux_device_t* device);

/**
 * Get number of frames processed
 */
uint64_t daux_get_frames_processed(daux_device_t* device);

/**
 * Get number of buffer underruns/overruns
 */
uint32_t daux_get_xruns(daux_device_t* device);

/* ========================================
 * Utility Functions
 * ======================================== */

/**
 * Get bytes per sample for a given format
 */
size_t daux_format_bytes(daux_format_t format);

/**
 * Get format name as string
 */
const char* daux_format_name(daux_format_t format);

/**
 * Get last error message
 */
const char* daux_get_error(void);

/**
 * List available audio devices
 * Returns number of devices found
 */
int daux_list_devices(void (*callback)(int index, const char* name, void* userdata), void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* DAUX_H */
