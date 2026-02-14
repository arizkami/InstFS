/*
 * OSMP Metadata API
 * Implementation for reading the metadata archive from an OSMP container
 */

#include "osmp_meta.h"
#include "instfs.h" // For osmp_master_header_t

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define OSMP_MAGIC "OSMP_IMG"

/* Header for an entry in the metadata archive */
typedef struct {
    char     path[256];
    uint64_t size;
} osmp_meta_entry_header_t;

/* Concrete definition of the OSMP_Meta_t handle */
struct OSMP_Meta_t {
    const uint8_t* mapped_file;     // Pointer to the start of the mmap'ed OSMP file
    size_t mapped_size;           // Full size of the mmap'ed file
    const uint8_t* meta_partition;  // Pointer to the start of the metadata partition
    size_t meta_size;             // Size of the metadata partition
};

/*
 * Mount the metadata partition from an .osmp container file
 */
OSMP_Meta_t* osmp_meta_mount(const char* filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return NULL;

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return NULL;
    }
    size_t file_size = sb.st_size;

    const uint8_t* mapped = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) {
        return NULL;
    }

    if (file_size < sizeof(osmp_master_header_t)) {
        munmap((void*)mapped, file_size);
        return NULL;
    }

    const osmp_master_header_t* header = (const osmp_master_header_t*)mapped;
    if (memcmp(header->magic, OSMP_MAGIC, 8) != 0) {
        munmap((void*)mapped, file_size);
        return NULL;
    }

    OSMP_Meta_t* meta = (OSMP_Meta_t*)calloc(1, sizeof(OSMP_Meta_t));
    if (!meta) {
        munmap((void*)mapped, file_size);
        return NULL;
    }

    meta->mapped_file = mapped;
    meta->mapped_size = file_size;
    meta->meta_partition = mapped + header->meta_offset;
    meta->meta_size = header->meta_size;

    // Bounds check
    if (header->meta_offset + header->meta_size > file_size) {
        osmp_meta_unmount(meta);
        return NULL;
    }

    return meta;
}

/*
 * Unmount an OSMP metadata archive and release resources
 */
void osmp_meta_unmount(OSMP_Meta_t* meta) {
    if (!meta) return;
    if (meta->mapped_file) {
        munmap((void*)meta->mapped_file, meta->mapped_size);
    }
    free(meta);
}

// Internal helper to iterate to a specific entry
static const osmp_meta_entry_header_t* _osmp_get_entry_header(OSMP_Meta_t* meta, uint32_t find_index, const char* find_name) {
    if (!meta) return NULL;

    const uint8_t* current_pos = meta->meta_partition;
    const uint8_t* end_pos = meta->meta_partition + meta->meta_size;
    uint32_t current_index = 0;

    while (current_pos < end_pos) {
        if (current_pos + sizeof(osmp_meta_entry_header_t) > end_pos) {
            // Not enough space for another header
            break;
        }
        
        const osmp_meta_entry_header_t* header = (const osmp_meta_entry_header_t*)current_pos;

        if (find_name) {
            if (strncmp(header->path, find_name, sizeof(header->path)) == 0) {
                return header;
            }
        } else { // find by index
            if (current_index == find_index) {
                return header;
            }
        }

        current_pos += sizeof(osmp_meta_entry_header_t) + header->size;
        current_index++;
    }

    return NULL; // Not found
}


/*
 * Find a file in the metadata archive by name
 */
const uint8_t* osmp_meta_find_file(OSMP_Meta_t* meta, const char* filename, uint64_t* size) {
    const osmp_meta_entry_header_t* header = _osmp_get_entry_header(meta, 0, filename);
    if (!header) {
        if (size) *size = 0;
        return NULL;
    }

    if (size) {
        *size = header->size;
    }
    // Return pointer to the data, which immediately follows the header
    return (const uint8_t*)header + sizeof(osmp_meta_entry_header_t);
}

/*
 * Get the number of files in the metadata archive
 */
uint32_t osmp_meta_get_count(OSMP_Meta_t* meta) {
    if (!meta) return 0;
    
    const uint8_t* current_pos = meta->meta_partition;
    const uint8_t* end_pos = meta->meta_partition + meta->meta_size;
    uint32_t count = 0;

    while (current_pos < end_pos) {
        if (current_pos + sizeof(osmp_meta_entry_header_t) > end_pos) {
            break;
        }
        const osmp_meta_entry_header_t* header = (const osmp_meta_entry_header_t*)current_pos;
        current_pos += sizeof(osmp_meta_entry_header_t) + header->size;
        count++;
    }
    return count;
}


/*
 * Get information about a file in the archive by its index
 */
int osmp_meta_get_entry(OSMP_Meta_t* meta, uint32_t index, const char** name, uint64_t* size) {
    const osmp_meta_entry_header_t* header = _osmp_get_entry_header(meta, index, NULL);
    if (!header) {
        return -1;
    }

    if (name) {
        *name = header->path;
    }
    if (size) {
        *size = header->size;
    }

    return 0;
}
