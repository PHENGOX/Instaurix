# Point d'entrée libnx original et protection RELRO

Étape du 25 octobre 2026 : le fichier `switch_crt0.s` de libnx, révision `b156b4774fb2df7b93402169509266da7547fc4c`, est désormais assemblé **sans modification** et exécuté dans un NRO de référence. Il appelle le `dynamic.c` original, applique la relocation, protège RELRO, sauvegarde la pile, puis atteint l'appel à `__libnx_init`.

## Ce qui est effectivement exécuté

Le banc conserve le marqueur HOMEBREW, le pointeur MOD0 et les extensions LNY0/LNY1/LNY2 émis par le source original. Les offsets MOD0 sont résolus par le script de liaison. Le chargeur valide les plages GOT et RELRO, les bornes fichier, l'alignement et la version LNY2=1 avant d'autoriser l'exécution. Les plages RELRO vides, les extensions incomplètes et les versions inconnues restent refusées dans ce profil.

Le chemin vérifié est le suivant :

1. Entrée NRO dans le crt0 original avec environnement et retour au chargeur.
2. Appel de `__nx_dynamic`, effacement BSS et relocation RELATIVE vers le rapport.
3. `svcSetMemoryPermission` (SVC 2) pour rendre deux pages RELRO accessibles en lecture seule.
4. `svcSetMemoryAttribute` (SVC 3) pour verrouiller ces permissions.
5. Sauvegarde de la pile initiale et passage des trois arguments à `__libnx_init`.
6. Arrêt explicite du banc à cette frontière, puis utilisation du `__nx_exit` original pour restaurer SP et revenir au chargeur.

**Ce banc conserve une sonde à la place de `__libnx_init`.** Elle enregistre les arguments et arrête le programme avec le marqueur `0xC001` (49 153), qui signifie « frontière atteinte ». Les symboles main, exit et exception du banc déclenchent un arrêt d'erreur s'ils sont atteints. L'[étape suivante](LIBNX_INIT.md) exécute désormais l'initialisation originale jusqu'à main dans une autre cible minimale sans services.

Le fichier fait 32 768 octets avec 4 096 octets de BSS. Le parcours compte **12 432 instructions**. Le rapport à `0x17000` contient le marqueur de passage, X0=`0xD00000`, X1=`UINT64_MAX`, le retour `0xD01000` et SP=`0x1000000`. Une BSS salie après chargement est effacée par le vrai code invité ; le crt0 écrit ensuite sa sauvegarde de pile à `0x18000`. La relocation à `0x15000` pointe vers le rapport.

Le test relève aussi chaque appel invité en avançant d'une instruction à la fois et écrit `build/libnx-crt0-trace.csv`. La séquence observée est SVC 2 au PC `0x1016C` (adresse `0x15000`, taille `0x2000`, permission R), SVC 3 au PC `0x10174` (même plage, masque et valeur `0x10`), puis le retour local F001 au PC `0xD01000` avec le marqueur `0xC001`. Les paramètres sont vérifiés par le test.

## SVC de protection bornés

Les SVC 2 et 3 sont disponibles uniquement pour la plage RELRO validée du profil homebrew. Cela ne fournit pas une implémentation générale des gestionnaires mémoire Horizon.

- SVC 2 accepte None, R et R/W, sans permission d'exécution. Les adresses et tailles doivent être alignées sur 4 Kio, la taille non nulle et l'ensemble de la plage inclus dans RELRO.
- SVC 3 accepte uniquement le masque et la valeur `MemAttr_IsPermissionLocked` (bit 4). Toutes les pages doivent déjà être en lecture seule. La suppression du verrou est refusée.
- Les changements sont précédés d'un contrôle de toutes les pages ; une erreur ne produit pas de changement partiel.
- Une plage verrouillée refuse toute nouvelle modification de permission. L'état et le bit d'attribut sont visibles dans QueryMemory, qui sépare ou regroupe les régions en tenant compte de ces attributs.
- Les pages de données, la pile, le tas, le texte et l'environnement restent hors de cette autorisation. Les droits de RAM appliquent réellement le refus d'écriture.

Les tests vérifient aussi les tailles/adresses invalides, les permissions interdites, les plages mixtes, les tentatives de déverrouillage, les extensions mal formées et la conservation du processus précédent sur un chargement refusé. Une exécution découpée par budgets de sept instructions reproduit le même rapport et le même résultat.

Validation complète après cette étape : **53 suites CTest réussies en Debug et en Release**. La seconde reconstruction reproduit exactement le SHA256 du manifeste.

## Reconstruction et provenance

```powershell
.\tools\build_libnx_crt0.ps1 -Python <chemin_python>
ctest --test-dir build -C Debug -R libnx_original_crt0_and_relro --output-on-failure
```

Le script utilise Zig 0.13.0, vérifie les sources contre l'archive libnx épinglée et compare le NRO à son empreinte enregistrée. Le source original, la licence et `reference.json` sont conservés dans `tests/programs/libnx_crt0`. L'option `--libnx-crt0` de l'empaqueteur préserve son en-tête existant ; elle ne génère pas les tables de relocation et ne remplace pas elf2nro. Le NRO reste lié à la base locale fixe `0x10000`.

## Chaîne complète et prochaine étape

La recherche locale n'a trouvé ni devkitA64, ni devkitPro pacman. Les métadonnées publiques officielles indiquent devkitA64 r30 et newlib 4.6.0.20260123-4. Les tentatives de lecture des paquets et index sur `pkg.devkitpro.org` ont répondu HTTP 403 dans cette session ; aucun installateur global n'a été lancé. **Ces outils ne sont donc pas installés ou validés dans le projet.** Le banc actuel reste compilé avec Zig et ne lie pas newlib.

Cette limite a ensuite été dépassée pour une cible minimale en [compilant des sources newlib officielles avec Zig](LIBNX_INIT.md) : l'initialisation libnx originale atteint main, avec des hooks applicatifs sans services. La chaîne devkitA64 complète et les services par défaut restent à traiter. Aucun appel manquant n'est remplacé par un succès artificiel pour annoncer leur prise en charge.

Sources publiques : [crt0 épinglé](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/switch_crt0.s), [dynamic.c épinglé](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/dynamic.c), [paquets officiels devkitA64](https://github.com/devkitPro/pacman-packages/tree/master/devkita64). Cette référence historique Switch ne démontre toujours pas de compatibilité Switch 2 native.
