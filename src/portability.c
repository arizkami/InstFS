#include "portability.h"

#ifdef _WIN32

void* mmap_file(const char* filepath, size_t* size, mmap_handle_t* handle) {
    handle->hFile = CreateFile(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle->hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    *size = GetFileSize(handle->hFile, NULL);
    if (*size == INVALID_FILE_SIZE) {
        CloseHandle(handle->hFile);
        return NULL;
    }

    handle->hMapping = CreateFileMapping(handle->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
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

#else // POSIX

#include <unistd.h>

long get_page_size() {
    return sysconf(_SC_PAGE_SIZE);
}

#endif // _WIN32
