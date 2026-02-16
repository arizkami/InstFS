#include "portability.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32

void* mmap_file(const char* filepath, size_t* size, mmap_handle_t* handle) {
    handle->hFile = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle->hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    // Use GetFileSizeEx for 64-bit file size support
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(handle->hFile, &fileSize)) {
        CloseHandle(handle->hFile);
        return NULL;
    }
    
    // Check if file size fits in size_t (important for 32-bit builds)
    if (fileSize.QuadPart > SIZE_MAX) {
        CloseHandle(handle->hFile);
        return NULL;
    }
    
    *size = (size_t)fileSize.QuadPart;

    // For files larger than 4GB, we need to pass the high DWORD
    DWORD dwMaximumSizeHigh = (DWORD)(fileSize.QuadPart >> 32);
    DWORD dwMaximumSizeLow = (DWORD)(fileSize.QuadPart & 0xFFFFFFFF);
    
    handle->hMapping = CreateFileMappingA(handle->hFile, NULL, PAGE_READONLY, 
                                          dwMaximumSizeHigh, dwMaximumSizeLow, NULL);
    if (handle->hMapping == NULL) {
        CloseHandle(handle->hFile);
        return NULL;
    }

    handle->ptr = MapViewOfFile(handle->hMapping, FILE_MAP_READ, 0, 0, 0);
    if (handle->ptr == NULL) {
        CloseHandle(handle->hMapping);
        CloseHandle(handle->hFile);
        return NULL;
    }

    return handle->ptr;
}

void unmap_file(mmap_handle_t* handle) {
    if (handle->ptr) {
        UnmapViewOfFile(handle->ptr);
    }
    if (handle->hMapping) {
        CloseHandle(handle->hMapping);
    }
    if (handle->hFile) {
        CloseHandle(handle->hFile);
    }
}

long get_page_size() {
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    return sys_info.dwPageSize;
}

DIR* opendir(const char* path) {
    DIR* dir = (DIR*)malloc(sizeof(DIR));
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

struct dirent* readdir(DIR* dir) {
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

    static struct dirent entry;
    strncpy(entry.d_name, dir->d_name, MAX_PATH - 1);
    entry.d_name[MAX_PATH - 1] = '\0';

    return &entry;
}

int closedir(DIR* dir) {
    if (!dir) return -1;
    if (dir->hFind != INVALID_HANDLE_VALUE) {
        FindClose(dir->hFind);
    }
    free(dir);
    return 0;
}

int gettimeofday(struct timeval* tv, void* tz) {
    (void)tz;
    FILETIME ft;
    uint64_t tmpres = 0;

    GetSystemTimeAsFileTime(&ft);

    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;

    // Convert to microseconds
    tmpres /= 10;
    // Convert from Windows epoch (1601) to Unix epoch (1970)
    tmpres -= 11644473600000000ULL;

    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);

    return 0;
}

int get_memory_usage(memory_info_t* info) {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        info->rss_kb = pmc.WorkingSetSize / 1024;
        info->vsize_kb = pmc.PrivateUsage / 1024;
        
        // On Windows, memory-mapped files don't show up in these metrics
        // WorkingSetSize = physical memory in use (includes mapped files that are paged in)
        // PrivateUsage = committed private memory (not including mapped files)
        // We can't reliably calculate shared memory on Windows like Linux does
        info->shared_kb = 0;
        
        return 0;
    }
    return -1;
}

#else // POSIX

#include <unistd.h>

long get_page_size() {
    return sysconf(_SC_PAGE_SIZE);
}

int get_memory_usage(memory_info_t* info) {
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

#endif // _WIN32
