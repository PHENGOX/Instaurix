# Client SM libnx original : IPC vérifié

Étape du 25 octobre 2026 : la cible NRO utilise désormais le `sm.c` original de libnx, révision `b156b4774fb2df7b93402169509266da7547fc4c`, au-dessus de l'[initialisation originale jusqu'à main](LIBNX_INIT.md). Aucun service `emu:*` n'est demandé par cette cible.

Le transport existant a accepté les requêtes produites par libnx. **Aucun correctif du CPU ou du dispatcher IPC n'a été nécessaire pour ce parcours.** Cette étape ajoute une référence externe reproductible et des tests sur ses paquets réels ; elle ne fournit pas encore les services natifs recherchés par une application complète.

## Parcours observé

Le hook applicatif appelle `smInitialize` deux fois. Le vrai compteur de références de libnx évite une seconde connexion et une seconde inscription. Le programme demande `set:sys`, clone la session du gestionnaire, réessaie la même recherche sur le clone et le ferme. Un premier smExit conserve la connexion d'origine ; le dernier smExit, pendant le nettoyage applicatif, la ferme.

| Échange | Session | Requête libnx | Résultat du profil local |
| --- | --- | --- | --- |
| 1 | Gestionnaire | Contrôle 3, QueryPointerBufferSize | SFCO, taille u16 zéro |
| 2 | Gestionnaire | Commande 0, RegisterClient avec SendPid | SFCO succès |
| 3 | Gestionnaire | Commande 1, GetService `set:sys` | PortNotFound, aucun handle |
| 4 | Gestionnaire | Contrôle 2, CloneCurrentObject | Un nouveau handle Move |
| 5 | Clone | GetService `set:sys` | PortNotFound, preuve que le clone partage l'état d'inscription |
| 6 | Clone | HIPC Close, puis CloseHandle | Clone fermé indépendamment |
| 7 | Gestionnaire | GetService après un premier smExit | Toujours actif, PortNotFound |
| 8 | Gestionnaire | HIPC Close, puis CloseHandle final | Connexion fermée, objet libnx remis à zéro |

Le parcours comporte une seule connexion au port `sm:`, huit SendSyncRequest et deux CloseHandle. Il termine en **3 117 instructions**, avec les mêmes indicateurs de constructeur/main/nettoyage que la cible précédente. Le résultat de main est 77, celui renvoyé par la sortie libnx au chargeur est zéro. Aucun piège de dépendance n'est atteint.

Les résultats PortNotFound (0x9FF), NotInitialized et AlreadyInitialized restent les **codes locaux du prototype** ; leur correspondance avec les résultats SM d'une console n'est pas revendiquée. Le service `set:sys` est réellement absent : le banc ne fabrique ni session de service natif ni version de firmware. Les hooks applicatifs restent minimaux et n'appellent pas encore les services par défaut.

## Validation des octets du SDK

Le test avance une instruction à la fois, capture chaque requête juste avant SVC 0x21 et la réponse juste après son traitement. Il vérifie notamment :

- Le contrôle QueryPointerBufferSize et la réponse contenant un u16 zéro avec la taille brute correcte.
- Le RegisterClient avec Header1=0x8000000A, marqueur SendPid et SFCI aligné à +32.
- La réponse de clonage avec un descripteur Move unique et un handle distinct.
- Le nom natif `set:sys`, les erreurs de service sans handle Move et la séparation entre résultat transport et résultat SFCO.
- Les fermetures HIPC sans corps de réponse fabriqué, puis les fermetures de handles dans le bon ordre.
- La conservation des ThreadVars à la fin de la TLR et le déterminisme avec des budgets de sept instructions.

Le fichier `build/libnx-sm-ipc.csv` conserve les requêtes et réponses complètes en hexadécimal. Il sert d'élément de diagnostic et ne contient que les données de cette cible synthétique.

Les mêmes octets SDK sont ensuite rejoués sur un processus propre. Les tests couvrent une recherche avant inscription, les paquets mal formés, l'échec d'une réponse en mémoire protégée sans publication de l'inscription, l'inscription répétée, deux connexions indépendantes et le refus d'un handle fermé. Les octets PID fournis par l'invité ne sont pas utilisés comme identité. L'authentification multiprocessus et les permissions SAC ne sont pas implémentées.

Validation complète : **56 suites CTest réussies en Debug et en Release**. Les reconstructions reproduisent les empreintes des deux cibles, avec et sans SM.

## Reconstruction

```powershell
.\tools\build_libnx_sm.ps1 -Python <chemin_python>
ctest --test-dir build -C Debug -R libnx_original_sm_ipc --output-on-failure
```

Le constructeur de cible partagé compile les sources libnx/newlib inchangées, puis ajoute sm.c et les appels applicatifs du banc. Le source SM conservé est comparé à celui de l'archive libnx épinglée. Le NRO, sa licence, les dépendances et les résultats attendus sont consignés dans `tests/programs/libnx_sm/reference.json` et dans le manifeste de la cible d'initialisation. Une seconde reconstruction reproduit son empreinte. La cible précédente sans SM conserve également son empreinte d'origine.

## Suite

L'[étape suivante](LIBNX_APP.md) exécute désormais l'initialisation applicative par défaut, franchit une ouverture APM bornée et localise le blocage sur `appletOE`. L'échec de `set:sys` est ignoré par ce parcours original ; aucune identité système n'a été inventée pour le contourner. Le hello-world original, l'affichage natif et la compatibilité native Switch 2 restent à démontrer.

Source publique épinglée : [libnx sm.c](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/source/services/sm.c).
