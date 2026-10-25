param([Parameter(Mandatory=$true)][string]$Python,
      [string]$Zig = "$PSScriptRoot/zig-windows-x86_64-0.13.0/zig.exe")
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$zigPath=(Resolve-Path -LiteralPath $Zig).Path
$pythonPath=(Resolve-Path -LiteralPath $Python).Path
$oldGlobal=$env:ZIG_GLOBAL_CACHE_DIR
$oldLocal=$env:ZIG_LOCAL_CACHE_DIR
Push-Location $root
try {
    $env:ZIG_GLOBAL_CACHE_DIR=Join-Path $root 'build/zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR=Join-Path $root 'build/zig-local-cache'
    & $pythonPath tools/generate_simd_arithmetic_reference.py
    if($LASTEXITCODE -ne 0){throw 'SIMD arithmetic assembly source generation failed'}
    & $zigPath -cc1as -triple aarch64-none-elf -target-feature +neon -target-feature +fp-armv8 -filetype obj -o build/simd_arithmetic_vectors.o tests/programs/cpu_simd_arithmetic/vectors.S
    if($LASTEXITCODE -ne 0){throw 'SIMD arithmetic assembly failed'}
    & $zigPath objcopy -O binary --only-section=.text build/simd_arithmetic_vectors.o tests/programs/cpu_simd_arithmetic/vectors.bin
    if($LASTEXITCODE -ne 0){throw 'SIMD arithmetic byte extraction failed'}
    & $pythonPath tools/generate_simd_arithmetic_reference.py --oracle
    if($LASTEXITCODE -ne 0){throw 'SIMD arithmetic oracle failed'}
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR=$oldGlobal
    $env:ZIG_LOCAL_CACHE_DIR=$oldLocal
    Pop-Location
}
