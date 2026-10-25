# Événements et reprise du processus

Étape d'intégration du 24 octobre 2026 : le CPU, la mémoire et les appels système travaillent ensemble sur un ELF ARM64 reproductible. Ce profil reste local, explicite (`--hle-reference`) et ne lance pas de jeux Switch 2.

## Périmètre

| SVC | Entrée | Sortie sur succès |
| --- | --- | --- |
| 0x45 CreateEvent | aucune | W0=0, W1=handle d'écriture, W2=handle de lecture |
| 0x11 SignalEvent | W0=handle d'écriture | W0=0 |
| 0x12 ClearEvent | W0=un des deux handles | W0=0 |
| 0x16 CloseHandle | W0=handle | W0=0 |
| 0x17 ResetSignal | W0=handle de lecture signalé | W0=0 |
| 0x18 WaitSynchronization | X1=tableau de handles, W2=nombre, X3=délai | W0=0, W1=index |

Ces conventions et les résultats proviennent de [SwitchBrew SVC](https://switchbrew.org/wiki/SVC), consulté le 24 octobre 2026. Elles ne prouvent pas l'identité des interfaces natives Switch 2.

Les événements conservent leur signal jusqu'à ClearEvent ou ResetSignal. Attendre ne consomme pas le signal. Les handles sont typés : seul le côté écriture peut signaler, seul le côté lecture peut attendre ou ResetSignal. ClearEvent accepte les deux côtés. ResetSignal sur un événement non signalé renvoie InvalidState (0xFA01).

La table locale contient 128 handles. Une paire est créée entièrement ou pas du tout (OutOfHandles 0xD201). Les identifiants fermés ne sont jamais réattribués dans un processus, même lorsque leur emplacement est réutilisé. Chaque extrémité garde une référence à l'objet ; fermer une extrémité laisse l'autre valide. Les attentes gardent elles aussi leurs références.

WaitSynchronization vérifie d'abord tout le tableau, limité à 64 handles de lecture. La mémoire doit être lisible et entièrement dans la RAM ; MMIO, débordements et traversées de pages interdites sont refusés. Le premier index signalé est retourné. Une erreur conserve X1. InvalidHandle=0xE401, InvalidPointer=0xE601, TimedOut=0xEA01 et TooManyHandles=0xEE01.

## Suspension et reprise

Le délai zéro est un sondage immédiat. Le délai -1 est une attente infinie. Si aucun objet n'est signalé, Machine retourne Stop::Waiting et conserve le contexte après le SVC. Appeler run à nouveau ne consomme aucune instruction tant que l'attente reste insatisfaite. Machine::signalEvent signale un handle d'écriture ; au prochain run, le résultat et l'index sont placés dans les registres avant la prochaine instruction. Les appels hôte sont séquentiels, sans accès concurrent garanti.

Un tableau vide est accepté : sondage expiré ou attente infinie sans source de réveil dans ce sous-ensemble. Un rechargement réussi réinitialise handles et attentes ; un chargement échoué conserve la session précédente.

Les durées positives sont maintenant prises en charge par [l'horloge virtuelle](HLE_TIME.md), pilotée explicitement par l'hôte. Aucun lien arbitraire entre nombre d'instructions et nanosecondes n'est introduit. L'attente est propre à chaque thread et Stop::Waiting signifie qu'aucun thread démarré ne peut s'exécuter ; voir [l'ordonnanceur à deux threads](HLE_THREADS.md). L'annulation reste absente.

## Vérification et suite

`hle_events_suspend_resume` exécute le parcours ELF complet, vérifie la conservation FP/SIMD/TLS/pile et les erreurs, droits, limites et durées non prises en charge. `hle_events_waiting_cli` vérifie le message d'attente. Sources et reconstruction dans [la démonstration](../tests/programs/hle_events/README.md).

Les jalons de deux threads et d'attentes à durée finie sont réalisés dans [HLE_THREADS.md](HLE_THREADS.md) et [HLE_TIME.md](HLE_TIME.md). Prochain jalon : transport IPC minimal et service de démonstration. Les extensions CPU suivront les blocages observés sur ces cibles ; le GPU reste absent.
