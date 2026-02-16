/*
 * test_stream - Test program for InstFS streaming API
 * 
 * Usage: test_stream <file.osmp> [instrument_index]
 */

#include "instfs.h"
#include "stream.h"
#include "intro.h"
#include "portability.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifndef _WIN32
#include <unistd.h>
#endif

/* Format memory size */
static void format_memory(long kb, char* buf, size_t buf_size) {
    if (kb < 1024) {
        snprintf(buf, buf_size, "%ld KB", kb);
    } else if (kb < 1024 * 1024) {
        snprintf(buf, buf_size, "%.2f MB", kb / 1024.0);
    } else {
        snprintf(buf, buf_size, "%.2f GB", kb / (1024.0 * 1024.0));
    }
}

/* Print memory usage */
static void print_memory_usage(const char* label) {
    memory_info_t mem;
    if (get_memory_usage(&mem) == 0) {
        char rss_str[64], vsize_str[64], shared_str[64];
        format_memory(mem.rss_kb, rss_str, sizeof(rss_str));
        format_memory(mem.vsize_kb, vsize_str, sizeof(vsize_str));
        format_memory(mem.shared_kb, shared_str, sizeof(shared_str));
        
        printf("  Memory [%s]:\n", label);
        printf("    RSS:    %s (resident)\n", rss_str);
        printf("    VSize:  %s (virtual)\n", vsize_str);
        printf("    Shared: %s (file-backed)\n", shared_str);
    }
}

/* Utility to get current time in microseconds */
static uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/* Format bytes to human-readable string */
static void format_bytes(uint64_t bytes, char* buf, size_t buf_size) {
    if (bytes < 1024) {
        snprintf(buf, buf_size, "%llu B", (unsigned long long)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(buf, buf_size, "%.2f KB", bytes / 1024.0);
    } else if (bytes < 1024 * 1024 * 1024) {
        snprintf(buf, buf_size, "%.2f MB", bytes / (1024.0 * 1024.0));
    } else {
        snprintf(buf, buf_size, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

/* Test 1: Sequential read test */
static void test_sequential_read(InstFS_t* fs, uint32_t index) {
    printf("\n=== Test 1: Sequential Read ===\n");
    
    memory_info_t mem_before, mem_after;
    get_memory_usage(&mem_before);
    
    InstFS_Stream_t* stream = stream_open(fs, index, STREAM_MODE_SEQUENTIAL);
    if (!stream) {
        printf("Failed to open stream\n");
        return;
    }
    
    uint64_t size = stream_size(stream);
    char size_str[64];
    format_bytes(size, size_str, sizeof(size_str));
    printf("Stream size: %s\n", size_str);
    
    print_memory_usage("after open");
    
    /* Read in chunks */
    const size_t CHUNK_SIZE = 4096;
    uint8_t* buffer = malloc(CHUNK_SIZE);
    uint64_t total_read = 0;
    
    uint64_t start_time = get_time_us();
    
    while (!stream_eof(stream)) {
        int64_t bytes_read = stream_read(stream, buffer, CHUNK_SIZE);
        if (bytes_read < 0) {
            printf("Read error\n");
            break;
        }
        if (bytes_read == 0) break;
        total_read += bytes_read;
    }
    
    uint64_t end_time = get_time_us();
    uint64_t elapsed_us = end_time - start_time;
    
    free(buffer);
    
    get_memory_usage(&mem_after);
    
    /* Get statistics */
    stream_stats_t stats;
    stream_get_stats(stream, &stats);
    
    char total_str[64];
    format_bytes(total_read, total_str, sizeof(total_str));
    
    printf("Total read: %s\n", total_str);
    printf("Time: %.3f ms\n", elapsed_us / 1000.0);
    printf("Throughput: %.2f MB/s\n", 
           (total_read / (1024.0 * 1024.0)) / (elapsed_us / 1000000.0));
    printf("Reads: %llu\n", (unsigned long long)stats.num_reads);
    printf("Seeks: %llu\n", (unsigned long long)stats.num_seeks);
    
    print_memory_usage("after read");
    
    long mem_delta = mem_after.rss_kb - mem_before.rss_kb;
    char delta_str[64];
    format_memory(mem_delta > 0 ? mem_delta : 0, delta_str, sizeof(delta_str));
    printf("  Memory delta: %s\n", delta_str);
    
    stream_close(stream);
}

/* Test 2: Random access test */
static void test_random_access(InstFS_t* fs, uint32_t index) {
    printf("\n=== Test 2: Random Access ===\n");
    
    InstFS_Stream_t* stream = stream_open(fs, index, STREAM_MODE_RANDOM);
    if (!stream) {
        printf("Failed to open stream\n");
        return;
    }
    
    uint64_t size = stream_size(stream);
    const size_t CHUNK_SIZE = 1024;
    uint8_t* buffer = malloc(CHUNK_SIZE);
    
    /* Perform random seeks and reads */
    const int NUM_RANDOM_READS = 100;
    srand(time(NULL));
    
    uint64_t start_time = get_time_us();
    
    for (int i = 0; i < NUM_RANDOM_READS; i++) {
        uint64_t random_pos = (uint64_t)rand() % (size - CHUNK_SIZE);
        stream_seek(stream, random_pos, SEEK_SET);
        stream_read(stream, buffer, CHUNK_SIZE);
    }
    
    uint64_t end_time = get_time_us();
    uint64_t elapsed_us = end_time - start_time;
    
    free(buffer);
    
    stream_stats_t stats;
    stream_get_stats(stream, &stats);
    
    printf("Random reads: %d\n", NUM_RANDOM_READS);
    printf("Time: %.3f ms\n", elapsed_us / 1000.0);
    printf("Avg per read: %.3f us\n", elapsed_us / (double)NUM_RANDOM_READS);
    printf("Seeks: %llu\n", (unsigned long long)stats.num_seeks);
    
    stream_close(stream);
}

/* Test 3: Zero-copy access test */
static void test_zero_copy(InstFS_t* fs, uint32_t index) {
    printf("\n=== Test 3: Zero-Copy Access ===\n");
    
    memory_info_t mem_before, mem_after;
    get_memory_usage(&mem_before);
    
    InstFS_Stream_t* stream = stream_open(fs, index, STREAM_MODE_SEQUENTIAL);
    if (!stream) {
        printf("Failed to open stream\n");
        return;
    }
    
    uint64_t size = stream_size(stream);
    uint64_t total_accessed = 0;
    
    print_memory_usage("before zero-copy");
    
    uint64_t start_time = get_time_us();
    
    /* Access data without copying */
    while (!stream_eof(stream)) {
        size_t available;
        const void* ptr = stream_get_ptr(stream, &available);
        if (!ptr || available == 0) break;
        
        /* Simulate processing by reading first byte */
        volatile uint8_t dummy = *(const uint8_t*)ptr;
        (void)dummy;
        
        /* Advance position */
        size_t advance = available < 4096 ? available : 4096;
        stream_seek(stream, advance, SEEK_CUR);
        total_accessed += advance;
    }
    
    uint64_t end_time = get_time_us();
    uint64_t elapsed_us = end_time - start_time;
    
    get_memory_usage(&mem_after);
    
    stream_stats_t stats;
    stream_get_stats(stream, &stats);
    
    char total_str[64];
    format_bytes(total_accessed, total_str, sizeof(total_str));
    
    printf("Total accessed: %s\n", total_str);
    printf("Time: %.3f ms\n", elapsed_us / 1000.0);
    printf("Throughput: %.2f MB/s\n", 
           (total_accessed / (1024.0 * 1024.0)) / (elapsed_us / 1000000.0));
    printf("Cache hits: %llu\n", (unsigned long long)stats.cache_hits);
    
    print_memory_usage("after zero-copy");
    
    long mem_delta = mem_after.rss_kb - mem_before.rss_kb;
    char delta_str[64];
    format_memory(mem_delta > 0 ? mem_delta : 0, delta_str, sizeof(delta_str));
    printf("  Memory delta: %s (should be minimal for zero-copy)\n", delta_str);
    
    stream_close(stream);
}

/* Test 4: Prefetch test */
static void test_prefetch(InstFS_t* fs, uint32_t index) {
    printf("\n=== Test 4: Prefetch Test ===\n");
    
    InstFS_Stream_t* stream = stream_open(fs, index, STREAM_MODE_SEQUENTIAL);
    if (!stream) {
        printf("Failed to open stream\n");
        return;
    }
    
    uint64_t size = stream_size(stream);
    const size_t CHUNK_SIZE = 65536; /* 64KB chunks */
    uint8_t* buffer = malloc(CHUNK_SIZE);
    
    /* Test with prefetch */
    stream_reset_stats(stream);
    stream_seek(stream, 0, SEEK_SET);
    
    uint64_t start_time = get_time_us();
    
    uint64_t pos = 0;
    while (pos < size) {
        /* Prefetch next chunk */
        if (pos + CHUNK_SIZE * 2 < size) {
            stream_prefetch(stream, pos + CHUNK_SIZE, CHUNK_SIZE);
        }
        
        /* Read current chunk */
        int64_t bytes_read = stream_read(stream, buffer, CHUNK_SIZE);
        if (bytes_read <= 0) break;
        pos += bytes_read;
    }
    
    uint64_t end_time = get_time_us();
    uint64_t elapsed_with_prefetch = end_time - start_time;
    
    /* Test without prefetch */
    stream_reset_stats(stream);
    stream_seek(stream, 0, SEEK_SET);
    
    start_time = get_time_us();
    
    pos = 0;
    while (pos < size) {
        int64_t bytes_read = stream_read(stream, buffer, CHUNK_SIZE);
        if (bytes_read <= 0) break;
        pos += bytes_read;
    }
    
    end_time = get_time_us();
    uint64_t elapsed_without_prefetch = end_time - start_time;
    
    free(buffer);
    
    printf("With prefetch:    %.3f ms\n", elapsed_with_prefetch / 1000.0);
    printf("Without prefetch: %.3f ms\n", elapsed_without_prefetch / 1000.0);
    printf("Speedup: %.2fx\n", 
           (double)elapsed_without_prefetch / elapsed_with_prefetch);
    
    stream_close(stream);
}

/* Test 5: Sample reading test */
static void test_sample_reading(InstFS_t* fs, uint32_t index) {
    printf("\n=== Test 5: Sample Reading ===\n");
    
    InstFS_Stream_t* stream = stream_open(fs, index, STREAM_MODE_SEQUENTIAL);
    if (!stream) {
        printf("Failed to open stream\n");
        return;
    }
    
    /* Assume 16-bit stereo samples */
    const size_t SAMPLE_SIZE = 4; /* 2 bytes * 2 channels */
    const size_t NUM_SAMPLES = 1024;
    
    int16_t* samples = malloc(NUM_SAMPLES * SAMPLE_SIZE);
    
    int64_t samples_read = stream_read_samples(stream, samples, 
                                               NUM_SAMPLES, SAMPLE_SIZE);
    
    if (samples_read > 0) {
        printf("Read %lld samples\n", (long long)samples_read);
        
        /* Calculate some basic statistics */
        int64_t sum_left = 0, sum_right = 0;
        int16_t min_left = 32767, max_left = -32768;
        int16_t min_right = 32767, max_right = -32768;
        
        for (int64_t i = 0; i < samples_read; i++) {
            int16_t left = samples[i * 2];
            int16_t right = samples[i * 2 + 1];
            
            sum_left += left;
            sum_right += right;
            
            if (left < min_left) min_left = left;
            if (left > max_left) max_left = left;
            if (right < min_right) min_right = right;
            if (right > max_right) max_right = right;
        }
        
        printf("Left channel:  avg=%d, min=%d, max=%d\n",
               (int)(sum_left / samples_read), min_left, max_left);
        printf("Right channel: avg=%d, min=%d, max=%d\n",
               (int)(sum_right / samples_read), min_right, max_right);
    }
    
    free(samples);
    stream_close(stream);
}

/* Convert frequency to MIDI note number */
static int frequency_to_midi(unsigned int freq) {
    if (freq == 0) return 60; // Default to middle C
    
    // MIDI note = 69 + 12 * log2(freq / 440)
    double note = 69.0 + 12.0 * log2((double)freq / 440.0);
    return (int)round(note);
}

/* Get note name from MIDI number */
static const char* midi_to_note_name(int midi) {
    static const char* note_names[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    int octave = (midi / 12) - 1;
    int note = midi % 12;
    static char buf[8];
    snprintf(buf, sizeof(buf), "%s%d", note_names[note], octave);
    return buf;
}

/* Simulate playing a note from the instrument */
static void play_note_simulation(InstFS_t* fs, uint32_t inst_index, 
                                 unsigned int frequency, unsigned int duration_ms) {
    int midi_note = frequency_to_midi(frequency);
    const char* note_name = midi_to_note_name(midi_note);
    
    printf("  [%4dms] Playing %s (%dHz, MIDI %d) for %dms\n", 
           0, note_name, frequency, midi_note, duration_ms);
    
    // Open stream for the instrument
    InstFS_Stream_t* stream = stream_open(fs, inst_index, STREAM_MODE_RANDOM);
    if (!stream) {
        printf("    ERROR: Failed to open stream\n");
        return;
    }
    
    uint64_t inst_size = stream_size(stream);
    
    // Simulate finding the right sample for this note
    // In a real implementation, this would use the JSON metadata to find
    // the appropriate sample based on key_range and vel_range
    
    // For simulation, just read a chunk of data from the instrument
    const size_t SAMPLE_CHUNK = 4096;
    uint8_t* sample_data = malloc(SAMPLE_CHUNK);
    
    // Seek to a position based on the MIDI note (simple simulation)
    uint64_t seek_pos = ((uint64_t)midi_note * inst_size) / 128;
    if (seek_pos + SAMPLE_CHUNK > inst_size) {
        seek_pos = inst_size > SAMPLE_CHUNK ? inst_size - SAMPLE_CHUNK : 0;
    }
    
    stream_seek(stream, seek_pos, SEEK_SET);
    int64_t bytes_read = stream_read(stream, sample_data, SAMPLE_CHUNK);
    
    if (bytes_read > 0) {
        // Calculate some statistics about the sample data
        int16_t* samples = (int16_t*)sample_data;
        size_t num_samples = bytes_read / 2;
        
        int64_t sum = 0;
        int16_t min_val = 32767, max_val = -32768;
        
        for (size_t i = 0; i < num_samples; i++) {
            int16_t val = samples[i];
            sum += abs(val);
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
        
        int avg_amplitude = (int)(sum / num_samples);
        int peak_amplitude = max_val > abs(min_val) ? max_val : abs(min_val);
        
        printf("    Sample data: %lld bytes read from offset %llu\n", 
               (long long)bytes_read, (unsigned long long)seek_pos);
        printf("    Amplitude: avg=%d, peak=%d, range=[%d, %d]\n",
               avg_amplitude, peak_amplitude, min_val, max_val);
        
        // Simulate note envelope (ADSR visualization)
        int attack_ms = 10;
        int decay_ms = 50;
        int sustain_level = 80; // percent
        int release_ms = 100;
        
        printf("    Envelope: A=%dms D=%dms S=%d%% R=%dms\n",
               attack_ms, decay_ms, sustain_level, release_ms);
        
        // Show hex dump of first 32 bytes
        printf("    Hex dump (first 32 bytes):\n      ");
        size_t dump_size = bytes_read < 32 ? bytes_read : 32;
        for (size_t i = 0; i < dump_size; i++) {
            printf("%02X ", sample_data[i]);
            if ((i + 1) % 16 == 0 && i < dump_size - 1) printf("\n      ");
        }
        printf("\n");
    } else {
        printf("    ERROR: Failed to read sample data\n");
    }
    
    free(sample_data);
    stream_close(stream);
}

/* Test 6: Melody playback simulation */
static void test_melody_playback(InstFS_t* fs, uint32_t index, float tempo_bpm) {
    printf("\n=== Test 6: Melody Playback Simulation (Live Mode) ===\n");
    printf("Playing melody with %d notes...\n\n", melody_length);
    
    memory_info_t mem_before, mem_after;
    get_memory_usage(&mem_before);
    
    // Calculate tempo multiplier from BPM
    // Original tempo: 400000 us/beat = 150 BPM (60000000 / 400000)
    float original_bpm = 60000000.0f / tempo;
    float tempo_multiplier = 1.0f;
    
    if (tempo_bpm > 0) {
        tempo_multiplier = tempo_bpm / original_bpm;
        printf("Original tempo: %d us/beat (%.1f BPM)\n", tempo, original_bpm);
        printf("Target tempo: %.1f BPM\n", tempo_bpm);
        printf("Speed multiplier: %.2fx\n\n", tempo_multiplier);
    } else {
        tempo_bpm = original_bpm;
        printf("Original tempo: %d us/beat (%.1f BPM)\n\n", tempo, original_bpm);
    }
    
    unsigned int original_duration = melody[melody_length - 1].start_time + 
                                     melody[melody_length - 1].duration;
    unsigned int adjusted_duration_total = (unsigned int)(original_duration / tempo_multiplier);
    
    printf("Melody will play for approximately %d ms (%.1f seconds)\n", 
           adjusted_duration_total, adjusted_duration_total / 1000.0);
    printf("Note duration: %d ms (0x%04X)\n", melody[0].duration, melody[0].duration);
    printf("Press Ctrl+C to stop playback\n\n");
    printf("Starting playback in 3...\n");
    usleep(1000000);
    printf("2...\n");
    usleep(1000000);
    printf("1...\n");
    usleep(1000000);
    printf("\n*** PLAYBACK STARTED ***\n\n");
    
    uint64_t playback_start_time = get_time_us();
    uint64_t last_note_time = playback_start_time;
    
    // Play each note in the melody with real-time delays
    for (int i = 0; i < melody_length; i++) {
        const struct Note* note = &melody[i];
        
        // Adjust timing based on tempo multiplier
        unsigned int adjusted_start = (unsigned int)(note->start_time / tempo_multiplier);
        unsigned int adjusted_duration = (unsigned int)(note->duration / tempo_multiplier);
        
        // Calculate when this note should start (in microseconds from playback start)
        uint64_t target_time = playback_start_time + (adjusted_start * 1000ULL);
        uint64_t current_time = get_time_us();
        
        // Wait until it's time to play this note
        if (current_time < target_time) {
            uint64_t wait_us = target_time - current_time;
            unsigned int wait_ms = (unsigned int)(wait_us / 1000);
            
            if (wait_ms > 0) {
                printf("  [Waiting %dms until next note...]\n", wait_ms);
                usleep(wait_us);
            }
        }
        
        // Get actual time when note starts
        uint64_t actual_start_time = get_time_us();
        unsigned int elapsed_ms = (unsigned int)((actual_start_time - playback_start_time) / 1000);
        unsigned int time_since_last = (unsigned int)((actual_start_time - last_note_time) / 1000);
        
        printf("\n[T=%dms] Note %d/%d (delta: %dms, duration: %dms):\n", 
               elapsed_ms, i + 1, melody_length, time_since_last, adjusted_duration);
        play_note_simulation(fs, index, note->frequency, adjusted_duration);
        
        last_note_time = actual_start_time;
        
        // Show progress bar
        int progress = ((i + 1) * 100) / melody_length;
        printf("  Progress: [");
        for (int p = 0; p < 50; p++) {
            if (p < progress / 2) printf("=");
            else if (p == progress / 2) printf(">");
            else printf(" ");
        }
        printf("] %d%%\n", progress);
    }
    
    uint64_t playback_end_time = get_time_us();
    uint64_t total_elapsed_us = playback_end_time - playback_start_time;
    
    printf("\n*** PLAYBACK FINISHED ***\n\n");
    
    get_memory_usage(&mem_after);
    
    printf("=== Melody Playback Complete ===\n");
    printf("Total notes: %d\n", melody_length);
    printf("Expected duration: %d ms\n", adjusted_duration_total);
    printf("Actual duration: %.3f ms\n", total_elapsed_us / 1000.0);
    printf("Timing accuracy: %.2f%%\n", 
           (adjusted_duration_total * 100.0) / (total_elapsed_us / 1000.0));
    printf("Avg time per note: %.3f ms\n", total_elapsed_us / (1000.0 * melody_length));
    printf("Effective BPM: %.1f\n", 
           (60000000.0 * tempo_multiplier) / tempo);
    
    print_memory_usage("after melody");
    
    long mem_delta = mem_after.rss_kb - mem_before.rss_kb;
    char delta_str[64];
    format_memory(mem_delta > 0 ? mem_delta : 0, delta_str, sizeof(delta_str));
    printf("  Memory delta: %s\n", delta_str);
}

/* Test 7: Note range analysis */
static void test_note_range_analysis(InstFS_t* fs, uint32_t index) {
    printf("\n=== Test 7: Note Range Analysis ===\n");
    
    // Analyze the frequency range in the melody
    unsigned int min_freq = melody[0].frequency;
    unsigned int max_freq = melody[0].frequency;
    
    for (int i = 1; i < melody_length; i++) {
        if (melody[i].frequency < min_freq) min_freq = melody[i].frequency;
        if (melody[i].frequency > max_freq) max_freq = melody[i].frequency;
    }
    
    int min_midi = frequency_to_midi(min_freq);
    int max_midi = frequency_to_midi(max_freq);
    
    printf("Frequency range: %dHz - %dHz\n", min_freq, max_freq);
    printf("MIDI range: %d (%s) - %d (%s)\n", 
           min_midi, midi_to_note_name(min_midi),
           max_midi, midi_to_note_name(max_midi));
    printf("Range span: %d semitones\n", max_midi - min_midi);
    
    // Count unique notes
    int note_histogram[128] = {0};
    for (int i = 0; i < melody_length; i++) {
        int midi = frequency_to_midi(melody[i].frequency);
        note_histogram[midi]++;
    }
    
    printf("\nNote frequency distribution:\n");
    for (int i = 0; i < 128; i++) {
        if (note_histogram[i] > 0) {
            printf("  %s (MIDI %d): %d times (%.1f%%)\n",
                   midi_to_note_name(i), i, note_histogram[i],
                   100.0 * note_histogram[i] / melody_length);
        }
    }
    
    (void)fs;
    (void)index;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.osmp> [instrument_index] [--tempo <BPM>]\n", argv[0]);
        fprintf(stderr, "  --tempo: Tempo in BPM (e.g., 140 for 140 BPM, default is 150 BPM)\n");
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  %s piano.osmp 0              # Play at original tempo (150 BPM)\n", argv[0]);
        fprintf(stderr, "  %s piano.osmp 0 --tempo 140  # Play at 140 BPM\n", argv[0]);
        fprintf(stderr, "  %s piano.osmp 0 --tempo 200  # Play at 200 BPM (faster)\n", argv[0]);
        return 1;
    }

    const char* filepath = argv[1];
    uint32_t index = 0;
    float tempo_bpm = 0.0f; // 0 means use original tempo
    
    // Parse arguments
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--tempo") == 0 && i + 1 < argc) {
            tempo_bpm = atof(argv[i + 1]);
            if (tempo_bpm <= 0.0f) {
                fprintf(stderr, "Invalid tempo: %s (must be > 0 BPM)\n", argv[i + 1]);
                return 1;
            }
            i++; // Skip the next argument
        } else if (argv[i][0] != '-') {
            // Assume it's the instrument index
            index = atoi(argv[i]);
        }
    }
    
    printf("========================================\n");
    printf("InstFS Stream API Test\n");
    printf("========================================\n");
    printf("File: %s\n", filepath);
    
    if (tempo_bpm > 0) {
        printf("Tempo: %.1f BPM\n", tempo_bpm);
    }
    
    /* Show initial memory usage */
    print_memory_usage("initial");
    
    /* Mount filesystem */
    InstFS_t* fs = instfs_mount_osmp(filepath, NULL);
    if (!fs) {
        fprintf(stderr, "Failed to mount OSMP file\n");
        return 1;
    }
    
    uint32_t count = instfs_get_count(fs);
    printf("Instruments: %u\n", count);
    
    print_memory_usage("after mount");
    
    if (index >= count) {
        fprintf(stderr, "Invalid instrument index: %u (max: %u)\n", index, count - 1);
        instfs_unmount(fs);
        return 1;
    }
    
    const char* name = instfs_get_name(fs, index);
    uint64_t size = 0;
    instfs_get_data(fs, index, &size);
    
    char size_str[64];
    format_bytes(size, size_str, sizeof(size_str));
    
    printf("Testing instrument [%u]: %s (%s)\n", index, name, size_str);
    
    /* Run tests */
    test_sequential_read(fs, index);
    test_random_access(fs, index);
    test_zero_copy(fs, index);
    test_prefetch(fs, index);
    test_sample_reading(fs, index);
    test_melody_playback(fs, index, tempo_bpm);
    test_note_range_analysis(fs, index);
    
    printf("\n========================================\n");
    printf("All tests completed\n");
    printf("========================================\n");
    
    print_memory_usage("final");
    
    instfs_unmount(fs);
    return 0;
}
