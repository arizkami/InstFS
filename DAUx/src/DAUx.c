/*
 * DAUx - Direct Audio Engine
 * Core implementation
 */

#include "DAUx.h"
#include "stream.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Global error message */
static char g_error_msg[256] = {0};

/* ========================================
 * Error Handling
 * ======================================== */

static void daux_set_error(const char* msg) {
    strncpy(g_error_msg, msg, sizeof(g_error_msg) - 1);
    g_error_msg[sizeof(g_error_msg) - 1] = '\0';
}

const char* daux_get_error(void) {
    return g_error_msg;
}

/* ========================================
 * Library Initialization
 * ======================================== */

int daux_init(void) {
    daux_set_error("");
    return 0;
}

void daux_shutdown(void) {
    /* Cleanup if needed */
}

/* ========================================
 * Configuration
 * ======================================== */

daux_config_t daux_default_config(void) {
    daux_config_t config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.format = DAUX_FORMAT_F32LE;
    config.buffer_frames = 512;
    config.period_frames = 128;
    config.mode = DAUX_MODE_PLAYBACK;
    return config;
}

/* ========================================
 * Device Management
 * ======================================== */

daux_device_t* daux_open(
    const char* device_name,
    const daux_config_t* config,
    daux_callback_t callback,
    void* userdata
) {
    if (!config || !callback) {
        daux_set_error("Invalid parameters");
        return NULL;
    }
    
    daux_device_t* device = (daux_device_t*)calloc(1, sizeof(daux_device_t));
    if (!device) {
        daux_set_error("Memory allocation failed");
        return NULL;
    }
    
    device->config = *config;
    device->callback = callback;
    device->userdata = userdata;
    device->running = 0;
    device->frames_processed = 0;
    device->xruns = 0;
    
    if (daux_stream_open(device, device_name) != 0) {
        free(device);
        return NULL;
    }
    
    return device;
}

void daux_close(daux_device_t* device) {
    if (!device) return;
    
    if (device->running) {
        daux_stop(device);
    }
    
    daux_stream_close(device);
    free(device);
}

int daux_start(daux_device_t* device) {
    if (!device) {
        daux_set_error("Invalid device");
        return -1;
    }
    
    if (device->running) {
        return 0; /* Already running */
    }
    
    return daux_stream_start(device);
}

int daux_stop(daux_device_t* device) {
    if (!device) {
        daux_set_error("Invalid device");
        return -1;
    }
    
    if (!device->running) {
        return 0; /* Already stopped */
    }
    
    return daux_stream_stop(device);
}

int daux_is_running(daux_device_t* device) {
    return device ? device->running : 0;
}

/* ========================================
 * Device Information
 * ======================================== */

uint32_t daux_get_sample_rate(daux_device_t* device) {
    return device ? device->config.sample_rate : 0;
}

uint32_t daux_get_buffer_frames(daux_device_t* device) {
    return device ? device->config.buffer_frames : 0;
}

uint64_t daux_get_latency_us(daux_device_t* device) {
    if (!device) return 0;
    
    /* Latency = buffer_frames / sample_rate * 1000000 */
    return ((uint64_t)device->config.buffer_frames * 1000000ULL) / device->config.sample_rate;
}

uint64_t daux_get_frames_processed(daux_device_t* device) {
    return device ? device->frames_processed : 0;
}

uint32_t daux_get_xruns(daux_device_t* device) {
    return device ? device->xruns : 0;
}

/* ========================================
 * Utility Functions
 * ======================================== */

size_t daux_format_bytes(daux_format_t format) {
    return daux_format_size(format);
}

const char* daux_format_name(daux_format_t format) {
    switch (format) {
        case DAUX_FORMAT_S16LE: return "S16LE";
        case DAUX_FORMAT_S24LE: return "S24LE";
        case DAUX_FORMAT_S32LE: return "S32LE";
        case DAUX_FORMAT_F32LE: return "F32LE";
        case DAUX_FORMAT_F64LE: return "F64LE";
        default: return "Unknown";
    }
}
