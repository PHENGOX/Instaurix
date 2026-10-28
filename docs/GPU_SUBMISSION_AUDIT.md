# GPU submission path audit

Follow-up: [historical FIFO packet decoding](GPU_PACKET_DECODER.md) now produces method traces from owned snapshots, including a documented SET_REFERENCE reference. No method executes and no job is admitted; earlier decoder boundaries below describe the previous milestone.

Follow-up: [GPFIFO memory preflight](GPU_SUBMISSION_PREFLIGHT.md) now validates channel-owned mapped command ranges and creates temporary owned byte copies. Valid opaque input is still refused; the earlier transport-only outcome below is historical.

Follow-up: [nvIoctl2 transport validation](GPU_IOCTL2_TRANSPORT.md) now decodes the actual three-buffer request. KickoffPb remains unsupported and no job is admitted; earlier transport statements below describe the preceding milestone.

Follow-up: [original full channel initialization and priority](GPU_CHANNEL_INITIALIZATION.md) now validates nvGpuChannelCreate/Close. Submission transport is the next bounded milestone; earlier priority boundaries below describe previous snapshots.

Follow-up: [GPU error-notifier configuration](GPU_ERROR_NOTIFIER.md) now implements the observed enable/disable profile. SetPriority is the next unsupported boundary; earlier notifier statements below describe the previous milestone.

Follow-up: [GPU error-event query](GPU_ERROR_EVENT.md) now supports the original event-3 request and copied read handles. SetErrorNotifier is the next unsupported boundary; earlier event-query statements below describe the preceding milestone.

Follow-up: [historical graphics-context allocation](GPU_GRAPHICS_CONTEXT.md) now implements the observed AllocObjCtx profile. The GPU error-event query is the next unsupported boundary; earlier context-allocation statements below describe the previous milestone.

Follow-up: [bounded GPFIFO allocation](GPU_GPFIFO_ALLOCATION.md) now supports the observed AllocGpfifoEx2 profile. AllocObjCtx is the next unsupported boundary; earlier allocation statements below describe the preceding milestone.

Follow-up: [original channel/address-space binding](GPU_ADDRESS_SPACE_BINDING.md) now implements BindChannel. The next observed boundary is AllocGpfifoEx2; earlier next-step statements below describe the preceding milestone.

Follow-up: [original GPU channel lifecycle](GPU_CHANNEL_LIFECYCLE.md) now supports Open, SetNvmapFd and Close. Priority, address-space binding and command submission remain unsupported; earlier next-step statements below describe the preceding milestone.

Implementation follow-up: [Nvmap-backed GPU mappings](LIBNX_GPU_MAPPINGS.md) now adds bounded automatic/fixed associations, metadata translation and backing-retention guards. Command consumption, channels and native Switch 2 validation remain separate work; earlier boundary statements below are historical.

Implementation follow-up: [GPU virtual reservations](LIBNX_GPU_VIRTUAL_ALLOCATIONS.md) now supports bounded original AllocSpace/FreeSpace calls. Nvmap mappings, channels and GPU command execution remain unsupported; older boundary statements below describe prior milestones.

Implementation follow-up: [original GPU address-space lifecycle](LIBNX_GPU_ADDRESS_SPACE.md) now validates typed Open/InitializeEx/Close with the unchanged reference client. GPU VA allocations, mappings and command submission remain unsupported. Source-audit statements below describe their earlier snapshot.

Audit date: October 27, 2026. This is a source/architecture audit, not an executed
GPU trace. No GPU commands or shaders are implemented by this milestone.
The source revision and exact local hashes are recorded in
GPU_SUBMISSION_AUDIT_SOURCES.json. The available libnx reference is from the
original Switch ecosystem; native Switch 2 submission remains unvalidated.

## What the current emulator actually provides

CPU-produced framebuffer snapshots use Nvmap IDs, CPU heap backing and Binder
queue slots. These are not GPU virtual addresses, GPFIFO entries or command
packets. The NV driver currently opens only /dev/nvmap and /dev/nvhost-ctrl;
other paths return FileNotFound in src/process.cpp. There is no GPU address
space, GPU channel, submission decoder, guest shader translator or Vulkan
backend. Empty framebuffer fences are not evidence of GPU synchronization.

## Pinned original libnx source path

| Layer | Source-observed requests | Missing emulator behavior |
| --- | --- | --- |
| Address-space creation | address_space.c opens /dev/nvhost-as-gpu, then InitializeEx(flags=1, selected page size) | Typed address-space endpoint, initialization and lifetime |
| Address allocation/mapping | AllocSpace/FreeSpace and MapBufferEx/UnmapBuffer in nvhost-as-gpu.c | Separate bounded GPU VA registry and retained Nvmap backing |
| GPU information | gpu.c opens /dev/nvhost-ctrl-gpu and requests characteristics, ZCull and TPC data | A justified reference capability profile; no invented native identity |
| Base channel | channel.c opens /dev/nvhost-gpu and sets the Nvmap FD | Typed channel endpoint and validated dependencies |
| GPU channel | gpu_channel.c binds its address space, allocates GPFIFO and object context, obtains an error event and sets priority | Channel state, queue ownership and lifecycle |
| Submission | KickoffPb uses nvIoctl2 with a separate array of entries; older SubmitGpfifo uses inline entries | IPC ioctl2 transport, bounded descriptor parsing, GPU memory lookup and completion |

nvGpuChannelAppendEntry stores a start IOVA and encodes a command count into
the entry's high word. This describes the pinned client's construction; it is
not sufficient to choose an architectural packet decoder or a native S2 ABI.
GPFIFO_QUEUE_SIZE is 0x800 in this client, not a proven emulator requirement.
The client selects KickoffPb at OS version >=4.0.0, otherwise SubmitGpfifo.
These version branches are historical libnx behavior, not Switch 2 detection.

## Class boundary that must remain explicit

Pinned include/switch/nvidia/ioctl.h defines its 3D class as 0xB197, used by
nvGpuChannelCreate. NVIDIA's manufacturer inventory associates clb197.h with
MAXWELL_B/T210 and clc797.h with AMPERE_B, including T239. Therefore this
historical libnx channel cannot establish the native target class merely by
successfully creating its object. Do not relabel B197 packets or shaders as
Switch 2 commands, and do not silently substitute a different class.

Manufacturer inventory, consulted October 27, 2026:
https://raw.githubusercontent.com/NVIDIA/open-gpu-doc/master/classes/3d/README.txt
This evolving source provides class/header associations, not the target's
complete driver ABI or an observed native command stream.

## Next bounded implementation milestone

Start a separate original-client address-space fixture. Link unchanged pinned
address_space.c and nvhost-as-gpu.c, call nvAddressSpaceCreate/Close with an
explicit documented local page-size profile, and record the actual first
request and error. Source inspection predicts the current unsupported Open;
that boundary has not yet been executed by this audit.

Then introduce a typed /dev/nvhost-as-gpu descriptor and transactional
InitializeEx state. Require exact request fields, validate descriptor ownership,
reject use before initialization, preserve state on failed reply publication,
and test clones, closure, fault and reload. Success at this stage means a
validated address-space lifecycle, not a GPU draw or executed command.

Next add bounded GPU virtual allocations and mappings, with a separate VA type,
overflow/overlap/range checks, retained backing and explicit unmap rules.
CPU addresses must never be accepted as GPU addresses through a cast. Only
then add the source-observed channel/submission path needed by the selected
reference, preserving unsupported calls as explicit errors.

Before choosing a first packet class, obtain a documented stream with
provenance and expected effects. A synthetic stream must be labeled synthetic.
Use a headless deterministic copy/clear oracle before adding a host graphics
backend. A triangle additionally requires identified guest shader support;
a host-written shader or an arbitrary Vulkan triangle does not validate it.
A native Switch 2 claim requires independent evidence for its interfaces,
classes, commands, shaders and reference output.

## Reproducible source references

All libnx paths above are relative to nx/ at revision
b156b4774fb2df7b93402169509266da7547fc4c:
https://github.com/switchbrew/libnx/tree/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/nvidia
The exact eight audited source/header SHA-256 hashes are in the adjacent JSON.
They identify source inspection; they do not imply GPU execution tests.
