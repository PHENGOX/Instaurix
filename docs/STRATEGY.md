# Stratégie Switch 2

Décision de travail du 24 octobre 2026. Objectif utilisateur : un émulateur sur PC capable, à terme, de lancer des jeux Switch 2. Ce document décrit la direction retenue ; il ne revendique aucune compatibilité actuelle avec ces jeux.

Priorité actualisée le 25 octobre : l'[audit de compatibilité](COMPATIBILITY_AUDIT.md) retient le hello-world public switch-examples/libnx comme cible externe historique, non native Switch 2. Le [chargeur NRO0 statique](NRO_LOADER.md) est testé ; le prochain verrou est le contrat de démarrage homebrew et MOD0/relocations. Le CPU continuera d'évoluer selon les blocages mesurés, sans assimiler les démos emu:* à une application externe exécutée.

## Architecture retenue

Windows x64 en première cible, C++20 et CMake. Approche hybride : exécution du code ARM64, reproduction des services système en HLE et traduction du graphisme vers Vulkan. Vulkan est un choix pour le PC, pas une affirmation sur les interfaces utilisées par les jeux de la console.

HLE signifie que nous reproduisons les résultats et effets visibles des services demandés par le programme. Cela exige aussi des erreurs, événements et règles de synchronisation corrects. Retourner systématiquement « succès » à un service inconnu ne constitue pas une implémentation.

Cette direction vise à concentrer le travail sur les interfaces nécessaires aux applications. Elle ne garantit ni performances ni compatibilité : ces deux points devront être mesurés. Elle évite de faire du démarrage intégral du système de la console un préalable au premier programme.

| Couche | Direction | État actuel |
| --- | --- | --- |
| CPU | Interpréteur de référence, puis traduction dynamique ARM64 vers x64 si nécessaire | Interpréteur partiel entier, FP/SIMD et atomiques, validé par corpus indépendants |
| Mémoire des processus | Adresses virtuelles, mappings et permissions lecture/écriture/exécution | RAM paginée avec droits R/W/X ; traduction MMU absente |
| Exécution | Contexte de processus, pile, mémoire locale de thread, arrêt explicite | ELF et NRO0 statiques, piles protégées, contextes entier/FP/SIMD/TLS ; deux threads ; bootstrap libnx absent |
| Noyau HLE | Appels système, objets et handles, événements puis threads | Mémoire/sortie, handles typés, événements et fin de thread ; attentes immédiates, infinies ou finies, temps virtuel piloté par l'hôte |
| Services HLE | Transport IPC et services choisis selon la cible | HIPC/CMIF partiel, buffers A/B, sm: limité, sessions, emu:demo et emu:fs sur instantané en lecture seule ; clients C testés ; services de jeux absents |
| GPU | Décodeur de commandes et shaders invités, synchronisation, backend Vulkan | Absent |
| Sortie graphique de développement | Contrôle visuel des pixels produits par les cibles ARM64 | emu:disp, séquence RGB/PNG horodatée, fenêtre Windows ; aucune commande GPU invitée |
| Audio / entrées / fichiers | Interfaces invitées séparées des fonctions Windows | emu:pad au clavier et emu:fs sur instantané en lecture seule ; audio et services natifs absents |
| Interface utilisateur | Bibliothèque, configuration, commandes et journaux après les premières cibles | Console de lancement |

Les adresses MMIO, le minuteur et les 16 Mio de la Machine actuelle restent un banc de développement. Ils ne définissent pas le modèle mémoire Switch 2. Garder ces tests pendant l'introduction d'un environnement applicatif distinct.

## Ce qui change dans les priorités

La gestion complète des exceptions privilégiées et ERET cesse d'être le prochain chantier par défaut. Pour un environnement HLE applicatif, traiter les appels système du programme et reprendre son exécution est prioritaire. Une implémentation complète du matériel sera envisagée seulement si une cible démontre ce besoin.

Conserver l'interpréteur pour diagnostiquer les erreurs ; différer le JIT jusqu'à disposer d'une cible reproductible et d'un profil de performances. Les instructions SIMD, atomiques et de virgule flottante seront ajoutées suivant les besoins constatés, sans supposer que le sous-ensemble actuel suffira à un jeu.

## Jalons et critères d'acceptation

| Jalon | Livrable | Preuve de réussite |
| --- | --- | --- |
| J0 — stratégie | Ce document et inventaire sourcé | Distinguer faits, hypothèses et blocages |
| J1 — application minimale | Programme ARM64 de test dont nous avons les sources, résultat en mémoire et fin explicite | Résultat déterministe ; fin distinguée du budget et des erreurs ; tests négatifs |
| J2 — chargement structuré | Chargeur de segments et permissions pour un format choisi et documenté | Sections et BSS correctes ; fichiers tronqués, plages invalides et variantes inconnues rejetés |
| J3 — environnement HLE | Sous-ensemble d'appels système, contexte de thread et IPC requis par la cible | Arguments, résultats, handles et buffers vérifiés ; appels non pris en charge identifiés |
| J4 — cible native Switch 2 | Programme ou trace de référence explicitement identifié comme natif Switch 2 | Comparaison des comportements observables avec une référence ; versions consignées |
| J5 — graphisme et interaction | Première chaîne graphique invitée comprise, entrées puis audio | Image produite par les commandes invitées ; synchronisation et interaction vérifiées |
| J6 — premier jeu | Un titre et une version déterminés | Chargement, menu puis séquence jouable reproductible ; erreurs restantes et performances mesurées |

J1 à J3 construisent des fondations. Leur succès ne prouve pas la compatibilité Switch 2. J4 nécessite une référence spécifique ; une application Switch 1, même exécutée sur une Switch 2 en mode de compatibilité, ne suffit pas.

## J1 — périmètre initial, désormais vérifié

Le jalon prévoit une petite cible ARM64 sans bibliothèque standard : une fonction appelée, une boucle, des accès à la pile, un résultat connu dans la mémoire et une demande de fin explicite. Garder ses sources et les instructions exactes de compilation avec l'artefact. Ne pas installer de chaîne de compilation avant d'avoir vérifié les outils disponibles.

Introduire un résultat d'exécution qui distingue instruction exécutée, demande système, fin normale et erreur. L'hôte traite une demande supportée puis reprend au bon PC. Pour cette cible, un mécanisme de sortie local peut servir de test mais devra être nommé « protocole de test » ; il ne sera pas présenté comme une ABI Switch 2.

Ajouter uniquement les instructions nécessaires à cette cible, avec priorité probable aux paires de registres pour la pile. Tester les débordements de plages et les demandes inconnues. La sortie normale doit rester distincte du budget atteint : le programme.bin existant fait RET vers zéro et s'arrête seulement grâce au budget.

Avant J2, examiner séparément NRO et NSO, leurs versions, leur initialisation et les données de référence disponibles. Un chargeur ELF peut servir à nos tests compilés ; il ne sera pas annoncé comme chargeur de jeux Switch 2.

## Conditions de passage et obstacles

- Sans contrat d'interface et référence vérifiable, continuer les fondations génériques et documenter l'inconnue ; ne pas inventer une ABI de console.
- Choisir pour chaque cible son origine, sa version, ses dépendances et le résultat attendu. Aucun titre commercial n'est actuellement sélectionné ou validé.
- Un rendu de démonstration produit directement par le PC valide le backend, pas le GPU invité. Le décodage des commandes et shaders reste une étape distincte.
- La présence d'un service sur les deux consoles ne prouve pas l'identité de ses commandes ou de ses structures.
- Réexaminer les hypothèses si les sources ou les mesures les contredisent. Ne fixer aucun calendrier de jeux jouables avant d'avoir une cible native et ses dépendances.

## Travail différé

Démarrage complet du firmware, précision cycle par cycle, reproduction de tous les périphériques physiques, interface élaborée, réseau en ligne et optimisation JIT ne sont pas des prérequis de J1. Les fonctions graphiques avancées sont reportées jusqu'à ce qu'une cible en ait besoin ; leur absence pourra limiter les jeux compatibles.

## Références

Point d'avancement actuel : [identité, messages et focus de l'application](LIBNX_APPLET_STATE.md). Prochain jalon : acquisition du premier plan, puis démarrage libnx jusqu'à main. Les ajouts CPU suivent les instructions réellement rencontrées ; le CPU reste partiel. Après ce démarrage, identifier les interfaces graphiques nécessaires avant d'engager le chemin GPU invité. Aucun délai de jeux jouables n'est encore établi.

Les constats externes et leurs limites sont réunis dans [l'inventaire des connaissances](SWITCH2_RESEARCH.md). Les décisions d'architecture ci-dessus sont des choix du projet, pas des spécifications officielles de la console.

## Avancement du 24 octobre 2026 : premier jalon CPU vérifié

J1 est réalisé dans le périmètre du protocole de test local : [programme C et entrée ARM64](../tests/programs/stack_sum/README.md), LDP/STP entiers W/X, pile restaurée, somme 1..10 égale à 55 en mémoire et fin explicite. Les huit suites CTest passent. Cela ne valide aucune ABI Switch 2.

Le chargement structuré ELF et les permissions mémoire sont désormais réalisés pour les tests ; la suite CPU consiste à couvrir les instructions requises par des cibles compilées plus larges. Avant de choisir un format natif, résoudre les questions de référence décrites dans l'inventaire. Le [plan GPU](GPU_PLAN.md) prépare séparément mémoire, commandes, shaders, synchronisation et Vulkan.


## J2 — chargement structuré et permissions réalisés pour les tests

Le [chargeur ELF64 AArch64 statique](ELF_LOADER.md) place les segments, initialise la BSS, vérifie le point d'entrée et configure une pile R/W non exécutable avec page de garde. Les protections de lecture, écriture et exécution sont vérifiées sur les accès CPU, y compris entre pages. Le programme C ELF produit 55 et se termine normalement en 94 instructions.

État de validation : 11 tests CTest réussis, dont 102 ELF invalides refusés. Ce jalon porte sur les fondations locales. Il ne couvre pas les formats natifs Switch 2, le démarrage d'un processus de console ni les services HLE réels. La prochaine étape consiste à élargir les cibles CPU compilées et à préparer le contexte applicatif HLE requis par une cible documentée.

## Début de J3 — processus et services mémoire de référence

Un [profil HLE explicite](HLE_RUNTIME.md) possède désormais un contexte entier de thread, ses registres locaux et trois SVC de référence : SetHeapSize, QueryMemory, ExitProcess. Un programme C compilé utilise ces fonctions, calcule 55 et se termine en 121 instructions. Les 15 tests CTest passent.

Ce début de J3 ne couvre pas encore les handles, l'ordonnancement multithread, les événements, l'IPC ou les services d'une application native Switch 2. Le profil sert à vérifier ces fondations avant d'élargir les interfaces ; il n'est pas présenté comme une validation de l'ABI Switch 2.


## Priorité CPU — premier palier entier réalisé

La priorité est désormais de consolider le CPU avant d'ajouter l'IPC. Le [bilan d'audit CPU](CPU_AUDIT.md) décrit les corrections de décodage, les familles entières ajoutées et leurs limites. Aucun nouveau service HLE n'a été ajouté pour ce palier.

Validation de ce premier palier : 18 tests CTest, dont 32 832 comparaisons avec un moteur ARM64 indépendant et un programme C 32/64 bits comparé à son exécution native. L'IPC reste différé pendant cette consolidation.

## Priorité CPU — atomiques et exclusives réalisées

Le [palier atomiques/exclusives](CPU_ATOMICS.md) ajoute les accès exclusifs simples et par paires, LDAR/STLR, les principales opérations LSE, CLREX et les barrières usuelles. Les réservations suivent les écritures RAM par granules de 16 octets. Validation de ce palier : 19 suites CTest, dont 206 instructions assembleur. L'exécution reste séquentielle ; la synchronisation entre threads hôtes et le modèle mémoire multicoeur restent à construire.

## Priorité CPU — premier sous-ensemble FP/SIMD réalisé

Le [palier FP/SIMD](CPU_FP_SIMD.md) inclut calculs scalaires/vectoriels, conversions, fusionnes, extrema, arrondis, reductions, transferts structures, tables et copies d'elements. Le [moteur flottant logiciel](CPU_SOFTFLOAT.md) traite sous-normaux, petitesse avant arrondi et FZ. Le [palier SIMD arithmetique](CPU_SIMD_ARITHMETIC.md) ajoute saturation/QC, retrecissements, decalages variables et multiplications elargies. Validation : 24 suites CTest, 41 304 cas entiers, 96 400 cas FP/SIMD generaux, 64 896 cas sous-normaux/FZ et 61 056 cas SIMD arithmetiques independants. Un programme de 343 instructions combine les differentes chaines et controle les resultats et flags. Le CPU reste partiel. Suite : plusieurs programmes C/C++ compiles plus varies, comparaison native et correction des instructions manquantes constatees, avant l'ordonnancement multithread. L'IPC reste differe.
