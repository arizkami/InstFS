#ifndef PORTABILITY_H
#define PORTABILITY_H

#include <stdint.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <psapi.h>
    #include <io.h>
    #include <stdio.h>
    #include <sys/stat.h>

    // Redefine open, read, close for Windows
    #define open _open
    #define read _read
    #define close _close
    #define stat _stat
    #define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)

    // Sleep function
    #define usleep(us) Sleep((us) / 1000)

    // Memory mapping
    typedef struct {
        HANDLE hFile;
        HANDLE hMapping;
        void* ptr;
    } mmap_handle_t;

    void* mmap_file(const char* filepath, size_t* size, mmap_handle_t* handle);
    void unmap_file(mmap_handle_t* handle);

    long get_page_size();

    // Directory operations
    typedef struct {
        HANDLE hFind;
        WIN32_FIND_DATAA findData;
        int first;
        char d_name[MAX_PATH];
    } DIR;

    struct dirent {
        char d_name[MAX_PATH];
    };

    DIR* opendir(const char* path);
    struct dirent* readdir(DIR* dir);
    int closedir(DIR* dir);

    // Time operations
    struct timeval {
        long tv_sec;
        long tv_usec;
    };

    int gettimeofday(struct timeval* tv, void* tz);

    // Memory info
    typedef struct {
        long rss_kb;
        long vsize_kb;
        long shared_kb;
    } memory_info_t;

    int get_memory_usage(memory_info_t* info);

#else // POSIX
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <dirent.h>

    long get_page_size();

    // Memory info
    typedef struct {
        long rss_kb;
        long vsize_kb;
        long shared_kb;
    } memory_info_t;

    int get_memory_usage(memory_info_t* info);

#endif // _WIN32

#endif // PORTABILITY_H
