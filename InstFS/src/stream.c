/*
 * Stream API - Memory-mapped streaming interface for InstFS
 * Implementation
 */

#include "stream.h"
#include "instfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "portability.h"
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <errno.h>

/* Concrete definition of stream handle */
struct InstFS_Stream_t {
    InstFS_t* fs;              /* Parent filesystem */
    uint32_t instrument_index; /* Instrument index */
    const uint8_t* data_ptr;   /* Pointer to instrument data */
    uint64_t data_size;        /* Size of instrument data */
    uint64_t position;         /* Current position in stream */
    stream_mode_t mode;        /* Access mode */
    stream_stats_t stats;      /* Statistics */
};

/*
 * Open a stream for an instrument
 */
InstFS_Stream_t* stream_open(InstFS_t* fs, uint32_t index, stream_mode_t mode) {
    if (!fs) return NULL;
    
    InstFS_Stream_t* stream = (InstFS_Stream_t*)calloc(1, sizeof(InstFS_Stream_t));
    if (!stream) return NULL;
    
    stream->fs = fs;
    stream->instrument_index = index;
    stream->mode = mode;
    stream->position = 0;
    
    /* Get pointer to instrument data */
    stream->data_ptr = instfs_get_data(fs, index, &stream->data_size);
    if (!stream->data_ptr) {
        free(stream);
        return NULL;
    }
    
    /* Apply initial madvise hints based on mode */
#ifndef _WIN32
    if (mode == STREAM_MODE_SEQUENTIAL) {
        madvise((void*)stream->data_ptr, stream->data_size, MADV_SEQUENTIAL);
    } else if (mode == STREAM_MODE_RANDOM) {
        madvise((void*)stream->data_ptr, stream->data_size, MADV_RANDOM);
    } else if (mode == STREAM_MODE_WILLNEED) {
        madvise((void*)stream->data_ptr, stream->data_size, MADV_WILLNEED);
    }
#endif
    
    return stream;
}

/*
 * Close a stream
 */
void stream_close(InstFS_Stream_t* stream) {
    if (!stream) return;
    
    /* Advise kernel we're done with this data */
#ifndef _WIN32
    if (stream->data_ptr && stream->data_size > 0) {
        madvise((void*)stream->data_ptr, stream->data_size, MADV_DONTNEED);
    }
#endif
    
    free(stream);
}

/*
 * Read data from stream
 */
int64_t stream_read(InstFS_Stream_t* stream, void* buffer, size_t size) {
    if (!stream || !buffer) return -1;
    
    if (stream->position >= stream->data_size) {
        return 0; /* EOF */
    }
    
    /* Calculate how much we can actually read */
    uint64_t available = stream->data_size - stream->position;
    if (size > available) {
        size = available;
    }
    
    /* Copy data */
    memcpy(buffer, stream->data_ptr + stream->position, size);
    stream->position += size;
    
    /* Update statistics */
    stream->stats.total_bytes_read += size;
    stream->stats.num_reads++;
    
    return (int64_t)size;
}

/*
 * Seek in stream
 */
int64_t stream_seek(InstFS_Stream_t* stream, int64_t offset, int whence) {
    if (!stream) return -1;
    
    int64_t new_pos;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = (int64_t)stream->position + offset;
            break;
        case SEEK_END:
            new_pos = (int64_t)stream->data_size + offset;
            break;
        default:
            return -1;
    }
    
    /* Bounds check */
    if (new_pos < 0) {
        new_pos = 0;
    } else if ((uint64_t)new_pos > stream->data_size) {
        new_pos = (int64_t)stream->data_size;
    }
    
    stream->position = (uint64_t)new_pos;
    stream->stats.num_seeks++;
    
    return new_pos;
}

/*
 * Get current position
 */
int64_t stream_tell(InstFS_Stream_t* stream) {
    if (!stream) return -1;
    return (int64_t)stream->position;
}

/*
 * Get stream size
 */
uint64_t stream_size(InstFS_Stream_t* stream) {
    if (!stream) return 0;
    return stream->data_size;
}

/*
 * Check if at end of stream
 */
int stream_eof(InstFS_Stream_t* stream) {
    if (!stream) return 1;
    return stream->position >= stream->data_size;
}

/*
 * Get direct pointer to data (zero-copy)
 */
const void* stream_get_ptr(InstFS_Stream_t* stream, size_t* available) {
    if (!stream) {
        if (available) *available = 0;
        return NULL;
    }
    
    if (stream->position >= stream->data_size) {
        if (available) *available = 0;
        return NULL;
    }
    
    if (available) {
        *available = (size_t)(stream->data_size - stream->position);
    }
    
    stream->stats.cache_hits++;
    return stream->data_ptr + stream->position;
}

#ifndef _WIN32
/*
 * Advise kernel about access pattern
 */
int stream_advise(InstFS_Stream_t* stream, uint64_t offset, size_t length, int advice) {
    if (!stream) return -1;
    
    if (offset >= stream->data_size) return -1;
    
    /* Adjust length if it exceeds data size */
    if (offset + length > stream->data_size) {
        length = stream->data_size - offset;
    }
    
    return madvise((void*)(stream->data_ptr + offset), length, advice);
}

/*
 * Prefetch data into cache
 */
int stream_prefetch(InstFS_Stream_t* stream, uint64_t offset, size_t length) {
    return stream_advise(stream, offset, length, MADV_WILLNEED);
}
#else // _WIN32
/*
 * Advise kernel about access pattern (Windows dummy)
 */
int stream_advise(InstFS_Stream_t* stream, uint64_t offset, size_t length, int advice) {
    (void)stream;
    (void)offset;
    (void)length;
    (void)advice;
    return -1; // Not supported on Windows
}

/*
 * Prefetch data into cache (Windows dummy)
 */
int stream_prefetch(InstFS_Stream_t* stream, uint64_t offset, size_t length) {
    (void)stream;
    (void)offset;
    (void)length;
    return -1; // Not supported on Windows
}
#endif // _WIN32

/*
 * Get stream statistics
 */
int stream_get_stats(InstFS_Stream_t* stream, stream_stats_t* stats) {
    if (!stream || !stats) return -1;
    
    *stats = stream->stats;
    return 0;
}

/*
 * Reset stream statistics
 */
void stream_reset_stats(InstFS_Stream_t* stream) {
    if (!stream) return;
    memset(&stream->stats, 0, sizeof(stream_stats_t));
}

/*
 * Read samples in a specific format
 */
int64_t stream_read_samples(InstFS_Stream_t* stream, void* buffer, 
                            size_t num_samples, size_t sample_size) {
    if (!stream || !buffer || sample_size == 0) return -1;
    
    size_t bytes_to_read = num_samples * sample_size;
    int64_t bytes_read = stream_read(stream, buffer, bytes_to_read);
    
    if (bytes_read < 0) return -1;
    
    return bytes_read / sample_size;
}
