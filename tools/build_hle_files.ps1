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
    $env:TEMP = Join-Path $root 'build/files-temp'
    $env:TMP = $env:TEMP
    New-Item -ItemType Directory -Force $env:TEMP | Out-Null
    $flags = @('-cc1','-triple','aarch64-none-elf','-O1','-ffreestanding')
    & $zigPath @flags -S tests/programs/hle_files/work.c -o tests/programs/hle_files/work.generated.s
    if ($LASTEXITCODE -ne 0) { throw 'Files client C assembly generation failed' }
    & $zigPath @flags -emit-obj tests/programs/hle_files/work.c -o build/hle_files_work.o
    if ($LASTEXITCODE -ne 0) { throw 'Files client C compilation failed' }
    & $zigPath -cc1as -triple aarch64-none-elf -filetype obj -o build/hle_files_start.o tests/programs/hle_client/start.S
    if ($LASTEXITCODE -ne 0) { throw 'Files client startup assembly failed' }
    & $zigPath cc -target aarch64-freestanding-none -nostdlib build/hle_files_start.o build/hle_files_work.o '-Wl,-T,tests/programs/hle_memory/link.ld' '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o tests/programs/hle_files/program.elf
    if ($LASTEXITCODE -ne 0) { throw 'Files client ELF link failed' }
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $oldGlobal
    $env:ZIG_LOCAL_CACHE_DIR = $oldLocal
    $env:TEMP = $oldTemp
    $env:TMP = $oldTmp
    Pop-Location
}
