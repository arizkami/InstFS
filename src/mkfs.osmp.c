/*
 * mkfs.osmp - Create an OSMP container image
 *
 * Usage: mkfs.osmp -o <output.osmp> -m <meta_dir> <inst_file1> [inst_file2] ...
 *
 * This tool packages:
 *  1. A directory of metadata files into a simple tar-like archive.
 *  2. A list of instrument files into an InstFS partition.
 * ...into a single .osmp container file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>

#define OSMP_MAGIC "OSMP_IMG"
#define OSMP_VERSION 1

#define INSTFS_MAGIC "INSTFS"
#define INSTFS_VERSION 0x00010000
#define INSTFS_MAX_NAME 256

/* --- Data Structures for OSMP Container --- */

typedef struct {
    char     magic[8];
    uint32_t version;
    uint64_t meta_offset;
    uint64_t meta_size;
    uint64_t instfs_offset;
    uint64_t instfs_size;
    uint64_t reserved[4];
} osmp_master_header_t;

typedef struct {
    char     path[256];
    uint64_t size;
} osmp_meta_entry_header_t;


/* --- Data Structures for InstFS Partition (copied for tool use) --- */

typedef struct {
    char     magic[8];
    uint32_t version;
    uint32_t num_instruments;
    uint64_t instrument_table_offset;
    uint64_t reserved[4];
} instfs_header_t;

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


/* --- Helper Functions --- */

// Helper to extract filename from path
const char* get_filename(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

// Function to write metadata files from a single directory and return total size
uint64_t write_meta_files(FILE* out_fp, const char* meta_dir) {
    char path[1024];
    struct dirent* entry;
    DIR* dir = opendir(meta_dir);
    if (!dir) {
        perror("Failed to open metadata directory");
        return 0;
    }

    uint64_t total_bytes_written = 0;
    while ((entry = readdir(dir)) != NULL) {
        snprintf(path, sizeof(path), "%s/%s", meta_dir, entry->d_name);
        struct stat path_stat;
        if (stat(path, &path_stat) != 0 || S_ISDIR(path_stat.st_mode)) {
            // Skip directories and files we can't stat
            continue;
        }

        // It's a file, write it to the archive
        FILE* in_fp = fopen(path, "rb");
        if (!in_fp) continue;

        // Prepare header
        osmp_meta_entry_header_t meta_header = {0};
        snprintf(meta_header.path, sizeof(meta_header.path), "%s", entry->d_name); // Store just the filename
        meta_header.size = path_stat.st_size;

        // Write header and data
        fwrite(&meta_header, sizeof(osmp_meta_entry_header_t), 1, out_fp);
        
        char* buffer = malloc(meta_header.size);
        if (buffer) {
            fread(buffer, meta_header.size, 1, in_fp);
            fwrite(buffer, meta_header.size, 1, out_fp);
            free(buffer);
            total_bytes_written += sizeof(osmp_meta_entry_header_t) + meta_header.size;
        }
        
        fclose(in_fp);
    }
    closedir(dir);
    return total_bytes_written;
}

// Function to write the InstFS partition and return its total size
uint64_t write_instfs_partition(FILE* out_fp, int num_inst_files, char** inst_filenames) {
    long partition_start_pos = ftell(out_fp);

    // 1. Prepare header and entries
    instfs_header_t header = {0};
    memcpy(header.magic, INSTFS_MAGIC, 6);
    header.version = INSTFS_VERSION;
    header.num_instruments = num_inst_files;
    header.instrument_table_offset = sizeof(instfs_header_t);

    instfs_entry_t* entries = calloc(num_inst_files, sizeof(instfs_entry_t));
    if (!entries) return 0;

    // 2. Write placeholders
    fwrite(&header, sizeof(instfs_header_t), 1, out_fp);
    fwrite(entries, sizeof(instfs_entry_t), num_inst_files, out_fp);

    // 3. Write names and data
    uint64_t names_base_offset = sizeof(instfs_header_t) + (num_inst_files * sizeof(instfs_entry_t));
    uint64_t data_base_offset = names_base_offset;
    for (int i=0; i < num_inst_files; ++i) {
        data_base_offset += strlen(get_filename(inst_filenames[i])) + 1;
    }

    uint64_t current_name_offset = names_base_offset;
    uint64_t current_data_offset = data_base_offset;

    for (int i=0; i < num_inst_files; ++i) {
        const char* name = get_filename(inst_filenames[i]);
        
        // Write name relative to partition start
        entries[i].name_offset = current_name_offset;
        fseek(out_fp, partition_start_pos + current_name_offset, SEEK_SET);
        fwrite(name, strlen(name) + 1, 1, out_fp);
        current_name_offset += strlen(name) + 1;

        // Write data relative to partition start
        FILE* in_fp = fopen(inst_filenames[i], "rb");
        fseek(in_fp, 0, SEEK_END);
        long file_size = ftell(in_fp);
        fseek(in_fp, 0, SEEK_SET);
        char* buffer = malloc(file_size);
        fread(buffer, file_size, 1, in_fp);
        fclose(in_fp);

        entries[i].data_offset = current_data_offset;
        entries[i].data_size = file_size;
        fseek(out_fp, partition_start_pos + current_data_offset, SEEK_SET);
        fwrite(buffer, file_size, 1, out_fp);
        free(buffer);
        current_data_offset += file_size;

        // Dummy metadata
        entries[i].format = 1; entries[i].sample_rate = 44100; entries[i].channels = 2; entries[i].bit_depth = 16;
    }

    // 4. Write final header and entries
    fseek(out_fp, partition_start_pos, SEEK_SET);
    fwrite(&header, sizeof(instfs_header_t), 1, out_fp);
    fwrite(entries, sizeof(instfs_entry_t), num_inst_files, out_fp);
    
    free(entries);
    fseek(out_fp, 0, SEEK_END); // Go to end of file for next operation
    return (uint64_t)(ftell(out_fp) - partition_start_pos);
}


/* --- Main Function --- */

int main(int argc, char *argv[]) {
    if (argc < 6 || strcmp(argv[1], "-o") != 0 || strcmp(argv[3], "-m") != 0) {
        fprintf(stderr, "Usage: %s -o <output.osmp> -m <meta_dir> <inst_file1> ...\n", argv[0]);
        return 1;
    }

    const char* output_filename = argv[2];
    const char* meta_dirname = argv[4];
    int num_inst_files = argc - 5;
    char** inst_filenames = &argv[5];

    FILE *out_fp = fopen(output_filename, "wb");
    if (!out_fp) {
        perror("Failed to open output file");
        return 1;
    }

    // 1. Write placeholder master header
    osmp_master_header_t master_header = {0};
    fwrite(&master_header, sizeof(osmp_master_header_t), 1, out_fp);

    // 2. Write metadata archive
    master_header.meta_offset = ftell(out_fp);
    master_header.meta_size = write_meta_files(out_fp, meta_dirname);

    // 3. Write InstFS partition
    master_header.instfs_offset = ftell(out_fp);
    master_header.instfs_size = write_instfs_partition(out_fp, num_inst_files, inst_filenames);

    // 4. Write final master header
    memcpy(master_header.magic, OSMP_MAGIC, 8);
    master_header.version = OSMP_VERSION;
    fseek(out_fp, 0, SEEK_SET);
    fwrite(&master_header, sizeof(osmp_master_header_t), 1, out_fp);

    fclose(out_fp);
    printf("Successfully created %s.\n", output_filename);
    printf("  - Metadata:  %llu bytes\n", master_header.meta_size);
    printf("  - InstrumentFS: %llu bytes (%d instruments)\n", master_header.instfs_size, num_inst_files);

    return 0;
}
