param([string]$Zig = "$PSScriptRoot/zig-windows-x86_64-0.13.0/zig.exe")
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$zigPath = (Resolve-Path -LiteralPath $Zig).Path
$oldGlobal = $env:ZIG_GLOBAL_CACHE_DIR
$oldLocal = $env:ZIG_LOCAL_CACHE_DIR
$oldTemp = $env:TEMP
$oldTmp = $env:TMP
Push-Location $root
try {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $root 'build/zig-local-cache'
    $env:TEMP = Join-Path $root 'build/atomic-temp'
    $env:TMP = $env:TEMP
    New-Item -ItemType Directory -Force $env:TEMP | Out-Null
    # Invoke Clang's assembler directly: Zig's cc driver drops LSE features for .S files.
    & $zigPath -cc1as -triple aarch64-none-elf -target-feature +lse -filetype obj -o build/atomic_vectors.o tests/programs/atomic_vectors.S
    if ($LASTEXITCODE -ne 0) { throw 'Atomic assembly failed' }
    & $zigPath objcopy -O binary --only-section=.text build/atomic_vectors.o tests/programs/atomic_vectors.bin
    if ($LASTEXITCODE -ne 0) { throw 'Atomic byte extraction failed' }
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $oldGlobal
    $env:ZIG_LOCAL_CACHE_DIR = $oldLocal
    $env:TEMP = $oldTemp
    $env:TMP = $oldTmp
    Pop-Location
}
