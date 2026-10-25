param([Parameter(Mandatory=$true)][string]$Python,
      [string]$Zig = "$PSScriptRoot/zig-windows-x86_64-0.13.0/zig.exe")
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$zigPath=(Resolve-Path -LiteralPath $Zig).Path
$oldGlobal=$env:ZIG_GLOBAL_CACHE_DIR
$oldLocal=$env:ZIG_LOCAL_CACHE_DIR
$oldTemp=$env:TEMP
$oldTmp=$env:TMP
Push-Location $root
try {
    & "$PSScriptRoot/build_libnx_dynamic.ps1" -Zig $zigPath
    $env:ZIG_GLOBAL_CACHE_DIR=Join-Path $root 'build/zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR=Join-Path $root 'build/zig-local-cache'
    $env:TEMP=Join-Path $root 'build/libnx-dynamic-temp'
    $env:TMP=$env:TEMP
    $fixture='tests/programs/libnx_crt0'
    $upstream='tools/reference-sources/libnx-b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/switch_crt0.s'
    if((Get-FileHash -LiteralPath $upstream).Hash -ne (Get-FileHash -LiteralPath "$fixture/switch_crt0.upstream.s").Hash) {
        throw 'crt0 differs from pinned upstream'
    }
    & $zigPath -cc1as -triple aarch64-none-elf -filetype obj "$fixture/switch_crt0.upstream.s" -o build/libnx_crt0.o
    if($LASTEXITCODE -ne 0) { throw 'Unmodified crt0 assembly failed' }
    & $zigPath -cc1as -triple aarch64-none-elf -filetype obj "$fixture/harness.S" -o build/libnx_crt0_harness.o
    if($LASTEXITCODE -ne 0) { throw 'Boundary harness assembly failed' }
    & $zigPath cc -target aarch64-freestanding-none -nostdlib build/libnx_crt0.o build/libnx_crt0_harness.o build/libnx_dynamic.o "-Wl,-T,$fixture/link.ld" '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o "$fixture/program.elf"
    if($LASTEXITCODE -ne 0) { throw 'crt0 probe link failed' }
    & $Python tools/package_nro_reference.py "$fixture/program.elf" "$fixture/program.nro" --libnx-crt0
    if($LASTEXITCODE -ne 0) { throw 'crt0 packaging failed' }
    $manifest=Get-Content -LiteralPath "$fixture/reference.json" -Raw | ConvertFrom-Json
    if((Get-FileHash -LiteralPath "$fixture/program.nro" -Algorithm SHA256).Hash -ne $manifest.nro_sha256) {
        throw 'crt0 NRO differs from its recorded SHA256'
    }
    Get-FileHash -LiteralPath "$fixture/program.nro" -Algorithm SHA256
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR=$oldGlobal
    $env:ZIG_LOCAL_CACHE_DIR=$oldLocal
    $env:TEMP=$oldTemp
    $env:TMP=$oldTmp
    Pop-Location
}
