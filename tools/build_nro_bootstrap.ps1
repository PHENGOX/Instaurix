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
    $fixture='tests/programs/nro_bootstrap'
    & $zigPath -cc1 -triple aarch64-none-elf -O1 -ffreestanding -emit-obj "$fixture/boot.c" -o build/nro_boot.o
    if($LASTEXITCODE -ne 0) { throw 'Bootstrap C compilation failed' }
    & $zigPath -cc1as -triple aarch64-none-elf -filetype obj "$fixture/start.S" -o build/nro_boot_start.o
    if($LASTEXITCODE -ne 0) { throw 'Bootstrap assembly failed' }
    & $zigPath cc -target aarch64-freestanding-none -nostdlib build/nro_boot_start.o build/nro_boot.o build/libnx_dynamic.o "-Wl,-T,$fixture/link.ld" '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o "$fixture/program.elf"
    if($LASTEXITCODE -ne 0) { throw 'Bootstrap link failed' }
    & $Python tools/package_nro_reference.py "$fixture/program.elf" "$fixture/program.nro"
    if($LASTEXITCODE -ne 0) { throw 'Bootstrap packaging failed' }
    & $Python -c 'import pathlib,struct; p=pathlib.Path("tests/programs/nro_bootstrap/program.nro"); b=bytearray(p.read_bytes()); struct.pack_into("<I",b,4,0x4000); p.write_bytes(b)'
    if($LASTEXITCODE -ne 0) { throw 'MOD0 reference patch failed' }
    $manifest=Get-Content -LiteralPath "$fixture/reference.json" -Raw | ConvertFrom-Json
    if((Get-FileHash -LiteralPath "$fixture/program.nro" -Algorithm SHA256).Hash -ne $manifest.nro_sha256) {
        throw 'Bootstrap NRO differs from its recorded SHA256'
    }
    Get-FileHash -LiteralPath "$fixture/program.nro" -Algorithm SHA256
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR=$oldGlobal
    $env:ZIG_LOCAL_CACHE_DIR=$oldLocal
    $env:TEMP=$oldTemp
    $env:TMP=$oldTmp
    Pop-Location
}
