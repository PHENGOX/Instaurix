param([Parameter(Mandatory=$true)][string]$Python,
      [string]$Zig = "$PSScriptRoot/zig-windows-x86_64-0.13.0/zig.exe",
      [switch]$SamplesOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$zigPath = (Resolve-Path -LiteralPath $Zig).Path
$pythonPath = (Resolve-Path -LiteralPath $Python).Path
$oldGlobal = $env:ZIG_GLOBAL_CACHE_DIR
$oldLocal = $env:ZIG_LOCAL_CACHE_DIR
Push-Location $root
try {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $root 'build/zig-local-cache'
    if (-not $SamplesOnly) {
        & $pythonPath tools/generate_fp_reference.py
        if ($LASTEXITCODE -ne 0) { throw 'FP/SIMD source generation failed' }
        & $zigPath -cc1as -triple aarch64-none-elf -target-feature +neon -target-feature +fp-armv8 -filetype obj -o build/fp_vectors.o tests/programs/cpu_fp/vectors.S
        if ($LASTEXITCODE -ne 0) { throw 'FP/SIMD assembly failed' }
        & $zigPath objcopy -O binary --only-section=.text build/fp_vectors.o tests/programs/cpu_fp/vectors.bin
        if ($LASTEXITCODE -ne 0) { throw 'FP/SIMD extraction failed' }
        & $pythonPath tools/generate_fp_reference.py --oracle
        if ($LASTEXITCODE -ne 0) { throw 'Independent FP/SIMD reference failed' }
    }
    $oldTemp = $env:TEMP
    $oldTmp = $env:TMP
    try {
        $env:TEMP = Join-Path $root 'build/fp-temp'
        $env:TMP = $env:TEMP
        New-Item -ItemType Directory -Force $env:TEMP | Out-Null
        $flags = @('-cc1','-triple','aarch64-none-elf','-O1','-ffreestanding','-ffp-contract=off')
        & $zigPath @flags -S tests/programs/cpu_fp/work.c -o tests/programs/cpu_fp/work.generated.s
        if ($LASTEXITCODE -ne 0) { throw 'FP workload assembly generation failed' }
        & $zigPath @flags -emit-obj tests/programs/cpu_fp/work.c -o build/fp_work.o
        if ($LASTEXITCODE -ne 0) { throw 'FP workload compilation failed' }
        & $zigPath -cc1as -triple aarch64-none-elf -target-feature +neon -target-feature +fp-armv8 -filetype obj -o build/fp_start.o tests/programs/cpu_fp/start.S
        if ($LASTEXITCODE -ne 0) { throw 'FP startup assembly failed' }
        & $zigPath cc -target aarch64-freestanding-none -nostdlib build/fp_start.o build/fp_work.o '-Wl,-T,tests/programs/cpu_fp/link.ld' '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o tests/programs/cpu_fp/program.elf
        if ($LASTEXITCODE -ne 0) { throw 'FP workload link failed' }
    } finally { $env:TEMP=$oldTemp; $env:TMP=$oldTmp }
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $oldGlobal
    $env:ZIG_LOCAL_CACHE_DIR = $oldLocal
    Pop-Location
}
