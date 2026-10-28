# libnx filesystem client TLS

The reference profile now uses real ELF **.tdata/.tbss** sections, a TLS load template and a separate main-thread copy reservation. Original newlibSetup initializes the copy and installs ThreadVars.tls_tp. The test program's first initialized word remains 0x1122334455667788; uninitialized variables start at zero.

## Reference compiler adaptation

Clang 18 in Zig 0.13 emits direct TPIDR_EL0 reads for __thread variables. libnx uses __aarch64_read_tp, which reads ThreadVars.tls_tp through TPIDRRO_EL0. tools/lower_libnx_tls.py adapts generated assembly for fs.c and tls_probe.c, then assembles it. Original readtp.s is linked unchanged.

The replacement preserves registers other than the MRS destination and maintains stack alignment, saving x0/return register when needed. The pinned original accessor changes only x0. The script requires at least one recognized read and rejects unexpected destinations. This depends on the pinned compiler/libnx source and belongs only to reference-build tools. It does not change emulated CPU architectural TPIDR_EL0 semantics or automatically patch external binaries. Exception unwinding inside this sequence is not validated.

Original fs.c/readtp.s remain intact, while the rebuilt NRO explicitly changes. reference.json records source/adaptation/probe/NRO hashes.

## Verification

libnx_tls_sections_and_contexts executes compiled functions reading/writing initialized/zero-filled __thread variables. It switches main and a second CPU context whose TLS is prepared from the ELF template by the test. Writes remain isolated, survive switching and preserve the template. Poisoned TPIDR_EL0 proves use of the libnx convention. Stack and ABI callee-saved registers are checked.

This is a test of **two logical contexts**, not complete libnx threadCreate/threadStart creation/initialization.

From main, tests call original fsOpenSdCardFileSystem with a snapshot explicitly mounted before loading. Command 18 reaches fsp-srv, libnx decodes the SD object as borrowed session/domain ID, and SessionMgr releases the client slot. Native file methods remain unimplemented.

## Startup and next stage

Normal startup still stops before fsdevMountSdmc: **15,244 instructions / 47 IPC exchanges, main unreached**. The tested SD call is an additional controlled call after initialization, not automatic mounting from __appInit. Rebuilt NRO: **245,760 bytes**, **4,096-byte BSS**.

Next: integrate original fsdev dependencies so __appInit performs the mount itself. Historical Switch libnx references do not establish native Switch 2 compatibility.

## Validation

**67 CTest suites pass in Debug and Release**. Strict rebuilding reproduces SHA-256 `54DE872938D75E0A1539F2142E47A8F0C21CDB7A074FBD4E0DCD5492714DF96B`.


Next milestone: [original fsdev registration and main](LIBNX_FSDEV.md).
