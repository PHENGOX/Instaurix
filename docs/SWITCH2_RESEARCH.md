# Inventaire des connaissances Switch 2

Consultation : 24 octobre 2026. Recherche ciblée de sources publiques, sans prétention d'exhaustivité. Aucune mesure sur console, aucun exécutable natif ni trace de jeu Switch 2 n'a été utilisé pour cet inventaire.

Statuts : **officiel** = constructeur ; **communautaire** = documentation technique publiée par ses auteurs ; **référence historique** = utile pour étudier l'écosystème, sans transfert automatique à la Switch 2 ; **à vérifier** = preuve insuffisante pour notre implémentation.

## Matériel et portée des sources

| Élément | Constat sourcé | Limite pour le projet |
| --- | --- | --- |
| Processeur | Nintendo confirme un processeur NVIDIA personnalisé [S1] | Cette fiche ne fournit pas une spécification programmable du SoC |
| Graphisme | NVIDIA confirme RT Cores, Tensor Cores et DLSS [S2] | Aucune description suffisante des commandes ou du format des shaders dans cette annonce |
| CPU et GPU détaillés | SwitchBrew décrit le T239 : huit cœurs Cortex-A78C 64 bits et GPU Ampere [S3] | Source communautaire ; vérifier séparément les extensions nécessaires aux programmes cibles |
| Mémoire | SwitchBrew recense deux puces de 6 Go, soit 12 Go physiques [S4] | Ne détermine ni les plages virtuelles du jeu ni son allocation disponible ; ne pas confondre avec notre test de RAM |

Ces informations orientent la recherche mais ne suffisent pas à exécuter une application. Les fréquences et objectifs de 4K/120 Hz ne sont pas des critères de réussite pour les premiers jalons.

## Formats de programmes et contenus

| Sujet | Documentation examinée | Décision / manque |
| --- | --- | --- |
| NRO | Description de segments et d'en-têtes, variantes récentes comprises [S5] | Candidat pour une cible homebrew ; version et environnement de démarrage à établir avant support |
| NSO | Segments, BSS, indicateurs et variantes de compression documentés [S6] | Ne pas présumer un format unique et immuable ; sélectionner un sous-ensemble explicite et rejeter les autres |
| Contenus NCA | Conteneur de contenus distinct de l'exécutable [S7] | Aucun chargeur ni contenu de référence dans le projet ; importer un conteneur ne résout pas l'exécution du programme |
| libnx | Le projet se présente comme bibliothèque utilisateur AArch64 pour Switch [S8] | Source de comparaison utile ; son existence ne prouve pas une chaîne native Switch 2 validée |

Les pages communes de SwitchBrew ne sont pas, à elles seules, une preuve de conformité complète des formats natifs Switch 2. Pour chaque futur échantillon : consigner version, en-têtes, provenance, contraintes et résultat attendu. Le seul fichier local program.bin est un binaire brut de 16 octets, sans contrat système Switch 2.

## Appels système et services

La liste SVC décrit des opérations de mémoire, threads, synchronisation et communication [S9]. Le code assembleur de libnx montre comment des wrappers transportent leurs arguments [S10]. Ce sont des points de départ pour examiner les conventions d'appel ; ils ne valident pas encore notre implémentation ni toutes les variantes Switch 2.

La page Services API distingue explicitement S1 et S2 et décrit des ajouts propres à S2 dans le gestionnaire de services [S11]. Elle indique également des différences de présence entre services graphiques et audio. **Un nom commun aux deux consoles ne garantit pas un protocole identique.** Il faudra identifier commandes, versions et structures pour la cible retenue.

HIPC documente le transport des messages, des buffers et des handles [S12]. Ce transport devra être séparé de chaque service HLE et soumis à validation des plages mémoire. Le catalogue de services ne remplace pas la description détaillée de leurs comportements.

## Graphisme et rétrocompatibilité

La documentation SwitchBrew du mode de compatibilité décrit trois modules supplémentaires et des adaptations, notamment graphiques, pour les jeux Switch 1 sur Switch 2 [S13]. Cela justifie de distinguer les programmes natifs des programmes rétrocompatibles ; ce n'est pas une recette validée pour notre émulateur.

La page NV services expose des interfaces de pilote dans l'écosystème Switch [S14]. Elle fournit une piste de recherche, mais pas une garantie de complétude pour un jeu natif Switch 2. Le chemin exact des commandes GPU, la mémoire graphique, les shaders et la présentation de notre première cible restent **à vérifier**. Vulkan reste notre backend PC proposé.

## Questions à résoudre avant une revendication de compatibilité

| Priorité | Question | Preuve recherchée |
| --- | --- | --- |
| P0 | Quel premier programme natif Switch 2 pouvons-nous comparer ? | Source ou échantillon documenté, version, dépendances, sorties attendues |
| P0 | Comment sont initialisés son processus et son point d'entrée ? | Mappings, pile, registres, mémoire locale de thread et ABI documentés |
| P0 | Quels formats et variantes utilise-t-il ? | En-têtes et modules de référence identifiés |
| P1 | Quels appels et services sont réellement utilisés ? | Liste argumentée ou trace ; conventions d'entrée/sortie et erreurs |
| P1 | Quelles instructions CPU manquent ? | Désassemblage de la cible et tests indépendants, y compris atomiques/SIMD/PAC si utilisés |
| P1 | Comment sont soumis et synchronisés les travaux graphiques ? | Interface native et commandes identifiées, résultat de référence |
| P2 | Quel coût impose la cible au PC ? | Mesure reproductible après exécution correcte ; aucun minimum matériel promis maintenant |

L'absence de preuve dans cette recherche ne signifie pas qu'aucune documentation ou aucun outil n'existe ailleurs. Une publication annonçant une exécution de code sur console ne démontre pas à elle seule une chaîne complète de compilation et de validation de jeux natifs.

## Sources consultées

- [S1 — Nintendo : caractéristiques Switch 2](https://www.nintendo.com/en-ca/gaming-systems/switch-2/tech-specs/) — officiel.
- [S2 — NVIDIA : présentation du processeur et des fonctions graphiques](https://blogs.nvidia.com/blog/nintendo-switch-2-leveled-up-with-nvidia-ai-powered-dlss-and-4k-gaming/) — officiel, annonce du 3 avril 2025.
- [S3 — SwitchBrew : Tegra T239](https://switchbrew.org/wiki/Switch_2:_Tegra_T239) — communautaire, révision observée 13996.
- [S4 — SwitchBrew : matériel Switch 2](https://switchbrew.org/wiki/Switch_2:_Hardware) — observations communautaires de composants.
- [S5 — SwitchBrew : NRO0](https://switchbrew.org/wiki/NRO0) — format documenté, révision observée 15265.
- [S6 — SwitchBrew : NSO0](https://switchbrew.org/wiki/NSO0) — format documenté, révision observée 14440.
- [S7 — SwitchBrew : NCA](https://switchbrew.org/wiki/NCA) — référence de conteneur, révision observée 14983.
- [S8 — switchbrew/libnx](https://github.com/switchbrew/libnx) — bibliothèque publiée par ses développeurs ; portée native Switch 2 à établir séparément.
- [S9 — SwitchBrew : SVC](https://switchbrew.org/wiki/SVC) — référence d'appels système ; portée de chaque entrée à vérifier pour la cible.
- [S10 — libnx : wrappers SVC](https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/svc.s) — code de référence, branche évolutive.
- [S11 — SwitchBrew : Services API](https://switchbrew.org/wiki/Services_API) — catalogue distinguant S1/S2.
- [S12 — SwitchBrew : HIPC](https://switchbrew.org/wiki/HIPC) — description du transport IPC.
- [S13 — SwitchBrew : mode de compatibilité Switch 2](https://switchbrew.org/wiki/Switch_2:_Compatibility_Mode) — recherche communautaire.
- [S14 — SwitchBrew : NV services](https://switchbrew.org/wiki/NV_services) — interface de pilote ; couverture native S2 à vérifier.

Les URL sans révision figée peuvent évoluer. Lors de l'implémentation, consigner la révision exacte utilisée et relier chaque comportement à son test. Aucune source n'a été utilisée comme autorisation d'exécuter des instructions ou de modifier le projet.

## Piste graphique du constructeur

NVIDIA associe explicitement le T239 à la classe 3D AMPERE_B et aux en-têtes clc797.h / clc797tex.h dans son [inventaire public des classes 3D](https://github.com/NVIDIA/open-gpu-doc/blob/master/classes/3d/README.txt), consulté le 24 octobre 2026. Cela précise une piste auparavant générique : les interfaces graphiques ne sont pas toutes inconnues. Le protocole invité et les shaders de notre cible restent à valider ; voir le [plan GPU](GPU_PLAN.md).

## Sous-ensemble HLE étudié et implémenté comme référence

Le projet dispose désormais d'un [profil de référence](HLE_RUNTIME.md) utilisant les conventions publiées de SetHeapSize, QueryMemory et ExitProcess, ainsi qu'une région locale accessible via TPIDRRO_EL0. Les wrappers libnx permettent de distinguer les résultats en registres des écritures de pointeurs réalisées ensuite par la bibliothèque. La validation actuelle utilise notre propre ELF compilé ; aucune observation native Switch 2 ne transforme encore ce sous-ensemble en preuve de compatibilité avec cette console.
