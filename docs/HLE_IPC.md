# Premier transport IPC HLE

Étape du 25 octobre 2026 : un programme ELF ARM64 ouvre une session vers un service local, écrit une requête dans sa région de thread et lit la réponse. Le transport emploie un sous-ensemble HIPC/CMIF étudié publiquement ; le port et les commandes du service sont propres au projet. Aucune compatibilité native Switch 2 n'est revendiquée. Une extension permet maintenant [les buffers Send/Receive](HLE_IPC_BUFFERS.md) ; le format sans descripteurs décrit ici reste accepté.

## Connexion et requête

| SVC | Entrée | Sortie |
| --- | --- | --- |
| 0x1F ConnectToNamedPort | X1 = nom terminé par zéro, en RAM lisible | W0 = résultat ; W1 = handle de session sur succès |
| 0x21 SendSyncRequest | W0 = handle de session | W0 = résultat du transport ; réponse dans le buffer IPC du thread |
| 0x16 CloseHandle | W0 = handle | W0 = résultat |

Le seul port enregistré est **`emu:demo`**. Les noms sont lus dans une limite locale de 12 octets, terminaison comprise. Un port inconnu renvoie une erreur ; aucune connexion implicite vers des services de console n'est créée.

Les sessions utilisent la même table de 128 handles que les événements et le second thread. Le port sm: et l'obtention de nouvelles sessions par MoveHandle sont maintenant décrits dans [HLE_SM.md](HLE_SM.md). Un slot est libéré à la fermeture, sans réattribuer l'ancien identifiant. Une session n'est ni un événement ni un thread ; SignalEvent, ClearEvent, ResetSignal et StartThread la refusent. WaitSynchronization sur cette session synchrone locale est explicitement refusé : aucun serveur invité ne gère d'état d'attente de session dans cette étape.

La session est un objet hôte sans état de service et peut être utilisée successivement par les deux threads du même processus. L'exécution reste séquentielle sur le CPU simulé. SendSyncRequest répond immédiatement et ne crée pas de processus serveur.

## Buffer et format acceptés

SendSyncRequest utilise les **256 premiers octets** de la région locale du thread courant : 0xFEE200 pour le principal dans la machine de 16 Mio, 0xC11200 pour le second. Le répartiteur emploie la région canonique du processus, sans lire un pointeur arbitraire dans les arguments du programme. Chaque thread possède son propre buffer.

Le buffer entier doit être en RAM lisible et inscriptible avant toute copie. La requête est copiée dans un tableau hôte borné, puis validée. Une réponse est construite séparément, avec les octets inutilisés mis à zéro, puis écrite par la copie mémoire transactionnelle. Une erreur de validation conserve les octets de la requête. Les octets après le buffer ne sont pas modifiés.

Le sous-ensemble accepte seulement :

- HIPC type **4**, sans descripteurs, PID, handles transférés ni liste de réception.
- Header1 contenant uniquement RawCount, de 8 à 62 mots de 32 bits selon la taille du message.
- En-tête CMIF **SFCI**, version 0, identifiant de commande et token nul, aligné à l'offset 16.
- Paramètres intégrés au message à partir de l'offset 32, avec les 16 octets de padding total CMIF prévus dans RawCount.

La réponse contient un en-tête HIPC de type 0, puis **SFCO** à l'offset 16, le résultat du service à +24 et les éventuelles données à +32. RawCount vaut 10 pour une réponse avec un entier de 64 bits, 8 pour une erreur de service sans données.

Les autres formes sont refusées explicitement, à l'exception des extensions [Send/Receive](HLE_IPC_BUFFERS.md), [SendPid/MoveHandle](HLE_SM.md) et des [contrôles de session](HLE_CLIENT.md). Les domaines, TIPC, les descripteurs statiques et les transferts généraux de handles restent absents.

## Service de démonstration

| Commande | Paramètres | Réponse |
| --- | --- | --- |
| 0 Add | Deux entiers non signés de 64 bits, exactement 16 octets | Somme modulo 2^64 |
| 1 GetVirtualTime | Aucun paramètre | Nanosecondes de l'horloge virtuelle locale |

Un identifiant de commande inconnu ou des paramètres de mauvaise taille produisent une réponse SFCO avec une erreur de service. W0 reste zéro parce que le transport s'est déroulé correctement. Une erreur d'enveloppe, de handle ou de mémoire est une erreur de transport dans W0, sans fausse réponse de succès.

Les codes de démonstration utilisent le module local 511 : InvalidMessage=0x3FF, UnsupportedFormat=0x5FF, UnknownCommand=0x7FF, PortNotFound=0x9FF, InvalidArguments=0xBFF. Ils ne sont pas présentés comme des erreurs natives de la console. Les erreurs de mémoire et de handles emploient les catégories déjà présentes dans le profil HLE.

Une erreur de transport conserve ici la session pour permettre de corriger la requête. Les comportements natifs de fermeture automatique en cas d'erreur ne sont pas reproduits. Un rechargement réussi réinitialise la table ; un chargement échoué garde la session existante.

## Démonstration et vérification

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_ipc\program.elf 1000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

L'ELF termine en **116 instructions**. Le principal calcule 20+35=55 via le service. Le second emploie la même session pour calculer 44+55=99 dans son propre buffer. Le principal attend sa fin, vérifie que sa réponse est restée intacte, demande le temps virtuel puis ferme la session.

Le banc vérifie aussi les limites de noms et d'enveloppes, les erreurs de service séparées des erreurs de transport, les droits mémoire, les types et identifiants de handles, l'épuisement partagé des ressources et l'identité du résultat avec des budgets d'une instruction.

Les descripteurs Send/Receive sont maintenant réalisés dans [HLE_IPC_BUFFERS.md](HLE_IPC_BUFFERS.md). Prochain jalon : gestionnaire de services avec transfert de handles de session. Le service de démonstration ne remplace pas les services attendus par un jeu ; le GPU et les formats d'applications natives restent absents.

## Sources étudiées

- [SwitchBrew SVC](https://switchbrew.org/wiki/SVC) : registres de ConnectToNamedPort et SendSyncRequest.
- [SwitchBrew HIPC](https://switchbrew.org/wiki/IPC_Marshalling) : structure du message, compteurs et padding CMIF.
- [libnx cmif.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/cmif.h) : en-têtes SFCI/SFCO et construction des messages.

Sources consultées le 25 octobre 2026 ; code du transport et service de démonstration écrits dans ce projet.
