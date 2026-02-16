# InstFS - Instrument File System

InstFS is a high-performance, read-only virtual filesystem designed to store and access musical instrument data. It packages instrument samples and metadata into a single `.osmp` container file, which can be efficiently read by applications or mounted as a standard filesystem using FUSE.

## Features

- **High-Performance:** Uses memory mapping (`mmap`) and kernel-level optimizations (`madvice`) for fast, zero-copy access to instrument data.
- **Streaming API:** Provides a C-level streaming API for sequential or random access to sample data, including performance statistics.
- **Cross-Platform:** The core library is written in standard C and should be portable to any POSIX-compliant system.
- **FUSE Driver:** Includes a FUSE driver (`instfs_fuse`) to mount `.osmp` files as a regular directory, making instruments accessible to any application.
- **Comprehensive Toolchain:** Comes with a full suite of tools for creating, inspecting, and testing filesystem images.
- **SFZ Support:** Includes a Python script to convert instruments from the popular SFZ format into the JSON format used by the toolchain.

## Architecture

The project consists of three main components:

1.  **`libinstfs`:** A C library that provides the core API for reading data from an `.osmp` file.
2.  **Tools:**
    -   `mkfs.osmp`: Creates `.osmp` container files from instrument samples and metadata.
    -   `inspect_osmp`: A utility to inspect the contents of an `.osmp` file.
    -   `test_stream`: A powerful tool for benchmarking the streaming API and simulating instrument playback.
    -   `instfs_fuse`: A FUSE driver to mount an `.osmp` file.
3.  **Scripts:**
    -   `sfz_to_json.py`: Converts SFZ instrument definitions to JSON.
    -   `midtohex.py`: Converts a MIDI file to a C header for use in the `test_stream` simulation.
    -   `verify_json_samples.py`: A utility to check for missing sample files in a JSON instrument definition.

An `.osmp` file has two main parts:

-   **Metadata Partition:** A simple archive for storing files like `instrument.json`.
-   **InstFS Partition:** A dedicated filesystem for the instrument sample data, optimized for fast access.

## Building

### Dependencies

-   **`gcc`**, **`make`**, `as`, `ar`
-   **`pkg-config`**
-   **`libfuse-dev`** (or equivalent for your system, e.g., `fuse-devel`, `osxfuse`)

### Compilation

To build the library and all tools, simply run `make`:

```bash
make
```

This will create a `build` directory containing the compiled library (`libinstfs.a`, `libinstfs.so`) and the command-line tools.

To install the library and tools into `/usr/local`, run:

```bash
sudo make install
```

## Usage

The typical workflow is:

1.  Define your instrument in SFZ format or create a JSON file by hand.
2.  Use `sfz_to_json.py` to convert your SFZ file to JSON (if applicable).
3.  Use `verify_json_samples.py` to ensure all your sample files exist.
4.  Use `mkfs.osmp` to package your instrument into an `.osmp` file.
5.  Use `inspect_osmp` to verify the contents of the `.osmp` file.
6.  Mount the `.osmp` file with `instfs_fuse` or use it directly in an application via `libinstfs`.

### Example Workflow

1.  **Convert an SFZ instrument to JSON:**

    ```bash
    python3 scripts/sfz_to_json.py path/to/my_instrument.sfz
    ```
    This will create `my_instrument.json` in the same directory.

2.  **Verify the JSON:**

    ```bash
    python3 scripts/verify_json_samples.py path/to/my_instrument.json
    ```

3.  **Create the OSMP file:**

    ```bash
    ./build/mkfs.osmp -o my_instrument.osmp -j path/to/my_instrument.json
    ```

4.  **Inspect the OSMP file:**

    ```bash
    ./build/inspect_osmp my_instrument.osmp
    ```

5.  **Mount the filesystem with FUSE:**

    ```bash
    mkdir -p /tmp/my_instrument_mount
    ./build/instfs_fuse my_instrument.osmp /tmp/my_instrument_mount
    
    # List the mounted instruments
    ls -l /tmp/my_instrument_mount
    
    # Unmount when done
    fusermount -u /tmp/my_instrument_mount
    ```
