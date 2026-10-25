# Clavier et démonstration interactive

Étape du 25 octobre 2026 : les flèches de la fenêtre Windows sont transmises à un client C ARM64 via le service local **emu:pad**. Le programme invité calcule le déplacement et présente l'image modifiée. Les interfaces HID et GPU natives Switch 2 restent absentes.

## Lancement

Depuis la racine du projet :

```powershell
.\tools\run_interactive.ps1
```

Le lanceur compile Release puis ouvre la démo avec le dossier de ressources déjà fourni. Commande équivalente :

```powershell
.\build\Release\Switch2Emu.exe .\tests\programs\hle_interactive\program.elf 50000000 --hle-reference --read-root .\tests\programs\hle_graphics\root --window
```

Les **flèches** déplacent le vaisseau ; **Échap** demande une sortie propre au programme invité. Fermer la fenêtre interrompt l'exécution comme précédemment. La démo termine aussi automatiquement après **60 images** et laisse la dernière image visible jusqu'à fermeture de la fenêtre. Le rythme réel dépend du calcul et des pauses de 100 ms, sans garantie de cadence.

## Entrées de la fenêtre

Le code traite uniquement les messages clavier destinés à sa propre fenêtre ; aucun hook ni interrogation globale du clavier n'est utilisé. Les relâchements de touches mettent à jour les bits maintenus. L'autorépétition ne bascule pas l'état. La perte de focus, la fermeture et la réouverture effacent les touches maintenues.

Échap est aussi conservé comme demande de quitter jusqu'à la sortie, pour qu'un appui bref ne soit pas perdu pendant le calcul d'une image. Ce drapeau d'action est distinct des touches maintenues et est réinitialisé à la fermeture/réouverture de la fenêtre.

Avant chaque tranche d'exécution, le lecteur transmet l'état à `Machine::setButtons`. Les touches sont également traitées pendant les pauses. Une transition brève entre deux lectures du programme peut ne pas être vue pour les directions : cette première interface fournit un état, sans file d'événements.

## Service local

GetServiceHandle accepte le nom de huit octets `emu:pad\0`. La commande CMIF **0 ReadButtons**, sans paramètres ni descripteurs, renvoie un masque u64 :

| Bit / valeur | Signification |
| --- | --- |
| 0 / 1 | Gauche |
| 1 / 2 | Droite |
| 2 / 4 | Haut |
| 3 / 8 | Bas |
| 4 / 16 | Quitter |

L'état initial et celui d'un nouveau processus valent zéro. L'injection hôte refuse les bits inconnus sans modifier l'état précédent. Elle exige le mode HLE ; une machine terminée ou en faute la refuse. Sans fenêtre ni injection, toutes les touches restent relâchées. Le montage de fichiers n'est pas nécessaire au service d'entrées lui-même.

Les requêtes non prises en charge ou mal formées conservent les règles de transport IPC. Les contrôles de session et handles sont communs aux autres services. La lecture n'avance pas l'horloge virtuelle et n'effectue pas d'accès à une interface clavier du PC.

## Programme invité

Le client C réutilise sprite.txt, emu:fs et emu:disp. Il démarre à x=48, y=16 et lit les boutons avant chaque image. Une direction seule déplace de quatre pixels ; deux directions opposées sur un même axe s'annulent. Les diagonales sont possibles. Le sprite de 64 × 64 reste dans l'image de 160 × 96 : x dans [0,96], y dans [0,32].

Le rapport à 0x12000 contient sept champs u64 : status, loaded, presented, hash, x, y, max_frames. Succès : status=1, loaded=272, position et nombre d'images selon les entrées. max_frames vaut 60 par défaut et est limité à 600 ; le banc le réduit pour les scénarios automatisés.

## Validation et suite

Le banc vérifie les pressions, relâchements, autorépétitions et perte de focus. Sous Windows, il envoie les messages à une fenêtre cachée appartenant au thread du test, puis vérifie Échap bref et la fermeture. Il ne constitue pas une séance manuelle de pilotage visuel.

Le client ARM64 reçoit une séquence déterministe de directions, dont diagonales et oppositions. Le banc vérifie chaque pixel de huit images, compare le parcours avec des budgets différents, teste les quatre bords, l'arrêt avant toute image par Échap, les erreurs IPC et la remise à zéro. Les scénarios n'utilisent pas le clavier réel de l'utilisateur.

Prochaine étape : confronter ce socle à une cible applicative plus complète et inventorier les services ou instructions qui la bloquent. Cette démo interactive reste un programme de référence du projet, sans compatibilité revendiquée avec les jeux Switch 2.
