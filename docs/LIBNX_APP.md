# Initialisation applicative par défaut et première session APM

Le 25 octobre 2026, une nouvelle cible exécute le **`__appInit` original de libnx**, avec ses clients setsys, applet et APM originaux. Elle ne remplace plus les hooks applicatifs par une initialisation minimale. La révision reste `b156b4774fb2df7b93402169509266da7547fc4c` et les sources libnx sont compilées sans modification.

## Blocage observé, puis dépassé

Les sources et l'exécution montrent que `set:sys` absent n'est pas, à lui seul, un arrêt obligatoire : libnx ignore l'échec de cette tentative d'obtention de la version système, puis continue vers appletInitialize. Pour une application ordinaire, cette dernière exige d'abord apmInitialize.

Avant le nouveau service, la cible s'arrêtait après 2 790 instructions avec le diagnostic libnx `InitFail_AM` (16 217). Après l'ajout borné de l'ouverture APM, **le même NRO** progresse jusqu'à la demande d'`appletOE` et s'arrête après **3 785 instructions**, avec le même diagnostic de haut niveau. La trace permet de distinguer ces deux causes : le code de diagnostic seul ne suffit pas à localiser le blocage.

| Étape native | Résultat actuel |
| --- | --- |
| smInitialize | Succès avec le client SM original |
| Recherche `set:sys` | PortNotFound local, aucun handle ; libnx poursuit |
| Recherche `apm` | Succès, session réelle transmise par handle Move |
| APM commande 0 OpenSession | Succès, sous-session distincte par handle Move |
| Recherche `appletOE` | PortNotFound local ; arrêt réel de l'initialisation applet |
| Nettoyage après échec | Le code original ferme la sous-session APM, puis sa session parente |

Le type d'applet initial provient maintenant de la valeur faible originale `AppletType_Default`, puis libnx le fait passer à Application après APM. La version système reste zéro. Aucun firmware, mode de performances ou fréquence n'est fabriqué. La cible n'atteint **ni les constructeurs ni main** et le test exige cet arrêt précis.

## Service natif APM : périmètre exact

Dans le profil explicite `--hle-reference --homebrew-entry`, le gestionnaire SM reconnaît désormais le nom natif de huit octets `apm`. Le profil HLE historique conserve son refus de ce service.

Seule la commande racine **0 OpenSession**, sans paramètre ni descripteur, est implémentée. Elle crée une sous-session de type APM distinct, publiée après l'écriture complète de la réponse SFCO. Le clonage, la requête de taille du buffer et la fermeture passent par les contrôles génériques déjà disponibles. Les sous-sessions ne peuvent pas ouvrir récursivement d'autres sessions.

Les commandes de lecture du mode de performances et de configuration de puissance restent explicitement **UnknownCommand**. L'ouverture d'une session est un vrai état IPC, pas une simulation de gestion matérielle de la puissance. Une sous-session reste indépendante si le client ferme sa session parente ; les identifiants fermés restent invalides.

La table commune de 128 handles borne les créations. Une réponse non inscriptible, un paquet mal formé ou un dépassement de quota ne publie pas de handle et ne consomme pas son prochain identifiant. Les demandes natives non reconnues restent des erreurs sans handle factice.

## Validation

Le test relève **10 échanges SendSyncRequest** du programme original dans `build/libnx-app-ipc.csv`, dont les demandes `set:sys`, `apm`, `appletOE`, la création de sous-session et les fermetures APM. La routine de nettoyage d'applet tente aussi une commande sur son contrôleur non initialisé ; le transport retourne InvalidHandle et le code libnx poursuit son nettoyage. Cette erreur n'est pas transformée en succès.

Les tests contrôlent le diagnostic original, l'absence de main/constructeurs, le type d'applet, l'absence de version inventée, les fermetures dans l'ordre et le déterminisme par budgets de sept instructions. Les paquets SDK sont rejoués pour tester les formats invalides, paramètres supplémentaires, commandes inconnues, réponses protégées, séparation parent/enfant, fermeture et quota. Le refus d'APM dans l'ancien profil est également vérifié.

Validation complète : **57 suites CTest réussies en Debug et en Release**. La reconstruction reproduit l'empreinte du nouveau NRO et conserve celles des cibles précédentes avec et sans SM.

## Reproduction et limites

```powershell
.\tools\build_libnx_app.ps1 -Python <chemin_python>
ctest --test-dir build -C Debug -R libnx_default_app_and_native_apm --output-on-failure
```

Le NRO fait 212 992 octets avec 4 096 octets de BSS. Son rapport de diagnostic est à `0x43000`. Le manifeste conserve les empreintes des sources APM/applet/set et du NRO ; les sources et le correctif newlib sont les mêmes que pour les cibles précédentes. Le placement laisse davantage d'espace au code original d'applet, dont le graphe de fonctions retenues est plus grand. Le script partagé conserve les empreintes des deux anciennes cibles.

Les dépendances HID, time, FS et les fonctions auxiliaires situées au-delà de l'arrêt actuel ne sont pas annoncées comme exécutables : leurs symboles manquants possèdent des pièges d'erreur explicites. Le test refuse ces marqueurs et attend uniquement le diagnostic libnx réel. L'arrêt de diagnostic et le pont exit du banc sont toujours propres au projet ; la libc de sortie complète et l'installation GCC devkitA64 restent hors de ce jalon.

## Prochaine étape

L'[étape suivante](LIBNX_APPLET_DOMAINS.md) ouvre désormais **appletOE**, convertit sa session en domaine et fournit le proxy applicatif et cinq interfaces. Le même NRO progresse jusqu'à GetAppletResourceUserId, encore absent. Les services d'affichage et les entrées natives restent à construire. Ce parcours valide une infrastructure historique Switch ; il ne démontre toujours pas le lancement de jeux ou la compatibilité native Switch 2.

Sources épinglées : [init.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/runtime/init.c), [applet.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/applet.c), [apm.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/apm.c).
