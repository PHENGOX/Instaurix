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
    if($Fixture -notin @('tests/programs/libnx_init','tests/programs/libnx_sm','tests/programs/libnx_app','tests/programs/libnx_fsdev','tests/programs/libnx_posix','tests/programs/libnx_vi','tests/programs/libnx_binder','tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers','tests/programs/libnx_nv','tests/programs/libnx_nvmap','tests/programs/libnx_nvmap_alloc','tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block')) { throw 'Unsupported reference fixture' }
    if($WithSm.IsPresent -ne ($Fixture -ne 'tests/programs/libnx_init') -or $WithDefaultServices.IsPresent -ne ($Fixture -in @('tests/programs/libnx_app','tests/programs/libnx_fsdev','tests/programs/libnx_posix','tests/programs/libnx_vi','tests/programs/libnx_binder','tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers','tests/programs/libnx_nv','tests/programs/libnx_nvmap','tests/programs/libnx_nvmap_alloc','tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block'))) {
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
            "$nx/source/kernel/event.c","$nx/source/kernel/tmem.c","$nx/source/kernel/wait.c",
            "$nx/source/services/hid.c","$nx/source/kernel/shmem.c","$nx/source/kernel/random.c",
            "$nx/source/services/time.c","$nx/source/services/fs.c","$nx/source/sf/sessionmgr.c","$nx/source/kernel/condvar.c")
        $sources+=@("$nl/stdlib/setenv.c","$nl/stdlib/setenv_r.c","$nl/stdlib/getenv.c","$nl/stdlib/getenv_r.c",
            "$nl/stdlib/environ.c","$nl/stdlib/envlock.c","$nl/errno/errno.c","$nl/stdlib/malloc.c","$nl/stdlib/mallocr.c",
            "$nl/stdlib/freer.c","$nl/stdlib/realloc.c","$nl/stdlib/reallocr.c","$nl/stdlib/mlock.c",
            "$nl/string/strchr.c","$nl/string/strncmp.c","$nl/string/memmove.c",
            'tools/reference-sources/newlib-4.6.0.20260123/libgloss/libsysbase/sbrk.c',
            'tools/reference-sources/newlib-4.6.0.20260123/libgloss/libsysbase/syscall_support.c')
    }
    if($Fixture -in @('tests/programs/libnx_fsdev','tests/programs/libnx_posix','tests/programs/libnx_vi','tests/programs/libnx_binder','tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers','tests/programs/libnx_nv','tests/programs/libnx_nvmap','tests/programs/libnx_nvmap_alloc','tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block')) {
        $sources+=@("$nx/source/runtime/devices/fs_dev.c","$nx/source/runtime/devices/path_buf.c",
            "$nx/source/runtime/alloc.c","$nx/source/runtime/util/utf/decode_utf8.c",
            "$nl/string/strcmp.c","$nl/string/strncat.c","$nl/string/strnlen.c",
            "$nl/search/bsearch.c","$nl/time/gmtime.c","$nl/time/gmtime_r.c",
            'tools/reference-sources/newlib-4.6.0.20260123/libgloss/libsysbase/iosupport.c',
            'tools/reference-sources/newlib-4.6.0.20260123/libgloss/libsysbase/chdir.c',
            'tools/reference-sources/newlib-4.6.0.20260123/libgloss/libsysbase/concatenate.c')
    }
    if($Fixture -in @('tests/programs/libnx_posix','tests/programs/libnx_vi','tests/programs/libnx_binder','tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) {
        $sources+=@("$nl/syscalls/sysopen.c","$nl/syscalls/sysread.c","$nl/syscalls/sysclose.c",
            'tools/reference-sources/newlib-4.6.0.20260123/libgloss/libsysbase/open.c',
            'tools/reference-sources/newlib-4.6.0.20260123/libgloss/libsysbase/read.c',
            'tools/reference-sources/newlib-4.6.0.20260123/libgloss/libsysbase/handle_manager.c')
    }
    if($Fixture -in @('tests/programs/libnx_vi','tests/programs/libnx_binder','tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $sources+="$nx/source/services/vi.c" }
    if($Fixture -in @('tests/programs/libnx_binder','tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $sources+="$nx/source/display/binder.c" }
    if($Fixture -in @('tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) {
        # The final unused function contains a GCC-only variable-length structure member.
        # Compile the unchanged original prefix containing Connect/Disconnect/Query.
        $producerSource=[IO.File]::ReadAllText((Join-Path $root "$nx/source/display/buffer_producer.c"))
        $cut=$producerSource.IndexOf('Result bqSetPreallocatedBuffer(')
        if($cut -lt 0) { throw 'Expected pinned buffer producer boundary' }
        [IO.File]::WriteAllText((Join-Path $root 'build/buffer_producer_reference.c'),$producerSource.Substring(0,$cut),[Text.UTF8Encoding]::new($false))
        $sources+="$nx/source/display/parcel.c"
        if($Fixture -in @('tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $sources+="$nx/source/display/buffer_producer.c" }
        else { $sources+='build/buffer_producer_reference.c' }
    }
    if($Fixture -in @('tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $sources+="$nx/source/display/native_window.c" }
    if($Fixture -in @('tests/programs/libnx_nv','tests/programs/libnx_nvmap','tests/programs/libnx_nvmap_alloc','tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $sources+=@("$nx/source/services/nv.c","$nl/stdlib/aligned_alloc.c","$nl/stdlib/malignr.c") }
    if($Fixture -in @('tests/programs/libnx_nvmap','tests/programs/libnx_nvmap_alloc','tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $sources+="$nx/source/nvidia/map.c" }
    if($Fixture -in @('tests/programs/libnx_nvmap_alloc','tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $sources+="$nx/source/nvidia/ioctl/nvmap.c" }
    if($WithDefaultServices) { $sources+="$fixture/tls_probe.c" }
    $sources+="$fixture/harness.c"
    if($Fixture -in @('tests/programs/libnx_fsdev','tests/programs/libnx_posix','tests/programs/libnx_vi','tests/programs/libnx_binder','tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers','tests/programs/libnx_nv','tests/programs/libnx_nvmap','tests/programs/libnx_nvmap_alloc','tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block')) { $sources+="$nl/string/strrchr.c" }
    if($WithDefaultServices) {
        & $zigPath -cc1as -triple aarch64-none-elf -filetype obj "$nx/source/runtime/readtp.s" -o build/libnx_readtp.o
        if($LASTEXITCODE -ne 0) { throw 'TLS reader assembly failed' }
        $objects+='build/libnx_readtp.o'
    }
    if($Fixture -in @('tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) {
        & $zigPath -cc1as -triple aarch64-none-elf -filetype obj "$nx/source/arm/cache.s" -o build/libnx_cache.o
        if($LASTEXITCODE -ne 0) { throw 'Original cache assembly failed' }
        $objects+='build/libnx_cache.o'
    }
    if($Fixture -in @('tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $sources+=@("$nx/source/nvidia/fence.c","$nx/source/nvidia/ioctl/nvhost-ctrl.c") }
    if($Fixture -in @('tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $sources+="$nx/source/display/framebuffer.c" }
    if($Fixture -in @('tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block')) { $sources+=@("$nx/source/nvidia/address_space.c","$nx/source/nvidia/ioctl/nvhost-as-gpu.c") }
    if($Fixture -in @('tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block')) { $sources+=@("$nx/source/nvidia/channel.c","$nx/source/nvidia/ioctl/nvchannel.c") }
    if($Fixture -in @('tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block')) { $sources+="$nx/source/nvidia/gpu_channel.c" }
    $index=0
    foreach($source in $sources) {
        $object="build/libnx-init-$index.o"
        $sourceFlags=@()
        if($WithDefaultServices -and $source -eq "$nx/source/services/hid.c") { $sourceFlags=@('-include',"$fixture/compat_hid_atomics.h") }
        if($WithDefaultServices -and $source -eq "$nx/source/services/time.c") { $sourceFlags=@('-include','stdint.h') }
        if($WithDefaultServices -and $source -match '/stdlib/(mallocr|freer|reallocr|malignr)\.c$') { $sourceFlags=@('-D_LIBC=1') }
        if($WithDefaultServices -and $source -match '/libsysbase/') { $sourceFlags=@('-D_BUILDING_LIBSYSBASE=1','-I',"$fixture/sysbase_config") }
        if($Fixture -in @('tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers') -and $source -eq "$nx/source/display/buffer_producer.c" -or ($Fixture -in @('tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block') -and $source -eq "$nx/source/nvidia/ioctl/nvchannel.c")) {
            $gcc=Join-Path $root 'tools/arm-gnu-14.3/bin/aarch64-none-elf-gcc.exe'
            if(!(Test-Path -LiteralPath $gcc)) { throw 'Run tools/prepare_arm_gnu_reference.ps1 to obtain the pinned original-producer compiler' }
            $gccVersion=& $gcc -dumpfullversion
            if($LASTEXITCODE -ne 0 -or $gccVersion -ne '14.3.1') { throw 'Original GNU-extension source requires the pinned Arm GNU 14.3.1 compiler' }
            & $gcc -O1 -ffreestanding -ffunction-sections -fdata-sections -fno-stack-protector -mgeneral-regs-only -D_HAVE_INITFINI_ARRAY=1 -D__SWITCH__ "-I$nx/include/switch" "-I$nx/source" "-I$nx/external/bsd/include" "-I$nl/machine/aarch64" -isystem "$nl/include" -c $source -o $object
        } elseif($WithDefaultServices -and ($source -eq "$nx/source/services/fs.c" -or $source -eq "$nx/source/runtime/devices/fs_dev.c" -or $source -eq "$fixture/tls_probe.c")) {
            & $zigPath @flags @sourceFlags -S $source -o build/libnx-tls-input.s
            if($LASTEXITCODE -ne 0) { throw "TLS compilation failed: $source" }
            & $Python tools/lower_libnx_tls.py build/libnx-tls-input.s build/libnx-tls-lowered.s
            if($LASTEXITCODE -ne 0) { throw 'TLS lowering failed' }
            & $zigPath -cc1as -triple aarch64-none-elf -filetype obj build/libnx-tls-lowered.s -o $object
        } else {
            & $zigPath @flags @sourceFlags -emit-obj $source -o $object
        }
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
    if($WithDefaultServices) {
        $symbolFlags=@()
        if($Fixture -in @('tests/programs/libnx_fsdev','tests/programs/libnx_posix','tests/programs/libnx_vi','tests/programs/libnx_binder','tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers','tests/programs/libnx_nv','tests/programs/libnx_nvmap','tests/programs/libnx_nvmap_alloc','tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block')) { $symbolFlags=@('--fsdev')
            if($Fixture -eq 'tests/programs/libnx_posix') { $symbolFlags+='--posix' }
            if($Fixture -in @('tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper')) { $symbolFlags+='--nv-cycle' }
            if($Fixture -in @('tests/programs/libnx_nv','tests/programs/libnx_nvmap','tests/programs/libnx_nvmap_alloc','tests/programs/libnx_nvmap_cycle','tests/programs/libnx_nvmap_wrapper','tests/programs/libnx_fence','tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $symbolFlags+='--nv' }
            if($Fixture -eq 'tests/programs/libnx_queue') { $symbolFlags+='--queue' }
            if($Fixture -in @('tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $symbolFlags+='--window' }
            if($Fixture -in @('tests/programs/libnx_vi','tests/programs/libnx_binder','tests/programs/libnx_queue','tests/programs/libnx_window','tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $symbolFlags+='--vi' } }
        if($Fixture -eq 'tests/programs/libnx_fence') { $symbolFlags=@('--fsdev','--fence') }
        if($Fixture -in @('tests/programs/libnx_gpu_as','tests/programs/libnx_gpu_va','tests/programs/libnx_gpu_map','tests/programs/libnx_gpu_channel','tests/programs/libnx_gpu_bind','tests/programs/libnx_gpu_fifo','tests/programs/libnx_gpu_context','tests/programs/libnx_gpu_event','tests/programs/libnx_gpu_notifier','tests/programs/libnx_gpu_channel_init','tests/programs/libnx_gpu_ioctl2','tests/programs/libnx_gpu_preflight','tests/programs/libnx_gpu_packets','tests/programs/libnx_gpu_copy','tests/programs/libnx_gpu_copy_2d','tests/programs/libnx_gpu_copy_block')) { $symbolFlags=@('--fsdev','--gpu-as') }
        if($Fixture -in @('tests/programs/libnx_register','tests/programs/libnx_dequeue','tests/programs/libnx_present','tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $symbolFlags=@('--fsdev','--vi','--window','--register') }
        if($Fixture -eq 'tests/programs/libnx_dequeue') { $symbolFlags+='--dequeue' }
        if($Fixture -eq 'tests/programs/libnx_present') { $symbolFlags+='--present' }
        if($Fixture -in @('tests/programs/libnx_framebuffer','tests/programs/libnx_framebuffer_present','tests/programs/libnx_framebuffer_wide','tests/programs/libnx_framebuffer_buffers')) { $symbolFlags=@('--fsdev','--vi','--window','--framebuffer') }
        & $Python tools/extract_reference_symbols.py "$fixture/program.elf" "$fixture/symbols.txt" @symbolFlags
        if($LASTEXITCODE -ne 0) { throw 'Reference symbol extraction failed' }
    }
    $manifest=Get-Content -LiteralPath "$fixture/reference.json" -Raw | ConvertFrom-Json
    if((Get-FileHash -LiteralPath "$fixture/program.nro").Hash -ne $manifest.nro_sha256) { throw 'Init NRO differs from recorded SHA256' }
    Get-FileHash -LiteralPath "$fixture/program.nro"
} finally {
    $env:ZIG_GLOBAL_CACHE_DIR=$oldGlobal;$env:ZIG_LOCAL_CACHE_DIR=$oldLocal
    $env:TEMP=$oldTemp;$env:TMP=$oldTmp
    Pop-Location
}
