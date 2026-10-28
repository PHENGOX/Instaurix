# Original nvMapCreate wrapper reference

Rebuild with tools/build_libnx_nvmap_wrapper.ps1. Original pinned libnx
nvMapCreate/nvMapClose functions are unchanged. The project harness completes
two CPU-cacheable linear-map cycles and records the resulting export IDs.
The original arm/cache.s is linked for the wrapper's conditional reference;
cache-maintenance instructions are not executed by this fixture.

map_report fields 0..6 are buffer-created, nvMapCreate result, first buffer word,
post-Close has_init, repeated-Free result, export ID and cleared-state check.
Fields 8..14 repeat the sequence. The common nv_report fields retain the same
positions as libnx_nvmap_cycle. See reference.json and adjacent license files.
The normal reference executes 60,250 instructions. No GPU buffer or rendered frame is produced.