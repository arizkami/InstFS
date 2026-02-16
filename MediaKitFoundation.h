/*
 * MediaKitFoundation - Cross-platform foundation library
 * Provides portability layer for Linux, Windows, and BSD
 */

#ifndef MEDIAKITFOUNDATION_H
#define MEDIAKITFOUNDATION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
    #define MKF_PLATFORM_WINDOWS 1
    #define MKF_PLATFORM_POSIX 0
#elif defined(__linux__)
    #define MKF_PLATFORM_LINUX 1
    #define MKF_PLATFORM_POSIX 1
    #define MKF_PLATFORM_WINDOWS 0
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    #define MKF_PLATFORM_BSD 1
    #define MKF_PLATFORM_POSIX 1
    #define MKF_PLATFORM_WINDOWS 0
#elif defined(__APPLE__)
    #define MKF_PLATFORM_MACOS 1
    #define MKF_PLATFORM_POSIX 1
    #define MKF_PLATFORM_WINDOWS 0
#else
    #define MKF_PLATFORM_POSIX 1
    #define MKF_PLATFORM_WINDOWS 0
#endif

/* Platform-specific includes */
#if MKF_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <psapi.h>
    #include <io.h>
    #include <sys/stat.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <sys/time.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <dirent.h>
#endif

/* ========================================
 * File I/O Portability
 * ======================================== */

#if MKF_PLATFORM_WINDOWS
    #define mkf_open _open
    #define mkf_read _read
    #define mkf_close _close
    #define mkf_stat _stat
    #define mkf_stat_t struct _stat
    #define MKF_S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#else
    #define mkf_open open
    #define mkf_read read
    #define mkf_close close
    #define mkf_stat stat
    #define mkf_stat_t struct stat
    #define MKF_S_ISDIR(m) S_ISDIR(m)
#endif

/* ========================================
 * Memory Mapping
 * ======================================== */

#if MKF_PLATFORM_WINDOWS
typedef struct {
    HANDLE hFile;
    HANDLE hMapping;
    void* ptr;
} mkf_mmap_handle_t;
#endif

void* mkf_mmap_file(const char* filepath, size_t* size, void** handle);
void mkf_unmap_file(void* handle);

/* ========================================
 * Directory Operations
 * ======================================== */

#if MKF_PLATFORM_WINDOWS
typedef struct {
    HANDLE hFind;
    WIN32_FIND_DATAA findData;
    int first;
    char d_name[MAX_PATH];
} mkf_dir_t;

struct mkf_dirent {
    char d_name[MAX_PATH];
};

mkf_dir_t* mkf_opendir(const char* path);
struct mkf_dirent* mkf_readdir(mkf_dir_t* dir);
int mkf_closedir(mkf_dir_t* dir);
#else
#define mkf_dir_t DIR
#define mkf_dirent dirent
#define mkf_opendir opendir
#define mkf_readdir readdir
#define mkf_closedir closedir
#endif

/* ========================================
 * Time Operations
 * ======================================== */

#if MKF_PLATFORM_WINDOWS
struct mkf_timeval {
    long tv_sec;
    long tv_usec;
};
int mkf_gettimeofday(struct mkf_timeval* tv, void* tz);
#define mkf_usleep(us) Sleep((us) / 1000)
#else
#define mkf_timeval timeval
#define mkf_gettimeofday gettimeofday
#define mkf_usleep usleep
#endif

/* ========================================
 * Memory Information
 * ======================================== */

typedef struct {
    long rss_kb;      /* Resident Set Size in KB */
    long vsize_kb;    /* Virtual memory size in KB */
    long shared_kb;   /* Shared memory in KB */
} mkf_memory_info_t;

int mkf_get_memory_usage(mkf_memory_info_t* info);

/* ========================================
 * System Information
 * ======================================== */

long mkf_get_page_size(void);

#ifdef __cplusplus
}
#endif

#endif /* MEDIAKITFOUNDATION_H */
