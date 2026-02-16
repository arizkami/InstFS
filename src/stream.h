/*
 * Stream API - Memory-mapped streaming interface for InstFS
 * Provides efficient streaming access to instrument data using mmap
 */

#ifndef INSTFS_STREAM_H
#define INSTFS_STREAM_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct InstFS_t InstFS_t;

/* Opaque handle for a stream */
typedef struct InstFS_Stream_t InstFS_Stream_t;

/* Stream modes */
typedef enum {
    STREAM_MODE_SEQUENTIAL,  /* Sequential access, optimized for forward reading */
    STREAM_MODE_RANDOM,      /* Random access, no prefetch optimization */
    STREAM_MODE_WILLNEED     /* Hint that data will be needed soon (madvise) */
} stream_mode_t;

/* Stream statistics */
typedef struct {
    uint64_t total_bytes_read;
    uint64_t num_reads;
    uint64_t num_seeks;
    uint64_t cache_hits;
    uint64_t cache_misses;
} stream_stats_t;

/*
 * Open a stream for an instrument
 * Parameters:
 *   fs    - The InstFS handle
 *   index - Instrument index
 *   mode  - Stream access mode
 * Returns: Stream handle, or NULL on error
 */
InstFS_Stream_t* stream_open(InstFS_t* fs, uint32_t index, stream_mode_t mode);

/*
 * Close a stream and release resources
 * Parameters:
 *   stream - The stream handle
 */
void stream_close(InstFS_Stream_t* stream);

/*
 * Read data from stream at current position
 * Parameters:
 *   stream - The stream handle
 *   buffer - Destination buffer
 *   size   - Number of bytes to read
 * Returns: Number of bytes read, or -1 on error
 */
int64_t stream_read(InstFS_Stream_t* stream, void* buffer, size_t size);

/*
 * Seek to a position in the stream
 * Parameters:
 *   stream - The stream handle
 *   offset - Offset from whence
 *   whence - SEEK_SET, SEEK_CUR, or SEEK_END
 * Returns: New position, or -1 on error
 */
int64_t stream_seek(InstFS_Stream_t* stream, int64_t offset, int whence);

/*
 * Get current position in stream
 * Parameters:
 *   stream - The stream handle
 * Returns: Current position, or -1 on error
 */
int64_t stream_tell(InstFS_Stream_t* stream);

/*
 * Get the size of the stream
 * Parameters:
 *   stream - The stream handle
 * Returns: Stream size in bytes
 */
uint64_t stream_size(InstFS_Stream_t* stream);

/*
 * Check if at end of stream
 * Parameters:
 *   stream - The stream handle
 * Returns: 1 if at end, 0 otherwise
 */
int stream_eof(InstFS_Stream_t* stream);

/*
 * Get direct pointer to data at current position (zero-copy)
 * Parameters:
 *   stream    - The stream handle
 *   available - Pointer to receive number of bytes available (can be NULL)
 * Returns: Pointer to data, or NULL if not available
 * Note: Pointer is valid until next stream operation
 */
const void* stream_get_ptr(InstFS_Stream_t* stream, size_t* available);

/*
 * Advise the kernel about access pattern
 * Parameters:
 *   stream - The stream handle
 *   offset - Offset in stream
 *   length - Length of region
 *   advice - POSIX_MADV_* constant
 * Returns: 0 on success, -1 on error
 */
int stream_advise(InstFS_Stream_t* stream, uint64_t offset, size_t length, int advice);

/*
 * Prefetch data into cache
 * Parameters:
 *   stream - The stream handle
 *   offset - Offset to prefetch from
 *   length - Number of bytes to prefetch
 * Returns: 0 on success, -1 on error
 */
int stream_prefetch(InstFS_Stream_t* stream, uint64_t offset, size_t length);

/*
 * Get stream statistics
 * Parameters:
 *   stream - The stream handle
 *   stats  - Pointer to receive statistics
 * Returns: 0 on success, -1 on error
 */
int stream_get_stats(InstFS_Stream_t* stream, stream_stats_t* stats);

/*
 * Reset stream statistics
 * Parameters:
 *   stream - The stream handle
 */
void stream_reset_stats(InstFS_Stream_t* stream);

/*
 * Read samples in a specific format (convenience function)
 * Parameters:
 *   stream       - The stream handle
 *   buffer       - Destination buffer
 *   num_samples  - Number of samples to read
 *   sample_size  - Size of each sample in bytes
 * Returns: Number of samples read, or -1 on error
 */
int64_t stream_read_samples(InstFS_Stream_t* stream, void* buffer, 
                            size_t num_samples, size_t sample_size);

#ifdef __cplusplus
}
#endif

#endif /* INSTFS_STREAM_H */
