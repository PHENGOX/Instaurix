# Switch2Emu

Je développe cet émulateur avec l’aide d’une intelligence artificielle pour l’écriture du code, les tests et l’analyse technique.

Émulateur expérimental en C++20 visant à terme l'exécution de jeux Nintendo Switch 2 sur PC. Le développement actuel porte sur les fondations ARM64 et les services système HLE. **Aucun jeu commercial ni aucune compatibilité native Switch 2 ne sont actuellement validés.**

Prototype de machine ARM64 en C++20 : CPU entier/FP/SIMD partiel, runtime HLE de référence et formats ELF/NRO statiques bornés. Il ne lance pas de jeux Switch 2.

Dernière étape : [identité, messages et focus](docs/LIBNX_APPLET_STATE.md). Le client libnx obtient une identité de processus stable et utilise un véritable événement de messages et sa routine d'attente originale. Il s'arrête désormais sur AcquireForegroundRights, avant main. Le profil exige `--hle-reference --homebrew-entry`. Prochaine priorité : acquisition du premier plan et dépendances observées du démarrage. La démo interactive reste accessible par `.\tools\run_interactive.ps1`.

## Compiler et tester (PowerShell)

Le dépôt contient les petits programmes et données de référence nécessaires aux tests. Les outils téléchargés, caches et dossiers de compilation sont exclus. Zig, Python et Unicorn servent à reconstruire certaines références ; ils ne sont pas requis pour compiler le cœur C++ et lancer les tests fournis. Les validations libnx concernent l'écosystème historique Switch et ne prouvent pas une compatibilité Switch 2. Dernière validation : **60 suites CTest réussies en Debug et Release**.

**Git LFS est nécessaire pour récupérer trois grandes références CPU.** Après clonage, exécuter `git lfs install` puis `git lfs pull` avant les tests. Le téléchargement ZIP de GitHub peut fournir des pointeurs LFS plutôt que les données. Les programmes de test ELF/NRO restent inclus normalement.

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

CMake 3.20 minimum et un compilateur C++20 sont necessaires. Sous Windows, les outils C++ de Visual Studio conviennent.

## Licence et contributions

Le code propre à Switch2Emu est distribué sous **GNU GPL version 3 uniquement** (`GPL-3.0-only`) ; voir [LICENSE](LICENSE). Les composants et références tiers conservent leurs licences : [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Les contributions sont les bienvenues : fournir un cas reproductible, préciser les limites et respecter les licences d'origine. Une contribution au code propre au projet est proposée sous GPL-3.0-only. Ne joindre aucun jeu, firmware, clé console ou fichier confidentiel aux contributions.

Projet indépendant, sans affiliation avec Nintendo. Pour préparer le dépôt GitHub : [guide de publication](docs/GITHUB_PUBLICATION.md).

## Executer le programme fourni

```powershell
.\build\Debug\Switch2Emu.exe .\program.bin 4
```

Resultat attendu : 4 instructions, X0=5, X1=7 et X2=12. Le programme contient MOVZ, MOVZ, ADD et RET. RET revient a X30, initialement nul ; le budget de 4 instructions arrete donc l'execution avant de recommencer.

Le premier argument est un fichier binaire ARM64 brut, charge a l'adresse 0. Sa taille doit etre non nulle, multiple de 4 et inferieure ou egale a 16 Mio. Le second argument est un budget strict d'instructions (100000 par defaut). Un budget atteint est un arret volontaire, pas la preuve que le programme s'est termine. Une erreur de fichier, d'instruction ou de bus renvoie un code de sortie non nul.

## Organisation

- `src/cpu.*` : interpretation ARM64 pas a pas ; tous les acces passent par Bus.
- `src/memory.*` : RAM, pages de 4 Kio allouees a la demande.
- `src/bus.*` : RAM et peripheriques MMIO ; un acces traversant une region est refuse.
- `src/machine.*` : assemblage des composants, chargement et boucle d'execution.
- `src/main.cpp` : interface en ligne de commande.
- `tests/legacy_tests.cpp` : verifications historiques, avec echec du processus si une ligne FAIL apparait.
- `tests/core_tests.cpp` : regressions, 5334 masques logiques et integration CPU/bus/minuteur/IRQ.

## Machine de developpement

Ces adresses sont propres au prototype et ne representent pas la carte materielle de la Switch 2.

| Adresse | Fonction |
| --- | --- |
| 0x00000000 a 0x00FFFFFF | RAM de 16 Mio ; programme a 0, pile initiale a 0x01000000 |
| 0x10000000 | Peripherique de test : ID +0, CONTROL +4, STATUS +8, DATA +12 (32 bits) |
| 0x10001000 | IRQ : PENDING +0 (ecriture de bits pour acquitter), ENABLE +4, ACTIVE +8 (32 bits) |
| 0x10002000 | Minuteur : COUNTER +0, PERIOD +8 (64 bits), CONTROL +16 (32 bits, bit 0 active) |

Une instruction reussie fait avancer le minuteur d'un tick. Le minuteur declenche l'IRQ 4. Une IRQ active et autorisee provoque un retour de la boucle au programme hote, qui peut l'acquitter et reprendre l'execution. L'entree dans une exception ARM64, les vecteurs, niveaux de privilege et ERET restent a implementer. Le minuteur ne modelise pas le temps reel ni les cycles d'un processeur physique.

## Limites et prochaines etapes

Le CPU reste partiel : permissions de pages et premier sous-ensemble FP/SIMD disponibles, mais pas de traduction MMU, exceptions architecturales ou execution multicoeur. Pas de GPU, audio, manettes, services systeme de jeux ou formats de jeux. Les tests actuels valident les cas couverts, pas toute la conformite ARM64.

Direction retenue : emulateur applicatif hybride, avec services systeme en HLE. Premier jalon realise : un programme C compile avec pile, resultat verifiable et fin explicite. Chargement ELF ARM64 statique et permissions memoire R/W/X desormais disponibles pour les programmes de test. Les exceptions materielles completes ne sont plus le prochain chantier par defaut. `Machine::load` remplace la RAM et reinitialise CPU et peripheriques apres un chargement reussi ; un echec conserve la session precedente.

## Strategie Switch 2

- [Architecture, jalons et prochain chantier](docs/STRATEGY.md)
- [Inventaire des connaissances et sources publiques](docs/SWITCH2_RESEARCH.md)

Ces documents distinguent le prototype ARM64 des interfaces natives Switch 2 restant a valider. Objectif a terme : jeux jouables sur Windows ; aucune compatibilite avec un jeu n'est encore revendiquee.

## Premier programme C et instructions de pile

Les paires de registres entiers LDP/STP sont implementees en W/X, avec offset signe, pre-index et post-index. Les variantes SIMD et les paires MMIO restent non prises en charge.

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\stack_sum\program.bin 500 --test-protocol
```

Cette demonstration compilee calcule 55 en memoire et se termine normalement via un protocole de test local. Le test d'integration verifie aussi la restauration de la pile. Ce protocole ne represente pas une ABI Switch 2 et reste desactive par defaut.

- [Sources, reproduction et limites du programme](tests/programs/stack_sum/README.md)
- [Plan technique du GPU](docs/GPU_PLAN.md)


## Programmes ELF ARM64 et protections memoire

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\elf_sum\program.elf 500 --test-protocol
```

Le programme ELF de reference calcule **55** en **94 instructions**, avec code R/X, constantes R, donnees R/W et pile non executable. Les **11 tests CTest passent**, y compris 102 variantes ELF invalides refusees. Les anciens binaires bruts restent disponibles et conservent leur memoire R/W/X historique.

- [Chargeur ELF, droits memoire et restrictions](docs/ELF_LOADER.md)
- [Sources et reproduction du programme ELF](tests/programs/elf_sum/README.md)

## Premier profil HLE de reference

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_memory\program.elf 500 --hle-reference
```

Ce profil ajoute un processus et un thread principal, les registres locaux du thread, puis SetHeapSize, QueryMemory et ExitProcess selon les conventions publiques de reference. La demonstration C produit **55 en 121 instructions** ; les **15 tests CTest passent**.

Le mode est explicite, reserve aux ELF et distinct du protocole de test precedent. Sa disposition memoire est locale et sa compatibilite native Switch 2 reste a valider.

- [Services, contexte, limites et sources](docs/HLE_RUNTIME.md)
- [Programme HLE et reproduction](tests/programs/hle_memory/README.md)

## Audit et consolidation du CPU entier

Le CPU traite maintenant les calculs entiers usuels W/X : arithmetique avec flags et retenue, multiplication/division, decalages et rotations, champs de bits et selections conditionnelles. Le decodage des acces memoire scalaires et des conditions a ete revu.

Validation du corpus entier actuel : **41 304 comparaisons independantes** sur **775 variantes**, incluant maintenant CCMP/CCMN. Un programme C 32/64 bits donne les memes resultats sur l'emulateur et en execution native, en 303 instructions invitees. La suite complete compte desormais 24 tests CTest avec les paliers suivants.

- [Audit, couverture et limites du CPU](docs/CPU_AUDIT.md)
- [Programme de calcul et reproduction des references](tests/programs/cpu_integer/README.md)

Le CPU n'est pas termine : FP/SIMD est encore partiel et plusieurs autres familles restent a developper. L'ajout de services HLE et d'IPC est differe pour donner la priorite a ces fondations.

## Palier atomiques et exclusives

LDXR/STXR, leurs variantes acquire/release, les paires exclusives, LDAR/STLR et les principales operations LSE (dont CAS et SWP) sont disponibles. Le suivi des reservations detecte les ecritures dans le meme granule RAM, y compris depuis l'hote ou un autre CPU execute sequentiellement. CLREX et les barrieres usuelles sont traites. Le modele reste sequentiel et ne fournit pas encore l'execution parallele multicoeur.

Validation de ce palier : **19 suites CTest**, avec une nouvelle suite utilisant **206 instructions assemblees independamment**. Voir [couverture, tests et limites](docs/CPU_ATOMICS.md).

## Premier palier FP/SIMD

Les 32 registres vectoriels, FPCR et FPSR sont sauvegardes dans le contexte du thread. Le CPU execute un premier ensemble de calculs flottants S/D, comparaisons, copies, acces memoire B/H/S/D/Q et operations vectorielles entieres. Les calculs sous-normaux et FZ sont maintenant pris en charge par un moteur logiciel ; les exceptions piegees et certains modes FPCR restent refuses.

Validation : **24 suites CTest reussies**, avec **96 400 cas FP/SIMD generaux** et **64 896 cas sous-normaux/FZ supplementaires**. Le meme calcul C float/double produit les memes bits sur l'hote et l'emulateur ; l'executable de test enchaine aussi des operations SIMD, conversions entier/flottant et S/D, et multiplication-addition fusionnee. Il termine en **343 instructions**.

Les conversions couvrent les entiers signes/non signes W/X et les cinq arrondis des conversions vers entier, avec saturation et flags d'erreur. FMADD/FMSUB/FNMADD/FNMSUB S/D utilisent un seul arrondi. Les modes FPCR non couverts restent explicites ; les sous-normaux et FZ sont disponibles.

Le SIMD entier inclut maintenant multiplications/accumulations, comparaisons signees et non signees, extrema, reductions, entrelacements et extractions, inversion d'ordre des elements, comptage de bits et decalages immediats. Une chaine de ces operations et de CCMP/CCMN est verifiee sur 112 octets par une reference scalaire separee.

Les vecteurs flottants 2S/4S/2D disposent de FADD/FSUB/FMUL/FDIV/FSQRT, FMLA/FMLS fusionnes et FABS/FNEG. Ils partagent le traitement scalaire des arrondis et NaN. Une erreur sur un element annule toute l'instruction. Une seconde chaine de 112 octets est comparee bit pour bit aux calculs natifs. Le moteur logiciel gere aussi les sous-normaux arithmetiques et FZ.

Trois extensions ajoutent les reductions flottantes par paires et extrema sur 4S, les conversions vectorielles entier/flottant et les instructions FRINT et minimum/maximum scalaires/vectorielles. Une chaine de 128 octets verifie leur integration. Les variantes NM, les zeros signes et la difference de flags entre FRINTI et FRINTX sont controles.

Les transferts multiple structures LD1/ST1 a LD4/ST4, les tables TBL/TBX et les copies DUP/INS d'elements et SMOV sont disponibles. Une chaine de 144 octets verifie desentrelacement, calcul, consultation de table et reentrelacement. Les acces incomplets sont refuses avant tout effet et les ecritures invalident les reservations exclusives.

- [Couverture, limites et suite CPU](docs/CPU_FP_SIMD.md)
- [Programme et reproduction](tests/programs/cpu_fp/README.md)

Le [moteur flottant logiciel](docs/CPU_SOFTFLOAT.md) calcule sur les bits avec des entiers exacts : il gere les sous-normaux, la petitesse avant arrondi, FZ et les flags UFC/IXC/IDC sans dependre des flottants du PC.

Le [palier SIMD arithmetique](docs/CPU_SIMD_ARITHMETIC.md) ajoute saturation/QC, retrecissements, decalages variables et multiplications elargies : **61 056 comparaisons independantes supplementaires**. La prochaine priorite est une suite de programmes C/C++ compiles plus varies, avant plusieurs threads.
