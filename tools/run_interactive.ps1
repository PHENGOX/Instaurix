$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    & cmake --build build --config Release
    if ($LASTEXITCODE -ne 0) { throw 'Compilation impossible' }
    & (Join-Path $root 'build/Release/Switch2Emu.exe') (Join-Path $root 'tests/programs/hle_interactive/program.elf') 50000000 --hle-reference --read-root (Join-Path $root 'tests/programs/hle_graphics/root') --window
    if ($LASTEXITCODE -ne 0) { throw 'La demonstration a signale une erreur' }
} finally { Pop-Location }
