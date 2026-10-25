# Programme ARM64 compilé : pile et somme

Cette cible de développement combine une entrée assembleur (`start.S`) et une fonction C (`sum.c`). Elle ne représente pas un exécutable natif Switch 2 et ne demande aucun firmware.

## Résultat vérifié

- Le point d'entrée sauvegarde X29/X30 avec STP sur une pile alignée de 16 octets.
- Il appelle `sum_to(10)`, compilée en ARM64 par Clang 18.1.6 fourni avec Zig 0.13.0.
- La fonction C utilise une boucle réelle, des accès à la pile et calcule 55.
- Le résultat est écrit sur 64 bits à l'adresse 0x1000 ; la pile et X29/X30 sont restaurés.
- `SVC #0xF000` demande une fin avec le code contenu dans X0 (ici zéro).

Ce SVC est un **protocole de test local activé explicitement**, pas un appel système Switch 2. Tout autre SVC reste non pris en charge. Sans activation du protocole, même ce SVC produit une erreur. Le compteur inclut le SVC traité : cette version du programme exécute 85 instructions.

Le C est une fonction de test pour n=10 ; son comportement pour n=0 n'est pas une cible de validation.

## Lancer

Depuis la racine du projet, après compilation habituelle :

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\stack_sum\program.bin 500 --test-protocol
ctest --test-dir build -C Debug --output-on-failure
```

Le lancement affiche une fin normale avec code 0. Le test `cpu_pairs_and_compiled_program` contrôle en plus le résultat mémoire 55 et la restauration de la pile. Atteindre le budget sans exécuter le SVC reste un arrêt différent.

## Reproduire les binaires

Les binaires de référence sont conservés dans `tests/programs/`, pour que les tests habituels ne dépendent pas d'un téléchargement ni d'un compilateur croisé.

Compilateur utilisé : [Zig 0.13.0 portable Windows x64](https://ziglang.org/download/0.13.0/zig-windows-x86_64-0.13.0.zip), archive SHA-256 :

```text
d859994725ef9402381e557c60bb57497215682e355204d754ee3df75ee3c158
```

Cette empreinte a été vérifiée contre l'[index officiel de téléchargement](https://ziglang.org/download/index.json). Le compilateur est extrait localement dans `tools/zig-windows-x86_64-0.13.0/` et exclu de Git. Aucun ajout au PATH système n'est nécessaire.

```powershell
.\tools\build_arm64_samples.ps1
# Ou avec une autre installation compatible :
.\tools\build_arm64_samples.ps1 -Zig 'C:\chemin\zig.exe'
```

Le script expose les options exactes : cible `aarch64-freestanding-none`, compilation C sans bibliothèque standard, lien avec `link.ld` à l'adresse zéro, puis extraction de `.text`. Il produit aussi `sum.generated.s`, conservé pour inspection. L'ELF intermédiaire reste dans `build/` ; l'émulateur charge uniquement le binaire brut.

Il assemble indépendamment `tests/programs/pair_vectors.S` avec le même outil pour produire `pair_vectors.bin`. Les tests de LDP/STP utilisent ces encodages de référence plutôt qu'un assembleur écrit pour notre CPU.

Empreintes des artefacts vérifiés :

| Fichier | SHA-256 |
| --- | --- |
| stack_sum/program.bin | 0695fd1292e450249977dd7b2ab3b8f15fef7d2f3f062845e4efa4b85b21a5f5 |
| pair_vectors.bin | 8dbeddf8423500de87363a2fa51064f23983abcd2319ac1bc46d295f858bc4f7 |

## Portée CPU de cette étape

LDP/STP entiers W/X et LDPSW sont pris en charge en offset signé, pré-index et post-index. Les tests couvrent les offsets limites, les transferts entre pages, les registres zéro, la pile, les erreurs de limites et les recouvrements de registres non pris en charge. Les paires en MMIO sont refusées avant l'accès : le prototype ne promet pas de sémantique de périphérique pour ces instructions. Les paires SIMD suivent leur décodeur distinct ; les paires entières non temporelles restent non prises en charge. LDPSW a été ajouté pour le [composant libnx externe](../../../docs/LIBNX_REFERENCE.md).

`CPU::stepDetailed()` distingue exécution, demande SVC et erreur. Le raccourci historique `step()` renvoie vrai uniquement pour une instruction ordinaire exécutée ; il renvoie faux sur un SVC sans définir une erreur CPU. Les boucles qui traitent des services doivent utiliser le résultat détaillé.

Références : [Arm, guide A64 des instructions de transfert](https://documentation-service.arm.com/static/5f3fa899428f7a6b3328fd44), [Zig 0.13.0 / LLVM 18](https://ziglang.org/download/0.13.0/release-notes.html). Ce test établit des comportements précis, pas une conformité ARM64 complète.
