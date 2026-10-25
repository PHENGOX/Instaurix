# References SIMD arithmetiques

61 056 cas sur 477 variantes pour saturation, retrecissement, decalages variables et multiplications elargies. Encodages : Clang 18.1.6 via Zig 0.13.0. Attendues : Unicorn 2.1.4.

`reference.bin` utilise le format `FPREF001` commun aux tests FP/SIMD ; `reference-layout.txt` controle la provenance de chaque bloc ; `reference.json` contient graine, versions, variantes et SHA-256. Les fichiers `vectors.S` et `vectors.bin` conservent les instructions assemblees.

Regeneration : `tools/build_simd_arithmetic_reference.ps1 -Python <python.exe>`. Les tests CTest lisent les artefacts conserves, sans dependance de generation.

Voir [couverture, limites et validation](../../../docs/CPU_SIMD_ARITHMETIC.md). La demonstration ELF de `cpu_fp` enchaine ces familles et compare ses resultats et flags QC a une reference separee.
