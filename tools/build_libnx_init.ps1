param([Parameter(Mandatory=$true)][string]$Python,
      [string]$Zig = "$PSScriptRoot/zig-windows-x86_64-0.13.0/zig.exe",
      [string]$Fixture = 'tests/programs/libnx_init',
      [switch]$WithSm,
      [switch]$WithDefaultServices)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$zigPath=(Resolve-Path -LiteralPath $Zig).Path
$zigRoot=Split-Path -Parent $zigPath
$oldGlobal=$env:ZIG_GLOBAL_CACHE_DIR
$oldLocal=$env:ZIG_LOCAL_CACHE_DIR
$oldTemp=$env:TEMP
$oldTmp=$env:TMP
Push-Location $root
try {
    & "$PSScriptRoot/build_libnx_dynamic.ps1" -Zig $zigPath
    & "$PSScriptRoot/prepare_newlib_reference.ps1"
    $env:ZIG_GLOBAL_CACHE_DIR=Join-Path $root 'build/zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR=Join-Path $root 'build/zig-local-cache'
    $env:TEMP=Join-Path $root 'build/libnx-dynamic-temp';$env:TMP=$env:TEMP
    $nx='tools/reference-sources/libnx-b156b4774fb2df7b93402169509266da7547fc4c/nx'
    $nl='tools/reference-sources/newlib-4.6.0.20260123/newlib/libc'
    if($Fixture -notin @('tests/programs/libnx_init','tests/programs/libnx_sm','tests/programs/libnx_app')) { throw 'Unsupported reference fixture' }
    if($WithSm.IsPresent -ne ($Fixture -ne 'tests/programs/libnx_init') -or $WithDefaultServices.IsPresent -ne ($Fixture -eq 'tests/programs/libnx_app')) {
        throw 'Fixture and service options must match'
    }
    $fixture=$Fixture
    if((Get-FileHash -LiteralPath "$nl/misc/init.c").Hash -ne (Get-FileHash -LiteralPath 'tests/programs/libnx_init/newlib_init.upstream.c').Hash) {
        throw 'Vendored newlib init differs from the verified archive'
    }
    $flags=@('-cc1','-triple','aarch64-none-elf','-O1','-ffreestanding','-fgnuc-version=4.2.1','-Wno-c23-extensions','-ffunction-sections','-fdata-sections',
        '-D_HAVE_INITFINI_ARRAY=1','-D__SWITCH__','-I',"$nx/include/switch",'-I',"$nx/source",'-I',"$nx/external/bsd/include",'-I',"$nl/machine/aarch64",'-isystem',"$nl/include",'-isystem',"$zigRoot/lib/include")
    $objects=@('build/libnx_init_crt0.o','build/libnx_init_svc.o')
    foreach($entry in @(@("$nx/source/runtime/switch_crt0.s",$objects[0]),@("$nx/source/kernel/svc.s",$objects[1]))) {
        & $zigPath -cc1as -triple aarch64-none-elf -filetype obj $entry[0] -o $entry[1]
        if($LASTEXITCODE -ne 0) { throw "Assembly failed: $($entry[0])" }
    }
    $sources=@("$nx/source/runtime/dynamic.c","$nx/source/runtime/init.c","$nx/source/runtime/env.c",
        "$nx/source/runtime/newlib.c","$nx/source/runtime/hosversion.c","$nx/source/runtime/argv.c",
        "$nx/source/kernel/virtmem.c","$nx/source/kernel/thread.c","$nx/source/kernel/mutex.c",
        "$nl/reent/impure.c","$nl/misc/init.c","$nl/ctype/ctype_.c","$nl/stdio/findfp.c",
        "$nl/string/strlen.c","$nl/string/strncpy.c","$nl/string/memcpy.c","$nl/string/memset.c")
    if($WithSm) {
        if((Get-FileHash -LiteralPath "$nx/source/services/sm.c").Hash -ne (Get-FileHash -LiteralPath 'tests/programs/libnx_sm/sm.upstream.c').Hash) {
            throw 'Vendored sm.c differs from pinned libnx'
        }
        $sources+="$nx/source/services/sm.c"
    }
    if($WithDefaultServices) {
        $sources+=@("$nx/source/services/set.c","$nx/source/services/applet.c","$nx/source/services/apm.c",
            "$nx/source/kernel/event.c","$nx/source/kernel/tmem.c","$nx/source/kernel/wait.c")
    }
    $sources+="$fixture/harness.c"
    $index=0
    foreach($source in $sources) {
        $object="build/libnx-init-$index.o"
        & $zigPath @flags -emit-obj $source -o $object
        if($LASTEXITCODE -ne 0) { throw "Compilation failed: $source" }
        $objects+=$object;$index++
    }
    if(Test-Path -LiteralPath "$fixture/missing.S") {
        & $zigPath -cc1as -triple aarch64-none-elf -filetype obj "$fixture/missing.S" -o build/libnx_init_missing.o
        if($LASTEXITCODE -ne 0) { throw 'Missing dependency traps assembly failed' }
        $objects+='build/libnx_init_missing.o'
    }
    $linker=if($WithDefaultServices) { "$fixture/link.ld" } else { 'tests/programs/libnx_init/link.ld' }
    & $zigPath cc -target aarch64-freestanding-none -nostdlib @objects "-Wl,-T,$linker" '-Wl,--gc-sections' '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o "$fixture/program.elf"
    if($LASTEXITCODE -ne 0) { throw 'Original init probe link failed' }
    & $Python tools/package_nro_reference.py "$fixture/program.elf" "$fixture/program.nro" --libnx-crt0
    if($LASTEXITCODE -ne 0) { throw 'Original init NRO packaging failed' }
    $manifest=Get-Content -LiteralPath "$fixture/reference.json" -Raw | ConvertFrom-Json
    if((Get-FileHash -LiteralPath "$fixture/program.nro").Hash -ne $manifest.nro_sha256) { throw 'Init NRO differs from recorded SHA256' }
    Get-FileHash -LiteralPath "$fixture/program.nro"
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR=$oldGlobal;$env:ZIG_LOCAL_CACHE_DIR=$oldLocal
    $env:TEMP=$oldTemp;$env:TMP=$oldTmp
    Pop-Location
}
