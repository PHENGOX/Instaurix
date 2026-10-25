param([string]$Zig = "$PSScriptRoot/zig-windows-x86_64-0.13.0/zig.exe")
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$zigPath = (Resolve-Path -LiteralPath $Zig).Path
$oldGlobal = $env:ZIG_GLOBAL_CACHE_DIR
$oldLocal = $env:ZIG_LOCAL_CACHE_DIR
Push-Location $root
try {
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $root 'build/zig-local-cache'
    & $zigPath -cc1as -triple aarch64-none-elf -filetype obj -o build/hle_ipc.o tests/programs/hle_ipc/start.S
    if ($LASTEXITCODE -ne 0) { throw 'IPC assembly failed' }
    & $zigPath cc -target aarch64-freestanding-none -nostdlib build/hle_ipc.o '-Wl,-T,tests/programs/hle_memory/link.ld' '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o tests/programs/hle_ipc/program.elf
    if ($LASTEXITCODE -ne 0) { throw 'IPC ELF link failed' }
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $oldGlobal
    $env:ZIG_LOCAL_CACHE_DIR = $oldLocal
    Pop-Location
}
