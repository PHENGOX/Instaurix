# Fichiers en lecture seule

Étape du 25 octobre 2026 : le service local **emu:fs**, obtenu via sm:, ouvre des fichiers d'un dossier choisi par l'hôte. Chaque fichier ouvert est une session IPC qui fournit taille et lectures positionnelles. Cette interface est propre au prototype ; elle ne reproduit pas fsp-srv et ne valide aucune compatibilité native Switch 2.

## Montage explicite et instantané

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_files\program.elf 50000 --hle-reference --read-root .\tests\programs\hle_files\root
```

Sans `--read-root`, emu:fs renvoie FilesDisabled. Le montage exige le mode HLE de référence. Le dossier est lu avant le chargement de l'ELF, dans un instantané immuable. Les requêtes invitées n'ouvrent aucun chemin sur le PC ; elles cherchent exclusivement dans cet instantané. Modifier ou supprimer un fichier hôte ensuite ne change pas la session en cours.

Limites locales : **32 fichiers**, **4 Mio par fichier**, **8 Mio au total**, aucun sous-dossier. Les noms font 1 à 64 octets ASCII parmi lettres, chiffres, point, tiret et underscore. `.` et `..` sont refusés, comme les séparateurs, deux-points, noms vides ou trop longs. Les noms sont comparés exactement, avec distinction de casse.

Seuls les fichiers ordinaires sont importés. Sous Windows, l'ouverture refuse les points de réanalyse et les dossiers, puis vérifie le parent réel du handle ouvert ; les jonctions et liens ne sont pas suivis comme fichiers invités. La racine explicitement choisie est résolue au montage. La branche POSIX emploie des ouvertures sans suivi de lien. Les tests de liens sont exécutés lorsque le système autorise leur création.

Un montage refusé ne remplace pas la configuration précédente. L'API `Machine::mountReadOnlyRoot` prépare le prochain chargement réussi ; elle ne remplace pas les fichiers d'un processus déjà chargé. Un rechargement réutilise l'instantané configuré, sans relire le dossier. Aucun hôte n'est modifié par les appels invités : création, écriture et suppression ne sont pas exposées.

## Interface locale

| Objet / commande | Entrées | Réponse |
| --- | --- | --- |
| emu:fs / 0 OpenFile | Un buffer Send contenant un nom suivi d'un zéro final ; aucun paramètre intégré | Un handle Move vers le fichier |
| Fichier / 1 GetSize | Aucun buffer ni paramètre | Taille u64 |
| Fichier / 0 Read | Un buffer Receive ; offset u64 puis longueur u64 dans les paramètres | Octets lus dans le buffer et nombre d'octets u64 dans SFCO |

Le nom doit se terminer par son unique zéro final dans la taille déclarée. Chaque Read est positionnel, sans curseur partagé : les clones lisent les mêmes octets immuables sans se déplacer mutuellement. Fermer le service emu:fs laisse les sessions de fichiers obtenues valides. Cloner un fichier et fermer l'original laisse le clone valide.

La longueur de lecture doit tenir dans la capacité Receive, elle-même bornée à 64 Kio. Une lecture à la fin ou au-delà renvoie zéro sans modifier le buffer. Une lecture qui dépasse la fin renvoie seulement les octets restants ; la capacité inutilisée n'est pas écrite. Les offsets sont non signés, sans addition pouvant déborder avant le contrôle de fin de fichier.

Les droits RAM, l'absence de MMIO, les limites de pages et le refus de recouvrir le buffer de commande restent contrôlés par le parseur IPC commun. Données et réponse utilisent l'écriture mémoire groupée existante. Un mauvais descripteur ne produit ni lecture partielle ni réponse de succès.

Codes locaux supplémentaires du module 511 : FilesDisabled=0x17FF, InvalidPath=0x19FF, FileNotFound=0x1BFF. Une opération inconnue, dont une tentative d'écriture, retourne UnknownCommand. Les erreurs de service sont dans SFCO ; les erreurs d'enveloppe ou de mémoire sont des erreurs de transport. Les handles ne sont publiés qu'après écriture de leur réponse complète.

## Démonstration et validation

Le client C `hle_files/work.c`, compilé par Clang 18.1.6 via Zig 0.13.0, ouvre sample.txt via sm: et emu:fs. Il ferme les sessions intermédiaires, vérifie la taille, lit et contrôle chaque octet, clone la session du fichier, vérifie une lecture partielle puis une lecture en fin de fichier.

Résultat : **4 006 instructions**. Le rapport à 0x12000 contient six entiers u64 : status=1, size=30, read=30, hash=530586646, partial=9, eof=0. La fixture contient exactement `Switch2Emu read-only fixture.` suivi d'un LF.

Le banc contrôle le rapport et les budgets de sept instructions, la persistance après suppression du fichier hôte, le montage désactivé, les noms interdits, les fichiers absents, la fin de fichier, les destinations interdites, les sous-dossiers, les quotas et les liens lorsque disponibles. Il crée puis supprime uniquement ses propres dossiers temporaires dans build.

La première démonstration graphique utilisant une ressource lue est réalisée dans [HLE_GRAPHICS.md](HLE_GRAPHICS.md). Prochain jalon : plusieurs images animées et présentation continue. Le GPU natif, les formats d'applications Switch 2 et les services de fichiers attendus par ses jeux restent à développer.
