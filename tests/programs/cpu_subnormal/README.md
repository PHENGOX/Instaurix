# References sous-normaux et FZ

64 896 cas sur 129 variantes, assembles par Clang 18.1.6 et executes avec Unicorn 2.1.4. Voir [le bilan du moteur flottant logiciel](../../../docs/CPU_SOFTFLOAT.md).

- `vectors.S` / `vectors.bin` : encodages de test.
- `reference.bin` : entrees, contexte attendu, flags et fenetre RAM au format `FPREF001`.
- `reference-layout.txt` : instruction et taille de chaque bloc, verifies par le lecteur C++.
- `reference.json` : variantes, provenance, graine et SHA-256.

Regeneration : `tools/build_subnormal_reference.ps1 -Python <python.exe>`. Les tests CTest n'ont besoin ni de Python ni d'Unicorn pour lire les references conservees. Le programme ELF de la cible `cpu_fp` comprend une chaine supplementaire de petits nombres et flags, comparee a des constantes attendues.
