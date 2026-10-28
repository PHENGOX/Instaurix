# Contributing to Instaurix

The project is experimental. No commercial game or native Switch 2 compatibility is currently validated. See [README](README.md), [strategy](docs/STRATEGY.md) and [third-party notices](THIRD_PARTY_NOTICES.md).

## Project language

Maintain all project-authored repository content in **English**: documentation, source comments, diagnostic messages, command-line help, tools, test labels and contribution descriptions. Preserve public API names, protocol identifiers and reference data. Keep upstream sources, license texts, copyright notices and verified binary artifacts unchanged.

## Changes and validation

Provide a reproducible case, explain resulting behavior and limitations, and run the checks appropriate to the change. Keep unknown interfaces explicitly unsupported rather than returning fabricated success. Record the provenance and hashes of reference artifacts. Historical Switch/libnx tests are not native Switch 2 compatibility evidence.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Use Git LFS for the three large CPU references described in README. Do not publish build directories, caches, downloaded toolchains, secrets, console keys, firmware or games. Contributions to project-owned code are offered under GPL-3.0-only; retain the original terms of third-party content.
