# Temps virtuel et attentes finies

Étape du 25 octobre 2026 : SleepThread et WaitSynchronization gèrent des durées positives en nanosecondes sur une horloge virtuelle déterministe. Aucun rythme de CPU ou comportement natif Switch 2 n'est déduit de ce modèle.

## Horloge pilotée par l'hôte

L'horloge commence à zéro au chargement. `Machine::advanceTime(ns)` l'avance explicitement ; `process().virtualTime()` lit sa valeur et `process().nextWakeDelay()` retourne la durée avant la prochaine échéance, ou aucune valeur en l'absence d'échéance. Une attente déjà résolue retourne un délai nul jusqu'à sa reprise.

`run(budget)` n'avance pas cette horloge. Il exécute strictement au plus le budget d'instructions demandé, et conserve les échéances entre les appels. Aucun temps du PC, aucune pause réelle et aucun coût arbitraire par instruction ne déterminent les délais invités. L'hôte peut avancer le temps même lorsqu'un autre thread est exécutable.

Le lanceur console pilote automatiquement les périodes où tous les threads sont bloqués : il avance jusqu'à la prochaine échéance et reprend avec le budget d'instructions restant. S'il n'y a que des attentes infinies, il affiche l'attente. Si un thread tourne sans se bloquer, le lanceur console n'avance pas le temps : un futur pilote d'exécution devra fournir une politique de progression pour ce cas. Cette étape fournit le mécanisme de délais, pas une calibration temporelle du CPU.

L'horloge utilise un compteur de nanosecondes non signé de 64 bits. Une avance qui le ferait déborder est refusée sans effet. Une durée invitée dont l'échéance dépasserait cette plage est explicitement non prise en charge. Une machine terminée ou en faute refuse une avance de temps. Un chargement réussi réinitialise horloge et délais ; un chargement échoué conserve la session précédente.

Le minuteur MMIO de développement conserve son compteur d'instructions indépendant. Il n'est pas une source de cette horloge et n'est pas recalibré en nanosecondes.

## Conventions invitées

| Appel | Comportement |
| --- | --- |
| SleepThread (SVC 0x0B), X0 > 0 | Bloque le thread jusqu'à l'échéance ; aucune valeur de résultat n'est écrite |
| SleepThread, X0 = 0 | Cède l'exécution comme avant |
| WaitSynchronization (SVC 0x18), X3 > 0 | Attend un objet ou l'échéance ; résultat 0 sur signal, TimedOut (0xEA01) sur expiration |
| WaitSynchronization, X3 = 0 | Sondage immédiat |
| WaitSynchronization, X3 = -1 | Attente infinie |
| GetSystemTick (SVC 0x1E) | X0 = compteur virtuel à 19,2 MHz, arrondi vers le bas |

Les autres valeurs négatives de délai ou de SleepThread restent non prises en charge. Le profil conserve un seul cœur et deux threads, avec les limites de [HLE_THREADS.md](HLE_THREADS.md).

Les conventions d'appels et la fréquence de référence Horizon sont étudiées dans [SwitchBrew SVC](https://switchbrew.org/wiki/SVC) et [libnx svc.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/kernel/svc.h), consultés le 25 octobre 2026. Cette fréquence n'est pas présentée comme validée pour la Switch 2. Les lectures directes de compteurs matériels ARM ne sont pas ajoutées dans cette étape.

## Ordre des signaux et expirations

Une attente résolue conserve son résultat jusqu'à la reprise. Un signal reçu avant l'expiration reste un succès même si l'événement est ensuite effacé ou si l'hôte avance le temps. Une expiration déjà constatée reste TimedOut malgré un signal ultérieur. À l'échéance exacte, l'ordre des appels hôte tranche : avancer le temps d'abord constate l'expiration ; signaler d'abord résout l'attente par succès. Ces appels sont séquentiels, sans accès concurrent garanti.

Un dépassement de délai ne modifie pas X1, le registre utilisé pour le tableau d'entrée et l'index de sortie. Un succès écrit l'index du premier objet signalé. Un tableau vide peut attendre une durée finie et expirer. Les contrôles de handles et de pointeurs continuent de précéder l'enregistrement de l'attente. Une attente de fin de thread peut elle aussi avoir une échéance.

Le réveil conserve PC après le SVC, pile, registres, FP/SIMD et TLS. Les références aux objets attendus sont libérées lorsque le résultat est fixé. La reprise n'exécute pas à nouveau le SVC.

## Vérification et suite

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_time\program.elf 1000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

La démonstration termine en **43 instructions**, à **1 500 ns virtuelles**, avec 55 en mémoire. Le principal attend un événement pendant 2 000 ns. Le second dort 1 000 ns, signale, dort encore 500 ns et quitte ; le principal attend sa fin. La variante où le principal n'attend que 500 ns vérifie l'expiration puis la poursuite normale.

Les tests couvrent les frontières avant/à l'échéance, l'ordre signal/expiration, un signal suivi d'un effacement, un autre thread exécutable, les attentes vides et infinies, les débordements, le compteur de ticks et l'identité des parcours avec des budgets d'une instruction.

Le transport IPC minimal est maintenant réalisé dans [HLE_IPC.md](HLE_IPC.md), avec un service de démonstration et des buffers propres à chaque thread. Prochain jalon : descripteurs de buffers IPC, puis gestionnaire de services. L'annulation, les verrous et une politique temporelle pendant le calcul restent à développer.
