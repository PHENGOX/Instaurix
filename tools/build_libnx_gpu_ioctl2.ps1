param([Parameter(Mandatory=$true)][string]$Python,
      [string]$Zig = "$PSScriptRoot/zig-windows-x86_64-0.13.0/zig.exe")
$ErrorActionPreference='Stop'
& "$PSScriptRoot/build_libnx_init.ps1" -Python $Python -Zig $Zig -Fixture 'tests/programs/libnx_gpu_ioctl2' -WithSm -WithDefaultServices
