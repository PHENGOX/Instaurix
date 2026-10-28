# ARM64 integer calculation tests

This target checks the first integer CPU milestone through an independent corpus and shared native/guest C program.

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\Instaurix.exe .\tests\programs\cpu_integer\program.elf 5000 --test-protocol
```

Guest C executes **303 instructions**: X2=`0x78335ef4bc270ff5`, X3=`0xc5473408`, exit 0. cpu_compiled_tests compares registers/memory against native functions from the same work.c. Unsigned calculations avoid C division by zero/signed-overflow undefined behavior.

manifest.json describes **775 vectors.S variants**; vectors.bin stores assembled encodings. reference.bin contains **41,304 Unicorn 2.1.4 cases** with deterministic inputs/expectations. CCMP/CCMN include W/X register/immediate forms, all 16 conditions, fallback flags, ZR and reserved encodings. reference.json records generator/seed/hash.

Ordinary C++ tests need neither Unicorn, Python nor network. Failures identify case, encoding, registers and flags; manifests map back to assembly, and the generator exposes cases per variant.

Format: `A64REF01`, little-endian u32 count; per case u32 instruction/options, initial u64 X0–X30/SP/NZCV, expected u64 X0–X30/SP/NZCV/PC, and optionally 512 expected RAM bytes. Initial PC=0x1000, memory window=0x4000; option bit 0 selects memory, bit 1 rejected encoding. These bytes are test data, not host-executed code.

```powershell
python -m pip install --target tools/python-deps unicorn==2.1.4
.\tools\build_cpu_reference.ps1 -Python 'C:\path\to\python.exe'
.\tools\build_arm64_samples.ps1
```

Zig 0.13.0 is the portable compiler used by [initial C tests](../stack_sum/README.md). Generation assembles Clang vectors and runs Unicorn; installing dependencies is explicit, never automatic CMake behavior. Caches stay under build/, without changing system PATH. Normal tests use supplied artifacts.

| Artifact | SHA-256 |
| --- | --- |
| reference.bin | 246dd419cd0a22458d18291996d03f23ea013949cfe7f5634155de3fb253d010 |
| vectors.bin | 14c33786a9fc9e702ed631f818913b93526ff9c3ed0e5484c0fa5a5e7bc6a22a |
| program.elf | 09305709049788e281b9dbcb1276623a3779aa41e3bae808eb700d8c49e2ad27 |

See [CPU audit](../../../docs/CPU_AUDIT.md). This corpus does not validate FP/SIMD, atomics or full ARM64 compliance.
