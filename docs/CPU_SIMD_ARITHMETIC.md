# Saturation, decalages variables et multiplications elargies

Palier du 24 octobre 2026. Trois groupes d'operations sont ajoutes au CPU, avec des formes vectorielles et les formes scalaires architecturales correspondantes.

| Groupe | Couverture |
| --- | --- |
| Saturation | SQADD/UQADD, SQSUB/UQSUB, SQABS/SQNEG ; elements 8/16/32/64 bits, formes vectorielles valides et scalaires B/H/S/D |
| Retrecissement saturant | SQXTN/UQXTN/SQXTUN, scalaires et vecteurs, variantes 2 ; sources 16/32/64 bits vers destinations 8/16/32 bits |
| Decalages variables | SSHL/USHL, SRSHL/URSHL, SQSHL/UQSHL, SQRSHL/UQRSHL ; vecteurs et formes scalaires valides |
| Multiplications elargies | SMULL/UMULL, SMLAL/UMLAL, SMLSL/UMLSL et variantes 2 ; sources 8/16/32 bits vers resultats 16/32/64 bits |

## Regles importantes

La saturation borne le resultat au domaine signe ou non signe de l'element. Elle pose **FPSR.QC (bit 27)** si au moins un element sature. Ce bit est cumulatif : une instruction suivante sans saturation ne l'efface pas ; MSR FPSR permet de le remettre a zero. NZCV et les autres bits FPSR sont preserves.

Les decalages utilisent uniquement l'**octet bas signe** de chaque element du registre de decalage. Les autres bits sont ignores. Une valeur positive decale a gauche, une valeur negative a droite ; les formes signees prolongent le signe. Les cas de decalage egal/superieur a la largeur et les arrondis sont traites sans decalage C++ indefini. Les variantes R arrondissent a droite selon leur regle entiere, independamment de FPCR.RMode ; les variantes Q saturent a gauche et posent QC lorsque necessaire.

Les multiplications elargies lisent la moitie basse ou haute des sources selon l'absence/presence du suffixe 2. Le resultat occupe toujours 128 bits. Les accumulations et soustractions s'effectuent modulo la largeur de destination, sans saturation et sans modifier QC. Les calculs C++ sont non signes pour eviter les debordements signes indefinis.

Les retrecissements simples produisent la moitie basse et effacent la moitie haute. Leurs variantes 2 conservent la moitie basse et remplacent la moitie haute. Les sources et l'ancienne destination sont capturees avant calcul, ce qui permet les alias entre registres. Les encodages reserves, les vecteurs .1D et les tailles elargies/retrecies non architecturales sont refuses sans effet partiel.

## Verification

- **61 056 cas sur 477 variantes** : encodages Clang 18.1.6 via Zig 0.13.0, attendus Unicorn 2.1.4.
- Comparaison du contexte entier et vectoriel complet, SP/NZCV, FPCR/FPSR et memoire. Inclut valeurs limites, aliases, decalages de -128 a 127, bruit dans les bits de decalage inutilises, QC initialement pose ou efface et donnees aleatoires.
- Le corpus est separe des references FP/SIMD generales et des references sous-normales, et reutilise leur format et controle de provenance par blocs.
- Le programme ELF integre enchaine saturation signee/non signee, decalages arrondis, retrecissement vers les moities basse/haute, produits elargis et accumules. **192 octets**, incluant instantanes QC et sa remise a zero, sont compares a une reference scalaire distincte.
- La demonstration complete termine en **343 instructions**. Les **24 suites CTest passent en Debug et Release** ; les corpus precedents restent conserves.

Ces tests ne prouvent pas la conformite de toutes les extensions Advanced SIMD et ne constituent pas une validation sur Switch 2.

## Reproduction

```powershell
./tools/build_simd_arithmetic_reference.ps1 -Python /chemin/vers/python.exe
./tools/build_fp_reference.ps1 -Python /chemin/vers/python.exe -SamplesOnly
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

## Prochaine etape

Consolider le CPU sur plusieurs programmes C/C++ compiles et optimises : calcul vectoriel, traitements de buffers et operations atomiques. Comparer leurs resultats a l'execution native et traiter les instructions manquantes constatees. Cette cible doit preceder l'integration d'un ordonnanceur de plusieurs threads. Le JIT reste a decider a partir de mesures de performances.

## References

- [Arm, guide de reference A64](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab).
- [Arm, bit QC en AArch64](https://documentation-service.arm.com/static/5f3e9263b13d4764d4613b69?token=).
- [Unicorn, moteur de comparaison](https://github.com/unicorn-engine/unicorn).
