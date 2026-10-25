# Entrée homebrew NRO et validation MOD0

Ce profil expérimental du 25 octobre 2026 fournit l'environnement d'entrée d'un **NRO de référence borné**. Il s'active uniquement avec `--hle-reference --homebrew-entry`. Les modes précédents conservent leurs conventions et refusent toujours MOD0. Le profil exige un NRO avec MOD0 et ne s'applique pas aux ELF ou aux binaires bruts.

## Environnement fourni

| Élément | Valeur locale |
| --- | --- |
| X0 | `0xD00000`, liste ConfigEntry en lecture seule, non exécutable |
| X1 | `UINT64_MAX` |
| X30 | `0xD01000`, trampoline de retour R/X |
| SP | Haut de la pile de 64 Kio, aligné sur 16 octets |
| MainThreadHandle, clé 1 | Handle réellement présent dans la table du processus |
| OverrideHeap, clé 3 | Tas R/W de 2 Mio à `0x200000` |
| EndOfList, clé 0 | Terminaison après ces deux entrées |

Les entrées font 24 octets. Aucun firmware, numéro de version système ou masque global de SVC n'est inventé. Les pages réservées à l'environnement et au retour ne peuvent pas recouvrir les segments. Le tas et ses informations QueryMemory sont effectivement initialisés.

Le handle principal peut être interrogé par GetThreadId (SVC 0x25), attendu, puis fermé. Il devient signalé à la sortie du thread ; StartThread sur le thread principal renvoie InvalidState. Le handle du second thread conserve son identité distincte. GetThreadId ne met pas en œuvre les pseudo-handles.

Le retour au chargeur utilise un protocole local `SVC F001`, accepté uniquement au PC du trampoline, depuis le thread principal, dans ce profil. Le résultat X0 est conservé. Ce mécanisme local n'est pas présenté comme un SVC Horizon. Le protocole de test F000 est incompatible avec ce profil.

## Validation avant exécution

La validation porte sur les plages fichier et régions mémoire, pas seulement sur les adresses dans la RAM :

- MOD0 et tables en mémoire fichier lisible et non modifiable ; offsets signés contrôlés, BSS identique à celle du NRO.
- Bornes des métadonnées d'exception et champ réservé ; [extensions LNY/RELRO désormais acceptées sous conditions](LIBNX_CRT0.md), avec plages GOT/RELRO vérifiées et LNY2 version 1.
- Table dynamique terminée, tags uniques. Sous-ensemble admis : DT_NULL, DT_RELA/RELASZ/RELAENT, DT_RELR/RELRSZ/RELRENT. Les autres tags sont refusés.
- Tailles d'entrées cohérentes, tables alignées, destinations R/W alignées sur huit octets et sans doublon.
- RELA : NONE ou RELATIVE avec symbole zéro ; addition signée bornée à la RAM locale.
- RELR : adresse directe avant bitmap, progression et additions bornées ; chaque destination du bitmap est vérifiée. Les valeurs initiales acceptées sont limitées aux additions positives sans débordement.

**Le chargeur ne réalise pas les relocations.** Le code invité libnx les applique une seule fois. Un refus de chargement conserve la RAM, le CPU et le processus précédents grâce à la préparation séparée dans Machine. La validation choisit un sous-ensemble restrictif ; elle ne promet pas de charger tous les NRO libnx courants.

## Programme vérifié

`tests/programs/nro_bootstrap/program.nro` contient un démarrage et une fonction principale originaux du projet, liés au **dynamic.c libnx inchangé** de la révision `b156b4774fb2df7b93402169509266da7547fc4c`. La licence, le manifeste et le script de reconstruction accompagnent cette référence. L'outil d'empaquetage statique réserve l'en-tête puis le script de ce banc renseigne son pointeur MOD0 fixe ; cela ne remplace pas elf2nro.

Le fichier de 24 576 octets utilise 4 096 octets de BSS. Il applique une relocation RELA, atteint sa fonction principale, vérifie les entrées, résout son handle par GetThreadId, écrit aux deux extrémités du tas et retourne **42 en 32 968 instructions**. Ce résultat non nul est volontaire. Le test salit aussi la BSS après chargement pour prouver que le code libnx l'efface réellement. Une variante RELR adresse/bitmap est exécutée séparément.

Les tests couvrent les budgets de sept instructions, les permissions, les métadonnées mal formées, les types/symboles inconnus, les débordements, les destinations dupliquées, l'absence de base RELR, le maintien de la session précédente, le cycle de vie du handle et le refus d'un F001 exécuté hors trampoline.

Validation complète : **52 suites CTest réussies en Debug et en Release**. La reconstruction reproduit exactement l'empreinte NRO du manifeste.

```powershell
.\tools\build_nro_bootstrap.ps1 -Python <chemin_python>
.\build\Debug\Switch2Emu.exe .\tests\programs\nro_bootstrap\program.nro 100000 --hle-reference --homebrew-entry
ctest --test-dir build -C Debug -R nro_homebrew_entry_and_validation --output-on-failure
```

Le CLI indique une fin normale avec code=42 et renvoie un statut hôte non nul pour signaler ce résultat invité. Le test CTest dédié vérifie explicitement que 42 est le résultat attendu. La reconstruction contrôle l'empreinte SHA256 du manifeste.

## Limite et suite

Ce banc initial n'utilise pas `switch_crt0.s`. Les étapes suivantes ont ajouté le [crt0 original et RELRO](LIBNX_CRT0.md), puis [l'initialisation libnx originale jusqu'à main](LIBNX_INIT.md) pour une cible minimale sans services. Le hello-world externe, les services par défaut et la chaîne devkitA64 complète restent des jalons distincts.

L'objectif du projet reste l'émulation. Cette référence historique Switch sert au développement ; son succès ne démontre pas de compatibilité native Switch 2.
