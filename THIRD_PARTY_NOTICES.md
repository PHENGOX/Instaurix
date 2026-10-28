# Third-party licenses and references

The root GPL-3.0-only license covers Instaurix's own code. It does not replace the licenses of the third-party files listed here. Reference guest programs are separate executables from the emulator's C++ core.

## libnx

Source: [switchbrew/libnx](https://github.com/switchbrew/libnx/tree/b156b4774fb2df7b93402169509266da7547fc4c), revision `b156b4774fb2df7b93402169509266da7547fc4c`. The main license is ISC, Copyright 2017-2018 libnx Authors. Individual source notices must also be retained.

This applies to `tests/programs/libnx_dynamic`, `libnx_crt0`, `libnx_init`, `libnx_sm`, `libnx_app`, and the `nro_bootstrap` reference context. Copies named `*.upstream.c` and `*.upstream.s` retain their original terms. Each relevant directory contains `LICENSE.libnx.md`. Associated ELF/NRO programs may contain libnx code. The `tools/build_libnx_*.ps1` scripts record the sources used; `reference.json` manifests record versions and hashes.

Notices for the libnx sources used in these builds are collected in [docs/licenses/LIBNX_SOURCE_NOTICES.txt](docs/licenses/LIBNX_SOURCE_NOTICES.txt).

The libnx_app reference includes the original HID, shmem and random clients. The ChaCha implementation in kernel/random.c carries a public-domain notice from D. J. Bernstein and a modification notice from plutoo; both are retained in the notice collection. The local compat_hid_atomics.h header adapts Clang compilation without modifying libnx sources.

The original services/time.c client is linked as well. Its build preincludes stdint.h for newlib header compatibility; the original source remains unchanged under its libnx license.

The original filesystem clients fs.c, sessionmgr.c and condvar.c are also included under the supplied libnx ISC license. Their notices are retained in the collection above. The TLS profile additionally links the original runtime/readtp.s. TLS probes and generated-assembly adaptation belong to this project.

## newlib and the devkitPro adaptation

The `libnx_init`, `libnx_sm` and `libnx_app` references contain part of newlib **4.6.0.20260123**, with the official **devkitA64_r29.2** patch. newlib combines several permissive licenses; do not label the entire library MIT or GPL. Terms are retained in `LICENSE.newlib.txt` in these three directories and in the headers of `newlib_init.upstream.c` copies.

Notices for compiled newlib sources are also retained in [docs/licenses/NEWLIB_SOURCE_NOTICES.txt](docs/licenses/NEWLIB_SOURCE_NOTICES.txt). Acknowledgement: this software includes software developed by the University of California, Berkeley. Contributor names are not used to endorse the project.

Source and patch preparation: `tools/prepare_newlib_reference.ps1`; URLs and SHA-256 hashes: `tests/programs/libnx_init/reference.json`. Downloaded archives remain in the excluded `tools/reference-sources/` cache.

The libnx_app reference also includes the original newlib environment and dlmalloc sources, plus libsysbase sbrk.c/syscall_support.c from the official patch. See [specific notices](docs/licenses/NEWLIB_ENV_SOURCE_NOTICES.txt) and the [retained COPYING.LIBGLOSS](tests/programs/libnx_app/LICENSE.libgloss.txt). The small build config.h is local to the project; third-party sources remain unchanged.

This product includes software developed by the University of California, Berkeley and its contributors.

## External development tools

- **Zig 0.13.0 / Clang** compiles guest programs. Local downloads and installations are excluded from the repository; the tools retain their own licenses.
- **Unicorn 2.1.4** is the independent engine used by Python CPU reference generators. It is not linked into the C++ core, and its local `tools/python-deps/` installation is excluded. Generated vectors and results do not redistribute the engine.
- **CMake and a C++20 compiler** are external build tools.

The switchbrew examples cited in documentation are research references. Mentioning an example does not grant a license to its contents. Rights to any future copied files must be checked separately.

## Console content

The supplied tests require no firmware, games or console keys. Reference ELF/NRO files are development programs accompanied by their sources and build scripts. Do not add proprietary content to these references.

The separate libnx_fsdev reference includes original libnx fsdev/allocation/UTF-8 helpers and patched newlib/libsysbase device-table, path, string, search and calendar sources. Original license files accompany the fixture; additional source headers and hashes are retained in [FSDEV_SOURCE_NOTICES.txt](docs/licenses/FSDEV_SOURCE_NOTICES.txt).

The libnx_posix reference adds original newlib syscall connectors and patched libsysbase open/read/handle-manager sources. Its fixture retains the same library licenses; additional source headers and hashes are in [POSIX_SOURCE_NOTICES.txt](docs/licenses/POSIX_SOURCE_NOTICES.txt).

The separate libnx_vi reference adds original services/vi.c under the supplied libnx ISC license. Its source hash and the hashes of inspected console/window sources are recorded in its manifest. Audited console/window sources are not claimed as executed by this reference.

The separate libnx_binder reference additionally links original display/binder.c under the supplied libnx ISC license. Its hash is recorded in the fixture manifest; the project harness and local parcel profile are described in docs/LIBNX_BINDER_LIFETIME.md.

The separate libnx_queue reference adds original parcel.c and the unchanged buffer_producer.c prefix containing Connect/Disconnect/Query. The excluded final function uses a GCC-only structure extension. The libnx ISC license is supplied with the fixture; source/prefix hashes and the build restriction are recorded in its manifest and docs/LIBNX_QUEUE_CONNECTION.md.

The separate libnx_window reference additionally compiles original display/native_window.c under the supplied libnx ISC license. Its source hash is recorded in the fixture manifest; inspected framebuffer.c is recorded separately and is not executed by this target.

The libnx_nv reference adds original services/nv.c and kernel/tmem.c, plus original newlib aligned_alloc.c and malignr.c using the already documented dlmalloc sources. Original licenses accompany the fixture. The harness's explicit transfer-memory budget and source hashes are recorded in its manifest and docs/LIBNX_NV_TRANSFER_MEMORY.md.

The separate libnx_nvmap reference additionally links original nvidia/map.c under the supplied libnx ISC license. Its hash is recorded in the fixture manifest. The original client reaches /dev/nvmap Open; map creation/ioctl success is not claimed.

The separate libnx_nvmap_alloc reference additionally links original nvidia/ioctl/nvmap.c under the supplied libnx ISC license. Its source hash is recorded in the fixture manifest. The original Create ioctl is observed and explicitly unsupported; no nvmap memory-object allocation is claimed.

The libnx_nvmap_cycle reference also uses the unchanged pinned libnx NVIDIA ioctl client sources and the adjacent copied license notices; it introduces no new third-party source dependency.

The libnx_nvmap_wrapper reference additionally links unchanged libnx source/arm/cache.s,
covered by the root libnx ISC license. Its CPU-cacheable configuration does not
execute cache maintenance. Source hashes accompany its reference.json manifest.

The libnx_fence reference uses unchanged nvidia/fence.c and
nvidia/ioctl/nvhost-ctrl.c from the same pinned libnx revision, under its root
ISC license. The adjacent fixture licenses and manifest record provenance.

The libnx_register reference compiles the complete unchanged display/buffer_producer.c,
including bqSetPreallocatedBuffer, under the same libnx root ISC license.
The Arm GNU compiler is a separately downloaded build tool, not distributed
in this repository. Its official archive provenance is in tools/arm-gnu-reference.json.

The libnx_dequeue reference additionally links the unchanged original
nvidia/fence.c and nvidia/ioctl/nvhost-ctrl.c already covered by the root libnx
ISC notices. It uses the same complete original GCC buffer producer build.

The libnx_present reference uses the same unchanged complete GCC producer,
native-window and linked fence/ioctl sources and libnx ISC notices as
libnx_dequeue. Its gradient pixels and harness are project-owned.

The libnx_framebuffer reference additionally compiles unchanged
display/framebuffer.c under the same pinned libnx root ISC license, alongside
the existing complete producer, NV and fence/ioctl sources. Its manifest
records source hashes; adjacent fixture licenses preserve the notices.

The libnx_framebuffer_present reference links the same unchanged framebuffer.c, arm/cache.s and complete producer sources under the licenses already recorded above. Its project harness adds linear pixel patterns; original SDK bodies remain unmodified.

The libnx_framebuffer_wide reference reuses the same unchanged pinned libnx/newlib/libgloss sources and licenses as libnx_framebuffer_present, with a separate project-owned geometry test harness.

The libnx_framebuffer_buffers reference links the same unchanged pinned libnx/newlib/libgloss sources and licenses as libnx_framebuffer_present. Its project-owned harness exercises separate original framebuffer and NWindow lifetimes for counts one through three.

The libnx_gpu_as reference additionally compiles unchanged libnx source/nvidia/address_space.c and source/nvidia/ioctl/nvhost-as-gpu.c under the pinned root ISC license. Source hashes are recorded in its manifest; original SDK function bodies are not patched.

The libnx_gpu_va reference reuses unchanged pinned libnx source/nvidia/address_space.c and source/nvidia/ioctl/nvhost-as-gpu.c under the same root ISC license. Its separate project-owned harness exercises original allocation/free wrappers; no original SDK function body is modified.

The libnx_gpu_map reference combines the same unchanged pinned libnx address_space.c, nvhost-as-gpu.c, map.c and nvmap.c under their recorded root ISC license, with a separate project-owned mapping/lifetime harness. No SDK body is patched.

The libnx_gpu_channel reference additionally links unchanged libnx nvidia/channel.c and nvidia/ioctl/nvchannel.c at revision b156b4774fb2df7b93402169509266da7547fc4c under the included ISC license. The project-owned harness and mixed Clang / Arm GNU build provenance are recorded in its reference.json and docs/GPU_CHANNEL_LIFECYCLE.md.

The libnx_gpu_bind reference links the same unchanged libnx address_space, nvhost-as-gpu, channel and nvchannel sources at revision b156b4774fb2df7b93402169509266da7547fc4c under the included ISC license. Its project-owned harness, hashes and build provenance are recorded in reference.json and docs/GPU_ADDRESS_SPACE_BINDING.md.

The libnx_gpu_fifo reference links unchanged libnx address_space, nvhost-as-gpu, channel and nvchannel sources at revision b156b4774fb2df7b93402169509266da7547fc4c under the included ISC license. The project-owned harness and source/binary hashes are recorded in its reference.json and docs/GPU_GPFIFO_ALLOCATION.md.

The libnx_gpu_context reference links the same unchanged original NV/channel sources at revision b156b4774fb2df7b93402169509266da7547fc4c under the included ISC license. Its project-owned harness, source hashes and binary provenance are recorded in reference.json and docs/GPU_GRAPHICS_CONTEXT.md.

The libnx_gpu_event reference links unchanged original NV/channel and kernel/event sources at revision b156b4774fb2df7b93402169509266da7547fc4c under the included ISC license. The project-owned harness and source/output hashes are recorded in reference.json and docs/GPU_ERROR_EVENT.md.

The libnx_gpu_notifier reference links unchanged original NV/channel and event sources at revision b156b4774fb2df7b93402169509266da7547fc4c under the included ISC license. Its project-owned harness and source/output hashes are recorded in reference.json and docs/GPU_ERROR_NOTIFIER.md.

The libnx_gpu_channel_init reference additionally links unchanged nvidia/gpu_channel.c at libnx revision b156b4774fb2df7b93402169509266da7547fc4c under the included ISC license. The project-owned harness, source hashes and binary provenance are recorded in reference.json and docs/GPU_CHANNEL_INITIALIZATION.md.

The libnx_gpu_ioctl2 reference links unchanged original GPU channel, NV and hosversion sources at revision b156b4774fb2df7b93402169509266da7547fc4c under the included ISC license. The project-owned harness temporarily selects a historical client version branch only around Kickoff. Source/header hashes and binary provenance are recorded in reference.json and docs/GPU_IOCTL2_TRANSPORT.md.

The libnx_gpu_preflight reference links unchanged original libnx GPU channel, Nvmap and address-space sources at revision b156b4774fb2df7b93402169509266da7547fc4c under the included ISC license. Its project-owned harness submits opaque test words only; source and binary hashes are recorded in reference.json and docs/GPU_SUBMISSION_PREFLIGHT.md.

The historical packet decoder and libnx_gpu_packets reference use NVIDIA's public B06F field definitions and a project-constructed SET_REFERENCE/DMA_NOP stream. Pinned primary-source revisions, URLs and hashes are in docs/GPU_PACKET_DECODER_SOURCES.json. NVIDIA's MIT notices are retained in tests/programs/libnx_gpu_packets/LICENSE.nvidia.md and LICENSE.nvidia-clb06f.txt. Original libnx bodies remain unchanged under their included ISC license.

The libnx_gpu_copy reference links the same unchanged pinned libnx/newlib sources under its included ISC and newlib/libgloss notices. Its producer and verifier are project-owned. Public NVIDIA B06F, B197 and B0B5 field provenance is recorded in docs/GPU_PACKET_DECODER_SOURCES.json; NVIDIA notices are retained alongside the fixture. The workload targets a synchronous historical HLE policy, not native Switch 2 hardware. See docs/LIBNX_GUEST_GPU_COPY.md and the fixture reference.json for provenance.

The libnx_gpu_copy_2d reference uses the same unchanged pinned libnx/newlib sources and retains their ISC/newlib/libgloss notices and the public NVIDIA notices alongside its fixture. Its multiline producer and byte/padding verifier are project-owned. See docs/LIBNX_GUEST_GPU_COPY_2D.md and its reference.json for workload, source/binary hashes and the synchronous historical HLE scope.

The restricted B0B5 block-source conversion uses public NVIDIA field definitions and references the historical 16Bx2 ordering in the same pinned libnx display/framebuffer.c source. Existing NVIDIA and libnx ISC notices are retained with the GPU/libnx fixtures. Its project-owned address helper and independent sector-order tests are documented with the pinned source hash in docs/GPU_MAXWELL_BLOCK_SOURCE_COPY.md; no original SDK function body is modified.

The libnx_gpu_copy_block reference links the same unchanged pinned libnx/newlib sources and retains their ISC/newlib/libgloss and public NVIDIA notices alongside the fixture. Its block-source producer and physical/row verifier are project-owned. See docs/LIBNX_GUEST_GPU_BLOCK_COPY.md and its reference.json for the local synchronous historical scope and source/binary provenance.
