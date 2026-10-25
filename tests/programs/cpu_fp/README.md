# References et programme FP/SIMD

Voir [le bilan de couverture et les limites](../../../docs/CPU_FP_SIMD.md).

- `vectors.S` / `vectors.bin` : 1 491 variantes assemblees par Clang 18.1.6.
- `reference.bin` : 96 400 executions attendues produites par Unicorn 2.1.4.
- `reference.json` : provenance, graine, SHA-256 et liste des variantes.
- `reference-layout.txt` : instruction et nombre d'echantillons par variante ; verifies par le lecteur C++ pour detecter un decalage entre encodages et entrees de reference.
- `work.c` : calcul C float/double partage avec le test natif ; produits volatiles pour empecher la fusion multiplication/addition dans le premier calcul. Le second calcul exerce conversions et une multiplication-addition explicitement fusionnee.
- `work.generated.s` : sortie du compilateur ARM64.
- `start.S`, `link.ld`, `program.elf` : programme de test local ELF en 343 instructions. Il conserve les chaines SIMD et sous-normaux/flags et ajoute 192 octets de saturation, retrecissements, decalages variables, produits elargis et instantanes QC. Ce programme n'est pas un homebrew natif Switch 2.

Regeneration : `tools/build_fp_reference.ps1 -Python <python.exe>`, depuis la racine du projet. Aucun firmware, cle ou code de jeu n'est requis.

Format du corpus : signature `FPREF001`, compteur little-endian 32 bits ; pour chaque cas, instruction et indice d'echantillon 32 bits, etat initial puis attendu, enfin 512 octets de RAM attendue. Chaque etat contient 31 GPR 64 bits, SP/NZCV 64 bits, FPCR/FPSR 32 bits et 32 registres V 128 bits. Le PC de depart est `0x1000`, celui attendu `0x1004`. La RAM initiale a `0x4000` contient `(i*37 + echantillon*13) mod 256` ; les lectures litterales disposent des 32 premiers octets de ce motif a `0x1008`.
