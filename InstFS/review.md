# InstFS Repository Review (2026-02)

This review focuses on build health, portability, API/tooling quality, and maintainability based on the current tree.

## What looks strong

1. **Clear modular structure**
   - The workspace has a top-level foundation library plus module-level builds (`InstFS`, `DAUx`), which keeps concerns separated cleanly. The module list and orchestration are explicit in the root build. 

2. **InstFS module currently builds successfully in isolation**
   - The `InstFS` submodule compiles and links (`libinstfs.a`, `libinstfs.so`, and CLI tools). That gives confidence that the core filesystem work is in good shape.

3. **Toolchain completeness is good**
   - The project includes C utilities (`mkfs.osmp`, `inspect_osmp`, `test_stream`) and Python asset scripts, which is a strong end-to-end developer workflow.

## Key findings / risks

1. **Root build is brittle due to hard dependency on DAUx ALSA headers**
   - The root `Makefile` always builds both `InstFS` and `DAUx` via `MODULES := InstFS DAUx` and exits on first module failure.
   - In this environment, `DAUx` fails to compile because `<alsa/asoundlib.h>` is unavailable, causing `make` at repo root to fail even though InstFS itself builds.
   - **Impact:** first-time contributors cannot run a clean root build unless audio dev packages are preinstalled.

2. **Linker executable-stack warning from assembly object**
   - Linking InstFS artifacts emits warnings that `headerfs.o` is missing `.note.GNU-stack`.
   - **Impact:** modern toolchains treat this increasingly strictly; future linkers may hard-fail.

3. **`test_stream` note-name formatting warning and potential edge behavior**
   - `midi_to_note_name()` writes into `static char buf[8]` with `snprintf("%s%d", ...)`, which currently triggers `-Wformat-truncation` during build optimization.
   - While practical values may be small, the warning indicates compiler-visible paths where truncation can occur.

4. **Developer UX issue: `test_stream --help` interpreted as a filename**
   - Running `./build/test_stream --help` attempts to mount `--help` as an OSMP file and exits with “Failed to mount OSMP file”.
   - **Impact:** command discoverability is weak; users cannot quickly find invocation options.

## Recommendations (priority order)

1. **Make DAUx optional at top-level build**
   - Add feature flags or environment switches (e.g., `WITH_DAUX=0`) and/or detect ALSA headers in DAUx make logic.
   - Keep default build resilient: build available modules and print a clear skipped-module summary.

2. **Fix GNU-stack annotation in assembly**
   - Add the appropriate non-executable stack note in `headerfs.S` (and DAUx assembly where relevant).

3. **Resolve `midi_to_note_name()` truncation warning**
   - Increase buffer size and/or clamp/validate midi inputs before formatting.

4. **Add explicit CLI usage handling in `test_stream`**
   - Handle `-h/--help` before attempting file operations.

5. **Document dependency tiers**
   - In the README and/or build output, separate mandatory dependencies (for InstFS) from optional module dependencies (DAUx/ALSA/FUSE variants).

## Commands run for this review

- `make -j4` (root): **failed** at DAUx due to missing ALSA headers.
- `make -j4` inside `InstFS/`: **pass**.
- `./build/mkfs.osmp` inside `InstFS/`: printed usage.
- `./build/test_stream --help` inside `InstFS/`: treated as input file and failed to mount.

