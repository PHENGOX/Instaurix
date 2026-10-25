# Transferts par buffers IPC

Étape du 25 octobre 2026 : le service local `emu:demo` reçoit et renvoie des données par des descripteurs HIPC A/B. Le programme peut transférer plus que les paramètres intégrés au buffer de commande de 256 octets. Ce profil reste un banc de développement sans validation native Switch 2.

## Périmètre

Le transport accepte jusqu'à un descripteur **Send/A** et un **Receive/B**, de mode Normal (0), sans statiques, descripteurs d'échange, liste de réception ni en-tête spécial. Les modes NonSecure/NonDevice restent refusés. Les versions CMIF, domaines et transferts de handles conservent les limites de [HLE_IPC.md](HLE_IPC.md).

Chaque descripteur occupe 12 octets après l'en-tête HIPC. Le parseur décode entièrement l'adresse et la taille, y compris leurs parties hautes ; une adresse extérieure à la RAM ne peut pas être ramenée dans la RAM par troncature. Le début CMIF est aligné à 16 octets après les descripteurs. Le nombre de mots de données brutes inclut le padding et doit tenir dans le buffer IPC de 256 octets.

La taille maximale locale est **64 Kio par descripteur**, capacité de sortie comprise. Un descripteur de taille nulle n'accède pas à son adresse. Une source non vide doit être entièrement en RAM lisible ; une destination non vide, entièrement en RAM inscriptible. MMIO, traversées de pages interdites et débordements de la RAM sont refusés. La destination ne peut pas recouvrir le buffer IPC courant. Les autres données du processus restent partagées entre ses threads.

Les noms, commandes et codes de service restent propres au projet. L'implémentation ne reproduit pas les mappings interprocessus d'un noyau : elle effectue une copie contrôlée pour ce service hôte.

## Commande 2 Copy

| Élément | Valeur |
| --- | --- |
| Port | `emu:demo` |
| En-tête | HIPC type 4, un Send et un Receive |
| CMIF | SFCI, version 0, commande 2, token 0 |
| Paramètres intégrés | Aucun |
| Entrée | Octets du descripteur Send |
| Sortie | Copie dans le descripteur Receive ; capacité au moins égale à l'entrée |
| Réponse | SFCO, résultat de service, nombre d'octets copiés en entier de 64 bits |

La partie inutilisée de la capacité de sortie reste inchangée. Les commandes 0 Add et 1 GetVirtualTime continuent d'utiliser uniquement leurs paramètres intégrés ; des descripteurs ajoutés à ces commandes sont explicitement refusés.

Si la capacité de sortie est insuffisante ou si les arguments de la commande sont incorrects, le transport renvoie une réponse SFCO avec InvalidArguments, sans copie de données. Une erreur de descripteur ou de droits mémoire est une erreur de transport, sans modifier ni la requête ni la destination. Deux codes locaux supplémentaires : InvalidBuffer=0xDFF et TransferLimit=0xFFF, module de démonstration 511.

## Écriture groupée et chevauchement

Le service sauvegarde d'abord les octets d'entrée dans un buffer hôte borné. Une source et une destination qui se chevauchent sont donc traitées selon le contenu d'entrée avant écriture.

`Memory::writeRanges` valide toutes les plages et prépare toutes les pages nécessaires avant de copier le moindre octet. La réponse IPC et la destination sont écrites dans cette même opération. Un échec de validation ou d'allocation laisse leurs contenus inchangés. Les écritures réussies mettent à jour les versions de réservation exclusive du CPU.

L'API mémoire générale applique les plages dans leur ordre lorsqu'elles se recouvrent. Le transport interdit le recouvrement entre sa propre réponse et la destination pour éviter une réponse ambiguë. Les écritures s'effectuent sur le même thread hôte ; aucune garantie d'accès concurrent depuis plusieurs threads hôtes n'est ajoutée.

## Démonstration et validation

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_ipc_buffers\program.elf 10000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

Le programme remplit un motif de **512 octets**, le copie via le service, vérifie chaque octet et termine en **5 677 instructions**. La taille retournée est 512 et le marqueur final vaut 1.

Le banc teste aussi 64 Kio sur plusieurs pages, les parties hautes des adresses et tailles, le chevauchement, les tailles nulles, la capacité insuffisante, les frontières de pages et de RAM, les droits, les modes non pris en charge, l'alignement CMIF et l'écriture groupée. Les anciennes commandes et leurs tests restent actifs.

Le gestionnaire de services et sa réponse avec handle de session sont maintenant réalisés dans [HLE_SM.md](HLE_SM.md). Prochaine étape : commandes de gestion des sessions CMIF, puis cible C. Les services natifs nécessaires aux jeux, les domaines et une grande partie des formes de buffers restent à implémenter.

## Référence étudiée

[libnx hipc.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/sf/hipc.h), consulté le 25 octobre 2026 : champs des descripteurs, modes et disposition des sections HIPC. Le code du parseur et la commande Copy ont été écrits dans le projet.
