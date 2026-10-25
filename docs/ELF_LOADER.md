# Chargeur ELF ARM64 et permissions mémoire

État vérifié le 24 octobre 2026. Ce chargeur sert aux programmes de test ARM64 autonomes ; il ne charge pas de jeux Switch 2 et ne fournit pas un environnement Linux.

## Utilisation

```powershell
cmake --build build --config Debug
.\build\Debug\Switch2Emu.exe .\tests\programs\elf_sum\program.elf 500 --test-protocol
ctest --test-dir build -C Debug --output-on-failure
```

Résultat de référence : **94 instructions**, **X2=55**, fin normale avec code 0. Le programme lit des données initialisées et des constantes, vérifie une variable BSS initialement nulle, appelle une fonction C, utilise la pile et écrit le résultat en mémoire.

Le protocole `SVC #0xF000` reste local au test et nécessite `--test-protocol`. ELF ne fournit pas à lui seul de services système.

## Périmètre accepté

- ELF64 little-endian, version 1, ABI System V sans extension, machine AArch64, type ET_EXEC et indicateurs d'architecture nuls.
- Fichier de 64 Mio maximum et table de 1 à 128 entrées de 56 octets. Les en-têtes ELF et la table des segments doivent être complets.
- Segments PT_LOAD : adresse virtuelle, taille fichier, taille mémoire, alignement et permissions vérifiés. Les adresses de chargement sont celles du fichier ; aucune relocation n'est effectuée.
- Copie des octets du segment puis initialisation à zéro du reste, notamment la BSS. Les tables de sections et les symboles ne sont pas nécessaires à l'exécution.
- Point d'entrée aligné sur 4 octets et première instruction entièrement présente dans un segment chargé exécutable.
- PT_NULL ignoré ; PT_NOTE et PT_PHDR traités comme métadonnées après contrôle de leur plage fichier. PT_GNU_STACK accepté sans droit d'exécution ni demande de pile supérieure à 64 Kio.

Références de format : [en-tête ELF](https://gabi.xinuos.com/elf/02-eheader.html), [segments et chargement ELF](https://gabi.xinuos.com/elf/07-pheader.html), [ABI ELF AArch64 d'Arm](https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst). Les restrictions supplémentaires ci-dessous sont des choix du prototype.

## Restrictions explicites

Les segments PT_LOAD doivent être triés par adresse, tenir dans la RAM disponible et ne pas partager de page, même s'ils auraient les mêmes permissions. Les segments à la fois inscriptibles et exécutables sont refusés. Les variantes non prises en charge sont rejetées, y compris ET_DYN/PIE, interpréteur dynamique, PT_DYNAMIC, TLS et numérotation étendue des segments.

Les propriétés de plateforme des notes, l'édition de liens dynamique, les arguments d'un processus Linux et la mémoire locale des threads ne sont pas implémentés. Un exécutable ELF AArch64 quelconque peut donc être refusé ou nécessiter des instructions/services absents.

## Mémoire protégée

Les droits R/W/X sont attachés aux pages de **4 Kio**. Les drapeaux ELF sont convertis en droits internes ; ils ne sont pas utilisés directement comme masque mémoire. Les pages non occupées par un segment ou la pile n'ont aucun droit.

La machine réserve les derniers **64 Kio** de RAM pour une pile R/W non exécutable ; la page qui la précède est une page de garde sans accès. Le SP initial vaut la fin de la RAM. Aucun segment ne peut recouvrir ces réservations.

Le chargement des instructions passe par un accès X distinct des lectures R : une page exécutable seule peut être exécutée sans autoriser une lecture de données. Les lectures et écritures de 1/2/4/8 octets vérifient toute leur plage avant transfert, y compris entre pages. Les paires LDP/STP vérifient les deux éléments ensemble pour les erreurs de plage et de permission.

Les contrôles s'appliquent dans Memory et passent par Bus jusqu'au CPU. Une erreur garde le PC et les registres de l'instruction fautive ; cela ne constitue pas encore une exception architecturale ARM64. Les erreurs signalées restent présentes jusqu'à `clearError()` ; la boucle CPU les réinitialise avant chaque instruction.

Les droits sont appliqués à la page entière, donc aux octets de remplissage de cette page aussi. Ce modèle reste une RAM avec adresses identiques aux offsets de stockage : aucune traduction MMU ni isolation entre plusieurs processus n'est ajoutée ici. Les périphériques de développement existants gardent leur routage MMIO ; ils ne forment pas une carte matérielle Switch 2.

## Chargement et reprise

Le chargeur prépare l'image dans une mémoire temporaire. Un fichier refusé ne remplace ni les données et permissions précédentes, ni les métadonnées de chargement. `Machine::load` ne réinitialise CPU, minuteur et périphériques qu'après le succès du chargement.

Après un succès, les anciennes données sont éliminées, la BSS est nulle et les périphériques repartent de leur état initial. Le point d'entrée ELF devient le PC ; les binaires bruts démarrent toujours à zéro.

La signature ELF est détectée dans le contenu. L'extension `.elf`, sans distinction de casse, exige aussi un ELF valide : elle ne retombe pas silencieusement sur un chargement brut. Les autres fichiers sans signature ELF gardent le mode brut historique, de taille multiple de 4 et limitée à la RAM. **Ce mode brut conserve les droits R/W/X sur toute la RAM** pour les anciens tests ; les protections de segments concernent le mode ELF.

## Validation

La suite CTest contient désormais 11 tests, dont une suite dédiée qui couvre : ELF produit par le compilateur, segments synthétiques indépendants, constantes, données, BSS entre pages, pile et page de garde, lectures const, permissions X seules, accès inter-pages, paires de registres, chargement échoué sans changement d'état et rechargement propre.

102 variantes ELF incorrectes sont refusées : troncatures, tables ou segments hors limites, architecture/versions non prises en charge, débordements, mauvais alignement, entrée invalide et collisions de pages. Le binaire brut historique et le programme C précédent continuent à passer.

Les sources et instructions de reproduction de la cible sont dans [le guide ELF de test](../tests/programs/elf_sum/README.md).

## Profil HLE optionnel

Le chargeur peut aussi préparer le [profil HLE de référence](HLE_RUNTIME.md) avec une réservation de tas et une région locale de thread. Dans ce mode, les conflits avec ces réservations sont refusés avant de remplacer la session. Le profil ajoute trois services de référence aux ELF, tout en restant distinct d'un environnement natif Switch 2 validé.
