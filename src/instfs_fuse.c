/*
 * instfs_fuse - FUSE driver for the InstFS (Instrument File System)
 *
 * This driver mounts an OSMP container file (which contains an InstFS partition)
 * as a read-only filesystem, allowing instruments to be accessed as files.
 *
 * Usage: instfs_fuse <OSMP_FILE> <MOUNTPOINT>
 */

#define _FILE_OFFSET_BITS 64 // Must be defined before including <fuse.h>
#define FUSE_USE_VERSION 26 // Changing to FUSE2 for compatibility

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h> // For PATH_MAX
#include <stdlib.h> // For malloc and free
// #include <fuse/fuse_opt.h> // Not needed for FUSE2 init signature

#include "instfs.h"
#include "osmp_meta.h"

// Global InstFS handle
static InstFS_t* instfs_handle = NULL;
static OSMP_Meta_t* osmp_meta_handle = NULL;

// Store OSMP file path for re-mounting in init if needed
static char osmp_filepath[PATH_MAX];

// Structure to hold context for open files
typedef struct {
    uint32_t inst_index;
    // Current read offset for the instrument - not really needed here, but kept from previous
    uint64_t offset; 
} instfs_file_handle_t;


static void* instfs_fuse_init(struct fuse_conn_info *conn) { // FUSE2 API: no cfg parameter
    (void) conn;
    
    // Mount the OSMP file and InstFS partition
    instfs_handle = instfs_mount_osmp(osmp_filepath, NULL);
    if (!instfs_handle) {
        fprintf(stderr, "Failed to mount InstFS partition from %s\n", osmp_filepath);
        return NULL; // Return NULL to indicate initialization failure
    }

    // Optionally mount metadata
    osmp_meta_handle = osmp_meta_mount(osmp_filepath);
    if (!osmp_meta_handle) {
        fprintf(stderr, "Warning: Failed to mount OSMP metadata from %s\n", osmp_filepath);
        // Not a critical error, just log it.
    }

    fprintf(stderr, "InstFS FUSE mounted successfully from %s\n", osmp_filepath);
    fprintf(stderr, "Found %u instruments.\n", instfs_get_count(instfs_handle));
    fprintf(stderr, "Found %u metadata files.\n", osmp_meta_get_count(osmp_meta_handle));

    return NULL; // No private data to pass
}

static void instfs_fuse_destroy(void* private_data) {
    (void) private_data;
    if (instfs_handle) {
        instfs_unmount(instfs_handle);
        instfs_handle = NULL;
    }
    if (osmp_meta_handle) {
        osmp_meta_unmount(osmp_meta_handle);
        osmp_meta_handle = NULL;
    }
    fprintf(stderr, "InstFS FUSE unmounted.\n");
}


static int instfs_fuse_getattr(const char *path, struct stat *stbuf) { // FUSE2 API: no fi parameter
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0555; // Directory, read-only
        stbuf->st_nlink = 2; // . and ..
    } else {
        // Try to find as instrument
        int index = instfs_find(instfs_handle, path + 1); // Skip leading '/'
        if (index != -1) {
            uint64_t size = 0;
            instfs_get_data(instfs_handle, (uint32_t)index, &size);
            stbuf->st_mode = S_IFREG | 0444; // Regular file, read-only
            stbuf->st_nlink = 1;
            stbuf->st_size = (off_t)size; // Cast to off_t
        } else {
            // Try to find as metadata file
            uint64_t meta_size = 0;
            const uint8_t* meta_data = osmp_meta_find_file(osmp_meta_handle, path + 1, &meta_size);
            if (meta_data) {
                stbuf->st_mode = S_IFREG | 0444; // Regular file, read-only
                stbuf->st_nlink = 1;
                stbuf->st_size = (off_t)meta_size; // Cast to off_t
            } else {
                res = -ENOENT; // Not found
            }
        }
    }
    return res;
}

static int instfs_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset) { // FUSE2 API
    (void) offset; // Not used for simple readdir

    if (strcmp(path, "/") != 0) {
        return -ENOENT;
    }

    // FUSE2 filler takes 4 args: buf, name, stbuf, off
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    // List instruments
    uint32_t count = instfs_get_count(instfs_handle);
    for (uint32_t i = 0; i < count; ++i) {
        const char *name = instfs_get_name(instfs_handle, i);
        if (name) {
            // Can optionally pass stat info here, for now NULL
            filler(buf, name, NULL, 0);
        }
    }

    // List metadata files
    uint32_t meta_count = osmp_meta_get_count(osmp_meta_handle);
    for (uint32_t i = 0; i < meta_count; ++i) {
        const char* name = NULL;
        if (osmp_meta_get_entry(osmp_meta_handle, i, &name, NULL) == 0 && name) {
            filler(buf, name, NULL, 0);
        }
    }

    return 0;
}

static int instfs_fuse_open(const char *path, struct fuse_file_info *fi) {
    int inst_index = instfs_find(instfs_handle, path + 1); // Skip leading '/'
    uint64_t meta_size = 0;
    const uint8_t* meta_data = osmp_meta_find_file(osmp_meta_handle, path + 1, &meta_size);

    if (inst_index == -1 && !meta_data) {
        return -ENOENT;
    }

    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EACCES; // Read-only filesystem
    }

    instfs_file_handle_t *fh = (instfs_file_handle_t*)malloc(sizeof(instfs_file_handle_t));
    if (!fh) {
        return -ENOMEM;
    }

    fh->offset = 0; // Not strictly used by instfs_read, but good to have context
    if (inst_index != -1) {
        fh->inst_index = (uint32_t)inst_index;
    } else {
        // Handle metadata files (temporarily disallow opening with file handle)
        free(fh);
        return -ENOENT;
    }
    
    fi->fh = (uint64_t)fh;
    return 0;
}

static int instfs_fuse_read(const char *path, char *buf, size_t size, off_t offset,
                            struct fuse_file_info *fi) {
    (void) path; // Unused since fi->fh is used

    if (!instfs_handle) return -EIO;

    // Handle instrument files
    if (fi->fh) {
        instfs_file_handle_t *fh = (instfs_file_handle_t*)fi->fh;
        uint64_t inst_size = 0;
        instfs_get_data(instfs_handle, fh->inst_index, &inst_size); // Get total size

        if ((uint64_t)offset >= inst_size) { // Cast offset to unsigned for comparison
            return 0; // EOF
        }

        int64_t bytes_read = instfs_read(instfs_handle, fh->inst_index, (uint8_t*)buf, (uint64_t)offset, size);
        if (bytes_read < 0) {
            return -EIO;
        }
        return (int)bytes_read;
    }

    // Handle metadata files (read directly by path, if not handled by open/fh)
    uint64_t meta_file_size = 0;
    const uint8_t* meta_file_data = osmp_meta_find_file(osmp_meta_handle, path + 1, &meta_file_size);
    if (meta_file_data) {
        if ((uint64_t)offset >= meta_file_size) { // Cast offset to unsigned for comparison
            return 0; // EOF
        }
        size_t to_read = size;
        if ((uint64_t)offset + to_read > meta_file_size) {
            to_read = meta_file_size - (uint64_t)offset;
        }
        memcpy(buf, meta_file_data + offset, to_read);
        return (int)to_read;
    }

    return -ENOENT;
}

static int instfs_fuse_release(const char *path, struct fuse_file_info *fi) {
    (void) path; // Unused
    if (fi->fh) {
        instfs_file_handle_t *fh = (instfs_file_handle_t*)fi->fh;
        free(fh);
        fi->fh = 0;
    }
    return 0;
}


static const struct fuse_operations instfs_oper = {
    .init       = instfs_fuse_init,
    .destroy    = instfs_fuse_destroy,
    .getattr    = instfs_fuse_getattr,
    .readdir    = instfs_fuse_readdir,
    .open       = instfs_fuse_open,
    .read       = instfs_fuse_read,
    .release    = instfs_fuse_release,
};

int main(int argc, char *argv[]) {
    // Store OSMP file path before FUSE processes argv
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <OSMP_FILE> <MOUNTPOINT>\n", argv[0]);
        return 1;
    }
    strncpy(osmp_filepath, argv[1], PATH_MAX - 1);
    osmp_filepath[PATH_MAX - 1] = '\0';

    // Shift arguments for fuse_main
    int fuse_argc = argc - 1;
    char** fuse_argv = (char**)malloc(sizeof(char*) * (fuse_argc + 1));
    if (!fuse_argv) {
        fprintf(stderr, "Failed to allocate memory for fuse_argv\n");
        return 1;
    }
    fuse_argv[0] = argv[0]; // Program name
    for (int i = 2; i < argc; ++i) {
        fuse_argv[i - 1] = argv[i];
    }
    fuse_argv[fuse_argc] = NULL; // Null terminate

    // Correcting argument shifting:
    // fuse_argc = argc; // Original argc, but we'll modify it
    // fuse_argv[0] = argv[0];
    // for (int i = 1; i < argc; ++i) {
    //     fuse_argv[i] = argv[i];
    // }
    // fuse_main expects {program_name, mount_point, ...fuse_options}
    // So if user gives: instfs_fuse <osmp_file> <mountpoint> [fuse_options]
    // fuse_argv should be: {instfs_fuse, <mountpoint>, [fuse_options]}
    // This means argv[1] (osmp_file) should be skipped.

    fuse_argc = argc - 1; // Program name + mountpoint + options
    char** final_fuse_argv = (char**)malloc(sizeof(char*) * (fuse_argc + 1));
    if (!final_fuse_argv) {
        fprintf(stderr, "Failed to allocate memory for final_fuse_argv\n");
        free(fuse_argv); // This was previous fuse_argv, free it
        return 1;
    }
    
    final_fuse_argv[0] = argv[0]; // Program name
    // Copy mountpoint and any other fuse options
    for (int i = 2; i < argc; ++i) {
        final_fuse_argv[i-1] = argv[i];
    }
    final_fuse_argv[fuse_argc] = NULL;

    int ret = fuse_main(fuse_argc, final_fuse_argv, &instfs_oper, NULL);

    free(final_fuse_argv);
    free(fuse_argv); // Free the old one as well, if it was allocated
    return ret;
}