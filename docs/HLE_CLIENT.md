# Sessions CMIF et client C

Étape du 25 octobre 2026 : les contrôles de session sont validés par un programme C compilé pour ARM64. Ce profil local ne valide ni un SDK complet ni une application native Switch 2.

## Contrôles acceptés

| Message HIPC | Fonction | Réponse du profil |
| --- | --- | --- |
| Type 5, commande 2 | CloneCurrentObject | Un handle Move vers un nouvel endpoint du même objet de service |
| Type 5, commande 3 | QueryPointerBufferSize | SFCO avec taille u16 zéro, RawCount=9 |
| Type 2, RawCount zéro | Close | Ferme l'endpoint, sans écrire de réponse SFCO |

Les contrôles utilisent CMIF version 0 et token zéro, sans paramètres, domaines ni descripteurs. Un contrôle inconnu renvoie une erreur de service. Une enveloppe mal formée est une erreur de transport et reste intacte en mémoire.

Le buffer de pointeurs vaut zéro parce que les descripteurs statiques ne sont pas implémentés. Cela ne représente pas la limite de 64 Kio des buffers Send/Receive A/B, qui restent utilisables.

Les clones partagent l'objet et son état de service : initialiser sm: rend cette initialisation visible à ses clones. Chaque endpoint conserve son propre état ouvert/fermé. La fermeture de l'original ne ferme pas les clones. La publication d'un nouveau handle intervient après écriture complète de la réponse, comme dans [HLE_SM.md](HLE_SM.md).

Un Close valide retourne zéro dans ce profil, conserve les octets de la requête et ferme l'endpoint. Les requêtes suivantes renvoient le code **local SessionClosed=0x15FF**. CloseHandle reste nécessaire pour libérer le slot ; ensuite l'ancien identifiant renvoie InvalidHandle. Ce choix de résultat n'est pas une reproduction exhaustive des transitions d'un noyau natif. Le buffer IPC doit rester entièrement lisible et inscriptible, y compris pour Close sans réponse.

## Programme compilé

`tests/programs/hle_client/work.c` est compilé avec Clang 18.1.6 via Zig 0.13.0, optimisation O1, environnement freestanding sans libc. Une courte source assembleur fournit le démarrage et les wrappers SVC. Le client est original, sans liaison à libnx ni au SDK Nintendo.

Il clone sm: avant son initialisation, initialise l'original puis le ferme. Il obtient emu:demo via le clone, clone le service et ferme sa première connexion. Il interroge le buffer de pointeurs, calcule 55, copie 512 octets par A/B et vérifie chaque octet ainsi qu'une empreinte FNV-1a 32 bits. Il lit le temps virtuel puis ferme toutes les sessions restantes avec Close et CloseHandle.

Aucune instruction CPU n'a dû être ajoutée pour ce parcours.

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_client\program.elf 50000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

Résultat : **14 342 instructions**. Le rapport à 0x12000 contient sept champs de 64 bits : status=1, sum=55, copied=512, hash=3505779141, pointer_size=0, time=temps virtuel injecté, flags=7. Le banc vérifie ce rapport ; la seule sortie normale du processus ne suffit pas à valider le programme.

Les tests couvrent aussi les budgets de sept instructions, la pile et les registres FP/SIMD/TLS, les contrôles mal formés, les réponses refusées, le clonage en table pleine, les identifiants non consommés sur échec et la nécessité de CloseHandle pour récupérer un slot.

Le profil reste limité à deux threads et sm: partiel. emu:demo est maintenant complété par [emu:fs en lecture seule](HLE_FILES.md), exercé par un autre client C. Les domaines, TIPC, buffers statiques et services natifs des jeux restent absents. Prochaine étape : employer une ressource lue dans une première démonstration graphique.

## Référence

[libnx cmif.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/cmif.h), consulté le 25 octobre 2026 : types Close/Control, CloneCurrentObject, QueryPointerBufferSize et résultat u16. Le client et le code du profil ont été écrits dans le projet.
