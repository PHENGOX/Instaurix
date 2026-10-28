param([string]$Destination = "$PSScriptRoot/arm-gnu-14.3")
$ErrorActionPreference='Stop'
$package='arm-gnu-toolchain-14.3.rel1-mingw-w64-x86_64-aarch64-none-elf.zip'
$expected='6B7D0680831FEB5E69B3F8B9DC05F633C6A7B543B411F863E0F7BD263413C5A5'
$archive=Join-Path $PSScriptRoot 'arm-gnu-14.3-aarch64.zip'
$url="https://developer.arm.com/-/media/Files/downloads/gnu/14.3.rel1/binrel/$package"
if(!(Test-Path -LiteralPath $archive)) { Invoke-WebRequest -Uri $url -OutFile $archive }
if((Get-FileHash -LiteralPath $archive).Hash -ne $expected) { throw 'Arm GNU archive SHA-256 mismatch; extraction refused' }
$compiler=Join-Path $Destination 'bin/aarch64-none-elf-gcc.exe'
if(!(Test-Path -LiteralPath $compiler)) { Expand-Archive -LiteralPath $archive -DestinationPath $Destination }
$version=& $compiler --version
if($LASTEXITCODE -ne 0 -or $version[0] -notmatch '14\.3\.1') { throw 'Unexpected Arm GNU compiler version' }
Write-Output $version[0]
