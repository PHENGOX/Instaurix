# Audit : passer des démos à une application externe

Audit du 25 octobre 2026, fondé sur le dépôt et sur des sources publiques. Les clients actuels sont écrits pour le profil du projet et ses services emu:*. Leur réussite ne mesure pas la compatibilité avec un SDK complet ni avec la Switch 2 native.

## Cible externe retenue

**switchbrew/switch-examples, graphics/printing/hello-world/source/main.c**, avec son démarrage libnx. Le dépôt se présente comme des exemples Switch pour devkitA64 et libnx. Cette cible appartient à l'écosystème historique Switch ; elle n'est pas retenue comme preuve native Switch 2.

Le source examiné sur master le 25 octobre appelle la console, la configuration des contrôleurs, une boucle d'applet et la mise à jour de l'affichage. Il ne nomme aucun service emu:*. Son succès devra être observé sans remplacer ces dépendances par nos interfaces de démonstration.

**Cette cible n'a pas encore été compilée ni exécutée dans le projet.** Aucun NRO tiers ni SDK Nintendo n'a été importé. Les révisions publiques sont désormais figées : switch-examples `669786898205b7beb25ff1731e72982e6d0397d3`, libnx `b156b4774fb2df7b93402169509266da7547fc4c`. Un [premier composant libnx inchangé est exécuté séparément](LIBNX_REFERENCE.md). La chaîne devkitA64/newlib complète reste à figer avant de produire le hello-world original.

## Écarts observés

| Couche | Ce que le dépôt exécute | Besoin de la cible / blocage |
| --- | --- | --- |
| Format | ELF ET_EXEC et nouveau profil NRO0 statique | Le NRO libnx comporte un démarrage MOD0 et une étape de liaison à traiter |
| Démarrage | Registres remis à zéro, pile et TLS propres au banc | Contrat homebrew, environnement d'entrée, retour au chargeur, handle principal |
| Liaison | Adresses fixes, aucun linker dynamique hôte | Métadonnées MOD0, GOT et relocations ; libnx appelle son propre démarrage dynamique |
| Mémoire | 16 Mio, tas local borné, QueryMemory simplifié | Allocation et informations demandées par l'initialisation libnx à confronter à une référence |
| Runtime | Wrappers SVC originaux, C freestanding sans libc | Runtime libnx/newlib, contexte de thread, initialisation et sortie |
| Services | sm: partiel, emu:demo/fs/disp/pad | Services réellement employés par console, applet et contrôleurs |
| Graphisme | RGB calculé par le CPU et transmis à emu:disp | Chemin d'affichage de la bibliothèque cliente, services et synchronisation correspondants |
| Entrées | Masque local de cinq boutons | Interfaces et structures du contrôleur utilisées par libnx |

Les premières lignes de switch_crt0 montrent l'appel de liaison dynamique et de l'initialisation avant main. Le source de l'initialisation contient aussi les choix de tas et les dépendances de services. L'audit de sources identifie ces obligations ; il ne fournit pas une trace d'exécution ni le premier SVC exact atteint par un binaire compilé.

## Avancée concrète de cette étape

Le chargeur accepte désormais un profil borné de **NRO0 statique**, décrit dans [NRO_LOADER.md](NRO_LOADER.md). Notre client C existant a été empaqueté dans ce format, avec une BSS distincte. Il parcourt **14 343 instructions** et produit le même rapport que sa version ELF correspondante.

Ce NRO est un **échantillon original du projet** et ne remplace pas le hello-world externe. Les NRO avec MOD0 restent refusés par défaut ; un [profil d'entrée expérimental](NRO_BOOTSTRAP.md) fournit désormais l'environnement, le handle principal et la validation des relocations pour un sous-ensemble explicite. Son banc exécute le composant dynamique libnx inchangé, avec un démarrage original du projet. Les variantes inconnues et les formats de contenu ne retombent pas silencieusement sur le chargement de code brut.

## Ordre de travail et critères de réussite

1. Figer la révision publique et la chaîne de compilation de la cible externe, ainsi que l'empreinte de son binaire.
2. Décrire puis tester le contrat de démarrage homebrew : environnement d'entrée, mémoire, TLS, handle de thread et sortie.
3. Valider MOD0 et les relocations nécessaires sur des références reproductibles, sans accepter silencieusement des types inconnus.
4. Exécuter une cible libnx minimale jusqu'à son premier arrêt, enregistrer les paramètres et corriger le blocage identifié. La console graphique complète peut nécessiter une étape intermédiaire sans affichage.
5. Pour le hello-world, obtenir le texte attendu, une boucle stable et une sortie par le bouton prévu, sans adaptation de son source vers emu:*.

Après le [profil d'entrée NRO](NRO_BOOTSTRAP.md), le [crt0 original avec RELRO](LIBNX_CRT0.md), [l'initialisation minimale jusqu'à main](LIBNX_INIT.md), le [client SM original](LIBNX_SM.md) et [APM](LIBNX_APP.md), le client ouvre désormais le [proxy appletOE et ses premières interfaces en domaine](LIBNX_APPLET_DOMAINS.md). La prochaine priorité est **GetAppletResourceUserId et les états de l'application réellement demandés**. La cible utilise un sous-ensemble des sources newlib compilées avec Zig ; la chaîne devkitA64 complète et les autres services restent à traiter. Les besoins CPU supplémentaires seront déterminés par les instructions rencontrées.

Le jalon natif Switch 2 exigera une cible distincte, son origine, sa version et une comparaison vérifiable. Le succès d'une référence historique Switch ne suffira pas.

## Sources consultées

- [switch-examples : dépôt](https://github.com/switchbrew/switch-examples) et [hello-world](https://github.com/switchbrew/switch-examples/blob/master/graphics/printing/hello-world/source/main.c).
- [libnx switch_crt0.s](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/switch_crt0.s) : point d'entrée et démarrage avant main.
- [libnx init.c](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/init.c) : initialisation du runtime et du tas.
- [SwitchBrew NRO](https://switchbrew.org/wiki/NRO) et [switch-tools elf2nro.c](https://github.com/switchbrew/switch-tools/blob/master/src/elf2nro.c) : format et conversion étudiés.

Consultation le 25 octobre 2026. Les versions et formes non confrontées au code restent des hypothèses de compatibilité, pas des fonctions annoncées comme prises en charge.
