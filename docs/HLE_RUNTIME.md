# Premier environnement HLE de référence

État au 24 octobre 2026. Cette étape ajoute un processus local avec un thread principal, un sous-ensemble de services mémoire et des registres de thread ARM64. **Aucune compatibilité native Switch 2 n'est revendiquée.** Les conventions étudiées viennent de l'écosystème Horizon/libnx ; elles doivent être confrontées à une cible native Switch 2 avant d'être considérées comme validées pour elle.

## Lancer la démonstration

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_memory\program.elf 500 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

Résultat : **121 instructions**, **X2=55**, fin normale HLE avec code 0. Le programme alloue un tas, appelle du C pour calculer la somme 1..10, écrit 55 dans le tas et la mémoire locale du thread, puis interroge les informations du tas et quitte le processus.

`--hle-reference` est explicite, désactivé par défaut et réservé aux ELF acceptés par le chargeur. Les binaires bruts sont refusés dans ce profil. Ce mode et `--test-protocol` sont mutuellement exclusifs dans l'interface de lancement. Les anciens tests continuent d'utiliser leur mode antérieur.

## Processus et thread

`Process` conserve l'état du processus et d'un thread principal local d'identifiant 1. Il possède la taille du tas, les types de pages et l'adresse de la région locale du thread. L'identifiant 1 n'est pas un handle de noyau émulé.

Le contexte sauvegarde les 32 emplacements de registre du prototype, PC, SP, PSTATE, TPIDR_EL0/TPIDRRO_EL0, les 32 registres SIMD 128 bits et FPCR/FPSR. Les états Ready, Running, Paused, Waiting, Exited et Faulted sont conservés aux frontières d'exécution. Les sauvegardes ne constituent pas un ordonnanceur multithread. Le sous-ensemble d'événements et son mécanisme d'attente/reprise sont décrits dans [HLE_EVENTS.md](HLE_EVENTS.md).

Le budget ou une interruption permet une reprise. Une fin reste définitive. Une erreur terminale empêche de contourner un appel inconnu en relançant simplement `run` ; il faut recharger un programme. Le CPU utilisé seul conserve ses mécanismes de test et de restauration de contexte.

## Services traités

Ces conventions de registres sont celles publiées pour les appels Horizon étudiés, distinctes des signatures C des wrappers libnx [H1, H2].

| SVC | Entrées utilisées | Résultat |
| --- | --- | --- |
| 0x01 SetHeapSize | X1 = taille | W0 = résultat ; X1 = base du tas sur succès |
| 0x06 QueryMemory | X0 = adresse du buffer MemoryInfo ; X2 = adresse interrogée | W0 = résultat ; W1 = PageInfo sur succès |
| 0x07 ExitProcess | Aucune | Fin du processus ; X0 n'est pas un code de sortie |

Les wrappers C peuvent effectuer eux-mêmes une écriture vers un pointeur après le retour du SVC. Le répartiteur ne doit donc pas prendre X0 de SetHeapSize ou X1 de QueryMemory pour un pointeur à écrire.

Un appel inconnu arrête le processus avec son numéro et l'adresse de l'instruction. Il n'est ni ignoré ni présenté comme réussi. Les erreurs d'arguments d'un appel pris en charge renvoient un résultat au programme, qui peut les traiter et continuer.

## Tas et limites locales

La taille demandée est un multiple de **2 Mio**. Le profil accepte la taille zéro et permet croissance/réduction. Le tas est fixé à **0x200000**, avec une capacité locale de **8 Mio**, sans ASLR. Les pages allouées sont R/W et non exécutables. Réduire le tas supprime les droits et le contenu des pages retirées ; leur réallocation donne des zéros. Une croissance conserve le contenu existant.

Le prototype renvoie notamment InvalidSize (0xCA01), OutOfMemory (0xD001), InvalidAddress (0xCC01), InvalidPointer (0xE601), ou OutOfResource (0xCE01). Les catégories s'appuient sur les références publiques [H1, H5] ; l'exhaustivité et l'ordre de toutes les validations d'un noyau réel ne sont pas reproduits. Les tailles supérieures à 4 Gio sont refusées, même si la limite locale de tas est bien plus basse.

## QueryMemory

Le buffer de sortie de **40 octets** est écrit en little-endian : base, taille, type, attributs, droits, compteurs de références IPC/périphériques et remplissage [H3]. Les attributs, compteurs et PageInfo sont zéro dans ce sous-ensemble.

L'interrogation regroupe les pages contiguës de même type et de mêmes droits. Les types simplifiés utilisés sont 0 pour non mappé, 3/4 pour image statique/modifiable, 5 pour tas, 11 pour pile et 12 pour région locale. Aucun attribut complet de bloc noyau n'est simulé.

Les adresses interrogées sont limitées aux **16 Mio** du modèle local. Une adresse extérieure renvoie une erreur au lieu de décrire tout l'espace virtuel d'une vraie console. Un buffer de sortie non inscriptible, traversant une page interdite, débordant ou situé dans un périphérique MMIO est refusé avant écriture. L'allocation des pages nécessaires à la copie précède la copie elle-même, pour éviter une sortie partielle.

## Région locale du thread et registres

Le profil réserve une page R/W non exécutable à **0xFEE000**. La région locale de 0x200 octets commence à **0xFEE200**, après un premier emplacement réservé. Ce choix d'adresse est propre au prototype ; sa structure de départ s'appuie sur les références publiques [H4]. Aucun démarrage complet du SDK ni objet ThreadPointer n'est construit.

- MRS TPIDRRO_EL0 retourne la base locale configurée par l'hôte.
- MRS/MSR TPIDR_EL0 permettent au programme de gérer son pointeur de thread, initialement nul.
- Une écriture invitée dans TPIDRRO_EL0 et les autres registres système non pris en charge provoquent une erreur.

Le CPU prend aussi en charge MOVZ/MOVK/MOVN W, émis par le compilateur de la démonstration. Ces écritures remettent à zéro la moitié haute du registre X ; les décalages réservés sont refusés.

## Chargement et validation

Les segments ELF ne doivent recouvrir ni la réservation du tas ni celle de la région locale, même si leur permission est nulle. L'image et le contexte sont préparés avant le remplacement de la session. Un conflit de profil laisse la mémoire, le CPU et le processus précédents en place. Un chargement réussi réinitialise le tas, les régions locales et les périphériques.

La validation comprend les anciennes suites, le programme HLE compilé, les conventions de registre, les erreurs de pointeurs et de taille, le redimensionnement du tas, l'état du thread, les registres système, les options de lancement et les conflits de disposition mémoire, ainsi que les événements ajoutés ensuite.

Les sources de la démonstration et ses empreintes sont dans [son guide](../tests/programs/hle_memory/README.md). Les handles d'événements, un [ordonnanceur local à deux threads](HLE_THREADS.md) et les [attentes à durée finie sur temps virtuel](HLE_TIME.md) sont maintenant présents. Il reste notamment le transport IPC et les services graphiques. Cette étape est une fondation HLE testée, pas le jalon complet d'exécution d'une application native Switch 2.

## Sources consultées

- [H1 — SwitchBrew, appels système](https://switchbrew.org/wiki/SVC) : conventions SetHeapSize, QueryMemory et ExitProcess ; référence communautaire.
- [H2 — libnx, wrappers SVC](https://github.com/switchbrew/libnx/blob/master/nx/source/kernel/svc.s) : séparation des conventions noyau et des pointeurs de sortie des wrappers.
- [H3 — libnx, structures et types mémoire](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h) : MemoryInfo et énumérations de référence.
- [H4 — SwitchBrew, région locale du thread](https://switchbrew.org/wiki/Thread_Local_Region) et [libnx, lecture du pointeur local](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/arm/tls.h).
- [H5 — Atmosphère, catégories de résultats SVC](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libvapours/include/vapours/results/svc_results.hpp) : noms et catégories publiques ; code du répartiteur du projet écrit séparément.

Sources évolutives consultées le 24 octobre 2026. Les adresses du profil, ses limites de ressources et son périmètre sont des décisions du projet.
