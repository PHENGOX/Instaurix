# Audit CPU : premier palier entier ARM64

État vérifié le 24 octobre 2026. La priorité de travail est le CPU ; aucun nouveau service HLE ni IPC n'a été ajouté pendant ce palier.

## Résultat

Le premier palier comptait 18 tests CTest et 32 832 comparaisons indépendantes sur 640 variantes. **Le corpus entier couvre maintenant 41 304 comparaisons sur 775 variantes**, avec CCMP/CCMN W/X, immédiats et registres, et encodages invalides supplémentaires. La suite complète compte 24 tests CTest, avec les paliers FP/SIMD, sous-normaux, SIMD arithmétique et atomiques. Un programme C mêlant calculs 32/64 bits produit les mêmes résultats dans l'émulateur et en exécution native x64 :

- 64 bits : `0x78335ef4bc270ff5` ;
- 32 bits : `0xc5473408` ;
- exécution invitée : **303 instructions**, pile restaurée et fin explicite.

Il s'agit d'un palier de validation du calcul entier, pas d'un CPU ARM64 complet ni d'une preuve de compatibilité Switch 2.

## Constats et corrections

| Point audité | Constat avant ce palier | Changement vérifié |
| --- | --- | --- |
| ADD/SUB immédiats | Variantes SUBS absentes ; WSP et SP mal distingués de ZR dans certaines formes | Traitement commun 32/64 bits, source/destination SP selon l'encodage, destination ZR pour les formes avec flags |
| Addition avec retenue | ADC/SBC absents | Calcul commun de la retenue et du débordement, avec préservation des autres bits PSTATE |
| Décalages | Les décodeurs de registre ne traitaient que des formes simples | Validation des quantités ; décalages signés/logiques et rotations dans les formes acceptées |
| Logique immédiate | La destination 31 était toujours ignorée | Destination SP dans les formes sans flags ; ANDS/TST avec N/Z et C/V remis à zéro |
| Conditions | Nombreux blocs de branchement séparés | Une évaluation des 16 codes de condition, partagée par branchements et sélections |
| Adresses relatives | Calculs passant inutilement par des additions signées | Déplacements relatifs calculés sans débordement signé C++ |
| Accès mémoire scalaires | Extension et mise à l'échelle d'offset registre non interprétées complètement ; code dupliqué par largeur | Décodage commun des tailles, extensions, offsets et mises à jour de base |
| Encodages inconnus | Certains masques acceptaient trop largement des variantes | Rejet explicite des combinaisons réservées ou non modélisées dans les familles traitées |
| Test historique SP | Deux tests étiquetés XZR ne vérifiaient que la conservation de X0 | Libellés corrigés et vérification de SP après ADD/SUB immédiats |

Les sources sont maintenant réparties entre `cpu.cpp` pour l'exécution/décodage principal, `cpu_integer.cpp` pour le calcul entier, `cpu_load_store.cpp` pour les accès scalaires et `cpu_pair.cpp` pour les paires. Les blocs remplacés ont été retirés ; ils ne restent pas comme chemins concurrents.

## Familles couvertes par cette étape

| Famille | Formes implémentées et exercées |
| --- | --- |
| Arithmétique | ADD/ADDS/SUB/SUBS immédiats, registres décalés et étendus ; ADC/ADCS/SBC/SBCS ; alias CMP/CMN/NEG selon ces encodages |
| Logique | AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS sur registres décalés ; AND/ORR/EOR/ANDS immédiats |
| Décalages et champs | LSLV/LSRV/ASRV/RORV ; SBFM/BFM/UBFM et leurs alias de décalage, extraction, insertion et extension ; EXTR/ROR |
| Multiplication | MADD/MSUB, MUL/MNEG ; formes longues signées/non signées ; SMULH/UMULH |
| Division | UDIV/SDIV W/X, y compris division par zéro et cas signé minimal divisé par -1 |
| Bits et octets | RBIT, REV16, REV32/REV selon largeur, CLZ et CLS |
| Conditions | CSEL/CSINC/CSINV/CSNEG ; B.cond et les branchements déjà présents |
| Comparaisons conditionnelles | CCMP/CCMN W/X, registre ou immédiat 5 bits ; les 16 conditions et les 16 combinaisons NZCV initiales sont exercées ; flags immédiats de repli lorsque la condition échoue |
| Mémoire scalaire | LDR/STR, formes signées, LDUR/STUR, offsets immédiats et registre UXTW/SXTW/LSL/SXTX, pré/post-index et lectures littérales W/X/SW |

Une famille annoncée ne signifie pas la prise en charge de toutes les extensions ARM ayant des noms proches. Les quantités de décalage et combinaisons réservées sont vérifiées. Les écritures W remettent à zéro la moitié haute de X. Les registres zéro et SP gardent une signification déterminée par la forme d'instruction.

## Validation indépendante

Les encodages sont produits par **Clang 18.1.6 via Zig 0.13.0**. Les résultats attendus sont générés par **Unicorn 2.1.4**, moteur ARM64 indépendant de notre interpréteur. Les scripts, l'assembleur et un manifeste lisible sont conservés avec les artefacts.

Chaque cas compare les registres X0–X30, SP, NZCV et le prochain PC ; les cas mémoire comparent aussi une fenêtre de 512 octets. Les autres bits du PSTATE local doivent rester inchangés. Les encodages invalides sélectionnés doivent être refusés sans changer les registres. Les entrées incluent valeurs limites, retenues, signes, décalages, combinaisons de flags et données déterministes, avec graine `0xA6402026`.

La comparaison C est un second contrôle : le même calcul unsigned, défini en C, est compilé pour le CPU hôte et pour l'invité. Elle vérifie un enchaînement d'instructions, des boucles et les résultats mémoire, en plus des tests instruction par instruction.

Unicorn peut aussi avoir des limites ou erreurs. Le corpus est fini, ne couvre pas toute l'ISA et ne remplace pas une comparaison avec le matériel cible. Voir [le guide des tests et de reproduction](../tests/programs/cpu_integer/README.md).

## Limites explicites et suite CPU

- Opérations exclusives/atomiques ajoutées dans le [palier suivant](CPU_ATOMICS.md), selon un modèle séquentiel ; toujours aucun modèle d'exécution parallèle multicoeur.
- Premier sous-ensemble flottant/Advanced SIMD et état de contexte ajoutés dans le [palier FP/SIMD](CPU_FP_SIMD.md), avec des limites explicites.
- CCMP/CCMN sont maintenant pris en charge ; d'autres familles et extensions entières restent absentes.
- Les variantes mémoire non privilégiées, PRFM et les paires non temporelles restent absents. Les transferts scalaires/vectoriels B/H/S/D/Q et paires S/D/Q sont ajoutés dans le palier FP/SIMD.
- La pile utilisée comme base mémoire doit être alignée sur 16 octets dans le profil local. Les chevauchements de base/destination avec writeback non modélisés sont refusés.
- Le modèle mémoire local refuse les débordements numériques d'adresse pour les transferts, au lieu de modéliser toutes les règles d'adressage d'un système complet. Les cas différentiels utilisent des adresses valides dans la fenêtre locale.
- Toujours aucun JIT, MMU complète, gestionnaire d'exceptions privilégiées ou démarrage natif Switch 2.

Les paliers atomiques/exclusives et un premier sous-ensemble FP/SIMD avec état de contexte sont maintenant réalisés et documentés séparément. Les conversions entier/flottant et S/D et les calculs fusionnés sont également ajoutés. La suite doit traiter les cas flottants encore refusés et élargir les opérations vectorielles. La conformité continue à être présentée par familles vérifiées, sans annoncer un CPU « terminé » sur la seule base d'un nombre d'instructions.

## Références

- [Arm : aperçu officiel de l'ISA A64](https://developer.arm.com/-/media/Files/pdf/graphics-and-multimedia/ARMv8_InstructionSetOverview.pdf), notamment les familles de multiplication/division et les comportements limites de division.
- [Arm : guide de référence A64](https://documentation-service.arm.com/static/5f3fa899428f7a6b3328fd44), pour les formes d'instructions.
- [Unicorn : projet et moteur indépendant](https://github.com/unicorn-engine/unicorn), version de test 2.1.4 [publiée sur PyPI](https://pypi.org/project/unicorn/2.1.4/).

Ces sources servent à la définition et à la comparaison ; aucune implémentation tierce n'est intégrée dans le cœur CPU du projet.
