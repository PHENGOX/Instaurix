# Première exécution d'un composant libnx externe

Le 25 octobre 2026, le composant `nx/source/runtime/dynamic.c` de libnx a été compilé **sans modification** et exécuté par le CPU de l'émulateur. La révision est `b156b4774fb2df7b93402169509266da7547fc4c`. Le fichier original et sa licence sont conservés dans `tests/programs/libnx_dynamic`. Les empreintes et la chaîne de compilation sont consignées dans `reference.json`.

Ce jalon valide un composant réel de démarrage dans un banc ELF du projet. Les étapes suivantes ont ajouté un [profil NRO MOD0](NRO_BOOTSTRAP.md), le [crt0 original avec RELRO](LIBNX_CRT0.md), puis [l'initialisation originale jusqu'à main pour une cible minimale sans services](LIBNX_INIT.md). Le hello-world externe et ses services restent à exécuter. Aucun support natif Switch 2 n'est déduit de cette référence historique Switch.

## Résultats vérifiés

| Cas | Résultat | Instructions |
| --- | --- | --- |
| MOD0 valide, BSS non nulle, RELA NONE/RELATIVE et RELR adresse/bitmap | BSS effacée, pointeurs attendus, emplacements non ciblés préservés | 2 353 |
| Signature MOD0 invalidée | Retour anticipé, mémoire conservée | 2 142 |
| Relocation non prise en charge | Appel réel de `diagAbortWithResult`, résultat libnx BadReloc = 857 | 2 253 |

Chaque cas est exécuté en continu puis avec des budgets de sept instructions ; le nombre d'instructions, le statut et le rapport mémoire doivent être identiques. L'hôte vérifie aussi directement les pointeurs produits.

Validation complète après cette modification : **51 suites CTest réussies en Debug et en Release**. Une seconde reconstruction du banc reproduit le SHA256 enregistré dans le manifeste.

Le compilateur a révélé un blocage CPU réel : **LDPSW**. Cette instruction est désormais implémentée avec extension signée des deux mots de 32 bits, offsets à pas de quatre octets et modes offset/pré/post-indexés. Les tests couvrent les limites signées, les registres et flags, les encodages réservés, les destinations identiques et les lectures hors mémoire. Les restrictions existantes sur chevauchements et MMIO restent applicables.

Le banc fournit son propre `memset` freestanding et un piège d'arrêt pour `diagAbortWithResult`. Sa sortie utilise le protocole de test explicite `SVC F000`. Les wrappers de protection mémoire exécutent les vrais numéros SVC 2 et 3 ; ils ne renvoient pas artificiellement un succès. Les extensions LNY de protection RELRO ne sont pas exercées dans ce jalon. Les métadonnées du banc sont fiables et fabriquées pour le test : cela ne constitue pas une validation de fichiers MOD0 non fiables.

## Reproduction

```powershell
.\tools\build_libnx_dynamic.ps1
cmake --build build --config Debug
ctest --test-dir build -C Debug -R "libnx_upstream_dynamic_runtime|cpu_pairs_and_compiled_program" --output-on-failure
```

La reconstruction exige Zig 0.13.0. Elle télécharge l'archive publique épinglée si nécessaire, vérifie son SHA256, extrait les en-têtes et compare le source conservé à celui de l'archive. Les en-têtes ELF et C proviennent de la distribution Zig ; aucune libc musl n'est liée. Il ne s'agit donc pas d'une chaîne devkitA64/newlib complète. Les tests CTest ordinaires utilisent le binaire fourni et n'accèdent pas au réseau.

## Contrat d'entrée à construire ensuite

Les sources épinglées `switch_crt0.s`, `env.h` et `env.c` imposent les points suivants pour un NRO :

- X0 désigne une liste de `ConfigEntry` de 24 octets : clé u32, flags u32, deux valeurs u64 ; une entrée de clé zéro termine la liste.
- X1 vaut `UINT64_MAX` ; X30 contient l'adresse de retour au chargeur et SP désigne une pile utilisable et alignée.
- Les entrées déclarées doivent correspondre à des ressources réelles : handle de thread principal, tas de remplacement, éventuels arguments et masques des SVC disponibles.
- X0 nul fait choisir à `envSetup` son chemin NSO ; conserver simplement les registres actuels à zéro ne fournit pas le bon environnement NRO.
- Le code invité appelle `__nx_dynamic` avant l'initialisation libnx. Le chargeur devra valider les tables et les destinations sans appliquer une seconde fois ces mêmes relocations.

L'[étape NRO suivante](NRO_BOOTSTRAP.md) fournit désormais un premier environnement, le handle principal et les contrôles MOD0/RELA/RELR avec retour au chargeur. L'[initialisation libnx originale](LIBNX_INIT.md) est ensuite exécutée jusqu'à main avec un sous-ensemble des sources newlib et des hooks sans services. La chaîne devkitA64 complète et les services par défaut restent à résoudre. Le présent banc ELF ne les simule pas.

## Sources publiques épinglées

- [libnx dynamic.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/dynamic.c)
- [libnx switch_crt0.s](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/switch_crt0.s)
- [libnx env.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/env.c)
- [Cible hello-world, révision conservée pour la suite](https://github.com/switchbrew/switch-examples/blob/669786898205b7beb25ff1731e72982e6d0397d3/graphics/printing/hello-world/source/main.c)
