/*
 * InstFS - Instrument File System API
 * Header file for accessing the InstFS partition within an OSMP container
 */

#ifndef INSTFS_H
#define INSTFS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle for an InstFS instance */
typedef struct InstFS_t InstFS_t;

/*
 * Master header for an OSMP container file.
 * This can be used to inspect the container layout.
 */
typedef struct {
    char     magic[8];        /* Should be "OSMP_IMG" */
    uint32_t version;
    uint64_t meta_offset;
    uint64_t meta_size;
    uint64_t instfs_offset;
    uint64_t instfs_size;
    uint64_t reserved[4];
} osmp_master_header_t;


/*
 * Mount an InstFS partition from an .osmp container file
 * Parameters:
 *   filepath - Path to the .osmp image file
 *   master_header_out - Optional pointer to receive a copy of the master header
 * Returns: A handle to the InstFS instance, or NULL on error
 */
InstFS_t* instfs_mount_osmp(const char* filepath, osmp_master_header_t* master_header_out);

/*
 * Mount an InstFS image from a memory buffer (for testing or embedded data)
 * Parameters:
 *   data - Pointer to the memory buffer containing a raw InstFS image
 *   size - Size of the memory buffer
 * Returns: A handle to the InstFS instance, or NULL on error
 */
InstFS_t* instfs_mount_mem(const void* data, size_t size);

/*
 * Unmount an InstFS image and release resources
 * Parameters:
 *   fs - The InstFS handle
 */
void instfs_unmount(InstFS_t* fs);

/*
 * Get the number of instruments in the filesystem
 * Parameters:
 *   fs - The InstFS handle
 * Returns: Number of instruments
 */
uint32_t instfs_get_count(InstFS_t* fs);

/*
 * Get instrument name by index
 * Parameters:
 *   fs    - The InstFS handle
 *   index - Instrument index (0 to count-1)
 * Returns: Instrument name string, or NULL if invalid
 */
const char* instfs_get_name(InstFS_t* fs, uint32_t index);

/*
 * Find instrument by name
 * Parameters:
 *   fs   - The InstFS handle
 *   name - Instrument name to search for
 * Returns: Instrument index, or -1 if not found
 */
int instfs_find(InstFS_t* fs, const char *name);

/*
 * Get instrument data pointer
 * Parameters:
 *   fs    - The InstFS handle
 *   index - Instrument index
 *   size  - Pointer to receive data size (can be NULL)
 * Returns: Pointer to instrument data, or NULL if invalid
 */
const uint8_t* instfs_get_data(InstFS_t* fs, uint32_t index, uint64_t *size);

/*
 * Get instrument metadata
 * Parameters:
 *   fs          - The InstFS handle
 *   index       - Instrument index
 *   format      - Pointer to receive format (can be NULL)
 *   sample_rate - Pointer to receive sample rate (can be NULL)
 *   channels    - Pointer to receive channel count (can be NULL)
 *   bit_depth   - Pointer to receive bit depth (can be NULL)
 * Returns: 0 on success, -1 on error
 */
int instfs_get_info(InstFS_t* fs, uint32_t index, uint32_t *format, 
                    uint32_t *sample_rate, uint16_t *channels, uint16_t *bit_depth);

/*
 * Read instrument data into buffer
 * Parameters:
 *   fs     - The InstFS handle
 *   index  - Instrument index
 *   buffer - Destination buffer
 *   offset - Offset in instrument data
 *   size   - Number of bytes to read
 * Returns: Number of bytes read, or negative on error
 */
int64_t instfs_read(InstFS_t* fs, uint32_t index, uint8_t *buffer, 
                    uint64_t offset, uint64_t size);

/*
 * List all instruments
 * Parameters:
 *   fs       - The InstFS handle
 *   callback - Function to call for each instrument
 *              (index, name, size)
 */
void instfs_list(InstFS_t* fs, void (*callback)(uint32_t index, const char *name, uint64_t size));

/*
 * Get filesystem statistics
 * Parameters:
 *   fs              - The InstFS handle
 *   total_size      - Pointer to receive total filesystem size (can be NULL)
 *   num_instruments - Pointer to receive instrument count (can be NULL)
 */
void instfs_stats(InstFS_t* fs, uint64_t *total_size, uint32_t *num_instruments);

#ifdef __cplusplus
}
#endif

#endif /* INSTFS_H */
