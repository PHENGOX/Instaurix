# Gestionnaire de services de référence

Étape du 25 octobre 2026 : un programme ELF ouvre `sm:`, initialise sa connexion, demande un service par son nom et reçoit un handle de session dans un descripteur HIPC Move. Il peut ensuite fermer le gestionnaire et continuer à utiliser le service obtenu. Ce profil reste local et ne revendique pas de compatibilité native Switch 2.

## Interface prise en charge

ConnectToNamedPort accepte maintenant **`sm:`** en plus du port de démonstration direct. Le transport reste SendSyncRequest, avec le buffer IPC propre au thread courant.

| Commande CMIF sur sm: | Entrée | Réponse |
| --- | --- | --- |
| 0 RegisterClient | En-tête spécial SendPid, paramètre intégré de 8 octets | SFCO avec résultat ; initialise cette connexion |
| 1 GetServiceHandle | Nom de service de 8 octets, sans descripteurs | SFCO et un handle Move sur succès |

Le profil local reconnaît **`emu:demo`**, **`emu:fs`**, **`emu:disp`** et **`emu:pad`** ; l'ouverture du service de fichiers exige le dossier de référence configuré. Le profil homebrew explicite ajoute **`apm`**, limité à [OpenSession](LIBNX_APP.md), et **`appletOE`**, avec ses [premiers objets de domaine](LIBNX_APPLET_DOMAINS.md). Le service demo conserve Add, GetVirtualTime et Copy décrits dans [HLE_IPC.md](HLE_IPC.md) et [HLE_IPC_BUFFERS.md](HLE_IPC_BUFFERS.md). Les recherches répétées créent des sessions indépendantes. L'[étape libnx originale](LIBNX_SM.md) vérifie l'inscription, le clonage et la fermeture avec les vrais paquets SDK.

Un client doit s'initialiser avant une recherche. Les nouvelles connexions directes ont des objets indépendants ; les clones partagent leur état d'initialisation. Une seconde initialisation renvoie AlreadyInitialized (0x13FF) ; une recherche prématurée renvoie NotInitialized (0x11FF). Ces codes appartiennent au module local de démonstration 511. Un service absent renvoie PortNotFound, jamais une session factice.

L'en-tête spécial d'entrée autorisé contient seulement le marqueur SendPid. Le PID et son paramètre de remplacement fournis dans la mémoire invitée ne servent pas d'identité déclarée par le programme : le service est attaché au seul processus hôte simulé. Le prototype ne reproduit ni authentification multiprocessus, ni permissions SAC, ni filtrage des services. Cette initialisation sert au parcours de référence et n'est pas une barrière de sécurité native.

TIPC, enregistrement dynamique, désenregistrement et autres commandes du gestionnaire restent non pris en charge. Une commande inconnue retourne une erreur de service. Les domaines du gestionnaire SM et les transferts généraux de handles entrants restent absents ; seul le profil applet possède pour l'instant un sous-ensemble de domaines. Les contrôles CloneCurrentObject, QueryPointerBufferSize et Close sont décrits dans [HLE_CLIENT.md](HLE_CLIENT.md). Le port `emu:demo` direct reste disponible pour les anciennes démonstrations.

## Format et durée de vie du handle

La réponse de GetServiceHandle utilise un en-tête HIPC avec un en-tête spécial contenant un seul MoveHandle, puis SFCO. Dans cette forme, Header1 vaut **0x80000008**, l'en-tête spécial **0x20**, le handle est à **+12**, SFCO à **+16** et le résultat du service à **+24**. Aucun handle n'est encodé en cas d'erreur de service.

Le service prépare le nouvel objet et le handle avant de construire la réponse. La table et son prochain identifiant ne sont modifiés qu'après l'écriture complète de la réponse dans la mémoire invitée. Un buffer non inscriptible ou une allocation impossible ne laisse donc pas de handle caché ni d'initialisation validée sans réponse. La table pleine renvoie OutOfHandles dans SFCO, sans en-tête Move.

La table de 128 handles reste partagée avec événements, threads et sessions directes. Fermer une connexion du gestionnaire ne ferme pas les sessions déjà obtenues. Fermer une session n'invalide pas ses voisines. Un emplacement libéré peut être réutilisé, mais son ancien identifiant reste invalide. Un rechargement réussi supprime toutes les connexions et leurs états.

Ici Move décrit la transmission d'un nouveau handle de service au client. Il n'y a pas encore de transfert général d'un handle existant entre deux processus invités.

## Démonstration et validation

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_sm\program.elf 1000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

L'ELF termine en **85 instructions**. Il ouvre sm:, s'initialise, obtient emu:demo, ferme sm:, calcule 20+35=55 via la session reçue puis ferme celle-ci. Le marqueur final vaut 1.

Les tests couvrent les réponses avec MoveHandle, la durée de vie indépendante, les états d'initialisation, les services inconnus, les messages mal formés, la protection du buffer de réponse, l'épuisement de la table et la conservation des identifiants sur échec. Le parcours reste identique avec des budgets d'une instruction.

Les commandes de gestion des sessions CMIF et la cible C sont maintenant réalisées dans [HLE_CLIENT.md](HLE_CLIENT.md). Prochaine étape : premiers accès aux fichiers en lecture seule. Les services natifs de graphisme et autres interfaces nécessaires aux jeux restent à construire.

## Sources étudiées

- [libnx sm.c](https://github.com/switchbrew/libnx/blob/master/nx/source/services/sm.c) : connexion sm:, RegisterClient via CMIF avec SendPid et GetServiceHandle avec réponse MoveHandle.
- [libnx hipc.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/hipc.h) : en-tête spécial, PID et emplacement des handles.

Sources consultées le 25 octobre 2026. Le code du gestionnaire et sa politique locale sont propres au projet.
