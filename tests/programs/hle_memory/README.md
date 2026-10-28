# HLE demonstration: heap, memory query and thread

Standalone C plus assembly entry, using three public Horizon-reference SVC conventions in a local profile without established Switch 2 compatibility.

```powershell
.\build\Debug\Instaurix.exe .\tests\programs\hle_memory\program.elf 500 --hle-reference
.\tools\build_arm64_samples.ps1
```

Verified: **121 instructions, X2=55, normal exit 0**. Final X0=123 demonstrates ExitProcess does not interpret it as exit status.

start.S allocates 2 MiB through SetHeapSize, reads TPIDRRO_EL0, calls fill_heap and QueryMemory. C computes 55 into heap and local-region+0x180. Entry checks Heap type, R/W and result, then SVC 7; failed checks hit an unknown terminal SVC.

MemoryInfo is at 0x12000; C result/last_tls follow in BSS. Heap=0x200000, local pointer=0xFEE200. These are prototype addresses, not console memory.

The script compiles work.c/start.S/link.ld and emits work.generated.s. Tests also use independently assembled tests/programs/hle_vectors.S. See [portable compiler](../stack_sum/README.md) and [HLE scope](../../../docs/HLE_RUNTIME.md). This program tests no handles/IPC/multithreading.

| Artifact | SHA-256 |
| --- | --- |
| hle_memory/program.elf | a5f79dfcd9825e725a6be21aa5daa6680d95668601c8583be3654d2c1b733ccf |
| hle_vectors.bin | 8c491b2579665c1b2aacff7b182a20a35ca8085fa6ba85b966b1e0a75f6aac8d |
