/*
 * inspect_osmp - Utility to inspect OSMP container files
 * 
 * Usage: inspect_osmp <file.osmp>
 */

#include "instfs.h"
#include "osmp_meta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_separator(void) {
    printf("========================================\n");
}

static void print_instrument_info(uint32_t index, const char *name, uint64_t size) {
    printf("  [%3u] %-40s %10llu bytes\n", index, name, (unsigned long long)size);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.osmp>\n", argv[0]);
        return 1;
    }

    const char* filepath = argv[1];
    
    print_separator();
    printf("OSMP Container Inspector\n");
    print_separator();
    printf("File: %s\n\n", filepath);

    // Mount InstFS partition
    osmp_master_header_t master_header;
    InstFS_t* fs = instfs_mount_osmp(filepath, &master_header);
    
    if (!fs) {
        fprintf(stderr, "Error: Failed to mount OSMP file\n");
        return 1;
    }

    // Display master header info
    printf("Master Header:\n");
    printf("  Magic:          %.8s\n", master_header.magic);
    printf("  Version:        %u\n", master_header.version);
    printf("  Metadata:       offset=%llu, size=%llu bytes\n", 
           (unsigned long long)master_header.meta_offset,
           (unsigned long long)master_header.meta_size);
    printf("  InstFS:         offset=%llu, size=%llu bytes\n\n",
           (unsigned long long)master_header.instfs_offset,
           (unsigned long long)master_header.instfs_size);

    // Display InstFS info
    uint64_t total_size = 0;
    uint32_t num_instruments = 0;
    instfs_stats(fs, &total_size, &num_instruments);
    
    printf("InstFS Partition:\n");
    printf("  Total Size:     %llu bytes\n", (unsigned long long)total_size);
    printf("  Instruments:    %u\n\n", num_instruments);

    if (num_instruments > 0) {
        printf("Instrument List:\n");
        instfs_list(fs, print_instrument_info);
        printf("\n");
    }

    instfs_unmount(fs);

    // Display metadata info
    OSMP_Meta_t* meta = osmp_meta_mount(filepath);
    if (meta) {
        uint32_t meta_count = osmp_meta_get_count(meta);
        printf("Metadata Archive:\n");
        printf("  Files:          %u\n\n", meta_count);

        if (meta_count > 0) {
            printf("Metadata Files:\n");
            for (uint32_t i = 0; i < meta_count; i++) {
                const char* name = NULL;
                uint64_t size = 0;
                if (osmp_meta_get_entry(meta, i, &name, &size) == 0) {
                    printf("  [%3u] %-40s %10llu bytes\n", 
                           i, name, (unsigned long long)size);
                    
                    // If it's a JSON file, show a preview
                    if (strstr(name, ".json") != NULL) {
                        const uint8_t* data = osmp_meta_find_file(meta, name, NULL);
                        if (data && size > 0) {
                            printf("\n        Preview (first 200 chars):\n        ");
                            size_t preview_len = size < 200 ? size : 200;
                            for (size_t j = 0; j < preview_len; j++) {
                                char c = data[j];
                                if (c == '\n') printf("\n        ");
                                else if (c >= 32 && c < 127) printf("%c", c);
                            }
                            if (size > 200) printf("...");
                            printf("\n\n");
                        }
                    }
                }
            }
        }

        osmp_meta_unmount(meta);
    } else {
        printf("Metadata Archive: (none or failed to mount)\n\n");
    }

    print_separator();
    printf("Inspection complete\n");
    print_separator();

    return 0;
}
