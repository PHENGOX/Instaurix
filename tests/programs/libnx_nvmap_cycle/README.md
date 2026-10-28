# Original libnx Nvmap object cycle

Rebuild with `tools/build_libnx_nvmap_cycle.ps1`. The original pinned libnx
NVIDIA client functions are unchanged. The project harness creates two 4 KiB
objects in independent service lifetimes, binds linear CPU-heap memory through the original Alloc ioctl,
checks unchanged CPU contents, frees each object and retries Free to observe
an invalid-handle error.

`map_report` fields 0..4 contain buffer-created, Alloc result, first buffer word,
Free result and repeated-Free result; fields 8..12 repeat the sequence.
The reference executes 59,363 instructions in the current local profile.
CPU backing is bound; no GPU address is mapped and no frame is produced. See the adjacent manifest
and license files for provenance, and `docs/LIBNX_NVMAP_OBJECTS.md` for policy.
