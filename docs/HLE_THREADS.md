# Ordonnanceur local à deux threads

Étape du 25 octobre 2026. Un ELF ARM64 original peut créer et démarrer un second thread, attendre son événement, puis attendre sa fin. Ce profil de développement ne valide aucune compatibilité native Switch 2.

## Appels et limites du profil

| SVC | Entrées | Résultat |
| --- | --- | --- |
| 0x08 CreateThread | X1=entrée, X2=argument, X3=sommet de pile, W4=priorité, W5=cœur | W0=résultat, W1=handle sur succès |
| 0x09 StartThread | W0=handle de thread | W0=résultat |
| 0x0A ExitThread | aucune | Termine seulement le thread courant |
| 0x0B SleepThread | X0=0 ou durée positive en ns | Cède l'exécution ou attend une échéance du temps virtuel |

Conventions étudiées dans [SwitchBrew SVC](https://switchbrew.org/wiki/SVC#CreateThread) et [libnx](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h), consultés le 25 octobre 2026. La politique d'ordonnancement et les adresses ci-dessous sont propres au projet.

Le profil accepte deux threads au total : le principal et un second créé une seule fois dans la vie du processus. Le second démarre suspendu jusqu'à StartThread. Une troisième création, même après la fin du second, renvoie OutOfResource. Fermer le handle ne termine pas le thread et ne libère pas ce quota. Démarrer deux fois renvoie InvalidState ; fournir un handle d'événement renvoie InvalidHandle.

L'entrée doit être alignée sur quatre octets et exécutable en RAM. La pile demandée doit être exactement **0xC10000**, sommet de la réservation locale de 64 Kio **[0xC00000,0xC10000)**. Les pages immédiatement avant et après sont interdites ; la pile est R/W et non exécutable. La page TLS du second est **0xC11000**, sa région locale débute à **0xC11200**. Ces zones sont réservées dès le chargement HLE et les segments ELF ne peuvent pas les recouvrir. La mémoire n'est pas cloisonnée entre threads d'un même processus.

Seule la priorité **32** et le cœur **0** (ou **-2**, cœur par défaut) sont pris en charge. Les autres choix arrêtent explicitement l'exécution comme fonction non prise en charge. Le profil ne reproduit pas les priorités, affinités, capacités et limites d'un noyau de console.

Le contexte initial du second est nul, sauf PC, SP, X0=argument et TPIDRRO_EL0=sa région locale. Il doit appeler ExitThread ; aucun trampoline de retour du SDK n'est installé. Chaque contexte conserve les registres entiers, PC, SP, PSTATE, les 32 vecteurs 128 bits, FPCR/FPSR et les registres TLS. Une restauration lors d'un changement de thread invalide la réservation exclusive du CPU.

## Politique d'exécution

Un seul CPU interprété exécute alternativement les threads prêts, dans un ordre circulaire. Le quantum est **64 instructions exécutées**, conservé entre appels hôte à run : changer le découpage des budgets ne change pas l'ordre d'exécution. Attendre, quitter un thread ou céder l'exécution provoque une décision immédiate. Un SignalEvent réussi provoque également cette décision afin de résoudre les attentes avant un éventuel ClearEvent suivant.

Chaque thread possède sa propre attente. Stop::Waiting signifie que plus aucun thread démarré n'est exécutable. Un appel hôte à run dans cet état ne consomme aucune instruction tant qu'un objet attendu n'est pas signalé. Un signal peut réveiller les deux threads ; l'événement reste signalé jusqu'à une remise à zéro explicite.

Le handle de thread est utilisable dans WaitSynchronization : il devient définitivement signalé à la fin du thread. Il ne peut être utilisé dans SignalEvent, ClearEvent ou ResetSignal. Une attente garde une référence à cet objet même si son handle est fermé.

ExitProcess termine tous les threads. ExitThread termine le courant ; le processus se termine normalement après la fin de tous ses threads créés. Un thread créé mais jamais démarré peut donc empêcher cette fin et laisser la machine en attente. Une faute de CPU ou un appel non pris en charge arrête le processus entier. Un chargement réussi réinitialise contextes, attentes et quantum ; un chargement échoué conserve la session.

Les délais finis sont maintenant pris en charge par [l'horloge virtuelle](HLE_TIME.md), pilotée explicitement par l'hôte. L'annulation, les verrous, la réutilisation de slots de threads et l'exécution multicœur restent absents. Les appels hôte restent séquentiels, sans garantie de sécurité pour des accès concurrents.

## Démonstration et validation

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_threads\program.elf 2000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

La démonstration termine en **367 instructions**, avec 55 en mémoire partagée. Le principal attend d'abord l'événement du second, puis sa fin. Les tests couvrent également un principal en boucle sans yield, un blocage des deux threads réveillés par l'hôte, l'identité du résultat avec des budgets d'une instruction, l'isolation des contextes et les erreurs d'arguments.

L'horloge virtuelle et les attentes à durée finie sont réalisées dans [HLE_TIME.md](HLE_TIME.md). Prochaine étape : transport IPC minimal avec une requête et une réponse de service. Le CPU sera étendu si ces cibles révèlent un blocage ; le GPU reste à construire.
