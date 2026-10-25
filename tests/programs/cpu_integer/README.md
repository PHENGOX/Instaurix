# Tests du calcul entier ARM64

Cette cible vérifie le premier palier de calcul entier du CPU. Elle contient un corpus de référence indépendant et un programme C compilé pour deux architectures.

## Tests habituels

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\Switch2Emu.exe .\tests\programs\cpu_integer\program.elf 5000 --test-protocol
```

Le C invité exécute 303 instructions. Résultats attendus : X2 = `0x78335ef4bc270ff5`, X3 = `0xc5473408`, code de fin 0. `cpu_compiled_tests` compare ces résultats à l'exécution native des fonctions du même fichier `work.c`, ainsi qu'à leurs sorties mémoire. Le programme utilise des calculs unsigned, sans division par zéro C ni débordement signé indéfini.

## Corpus indépendant

`manifest.json` décrit les 775 variantes assemblées dans `vectors.S` ; `vectors.bin` contient leurs encodages. `reference.bin` contient **41 304 cas** générés par Unicorn 2.1.4, avec des entrées déterministes et leurs résultats attendus. CCMP/CCMN ajoutent les formes W/X, registre/immédiat, les 16 conditions, les flags de repli, les registres zéro et des encodages réservés. `reference.json` consigne le générateur, la graine et l'empreinte.

Le test C++ lit les références préproduites. Il n'a besoin ni d'Unicorn, ni de Python, ni d'un accès réseau pour s'exécuter. Les écarts affichent le numéro du cas, l'encodage, les registres et les flags concernés. Le manifeste permet de retrouver l'instruction source ; le générateur expose le nombre de cas par variante.

Format du fichier : signature `A64REF01`, nombre de cas little-endian sur 32 bits, puis pour chaque cas : instruction et options sur 32 bits, X0–X30/SP/NZCV initiaux, X0–X30/SP/NZCV/PC attendus sur 64 bits, et 512 octets mémoire attendus si le cas porte sur la mémoire. PC initial = 0x1000 ; fenêtre mémoire = 0x4000. Le bit 0 des options indique cette fenêtre, le bit 1 un encodage refusé. Ces octets sont des données de test, pas du code exécuté sur l'hôte.

## Reproduire le corpus

Compilateur : Zig portable 0.13.0, comme pour les [premiers tests C](../stack_sum/README.md). Dépendance Python de génération : `unicorn==2.1.4`, installée localement dans `tools/python-deps/`, exclu de Git.

Avec un Python disponible, depuis la racine du projet :

```powershell
python -m pip install --target tools/python-deps unicorn==2.1.4
.\tools\build_cpu_reference.ps1 -Python 'C:\chemin\vers\python.exe'
```

Le script génère l'assembleur et le manifeste, assemble via Clang, extrait les instructions puis calcule les références avec Unicorn. Le téléchargement de la dépendance est une étape explicite ; les tests CMake ne l'effectuent pas.

Pour reconstruire le programme C invité :

```powershell
.\tools\build_arm64_samples.ps1
```

Les scripts gardent leurs caches dans `build/` et n'ajoutent aucun outil au PATH système.

## Empreintes vérifiées

| Artefact | SHA-256 |
| --- | --- |
| reference.bin | 246dd419cd0a22458d18291996d03f23ea013949cfe7f5634155de3fb253d010 |
| vectors.bin | 14c33786a9fc9e702ed631f818913b93526ff9c3ed0e5484c0fa5a5e7bc6a22a |
| program.elf | 09305709049788e281b9dbcb1276623a3779aa41e3bae808eb700d8c49e2ad27 |

Voir [l'audit CPU](../../../docs/CPU_AUDIT.md) pour les corrections, familles couvertes et limites. Le corpus ne valide ni FP/SIMD, ni les opérations atomiques, ni toute la conformité ARM64.
