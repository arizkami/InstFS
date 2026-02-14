/*
 * OSMP Metadata API
 * Header file for reading the metadata archive from an OSMP container
 */

#ifndef OSMP_META_H
#define OSMP_META_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle for an OSMP metadata instance */
typedef struct OSMP_Meta_t OSMP_Meta_t;

/*
 * Mount the metadata partition from an .osmp container file
 * Parameters:
 *   filepath - Path to the .osmp image file
 * Returns: A handle to the OSMP metadata instance, or NULL on error
 */
OSMP_Meta_t* osmp_meta_mount(const char* filepath);

/*
 * Unmount an OSMP metadata archive and release resources
 * Parameters:
 *   meta - The OSMP_Meta_t handle
 */
void osmp_meta_unmount(OSMP_Meta_t* meta);

/*
 * Find a file in the metadata archive by name
 * Parameters:
 *   meta     - The OSMP_Meta_t handle
 *   filename - The path of the file to find (e.g., "info/settings.json")
 *   size     - Pointer to receive the file size (can be NULL)
 * Returns: A const pointer to the file's data, or NULL if not found
 */
const uint8_t* osmp_meta_find_file(OSMP_Meta_t* meta, const char* filename, uint64_t* size);

/*
 * Get the number of files in the metadata archive
 * Parameters:
 *   meta - The OSMP_Meta_t handle
 * Returns: The number of files, or 0 on error
 */
uint32_t osmp_meta_get_count(OSMP_Meta_t* meta);

/*
 * Get information about a file in the archive by its index
 * Parameters:
 *   meta  - The OSMP_Meta_t handle
 *   index - The index of the file (0 to count-1)
 *   name  - Pointer to receive a const char* for the filename (can be NULL)
 *   size  - Pointer to receive the file size (can be NULL)
 * Returns: 0 on success, -1 if index is out of bounds
 */
int osmp_meta_get_entry(OSMP_Meta_t* meta, uint32_t index, const char** name, uint64_t* size);


#ifdef __cplusplus
}
#endif

#endif /* OSMP_META_H */
