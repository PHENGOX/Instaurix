# Calcul flottant logiciel et sous-normaux

Etat du 24 octobre 2026. Le CPU execute maintenant les calculs binary32/binary64 implementes, scalaires et vectoriels, sans utiliser les operations flottantes de l'hote. Ce palier remplace les anciens refus des operandes/resultats sous-normaux et du mode FPCR.FZ.

## Representation et calcul

`src/cpu_softfloat.cpp` utilise des entiers de 4096 bits stockes dans 128 mots de 32 bits, sans allocation dynamique. Un operande fini est decompose en signe, significande entier et puissance de deux.

- Addition/soustraction : alignement exact des significandes puis addition ou difference signee.
- Multiplication : produit exact des significandes, jusqu'a 106 bits pour binary64.
- FMA : produit exact puis addition de l'accumulateur avant l'unique arrondi. L'alignement maximal de ces formats demande au plus 3172 positions binaires.
- Division : quotient entier avec suffisamment de bits de garde et reste conserve sous forme de bit collant.
- Racine carree : racine entiere par paires de bits et reste collant.
- Conversion S/D : passage par la meme representation exacte et le meme arrondi final.

Le traitement commun des NaN, infinis, zeros signes et controles precede les calculs finis. Le profil conserve la priorite des NaN signalisants et les regles propres aux calculs fusionnes. Aucune implementation tierce de calcul flottant n'est integree.

## Arrondis et flags

Les quatre modes FPCR.RMode et Default NaN restent disponibles. Les flags cumulatifs FPSR sont conserves et combines entre les elements vectoriels : IOC, DZC, OFC, UFC, IXC et IDC.

Dans le profil architectural sans Alternate Handling :

- En mode graduel (FZ=0), un resultat sous-normal exact ne pose ni UFC ni IXC.
- Un resultat minuscule inexact pose UFC et IXC. La petitesse est detectee **avant arrondi**, meme si le resultat arrondi devient le plus petit nombre normal.
- Avec FZ=1, une entree sous-normale est remplacee par un zero de meme signe et pose IDC.
- Un resultat non nul minuscule avec FZ=1 devient un zero de meme signe et pose UFC ; cette mise a zero n'ajoute pas IXC.
- Les annulations exactes et les zeros signes respectent le mode d'arrondi et les signes des operations.

FZ est applique aux calculs, comparaisons, conversions, extrema et arrondis qui le demandent. Les copies FMOV, FABS/FNEG et les mouvements de donnees conservent leurs traitements de bits propres. La sauvegarde/restauration d'un environnement flottant hote n'est plus necessaire : le moteur ne le modifie pas.

## Validation

Le corpus FP/SIMD precedent de **96 400 cas sur 1 491 variantes** reste utilise. Un corpus independant supplementaire ajoute **64 896 cas sur 129 variantes** : operations S/D, calculs fusionnes, formes vectorielles et par paires, comparaisons, conversions, extrema et arrondis. Il inclut les deux valeurs de FZ, DN, les quatre modes d'arrondi, les frontieres normal/sous-normal, les egalites d'arrondi, les produits et alignements extremes, des valeurs aleatoires, les NaN/infinis et les zeros signes.

Les encodages sont produits par Clang 18.1.6 via Zig 0.13.0 ; les resultats et flags attendus viennent d'Unicorn 2.1.4. Le lecteur compare tout le contexte expose et verifie le decoupage des variantes via `reference-layout.txt`. Le test d'environnement hote conserve un arrondi different et FTZ/DAZ actifs pour detecter une dependance accidentelle au calcul du PC.

Le programme ELF de demonstration comprend 128 octets de resultats/flags : produits sous-normaux exacts et inexacts S/D, FZ en entree et sortie, calcul vectoriel minuscule et conversion D vers S avec underflow. Ces octets sont verifies contre des valeurs attendues explicites. La demonstration comptait 308 instructions lors de ce palier ; elle en compte maintenant **343** avec les [ajouts SIMD suivants](CPU_SIMD_ARITHMETIC.md).

La suite complete compte maintenant **24 tests CTest**, tous reussis en Debug et Release avec MSVC. Ces comparaisons ne remplacent pas une validation sur le materiel cible ni une preuve exhaustive de conformite ARM64.

## Reproduction

```powershell
./tools/build_subnormal_reference.ps1 -Python /chemin/vers/python.exe
./tools/build_fp_reference.ps1 -Python /chemin/vers/python.exe -SamplesOnly
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Le corpus est conserve dans `tests/programs/cpu_subnormal`. Le generateur reutilise le format du corpus FP/SIMD, avec un preparateur d'entrees specifique. Les manifestes conservent les versions, la graine, les variantes et les empreintes des references.

## Limites et suite

Les exceptions flottantes piegees restent refusees avec un diagnostic. FP16, FPCR.FZ16/AHP, les nouvelles extensions Alternate Handling, les conversions fixed-point et plusieurs familles SIMD restent absentes. Ce moteur est une reference de correction ; aucune promesse de performances pour des jeux ou d'execution multicoeur n'est faite. Saturation, decalages variables et multiplications elargies sont maintenant ajoutes ; la suite concerne des programmes C/C++ plus varies.

## References

- [Arm, exceptions et detection de petitesse avant arrondi](https://support.arm.com/documentation/dui0349/c/floating-point-support/ieee-754-arithmetic/exceptions).
- [Arm, pseudocode des operations et de FPRound](https://support.arm.com/documentation/ddi0487/mc/-Part-E-The-AArch32-Application-Level-Architecture/-Chapter-E1-The-AArch32-Application-Level-Programmers--Model/-E1-3-Advanced-SIMD-and-floating-point-instructions/-E1-3-11-Pseudocode-description-of-floating-point-operations?lang=en).
- [Unicorn, moteur de comparaison independant](https://github.com/unicorn-engine/unicorn).
