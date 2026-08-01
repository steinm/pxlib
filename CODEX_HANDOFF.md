# Codex handoff: pxlib / pypxlib performance work

Last updated: 2026-08-01 (America/Bogota)

## Goal

Make Paradox DB reads fast enough that pypxlib is not the bottleneck while
exporting into PostgreSQL. The test databases are in:

`/mnt/d/Downloads/lafelicidad/firestec`

Important files:

- `tarticulos.db`: 10,276 rows, 214 fields, about 26 MB.
- `tlinea.DB`: 2,484,345 rows, 65 fields, about 2 GB.

## Repositories

### `/home/chi/code/pypxlib`

Remote: `https://github.com/happinessgun/pypxlib`

Branch `master` is clean and synchronized with `origin/master`.

Relevant commits:

- `3c8c612 Add 32-bit Linux bulk read support`
- `0bea2c3 Speed up Paradox bulk reads`

Implemented API:

- `Table.iter_tuples()` avoids `Row` objects and frees each native record.
- `Table.iter_tuples_native()` retrieves and converts batches in one native call.
- `Table.iter_tuple_batches_parallel()` uses multiprocessing on Linux.
- On 64-bit Linux, fork workers inherit the open/memory-mapped table.
- On 32-bit Linux, every worker reopens the table because sharing the inherited
  stdio state was unsafe when a roughly 2 GB mapping could not be created.
- Bundled 32-bit Linux `pypxlib/pxlib_ctypes/libpx.so` is pxlib 0.6.10 and
  exports `PX_retrieve_records`, `PX_free_record`, and `PX_free_records`.
- `tests/test_linux_32.sh` runs the suite under `python:3.11-slim-bookworm` with
  `--platform linux/386`.
- `tests/data/fast_read.db` is a small real Paradox fixture.

Validation:

- Host 64-bit: `python3 -m unittest tests/test_iter_tuples.py` passes (11 tests,
  one expected 32-bit-only skip).
- Actual 32-bit Python Docker run: all 11 tests pass.
- `python3 setup.py check` passes, apart from the existing version-format warning.

The temporary GitHub PAT supplied in the earlier thread was removed from the
credential cache and must not be copied into code, documentation, or commands.

### `/home/chi/code/pxlib`

This worktree has intentional, uncommitted performance work. Preserve it:

```text
 M CMakeLists.txt
 M include/paradox.h.in
 M src/paradox.c
 M src/px_io.c
 M src/px_io.h
?? tests/fast_read.c
```

Main native changes:

- Fast indexed record lookup and memory-mapped reads where address space permits.
- New exported functions: `PX_retrieve_records`, `PX_free_record`, and
  `PX_free_records`.
- Correct cleanup for mapped and buffered streams.
- CMake version corrected from 0.6.8 to 0.6.10.
- UNIX builds link `libm` (needed by the 32-bit build for `fmod`).
- CTest target `fast_read_test`; its source undefines `NDEBUG` because setup
  operations are intentionally inside assertions.

The 32-bit CMake Release build and CTest passed 1/1. These pxlib source changes
were not committed or pushed because the user explicitly asked to commit and
push pypxlib at that stage.

## Benchmark command

From `/home/chi/code/pypxlib`:

```bash
PYTHONPATH=. python3 benchmarks/read.py DATABASE \
  --workers 4 --batch-size 1000 --native
```

The checksum must match when comparing implementations.

## Recorded results

### 64-bit Linux

- `tarticulos.db`, native, 1 worker: 6,190 rows/s, checksum 1,339,828.
- `tarticulos.db`, native, 4 workers: 18,566 rows/s, checksum 1,339,828.
- Full `tlinea.DB`, native, 4 workers: 47,045 rows/s; open 35.247 s,
  read 52.808 s, checksum 219,772,630.

### 32-bit Linux

- `tarticulos.db`, original/legacy iterator: 1,469 rows/s; read 6.995 s;
  peak RSS 92,392 KB; checksum 1,339,828.
- `tarticulos.db`, new native batching, 1 worker: 1,822 rows/s; read 5.640 s;
  peak RSS 16,640 KB; checksum 1,339,828. This is 24% higher throughput.
- `tarticulos.db`, new native batching, 4 workers: 3,053 rows/s; read 3.366 s;
  peak parent RSS 20,860 KB; checksum 1,339,828. This is 2.08x the original.
- `tlinea.DB`, original iterator, first 100,000 rows: 1,672 rows/s;
  read 59.826 s; peak RSS 266,368 KB; checksum 8,712,169.
- `tlinea.DB`, new native batching, first 100,000 rows: 2,144 rows/s;
  read 46.652 s; peak RSS 16,020 KB; checksum 8,712,169. This is 28% higher
  throughput and avoids the original native-record memory growth.
- Full `tlinea.DB`, new native batching, 4 workers: 4,953 rows/s;
  open 20.666 s, read 501.581 s, checksum 219,772,630.

The original full `tlinea.DB` run was deliberately not attempted: its iterator
does not free each retrieved native record, and the 100k sample already consumed
266 MB. It would likely exhaust a 32-bit address space.

## Why 64-bit is faster

This is not merely pointer width. The approximately 2 GB `tlinea.DB` can be
memory-mapped by the 64-bit process; a 32-bit process cannot reliably reserve
that much contiguous virtual address space and falls back to buffered reads.
The 64-bit fork workers also inherit the mapping, while the safe 32-bit worker
path reopens the table. x86-64/Python ABI efficiency helps conversion too.

For production PostgreSQL exports, prefer 64-bit Python. Keep 32-bit support for
compatibility.

## Windows builds: next work

The existing bundled Windows files are valid PE binaries but are old:

- `pxlib.dll`: 32-bit (`PE32`, Intel 80386).
- `pxlib_x64.dll`: 64-bit (`PE32+`, x86-64).

Neither currently exports `PX_retrieve_records` or `PX_free_records`, so both
must be rebuilt from this modified pxlib source before the native batch iterator
can work on Windows.

With Visual Studio 2022 and its Desktop development with C++ workload:

```powershell
cmake -S . -B build-win32 -G "Visual Studio 17 2022" -A Win32 `
  -DBUILD_TESTING=ON -DENABLE_GSF=OFF
cmake --build build-win32 --config Release
ctest --test-dir build-win32 -C Release --output-on-failure

cmake -S . -B build-win64 -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_TESTING=ON -DENABLE_GSF=OFF
cmake --build build-win64 --config Release
ctest --test-dir build-win64 -C Release --output-on-failure
```

Copy outputs into pypxlib:

```powershell
Copy-Item build-win32\Release\pxlib.dll `
  ..\pypxlib\pypxlib\pxlib_ctypes\pxlib.dll
Copy-Item build-win64\Release\pxlib.dll `
  ..\pypxlib\pypxlib\pxlib_ctypes\pxlib_x64.dll
```

Verify exports with `dumpbin /exports`. Test the x86 DLL under 32-bit Python and
the x64 DLL under 64-bit Python. The loader already selects `pxlib.dll` for
32-bit Python and `pxlib_x64.dll` for 64-bit Python.

`iter_tuples_native()` should work after replacing the DLLs. Multiprocessing is
currently explicitly Linux-only and will need a Windows `spawn` implementation
whose workers reopen the table. Existing multiprocessing tests also need a
Windows-aware adjustment before using the whole suite as Windows validation.
