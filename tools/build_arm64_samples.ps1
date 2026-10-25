param([string]$Zig = "$PSScriptRoot/zig-windows-x86_64-0.13.0/zig.exe")
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$zigPath = (Resolve-Path -LiteralPath $Zig).Path
Push-Location $root
$oldGlobalCache = $env:ZIG_GLOBAL_CACHE_DIR
$oldLocalCache = $env:ZIG_LOCAL_CACHE_DIR
try {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $root 'build/zig-local-cache'
    $flags = @('-target', 'aarch64-freestanding-none', '-O1', '-g0', '-ffreestanding', '-fno-stack-protector', '-fno-vectorize', '-fno-slp-vectorize', '-fno-unroll-loops')
    $sample = 'tests/programs/stack_sum'
    & $zigPath cc @flags -S "$sample/sum.c" -o "$sample/sum.generated.s"
    if ($LASTEXITCODE -ne 0) { throw 'C -> ARM64 assembly failed' }
    & $zigPath cc @flags -nostdlib "$sample/start.S" "$sample/sum.c" '-Wl,-T,tests/programs/stack_sum/link.ld' '-Wl,--build-id=none' -o 'build/stack_sum.elf'
    if ($LASTEXITCODE -ne 0) { throw 'ARM64 link failed' }
    & $zigPath objcopy -O binary --only-section=.text 'build/stack_sum.elf' "$sample/program.bin"
    if ($LASTEXITCODE -ne 0) { throw 'ELF -> binary failed' }
    & $zigPath cc @flags -c tests/programs/pair_vectors.S -o build/pair_vectors.o
    if ($LASTEXITCODE -ne 0) { throw 'Pair vectors assembly failed' }
    & $zigPath objcopy -O binary --only-section=.text build/pair_vectors.o tests/programs/pair_vectors.bin
    if ($LASTEXITCODE -ne 0) { throw 'Pair vectors extraction failed' }
    $elfSample = 'tests/programs/elf_sum'
    & $zigPath cc @flags -S "$elfSample/sum.c" -o "$elfSample/sum.generated.s"
    if ($LASTEXITCODE -ne 0) { throw 'ELF sample C compilation failed' }
    & $zigPath cc @flags -nostdlib "$elfSample/start.S" "$elfSample/sum.c" '-Wl,-T,tests/programs/elf_sum/link.ld' '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o "$elfSample/program.elf"
    if ($LASTEXITCODE -ne 0) { throw 'ELF sample link failed' }
    $hleSample = 'tests/programs/hle_memory'
    & $zigPath cc @flags -S "$hleSample/work.c" -o "$hleSample/work.generated.s"
    if ($LASTEXITCODE -ne 0) { throw 'HLE C compilation failed' }
    & $zigPath cc @flags -nostdlib "$hleSample/start.S" "$hleSample/work.c" '-Wl,-T,tests/programs/hle_memory/link.ld' '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o "$hleSample/program.elf"
    if ($LASTEXITCODE -ne 0) { throw 'HLE sample link failed' }
    & $zigPath cc @flags -c tests/programs/hle_vectors.S -o build/hle_vectors.o
    if ($LASTEXITCODE -ne 0) { throw 'HLE vectors assembly failed' }
    & $zigPath objcopy -O binary --only-section=.text build/hle_vectors.o tests/programs/hle_vectors.bin
    if ($LASTEXITCODE -ne 0) { throw 'HLE vectors extraction failed' }
    $integerSample = 'tests/programs/cpu_integer'
    & $zigPath cc @flags -S "$integerSample/work.c" -o "$integerSample/work.generated.s"
    if ($LASTEXITCODE -ne 0) { throw 'Integer workload C compilation failed' }
    & $zigPath cc @flags -nostdlib "$integerSample/start.S" "$integerSample/work.c" '-Wl,-T,tests/programs/cpu_integer/link.ld' '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o "$integerSample/program.elf"
    if ($LASTEXITCODE -ne 0) { throw 'Integer workload link failed' }
    & $zigPath version
    Get-FileHash "$sample/program.bin", tests/programs/pair_vectors.bin, "$elfSample/program.elf", "$hleSample/program.elf", tests/programs/hle_vectors.bin, "$integerSample/program.elf" -Algorithm SHA256
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $oldGlobalCache
    $env:ZIG_LOCAL_CACHE_DIR = $oldLocalCache
    Pop-Location
}




