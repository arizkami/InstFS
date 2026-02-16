#ifndef PORTABILITY_H
#define PORTABILITY_H

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <io.h>
    #include <stdio.h>

    // Redefine open, read, close for Windows
    #define open _open
    #define read _read
    #define close _close

    // Memory mapping
    typedef struct {
        HANDLE hFile;
        HANDLE hMapping;
        void* ptr;
    } mmap_handle_t;

    void* mmap_file(const char* filepath, size_t* size, mmap_handle_t* handle);
    void unmap_file(mmap_handle_t* handle);

    long get_page_size();

#else // POSIX
    #include <sys/mman.h>
    #include <unistd.h>
    #include <fcntl.h>

    long get_page_size();

#endif // _WIN32

#endif // PORTABILITY_H
