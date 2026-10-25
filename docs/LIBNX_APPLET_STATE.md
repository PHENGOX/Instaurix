# Identité, messages et focus

Ce palier prolonge les domaines appletOE dans le profil `--hle-reference --homebrew-entry`. Il reste une référence libnx historique Switch, sans validation native Switch 2.

## Identité du processus

GetAppletResourceUserId renvoie un u64 attribué lors de l'initialisation réussie du processus. Il reste stable entre sessions, domaines, clones et threads. Les identifiants sont monotones dans l'exécution hôte, sans recyclage ni dépendance au PID ou à l'horloge du PC. Un nouveau chargement réussi reçoit une nouvelle identité ; un chargement échoué conserve la session précédente.

La sortie ou une faute retire l'état applicatif actif, vide les messages et désactive l'événement. L'identifiant historique reste consultable pour les diagnostics ; les méthodes applicatives refusent l'état inactif.

## Messages et focus

CommonStateGetter fournit un événement copié lisible, ReceiveMessage et GetCurrentFocusState. Plusieurs handles observent le même événement. La file contient au plus 32 messages. Une transition de focus ajoute FocusStateChanged (15) et signale l'événement ; consommer le dernier message le désactive. Une file vide renvoie AM NoMessages (0x680).

Une demande identique à l'état courant ne crée pas de message. Une transition sur file pleine échoue sans modifier le focus. Une réponse impossible à écrire ne publie pas de handle et ne consomme pas de message. ClearEvent et ResetSignal ne peuvent pas effacer cet événement géré par le service. Le handle lisible ne permet pas SignalEvent.

Sans fenêtre, l'application virtuelle commence au premier plan. L'hôte peut modifier cet état avec `Machine::setAppletFocus`. La fenêtre interactive transmet son focus réel et continue à traiter les événements pendant une attente sans échéance. Cette politique locale ne reproduit pas encore la gestion complète des applets Horizon.

## Attente originale et compteur CPU

La cible inclut maintenant le fichier original libnx `kernel/wait.c`, à la place du piège de dépendance waitObjects. CNTPCT_EL0 et CNTFRQ_EL0 utilisent le même temps virtuel que GetSystemTick : fréquence de référence 19,2 MHz, progression commandée par l'hôte. Les instructions seules ne font pas avancer cette horloge. Sans liaison de compteur, le CPU refuse ces lectures explicitement.

Le test d'intégration démarre hors focus, laisse le client vider sa notification puis se bloquer dans sa routine d'attente. Un changement de focus hôte le réveille : **7 178 instructions avant attente, puis 1 553 jusqu'au blocage suivant**. Répéter une attente sans événement n'exécute pas d'instructions invitées.

## Résultat et suite

Au premier plan dès le départ, le parcours exécute **7 428 instructions et 35 échanges IPC**, puis termine avec le diagnostic libnx AM 16217. Le premier appel manquant est `IWindowController::AcquireForegroundRights` (commande 10). Le nettoyage appelle également une méthode SelfController encore absente. AudioController, DisplayController et DebugFunctions sont créés, sans leurs fonctionnalités.

`main` n'est pas atteint. Aucun affichage natif, son, GPU ou jeu Switch 2 n'est validé par ce palier.

Le NRO a été reconstruit pour inclure l'attente originale ; son empreinte change donc : `10F4CF1CB2CD69FCB75C8935CB39A9F84210114236688BB50F3495AF2409553F`. La révision libnx reste `b156b4774fb2df7b93402169509266da7547fc4c`. Les sources SDK utilisées restent originales. La construction de référence avec Zig/Clang et les dépendances documentées reste partielle, sans installation complète du SDK.

Tests dédiés : `applet_identity_messages_and_native_wait`, `cpu_counter_and_virtual_clock`, `native_applet_domains_and_lifetimes`, `libnx_default_app_and_native_apm`.

Validation complète : **60 suites CTest réussies en Debug et en Release**. La reconstruction du NRO reproduit l'empreinte indiquée ci-dessus.

Prochaine étape : définir et vérifier l'acquisition du premier plan, puis suivre les prochains appels originaux nécessaires au démarrage. Le travail GPU sera guidé par les interfaces graphiques réellement utilisées.
