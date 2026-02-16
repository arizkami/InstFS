/*
 * InstFS - Instrument File System
 * A virtual filesystem for storing and accessing instrument samples and patches
 */

#include "instfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "portability.h"

/* Magic Numbers */
#define OSMP_MAGIC "OSMP_IMG"
#define INSTFS_MAGIC "INSTFS"
#define INSTFS_VERSION 0x00010000
#define INSTFS_MAX_NAME 256

/* InstFS Header structure (must match headerfs.S and mkfs.osmp) */
typedef struct {
    char magic[8];
    uint32_t version;
    uint32_t num_instruments;
    uint64_t instrument_table_offset;
    uint64_t reserved[4];
} instfs_header_t;

/* Instrument entry structure (must match mkfs.osmp) */
typedef struct {
    uint64_t name_offset;
    uint64_t data_offset;
    uint64_t data_size;
    uint32_t format;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bit_depth;
    uint64_t reserved[2];
} instfs_entry_t;

/* Concrete definition of the InstFS handle */
struct InstFS_t {
    const uint8_t* base_ptr;      /* Base address of the InstFS partition data */
    size_t total_size;            /* Total size of the InstFS partition */
    instfs_header_t* header;      /* Pointer to the header within the partition */
    instfs_entry_t* entries;      /* Pointer to the instrument table */
    
#ifdef _WIN32
    mmap_handle_t mmap_handle;
#else
    /* For unmapping a region from a larger file */
    const uint8_t* mmap_start;    /* The actual start of the mmaped region */
    size_t mmap_size;             /* The actual size of the mmaped region */
#endif
};

/* Internal helper to initialize a handle from a memory region */
static InstFS_t* _instfs_init_from_mem(const void* data, size_t size) {
    if (!data || size < sizeof(instfs_header_t)) {
        return NULL;
    }

    InstFS_t* fs = (InstFS_t*)calloc(1, sizeof(InstFS_t));
    if (!fs) {
        return NULL;
    }

    fs->base_ptr = (const uint8_t*)data;
    fs->total_size = size;
    fs->header = (instfs_header_t*)fs->base_ptr;

    /* Verify InstFS magic number and version */
    if (memcmp(fs->header->magic, INSTFS_MAGIC, 6) != 0 || fs->header->version != INSTFS_VERSION) {
        free(fs);
        return NULL;
    }

    uint64_t table_offset = fs->header->instrument_table_offset;
    uint64_t table_size = fs->header->num_instruments * sizeof(instfs_entry_t);
    if (table_offset + table_size > size) {
        free(fs);
        return NULL;
    }

    fs->entries = (instfs_entry_t*)(fs->base_ptr + table_offset);
    return fs;
}

/*
 * Mount an InstFS partition from an .osmp container file
 */
#ifndef _WIN32
InstFS_t* instfs_mount_osmp(const char* filepath, osmp_master_header_t* master_header_out) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return NULL;

    osmp_master_header_t header;
    if (read(fd, &header, sizeof(osmp_master_header_t)) != sizeof(osmp_master_header_t)) {
        close(fd);
        return NULL;
    }

    if (memcmp(header.magic, OSMP_MAGIC, 8) != 0) {
        close(fd);
        return NULL;
    }
    
    if (master_header_out) {
        *master_header_out = header;
    }

    size_t instfs_part_size = header.instfs_size;
    off_t instfs_part_offset = header.instfs_offset;

    // mmap requires offset to be a multiple of page size
    off_t page_size = get_page_size();
    off_t offset_in_page = instfs_part_offset % page_size;
    off_t mmap_offset = instfs_part_offset - offset_in_page;
    size_t mmap_size = instfs_part_size + offset_in_page;

    const uint8_t* mapped = (const uint8_t*)mmap(NULL, mmap_size, PROT_READ, MAP_SHARED, fd, mmap_offset);
    close(fd);

    if (mapped == MAP_FAILED) {
        return NULL;
    }

    const uint8_t* instfs_data = mapped + offset_in_page;
    InstFS_t* fs = _instfs_init_from_mem(instfs_data, instfs_part_size);
    if (fs) {
        fs->mmap_start = mapped;
        fs->mmap_size = mmap_size;
    } else {
        munmap((void*)mapped, mmap_size);
    }

    return fs;
}
#else // _WIN32
InstFS_t* instfs_mount_osmp(const char* filepath, osmp_master_header_t* master_header_out) {
    mmap_handle_t mmap_h;
    size_t file_size;
    const uint8_t* mapped = mmap_file(filepath, &file_size, &mmap_h);
    if (!mapped) {
        return NULL;
    }

    if (file_size < sizeof(osmp_master_header_t)) {
        unmap_file(&mmap_h);
        return NULL;
    }

    osmp_master_header_t header;
    memcpy(&header, mapped, sizeof(osmp_master_header_t));

    if (memcmp(header.magic, OSMP_MAGIC, 8) != 0) {
        unmap_file(&mmap_h);
        return NULL;
    }
    
    if (master_header_out) {
        *master_header_out = header;
    }

    size_t instfs_part_size = header.instfs_size;
    uint64_t instfs_part_offset = header.instfs_offset;

    if (instfs_part_offset + instfs_part_size > file_size) {
        unmap_file(&mmap_h);
        return NULL;
    }

    const uint8_t* instfs_data = mapped + instfs_part_offset;
    InstFS_t* fs = _instfs_init_from_mem(instfs_data, instfs_part_size);
    if (fs) {
        fs->mmap_handle = mmap_h;
    } else {
        unmap_file(&mmap_h);
    }

    return fs;
}
#endif // _WIN32

/*
 * Mount an InstFS image from a memory buffer
 */
InstFS_t* instfs_mount_mem(const void* data, size_t size) {
    InstFS_t* fs = _instfs_init_from_mem(data, size);
    if (fs) {
#ifdef _WIN32
        memset(&fs->mmap_handle, 0, sizeof(mmap_handle_t));
#else
        fs->mmap_start = NULL; // Not memory-mapped
        fs->mmap_size = 0;
#endif
    }
    return fs;
}

/*
 * Unmount an InstFS image and release resources
 */
void instfs_unmount(InstFS_t* fs) {
    if (!fs) return;
#ifdef _WIN32
    unmap_file(&fs->mmap_handle);
#else
    if (fs->mmap_start) {
        munmap((void*)fs->mmap_start, fs->mmap_size);
    }
#endif
    free(fs);
}

/*
 * Get the number of instruments
 */
uint32_t instfs_get_count(InstFS_t* fs) {
    if (!fs) return 0;
    return fs->header->num_instruments;
}

/*
 * Get instrument name by index
 */
const char* instfs_get_name(InstFS_t* fs, uint32_t index) {
    if (!fs || index >= fs->header->num_instruments) return NULL;
    
    instfs_entry_t *entry = &fs->entries[index];
    if (entry->name_offset >= fs->total_size) return NULL;
    return (const char *)(fs->base_ptr + entry->name_offset);
}

/*
 * Find instrument by name
 */
int instfs_find(InstFS_t* fs, const char *name) {
    if (!fs || !name) return -1;
    
    for (uint32_t i = 0; i < fs->header->num_instruments; i++) {
        const char *entry_name = instfs_get_name(fs, i);
        if (entry_name && strncmp(entry_name, name, INSTFS_MAX_NAME) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/*
 * Get instrument data
 */
const uint8_t* instfs_get_data(InstFS_t* fs, uint32_t index, uint64_t *size) {
    if (!fs || index >= fs->header->num_instruments) {
        if (size) *size = 0;
        return NULL;
    }
    
    instfs_entry_t *entry = &fs->entries[index];
    if (size) *size = entry->data_size;
    
    if (entry->data_offset + entry->data_size > fs->total_size) return NULL;
    return fs->base_ptr + entry->data_offset;
}

/*
 * Get instrument metadata
 */
int instfs_get_info(InstFS_t* fs, uint32_t index, uint32_t *format, 
                    uint32_t *sample_rate, uint16_t *channels, uint16_t *bit_depth) {
    if (!fs || index >= fs->header->num_instruments) return -1;
    
    instfs_entry_t *entry = &fs->entries[index];
    if (format) *format = entry->format;
    if (sample_rate) *sample_rate = entry->sample_rate;
    if (channels) *channels = entry->channels;
    if (bit_depth) *bit_depth = entry->bit_depth;
    
    return 0;
}

/*
 * Read instrument data into buffer
 */
int64_t instfs_read(InstFS_t* fs, uint32_t index, uint8_t *buffer, 
                    uint64_t offset, uint64_t size) {
    if (!fs || !buffer || index >= fs->header->num_instruments) return -1;
    
    uint64_t data_size = 0;
    const uint8_t* data_ptr = instfs_get_data(fs, index, &data_size);
    if (!data_ptr) return -1;
    
    if (offset >= data_size) return 0;
    
    uint64_t available = data_size - offset;
    if (size > available) size = available;
    
    memcpy(buffer, data_ptr + offset, size);
    return (int64_t)size;
}

/*
 * List all instruments
 */
void instfs_list(InstFS_t* fs, void (*callback)(uint32_t index, const char *name, uint64_t size)) {
    if (!fs || !callback) return;
    
    for (uint32_t i = 0; i < fs->header->num_instruments; i++) {
        uint64_t size = 0;
        const char *name = instfs_get_name(fs, i);
        instfs_get_data(fs, i, &size);
        if (name) {
            callback(i, name, size);
        }
    }
}

/*
 * Get filesystem statistics
 */
void instfs_stats(InstFS_t* fs, uint64_t *total_size, uint32_t *num_instruments) {
    if (!fs) {
        if (total_size) *total_size = 0;
        if (num_instruments) *num_instruments = 0;
        return;
    }
    
    if (num_instruments) *num_instruments = fs->header->num_instruments;
    if (total_size) *total_size = fs->total_size;
}
