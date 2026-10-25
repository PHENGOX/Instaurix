# Initialisation libnx originale jusqu'à main

Le 25 octobre 2026, une cible NRO minimale atteint `main` en exécutant le corps original de `__libnx_init`. Le point d'arrêt du banc précédent n'est plus utilisé par cette cible. Le compilateur reste Zig 0.13.0 / Clang 18.1.6 ; des sources newlib officielles et leur correctif devkitA64 remplacent la dépendance aux paquets binaires inaccessibles.

## Parcours réellement exécuté

Les sources libnx de la révision `b156b4774fb2df7b93402169509266da7547fc4c` restent inchangées : crt0, dynamic, init, env, newlibSetup, virtmemSetup, initialisation du tas, du thread, argvSetup et sortie libnx. Les wrappers SVC proviennent également du `svc.s` original.

La cible appelle les étapes suivantes :

1. Relocations et protection RELRO par libnx.
2. Lecture des ConfigEntry par envSetup.
3. Initialisation des ThreadVars et copie des données TLS par newlibSetup.
4. Lecture des régions mémoire par virtmemSetup.
5. Utilisation du vrai tas de remplacement par l'initialisation du tas libnx.
6. Configuration du thread principal, interrogation de sa pile et mutex libnx non contesté.
7. Construction d'un argv vide par argvSetup.
8. Hook applicatif minimal, puis constructeur appelé par le `__libc_init_array` original de newlib.
9. Fonction main du banc, hook de nettoyage et retour par la sortie libnx originale.

**La cible ne demande aucun service par défaut.** Elle définit les hooks faibles `__appInit` et `__appExit` pour choisir une application sans console, applet, contrôleurs ou fichiers. Ces hooks ne remplacent pas les fonctions d'initialisation libnx énumérées ci-dessus. Le programme principal est original au projet ; ce n'est toujours pas le hello-world externe.

Un pont exit du banc enregistre le résultat de main puis appelle `__libnx_exit`. Dans cette révision, la fonction libnx retourne zéro au chargeur : le test vérifie donc séparément le résultat main=77 et le résultat chargeur=0. La sortie complète de la libc et le vidage des flux stdio ne sont pas exercés. Les dépendances hors profil, smAddOverrideHandle et nxlinkSetup, disposent de pièges d'erreur explicites, jamais de succès artificiels ; aucun n'est atteint dans le parcours validé.

## Corrections de l'émulateur

### Base TLS et IPC

Le profil homebrew plaçait encore la base du registre TPIDRRO_EL0 à TLR+0x200, convention héritée des anciens bancs. libnx utilise la base TLR elle-même. Ce profil utilise désormais la base de page pour le thread principal et le second thread ; les anciens profils gardent leur convention.

Les ThreadVars libnx se trouvent à TLR+0x1E0 : signature à +0x1E0, handle à +0x1E4, pointeur de thread à +0x1E8, pointeur reent newlib à +0x1F0 et pointeur TLS à +0x1F8. Le buffer IPC local utilise maintenant cette même base TLR dans le profil homebrew. Le test vérifie les valeurs écrites par le vrai code invité.

### GetInfo borné, SVC 0x29

Le profil répond uniquement aux informations locales demandées par virtmemSetup, avec le pseudo-handle du processus courant et sub-ID zéro :

| IDs | Information et valeur locale |
| --- | --- |
| 2, 3 | Réservation alias : base 0xA00000, taille 0x200000 |
| 4, 5 | Réservation du tas : base 0x200000, capacité 0x800000 |
| 12, 13 | Intervalle d'adresses du profil : 0x10000 jusqu'à la fin des 16 Mio |
| 14, 15 | Pile principale : base 0xFF0000, taille 0x10000 |
| 28 | Taille alias additionnelle : zéro |

Le chargeur refuse désormais aussi un segment qui recouvre la réservation alias annoncée. Cette réservation est un intervalle virtuel libre et ne signifie pas que les SVC de création d'alias sont implémentés. Les IDs inconnus restent explicitement non pris en charge, les mauvais handles et sub-IDs renvoient une erreur sans publier une fausse valeur. Les données ne prétendent pas décrire la mémoire d'une vraie console.

### Instruction MOVI rencontrée

Le compilateur a émis `MOVI` avec immédiat masque d'octets, pour D ou V.2D. Ce sous-ensemble est maintenant implémenté ; il ne constitue pas la prise en charge de toutes les formes MOVI/MVNI. Un corpus Unicorn 2.1.4 couvre les 256 masques, les deux largeurs et les destinations 0/31, soit **1 024 cas indépendants**. Le test vérifie le registre cible, la remise à zéro de la moitié haute pour D et la conservation des autres registres et flags.

## Résultats et trace

La cible de 81 920 octets termine en **1 660 instructions**. Le rapport à `0x23000` contient les indicateurs appInit/constructeur/main/appExit=15, les ThreadVars, les bornes du tas, les données TLS initialisées et le résultat de main=77. Aucun piège de dépendance n'est atteint. Une exécution par budgets de sept instructions reproduit le même rapport et le même nombre d'instructions.

`build/libnx-init-trace.csv` conserve les **13 appels observés** : SVC 2 et 3, neuf GetInfo avec IDs 2, 3, 28, 4, 5, 12, 13, 14, 15, un QueryMemory pour la pile, puis le retour local F001. Le test vérifie cet ordre. Les tests directs couvrent aussi GetInfo, les arguments invalides et le conflit d'une image avec la réservation alias.

Validation complète : **55 suites CTest réussies en Debug et en Release**. Une seconde reconstruction depuis les sources rafraîchies reproduit exactement le SHA256 du NRO enregistré dans le manifeste.

## Provenance et reconstruction

```powershell
.\tools\build_libnx_init.ps1 -Python <chemin_python>
ctest --test-dir build -C Debug -R "libnx_original_init_to_main|cpu_movi_mask_independent_reference" --output-on-failure
```

Le script télécharge puis vérifie newlib 4.6.0.20260123 et le correctif officiel devkitA64 `newlib-4.6.0.20260123-4.patch`, applique ce dernier dans le cache du projet et compile un sous-ensemble des sources originales. La préparation rafraîchit le cache depuis l'archive vérifiée ; elle refuse de restaurer un cache modifié qui ne correspond plus au correctif. Le manifeste conserve les URLs, empreintes, options et résultats attendus. Les licences et le source newlib d'initialisation des constructeurs accompagnent le banc.

Ce n'est pas une installation devkitA64 GCC ni une libc configurée complète. La configuration minimale choisit `__SWITCH__`, `_HAVE_INITFINI_ARRAY=1`, le modèle reent par défaut des sources et la compatibilité GNU habituelle de Clang. Les fonctions newlib effectivement liées fournissent les données reent/stdio, l'itération des constructeurs, les tables ctype et les opérations élémentaires de chaînes/mémoire. Aucun test de printf, malloc, fichiers ou threads pthread n'est annoncé. Le correctif officiel produit un avertissement sur une fonction inline de condition variable sans retour ; cette fonction n'est pas appelée et les sources n'ont pas été modifiées pour masquer l'avertissement.

## Suite

L'[étape suivante](LIBNX_SM.md) exécute désormais **smInitialize original**, le clonage et la fermeture de sessions, et vérifie les octets IPC réellement produits. La prochaine priorité est le premier service natif requis après SM. La chaîne devkitA64 complète et le hello-world original restent des jalons distincts. Ce succès sur l'écosystème historique Switch ne valide pas la compatibilité native Switch 2.

Sources : [libnx init.c épinglé](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/init.c), [newlib officiel](https://sourceware.org/newlib/), [correctif devkitA64 épinglé](https://github.com/devkitPro/buildscripts/blob/devkitA64_r29.2/patches/newlib-4.6.0.20260123-4.patch).
