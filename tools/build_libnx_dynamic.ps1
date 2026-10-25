param([string]$Zig = "$PSScriptRoot/zig-windows-x86_64-0.13.0/zig.exe")
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$zigPath = (Resolve-Path -LiteralPath $Zig).Path
$zigRoot = Split-Path -Parent $zigPath
$revision = 'b156b4774fb2df7b93402169509266da7547fc4c'
$archiveHash = '459F39C54C187A48BDAC78F2A4A202BFB7AA7707E76BC1C50C030FB1817C5503'
$cache = Join-Path $PSScriptRoot 'reference-sources'
$archive = Join-Path $cache "libnx-$revision.zip"
$source = Join-Path $cache "libnx-$revision"
$oldGlobal = $env:ZIG_GLOBAL_CACHE_DIR
$oldLocal = $env:ZIG_LOCAL_CACHE_DIR
$oldTemp = $env:TEMP
$oldTmp = $env:TMP
Push-Location $root
try {
    if ((& $zigPath version) -ne '0.13.0') { throw 'This reference is pinned to Zig 0.13.0' }
    New-Item -ItemType Directory -Force $cache | Out-Null
    if (!(Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest -Uri "https://codeload.github.com/switchbrew/libnx/zip/$revision" -OutFile $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $archiveHash) {
        throw 'Pinned libnx archive hash mismatch'
    }
    # Refresh headers from the verified archive, including when a cache already exists.
    Expand-Archive -LiteralPath $archive -DestinationPath $cache -Force
    $upstream = Join-Path $source 'nx/source/runtime/dynamic.c'
    $fixture = 'tests/programs/libnx_dynamic'
    if ((Get-FileHash -LiteralPath $upstream).Hash -ne
        (Get-FileHash -LiteralPath "$fixture/dynamic.upstream.c").Hash) {
        throw 'Vendored dynamic.c differs from the pinned upstream source'
    }
    $env:ZIG_GLOBAL_CACHE_DIR = Join-Path $root 'build/zig-cache'
    $env:ZIG_LOCAL_CACHE_DIR = Join-Path $root 'build/zig-local-cache'
    $env:TEMP = Join-Path $root 'build/libnx-dynamic-temp'
    $env:TMP = $env:TEMP
    New-Item -ItemType Directory -Force $env:TEMP | Out-Null
    $flags = @('-cc1','-triple','aarch64-none-elf','-O1','-ffreestanding','-ffunction-sections','-fdata-sections')
    & $zigPath @flags -I "$source/nx/include/switch" -isystem "$zigRoot/lib/include" -isystem "$zigRoot/lib/libc/include/aarch64-linux-musl" -isystem "$zigRoot/lib/libc/include/generic-musl" -emit-obj "$fixture/dynamic.upstream.c" -o build/libnx_dynamic.o
    if ($LASTEXITCODE -ne 0) { throw 'Unmodified libnx dynamic.c compilation failed' }
    & $zigPath @flags -emit-obj "$fixture/probe.c" -o build/libnx_dynamic_probe.o
    if ($LASTEXITCODE -ne 0) { throw 'Probe compilation failed' }
    & $zigPath -cc1as -triple aarch64-none-elf -filetype obj "$fixture/start.S" -o build/libnx_dynamic_start.o
    if ($LASTEXITCODE -ne 0) { throw 'Probe assembly failed' }
    & $zigPath cc -target aarch64-freestanding-none -nostdlib build/libnx_dynamic_start.o build/libnx_dynamic_probe.o build/libnx_dynamic.o "-Wl,-T,$fixture/link.ld" '-Wl,--build-id=none' '-Wl,-z,max-page-size=4096' -o "$fixture/program.elf"
    if ($LASTEXITCODE -ne 0) { throw 'Probe link failed' }
    $manifest = Get-Content -LiteralPath "$fixture/reference.json" -Raw | ConvertFrom-Json
    if ((Get-FileHash -LiteralPath "$fixture/program.elf" -Algorithm SHA256).Hash -ne $manifest.program_sha256) {
        throw 'Probe binary differs from its recorded SHA256'
    }
    Get-FileHash -LiteralPath "$fixture/program.elf" -Algorithm SHA256
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR = $oldGlobal
    $env:ZIG_LOCAL_CACHE_DIR = $oldLocal
    $env:TEMP = $oldTemp
    $env:TMP = $oldTmp
    Pop-Location
}
