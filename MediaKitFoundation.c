/*
 * MediaKitFoundation - Cross-platform foundation library implementation
 */

#include "MediaKitFoundation.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ========================================
 * Memory Mapping Implementation
 * ======================================== */

#if MKF_PLATFORM_WINDOWS

void* mkf_mmap_file(const char* filepath, size_t* size, void** handle) {
    mkf_mmap_handle_t* h = (mkf_mmap_handle_t*)malloc(sizeof(mkf_mmap_handle_t));
    if (!h) return NULL;

    h->hFile = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, 
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h->hFile == INVALID_HANDLE_VALUE) {
        free(h);
        return NULL;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(h->hFile, &fileSize)) {
        CloseHandle(h->hFile);
        free(h);
        return NULL;
    }
    
    if (fileSize.QuadPart > SIZE_MAX) {
        CloseHandle(h->hFile);
        free(h);
        return NULL;
    }
    
    *size = (size_t)fileSize.QuadPart;

    DWORD dwMaximumSizeHigh = (DWORD)(fileSize.QuadPart >> 32);
    DWORD dwMaximumSizeLow = (DWORD)(fileSize.QuadPart & 0xFFFFFFFF);
    
    h->hMapping = CreateFileMappingA(h->hFile, NULL, PAGE_READONLY, 
                                     dwMaximumSizeHigh, dwMaximumSizeLow, NULL);
    if (h->hMapping == NULL) {
        CloseHandle(h->hFile);
        free(h);
        return NULL;
    }

    h->ptr = MapViewOfFile(h->hMapping, FILE_MAP_READ, 0, 0, 0);
    if (h->ptr == NULL) {
        CloseHandle(h->hMapping);
        CloseHandle(h->hFile);
        free(h);
        return NULL;
    }

    *handle = h;
    return h->ptr;
}

void mkf_unmap_file(void* handle) {
    if (!handle) return;
    mkf_mmap_handle_t* h = (mkf_mmap_handle_t*)handle;
    if (h->ptr) UnmapViewOfFile(h->ptr);
    if (h->hMapping) CloseHandle(h->hMapping);
    if (h->hFile) CloseHandle(h->hFile);
    free(h);
}

#else /* POSIX */

void* mkf_mmap_file(const char* filepath, size_t* size, void** handle) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return NULL;

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return NULL;
    }

    *size = st.st_size;
    void* ptr = mmap(NULL, *size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (ptr == MAP_FAILED) {
        return NULL;
    }

    *handle = ptr;
    return ptr;
}

void mkf_unmap_file(void* handle) {
    /* On POSIX, we need to track the size separately for munmap */
    /* For now, this is a simplified version - modules should track size */
    (void)handle;
}

#endif

/* ========================================
 * Directory Operations Implementation
 * ======================================== */

#if MKF_PLATFORM_WINDOWS

mkf_dir_t* mkf_opendir(const char* path) {
    mkf_dir_t* dir = (mkf_dir_t*)malloc(sizeof(mkf_dir_t));
    if (!dir) return NULL;

    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    dir->hFind = FindFirstFileA(search_path, &dir->findData);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }

    dir->first = 1;
    return dir;
}

struct mkf_dirent* mkf_readdir(mkf_dir_t* dir) {
    if (!dir) return NULL;

    if (dir->first) {
        dir->first = 0;
    } else {
        if (!FindNextFileA(dir->hFind, &dir->findData)) {
            return NULL;
        }
    }

    strncpy(dir->d_name, dir->findData.cFileName, MAX_PATH - 1);
    dir->d_name[MAX_PATH - 1] = '\0';

    static struct mkf_dirent entry;
    strncpy(entry.d_name, dir->d_name, MAX_PATH - 1);
    entry.d_name[MAX_PATH - 1] = '\0';

    return &entry;
}

int mkf_closedir(mkf_dir_t* dir) {
    if (!dir) return -1;
    if (dir->hFind != INVALID_HANDLE_VALUE) {
        FindClose(dir->hFind);
    }
    free(dir);
    return 0;
}

#endif

/* ========================================
 * Time Operations Implementation
 * ======================================== */

#if MKF_PLATFORM_WINDOWS

int mkf_gettimeofday(struct mkf_timeval* tv, void* tz) {
    (void)tz;
    FILETIME ft;
    uint64_t tmpres = 0;

    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    tmpres /= 10;
    tmpres -= 11644473600000000ULL;

    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);

    return 0;
}

#endif

/* ========================================
 * Memory Information Implementation
 * ======================================== */

#if MKF_PLATFORM_WINDOWS

int mkf_get_memory_usage(mkf_memory_info_t* info) {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        info->rss_kb = pmc.WorkingSetSize / 1024;
        info->vsize_kb = pmc.PrivateUsage / 1024;
        info->shared_kb = 0;
        return 0;
    }
    return -1;
}

#else /* POSIX */

int mkf_get_memory_usage(mkf_memory_info_t* info) {
    FILE* fp = fopen("/proc/self/status", "r");
    if (!fp) return -1;
    
    char line[256];
    info->rss_kb = 0;
    info->vsize_kb = 0;
    info->shared_kb = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &info->rss_kb);
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%ld", &info->vsize_kb);
        } else if (strncmp(line, "RssFile:", 8) == 0) {
            sscanf(line + 8, "%ld", &info->shared_kb);
        }
    }
    
    fclose(fp);
    return 0;
}

#endif

/* ========================================
 * System Information Implementation
 * ======================================== */

long mkf_get_page_size(void) {
#if MKF_PLATFORM_WINDOWS
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
#else
    return sysconf(_SC_PAGE_SIZE);
#endif
}
