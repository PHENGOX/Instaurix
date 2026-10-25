$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$cache=Join-Path $PSScriptRoot 'reference-sources'
$archive=Join-Path $cache 'newlib-4.6.0.20260123.tar.gz'
$patch=Join-Path $cache 'newlib-switch.patch'
$source=Join-Path $cache 'newlib-4.6.0.20260123'
New-Item -ItemType Directory -Force $cache | Out-Null
if(!(Test-Path -LiteralPath $archive)) { Invoke-WebRequest 'https://sourceware.org/pub/newlib/newlib-4.6.0.20260123.tar.gz' -OutFile $archive }
if(!(Test-Path -LiteralPath $patch)) { Invoke-WebRequest 'https://raw.githubusercontent.com/devkitPro/buildscripts/refs/tags/devkitA64_r29.2/patches/newlib-4.6.0.20260123-4.patch' -OutFile $patch }
if((Get-FileHash -LiteralPath $archive).Hash -ne '6FF27E3BF022666F43F7802255BE680EEFF722AC181B1725D21E2E8318604EE3' -or
   (Get-FileHash -LiteralPath $patch).Hash -ne '6D5ABA4356553C727765C98E7553D3784F31D361C856E220EF27CC26353FB88F') {
    throw 'Official newlib source or devkitA64 patch hash mismatch'
}
# Restore the previously patched cache before refreshing from the verified archive.
if(Test-Path -LiteralPath "$source/newlib/libc/machine/aarch64/sys/lock.h") {
    & git -C $source apply --reverse --check $patch 2>$null
    if($LASTEXITCODE -ne 0) { throw 'Modified newlib cache cannot be refreshed safely' }
    & git -C $source apply --reverse --whitespace=nowarn $patch 2>$null
    if($LASTEXITCODE -ne 0) { throw 'Could not restore cached newlib sources' }
}
& tar -xf $archive -C $cache
if($LASTEXITCODE -ne 0) { throw 'newlib extraction failed' }
& git -C $source apply --check $patch 2>$null
if($LASTEXITCODE -ne 0) { throw 'Official newlib patch does not match sources' }
& git -C $source apply --whitespace=nowarn $patch 2>$null
if($LASTEXITCODE -ne 0) { throw 'newlib patch application failed' }
