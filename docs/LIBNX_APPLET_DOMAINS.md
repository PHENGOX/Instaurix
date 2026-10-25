# appletOE : domaines et objets IPC

Compte rendu historique : la suite actuelle est décrite dans [identité, messages et focus](LIBNX_APPLET_STATE.md).

Étape du 25 octobre 2026 : le même NRO d'initialisation applicative originale ouvre désormais `appletOE`, convertit sa session en domaine et reçoit le proxy applicatif puis cinq interfaces. Le client libnx reste inchangé. L'arrêt suivant est la commande **IWindowController::GetAppletResourceUserId**, encore non prise en charge.

## Parcours réel du client

| Opération SDK | Objet produit dans le domaine |
| --- | --- |
| Conversion de la session appletOE, contrôle CMIF 0 | Racine 1 |
| Racine commande 0, OpenApplicationProxy | Proxy 2 |
| Proxy commande 20 | Fonctions applicatives 3 |
| Proxy commande 11 | Créateur d'applets de bibliothèque 4 |
| Proxy commande 0 | CommonStateGetter 5 |
| Proxy commande 1 | SelfController 6 |
| Proxy commande 2 | WindowController 7 |
| WindowController commande 1 | UnknownCommand explicite, sans résultat ni nouvel objet |

Ces créations attribuent de vrais identifiants dans une table bornée. Elles ne prétendent pas implémenter les méthodes fonctionnelles de ces interfaces. Le service ne fournit encore ni identifiant de ressource utilisateur, ni état de focus, ni événement ou écran d'applet.

Le nettoyage original tente la commande 13 du SelfController avec trois octets de paramètres. Le transport accepte ce format non multiple de quatre, puis renvoie UnknownCommand au niveau du service. Les fermetures d'objets se produisent ensuite dans l'ordre 7, 6, 5, 4, 3, 2. Enfin, libnx ferme le handle appletOE, puis les deux handles APM.

Le parcours compte maintenant **5 851 instructions et 26 échanges SendSyncRequest**. Le diagnostic final reste InitFail_AM (16 217), parce que la fonctionnalité applet demandée n'est pas encore disponible. Les constructeurs et main ne sont toujours pas atteints dans ce parcours par défaut. Le NRO conserve exactement son empreinte précédente : la progression vient de l'émulateur.

## Modèle de domaine

La conversion est limitée aux sessions appletOE du profil homebrew explicite. La racine reçoit l'ID 1. Une seconde conversion renvoie InvalidState sans réinitialiser la table. Les contrôles de taille du buffer et de clonage restent disponibles.

- Un domaine possède au maximum **64 objets**, racine comprise, séparés de la table des 128 handles noyau.
- Les identifiants augmentent et ne sont pas réutilisés après fermeture. Un emplacement libéré peut accueillir un nouvel identifiant.
- Les clones d'une même session partagent le domaine, mais gardent leurs propres handles et leur propre état de fermeture.
- Fermer un objet ne ferme pas la session. Fermer un endpoint n'invalide pas les objets accessibles par un clone encore actif. Le dernier endpoint libère sa référence à la table.
- Deux connexions appletOE indépendantes possèdent des espaces d'identifiants indépendants. Le même nombre peut désigner des objets distincts dans ces domaines.
- Les créations et conversions ne deviennent visibles qu'après l'écriture complète de la réponse. Une réponse protégée ne consomme pas d'identifiant et ne publie pas de table ou d'objet.

La table d'objets conserve le type de chaque interface. Seules les commandes de création indiquées ci-dessus sont implémentées. Les autres méthodes renvoient une erreur de service. Les codes InvalidObject et OutOfObjects sont des codes locaux du prototype, pas une annonce d'équivalence avec les résultats Horizon.

## Sous-ensemble de protocole

Les messages HIPC type 4 portent un en-tête de domaine puis SFCI. Le parseur vérifie les tailles brutes, les limites du buffer, les champs réservés, les versions et tokens supportés. Les tailles CMIF non multiples de quatre sont admises avec l'arrondi correct du nombre de mots HIPC ; le client original produit notamment un payload de trois octets pendant son nettoyage.

OpenApplicationProxy accepte uniquement la combinaison réellement produite ici : SendPid, un handle copié correspondant au pseudo-handle du processus courant et un paramètre réservé u64 nul. Les octets PID de l'invité ne servent pas d'identité. Aucun transfert général de handles ou d'objets entre processus n'est ajouté.

Une réponse de domaine comporte son en-tête de sortie, SFCO et, en cas de création réussie, un identifiant d'objet. Elle ne transmet pas un nouveau handle noyau. Les contrôles de conversion/clonage conservent leur réponse CMIF hors domaine. Une fermeture d'objet valide conserve les octets de requête sans fabriquer de corps SFCO ; une fermeture répétée renvoie InvalidObject au transport.

Les domaines avec buffers/descripteurs, objets entrants, contextes, conversion en handle indépendant et autres contrôles non implémentés restent refusés. Il ne s'agit pas encore d'un moteur de domaines Horizon général.

## Vérification

```powershell
ctest --test-dir build -C Debug -R "native_applet_domains_and_lifetimes|libnx_default_app_and_native_apm" --output-on-failure
```

Le test exécute le client original instruction par instruction et contrôle les réponses réellement lues par libnx : une conversion, six créations d'objets, deux erreurs fonctionnelles et six fermetures d'objets. Le rapport et le nombre d'instructions restent identiques avec des budgets de sept instructions.

Les requêtes SDK sont rejouées pour tester les en-têtes mal formés, le mauvais type de handle de processus, les paramètres réservés, une conversion ou création sans réponse inscriptible, les identifiants fermés, le partage entre clones, les domaines indépendants et les quotas. Une table pleine ne publie aucun objet ; après libération d'un emplacement, le prochain ID ne présente pas de trou causé par la tentative échouée. La trace complète reste dans `build/libnx-app-ipc.csv`.

Validation complète : **58 suites CTest réussies en Debug et en Release**. Le NRO conserve l'empreinte `64A983071DEC5EF405DF6B797807326684F074BA8182B9BB62B97BAA37774F6E` ; aucun changement du programme invité n'a été nécessaire pour cette progression.

## Prochaine étape

Le prochain besoin observé est l'identité de ressource de l'application, puis les objets de contrôle et événements réellement interrogés par libnx. Il faudra rattacher ces valeurs et états au processus émulé et vérifier leur cycle de vie. Les états de focus, notifications, affichage et entrées ne doivent pas être remplacés par des succès artificiels pour atteindre main. Ce jalon reste une référence de l'écosystème historique Switch, sans preuve de compatibilité native Switch 2.

Sources épinglées : [format CMIF libnx](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/sf/cmif.h), [client applet original](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/applet.c).
