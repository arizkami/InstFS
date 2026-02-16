/*
 * mkfs.osmp - Create an OSMP container image
 *
 * Usage: mkfs.osmp -o <output.osmp> -m <meta_dir> <inst_file1> [inst_file2] ...
 *        mkfs.osmp -o <output.osmp> -j <instrument.json>
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
#include <ctype.h>

/* Include portability layer first for cross-platform support */
#include "portability.h"

/* Include InstFS headers for structure definitions */
#include "instfs.h"
#include "osmp_meta.h"

#define OSMP_MAGIC "OSMP_IMG"
#define OSMP_VERSION 1

#define INSTFS_MAGIC "INSTFS"
#define INSTFS_VERSION 0x00010000
#define INSTFS_MAX_NAME 256

/* --- Data Structures for OSMP Container --- */
/* Note: osmp_master_header_t is now defined in instfs.h */

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

/* --- JSON Parsing Structures --- */

typedef struct {
    char sample[512];
    int key_range[2];
    int vel_range[2];
    int root_key;
    int tune;
    int volume;
    char loop_mode[32];
    struct {
        float attack;
        float decay;
        float sustain;
        float release;
    } amp_env;
} json_region_t;

typedef struct {
    json_region_t* regions;
    int num_regions;
    char base_dir[512];
} json_instrument_t;


/* --- Helper Functions --- */

// Skip whitespace in JSON
static char* skip_whitespace(char* p) {
    while (*p && isspace(*p)) p++;
    return p;
}

// Parse a JSON string value (expects opening quote, returns pointer after closing quote)
static char* parse_json_string(char* p, char* out, size_t out_size) {
    if (*p != '"') return NULL;
    p++; // skip opening quote
    
    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && *(p+1)) {
            p++; // skip escape char
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    
    if (*p == '"') p++; // skip closing quote
    return p;
}

// Parse a JSON number
static char* parse_json_number(char* p, double* out) {
    char* end;
    *out = strtod(p, &end);
    return end;
}

// Find a key in JSON object and return pointer to its value
static char* find_json_key(char* json, const char* key) {
    char* p = json;
    size_t key_len = strlen(key);
    
    while (*p) {
        p = skip_whitespace(p);
        if (*p == '"') {
            char* key_start = p + 1;
            char* key_end = strchr(key_start, '"');
            if (key_end && (size_t)(key_end - key_start) == key_len && 
                strncmp(key_start, key, key_len) == 0) {
                p = key_end + 1;
                p = skip_whitespace(p);
                if (*p == ':') {
                    p++;
                    return skip_whitespace(p);
                }
            }
        }
        p++;
    }
    return NULL;
}

// Parse JSON instrument file
static json_instrument_t* parse_json_instrument(const char* json_path) {
    FILE* fp = fopen(json_path, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open JSON file: %s\n", json_path);
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* json_data = malloc(file_size + 1);
    if (!json_data) {
        fclose(fp);
        return NULL;
    }
    
    size_t bytes_read = fread(json_data, 1, file_size, fp);
    json_data[bytes_read] = '\0';
    fclose(fp);
    
    json_instrument_t* inst = calloc(1, sizeof(json_instrument_t));
    if (!inst) {
        free(json_data);
        return NULL;
    }
    
    // Extract base directory from JSON path
    const char* last_slash = strrchr(json_path, '/');
    if (!last_slash) last_slash = strrchr(json_path, '\\');
    if (last_slash) {
        size_t dir_len = last_slash - json_path + 1;
        if (dir_len < sizeof(inst->base_dir)) {
            strncpy(inst->base_dir, json_path, dir_len);
            inst->base_dir[dir_len] = '\0';
        }
    }
    
    // Find regions array
    char* regions_start = find_json_key(json_data, "regions");
    if (!regions_start || *regions_start != '[') {
        fprintf(stderr, "No 'regions' array found in JSON\n");
        free(json_data);
        free(inst);
        return NULL;
    }
    
    // Count regions
    int region_count = 0;
    char* p = regions_start + 1;
    int brace_depth = 0;
    while (*p) {
        if (*p == '{') {
            if (brace_depth == 0) region_count++;
            brace_depth++;
        } else if (*p == '}') {
            brace_depth--;
        } else if (*p == ']' && brace_depth == 0) {
            break;
        }
        p++;
    }
    
    inst->num_regions = region_count;
    inst->regions = calloc(region_count, sizeof(json_region_t));
    if (!inst->regions) {
        free(json_data);
        free(inst);
        return NULL;
    }
    
    // Parse each region
    p = regions_start + 1;
    int region_idx = 0;
    while (*p && region_idx < region_count) {
        p = skip_whitespace(p);
        if (*p != '{') {
            p++;
            continue;
        }
        
        // Find the closing brace for this region
        char* region_start = p;
        brace_depth = 1;
        p++;
        while (*p && brace_depth > 0) {
            if (*p == '{') brace_depth++;
            else if (*p == '}') brace_depth--;
            p++;
        }
        char* region_end = p;
        
        // Parse region fields
        json_region_t* region = &inst->regions[region_idx];
        
        char* sample_val = find_json_key(region_start, "sample");
        if (sample_val) {
            parse_json_string(sample_val, region->sample, sizeof(region->sample));
        }
        
        char* key_range_val = find_json_key(region_start, "key_range");
        if (key_range_val && *key_range_val == '[') {
            double val;
            key_range_val++;
            key_range_val = parse_json_number(skip_whitespace(key_range_val), &val);
            region->key_range[0] = (int)val;
            key_range_val = skip_whitespace(key_range_val);
            if (*key_range_val == ',') key_range_val++;
            key_range_val = parse_json_number(skip_whitespace(key_range_val), &val);
            region->key_range[1] = (int)val;
        }
        
        char* vel_range_val = find_json_key(region_start, "vel_range");
        if (vel_range_val && *vel_range_val == '[') {
            double val;
            vel_range_val++;
            vel_range_val = parse_json_number(skip_whitespace(vel_range_val), &val);
            region->vel_range[0] = (int)val;
            vel_range_val = skip_whitespace(vel_range_val);
            if (*vel_range_val == ',') vel_range_val++;
            vel_range_val = parse_json_number(skip_whitespace(vel_range_val), &val);
            region->vel_range[1] = (int)val;
        }
        
        char* root_key_val = find_json_key(region_start, "root_key");
        if (root_key_val) {
            double val;
            parse_json_number(root_key_val, &val);
            region->root_key = (int)val;
        }
        
        char* tune_val = find_json_key(region_start, "tune");
        if (tune_val) {
            double val;
            parse_json_number(tune_val, &val);
            region->tune = (int)val;
        }
        
        char* volume_val = find_json_key(region_start, "volume");
        if (volume_val) {
            double val;
            parse_json_number(volume_val, &val);
            region->volume = (int)val;
        }
        
        char* loop_mode_val = find_json_key(region_start, "loop_mode");
        if (loop_mode_val) {
            parse_json_string(loop_mode_val, region->loop_mode, sizeof(region->loop_mode));
        }
        
        // Parse amp_env object
        char* amp_env_val = find_json_key(region_start, "amp_env");
        if (amp_env_val && *amp_env_val == '{') {
            char* attack_val = find_json_key(amp_env_val, "attack");
            if (attack_val) parse_json_number(attack_val, (double*)&region->amp_env.attack);
            
            char* decay_val = find_json_key(amp_env_val, "decay");
            if (decay_val) parse_json_number(decay_val, (double*)&region->amp_env.decay);
            
            char* sustain_val = find_json_key(amp_env_val, "sustain");
            if (sustain_val) parse_json_number(sustain_val, (double*)&region->amp_env.sustain);
            
            char* release_val = find_json_key(amp_env_val, "release");
            if (release_val) parse_json_number(release_val, (double*)&region->amp_env.release);
        }
        
        region_idx++;
        p = region_end;
    }
    
    free(json_data);
    return inst;
}

// Free JSON instrument structure
static void free_json_instrument(json_instrument_t* inst) {
    if (inst) {
        if (inst->regions) free(inst->regions);
        free(inst);
    }
}

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
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

#ifdef _WIN32
        snprintf(path, sizeof(path), "%s\\%s", meta_dir, entry->d_name);
#else
        snprintf(path, sizeof(path), "%s/%s", meta_dir, entry->d_name);
#endif
#ifdef _WIN32
        struct _stat path_stat;
#else
        struct stat path_stat;
#endif
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
            size_t bytes_read = fread(buffer, meta_header.size, 1, in_fp);
            if (bytes_read > 0) {
                fwrite(buffer, meta_header.size, 1, out_fp);
                total_bytes_written += sizeof(osmp_meta_entry_header_t) + meta_header.size;
            }
            free(buffer);
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
        if (!in_fp) {
            fprintf(stderr, "Warning: Failed to open %s\n", inst_filenames[i]);
            continue;
        }
        fseek(in_fp, 0, SEEK_END);
        long file_size = ftell(in_fp);
        fseek(in_fp, 0, SEEK_SET);
        char* buffer = malloc(file_size);
        size_t bytes_read = fread(buffer, file_size, 1, in_fp);
        fclose(in_fp);

        entries[i].data_offset = current_data_offset;
        entries[i].data_size = file_size;
        fseek(out_fp, partition_start_pos + current_data_offset, SEEK_SET);
        if (bytes_read > 0) {
            fwrite(buffer, file_size, 1, out_fp);
        }
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

// Collect unique sample files from JSON instrument
static char** collect_sample_files(json_instrument_t* inst, int* out_count) {
    if (!inst || inst->num_regions == 0) {
        *out_count = 0;
        return NULL;
    }
    
    // Allocate array for sample paths (max = num_regions, but will likely be less)
    char** samples = malloc(inst->num_regions * sizeof(char*));
    int sample_count = 0;
    
    for (int i = 0; i < inst->num_regions; i++) {
        if (strlen(inst->regions[i].sample) == 0) continue;
        
        // Build full path
        char full_path[1024];
        if (inst->base_dir[0] != '\0') {
            snprintf(full_path, sizeof(full_path), "%s%s", inst->base_dir, inst->regions[i].sample);
        } else {
            snprintf(full_path, sizeof(full_path), "%s", inst->regions[i].sample);
        }
        
        // Check if already in list
        int found = 0;
        for (int j = 0; j < sample_count; j++) {
            if (strcmp(samples[j], full_path) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found) {
            samples[sample_count] = strdup(full_path);
            sample_count++;
        }
    }
    
    *out_count = sample_count;
    return samples;
}

// Generate metadata JSON from instrument JSON
static char* generate_metadata_json(json_instrument_t* inst) {
    if (!inst) return NULL;
    
    // Estimate size needed
    size_t estimated_size = 1024 + (inst->num_regions * 512);
    char* json = malloc(estimated_size);
    if (!json) return NULL;
    
    char* p = json;
    size_t remaining = estimated_size;
    
    // Start JSON object
    int written = snprintf(p, remaining, "{\n  \"instrument\": {\n");
    p += written; remaining -= written;
    
    // Add basic info
    written = snprintf(p, remaining, 
        "    \"num_regions\": %d,\n"
        "    \"num_samples\": %d,\n",
        inst->num_regions,
        inst->num_regions);
    p += written; remaining -= written;
    
    // Add regions array
    written = snprintf(p, remaining, "    \"regions\": [\n");
    p += written; remaining -= written;
    
    for (int i = 0; i < inst->num_regions && remaining > 512; i++) {
        json_region_t* r = &inst->regions[i];
        
        written = snprintf(p, remaining,
            "      {\n"
            "        \"sample\": \"%s\",\n"
            "        \"key_range\": [%d, %d],\n"
            "        \"vel_range\": [%d, %d],\n"
            "        \"root_key\": %d,\n"
            "        \"tune\": %d,\n"
            "        \"volume\": %d,\n"
            "        \"loop_mode\": \"%s\",\n"
            "        \"amp_env\": {\n"
            "          \"attack\": %.4f,\n"
            "          \"decay\": %.4f,\n"
            "          \"sustain\": %.4f,\n"
            "          \"release\": %.4f\n"
            "        }\n"
            "      }%s\n",
            r->sample,
            r->key_range[0], r->key_range[1],
            r->vel_range[0], r->vel_range[1],
            r->root_key, r->tune, r->volume,
            r->loop_mode,
            r->amp_env.attack, r->amp_env.decay,
            r->amp_env.sustain, r->amp_env.release,
            (i < inst->num_regions - 1) ? "," : "");
        
        p += written; remaining -= written;
    }
    
    written = snprintf(p, remaining, "    ]\n  }\n}\n");
    p += written; remaining -= written;
    
    return json;
}

// Write generated metadata to archive
static uint64_t write_generated_metadata(FILE* out_fp, json_instrument_t* inst) {
    if (!inst) return 0;
    
    char* metadata_json = generate_metadata_json(inst);
    if (!metadata_json) return 0;
    
    uint64_t json_size = strlen(metadata_json);
    
    // Write metadata entry header
    osmp_meta_entry_header_t meta_header = {0};
    snprintf(meta_header.path, sizeof(meta_header.path), "instrument.json");
    meta_header.size = json_size;
    
    fwrite(&meta_header, sizeof(osmp_meta_entry_header_t), 1, out_fp);
    fwrite(metadata_json, json_size, 1, out_fp);
    
    free(metadata_json);
    
    return sizeof(osmp_meta_entry_header_t) + json_size;
}

// Verify OSMP file integrity
static int verify_osmp_file(const char* filepath) {
    InstFS_t* fs = instfs_mount_osmp(filepath, NULL);
    if (!fs) {
        fprintf(stderr, "Failed to mount InstFS partition\n");
        return -1;
    }
    
    uint32_t count = instfs_get_count(fs);
    printf("\nVerification:\n");
    printf("  InstFS mounted successfully\n");
    printf("  Found %u instruments\n", count);
    
    // List first few instruments
    int list_count = count < 5 ? count : 5;
    for (int i = 0; i < list_count; i++) {
        const char* name = instfs_get_name(fs, i);
        uint64_t size = 0;
        instfs_get_data(fs, i, &size);
        printf("    [%d] %s (%llu bytes)\n", i, name, (unsigned long long)size);
    }
    
    if (count > 5) {
        printf("    ... and %u more\n", count - 5);
    }
    
    instfs_unmount(fs);
    
    // Try to mount metadata
    OSMP_Meta_t* meta = osmp_meta_mount(filepath);
    if (meta) {
        uint32_t meta_count = osmp_meta_get_count(meta);
        printf("  Metadata: %u files\n", meta_count);
        
        for (uint32_t i = 0; i < meta_count; i++) {
            const char* name = NULL;
            uint64_t size = 0;
            if (osmp_meta_get_entry(meta, i, &name, &size) == 0) {
                printf("    - %s (%llu bytes)\n", name, (unsigned long long)size);
            }
        }
        
        osmp_meta_unmount(meta);
    }
    
    return 0;
}


/* --- Main Function --- */

int main(int argc, char *argv[]) {
    const char* output_filename = NULL;
    const char* meta_dirname = NULL;
    const char* json_filename = NULL;
    int num_inst_files = 0;
    char** inst_filenames = NULL;
    json_instrument_t* json_inst = NULL;
    
    // Parse command line arguments
    if (argc >= 4 && strcmp(argv[1], "-o") == 0) {
        output_filename = argv[2];
        
        // Check for JSON mode: -j <json_file>
        if (argc >= 5 && strcmp(argv[3], "-j") == 0) {
            json_filename = argv[4];
            
            // Parse JSON file
            json_inst = parse_json_instrument(json_filename);
            if (!json_inst) {
                fprintf(stderr, "Failed to parse JSON file: %s\n", json_filename);
                return 1;
            }
            
            // Collect sample files from JSON
            inst_filenames = collect_sample_files(json_inst, &num_inst_files);
            if (!inst_filenames || num_inst_files == 0) {
                fprintf(stderr, "No valid samples found in JSON file\n");
                free_json_instrument(json_inst);
                return 1;
            }
            
            printf("Parsed JSON: %d regions, %d unique samples\n", 
                   json_inst->num_regions, num_inst_files);
            
            // Use empty metadata for JSON mode
            meta_dirname = NULL;
        }
        // Check for traditional mode: -m <meta_dir> <files...>
        else if (argc >= 6 && strcmp(argv[3], "-m") == 0) {
            meta_dirname = argv[4];
            num_inst_files = argc - 5;
            inst_filenames = &argv[5];
        }
        else {
            goto usage_error;
        }
    } else {
        goto usage_error;
    }

    FILE *out_fp = fopen(output_filename, "wb");
    if (!out_fp) {
        perror("Failed to open output file");
        if (json_inst) {
            if (inst_filenames) {
                for (int i = 0; i < num_inst_files; i++) free(inst_filenames[i]);
                free(inst_filenames);
            }
            free_json_instrument(json_inst);
        }
        return 1;
    }

    // 1. Write placeholder master header
    osmp_master_header_t master_header = {0};
    fwrite(&master_header, sizeof(osmp_master_header_t), 1, out_fp);

    // 2. Write metadata archive (if provided)
    master_header.meta_offset = ftell(out_fp);
    if (meta_dirname) {
        master_header.meta_size = write_meta_files(out_fp, meta_dirname);
    } else if (json_inst) {
        // Generate metadata from JSON
        master_header.meta_size = write_generated_metadata(out_fp, json_inst);
    } else {
        master_header.meta_size = 0;
    }

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
    printf("  - Metadata:     %llu bytes\n", (unsigned long long)master_header.meta_size);
    printf("  - InstrumentFS: %llu bytes (%d instruments)\n", 
           (unsigned long long)master_header.instfs_size, num_inst_files);
    
    // Verify the created file
    printf("\n");
    if (verify_osmp_file(output_filename) == 0) {
        printf("\nOSMP file created and verified successfully!\n");
    }

    // Cleanup
    if (json_inst) {
        if (inst_filenames) {
            for (int i = 0; i < num_inst_files; i++) free(inst_filenames[i]);
            free(inst_filenames);
        }
        free_json_instrument(json_inst);
    }

    return 0;

usage_error:
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s -o <output.osmp> -m <meta_dir> <inst_file1> ...\n", argv[0]);
    fprintf(stderr, "  %s -o <output.osmp> -j <instrument.json>\n", argv[0]);
    return 1;
}
