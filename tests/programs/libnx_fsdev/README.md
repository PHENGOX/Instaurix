# Original libnx fsdev reference

Separate integration target using pinned libnx b156b4774fb2df7b93402169509266da7547fc4c and official patched newlib 4.6.0.20260123. Project wrapper sources reuse libnx_app harness/TLS probes and compatibility configuration. The linker and explicit trap file are local; upstream library sources are compiled unchanged, with the documented generated-assembly TLS adaptation.

Rebuild from repository root: `tools/build_libnx_fsdev.ps1 -Python <python_path>`.

Normal startup reaches constructors/main and original cleanup. Project main returns 77; libnx returns loader result zero. With an explicit snapshot, original fsdev registers sdmc; without one, optional mounting fails and startup continues. Native file operations remain unsupported.

reference.json records source/artifact hashes and expected results. All three original license files are retained. See [the milestone report](../../../docs/LIBNX_FSDEV.md) and [additional source notices](../../../docs/licenses/FSDEV_SOURCE_NOTICES.txt).
