# Chargeur NRO0 statique de référence

Étape du 25 octobre 2026. Le chargeur reconnaît NRO0 à l'offset 0x10 et l'extension .nro, puis valide le fichier avant de remplacer la mémoire. Ce support est un sous-ensemble choisi pour les essais ; il n'est pas une annonce de chargement des homebrews libnx standards ou des jeux Switch 2.

## Profil accepté

- En-tête de 0x80 octets, version 0, flags zéro et aucun en-tête additionnel.
- Taille déclarée exactement égale au fichier ; aucun ASET ou autre contenu ajouté.
- Trois segments aux offsets et tailles fichier alignés sur 4 Kio ; text commence à zéro et contient l'en-tête.
- Pas de chevauchement, débordement RAM ou recouvrement de la pile/page de garde.
- Métadonnées dynamiques étendues nulles ; identifiant de module conservé sans interprétation de confiance.

La base locale est **0x10000**, avec entrée à cette base et sans ASLR. Text devient R/X, rodata R, data et BSS R/W sans exécution. Les trous restent interdits. La BSS commence après data et est initialisée à zéro. La pile protégée de 64 Kio reste en haut de la machine de développement.

Les adresses, le tas et les régions de thread du profil HLE restent ceux du prototype. Process vérifie leurs conflits comme pour ELF. Le NRO statique de test démarre avec les mêmes conventions locales que nos ELF, sans environnement homebrew annoncé implicitement.

Un pointeur MOD0 non nul doit être aligné, couvert par un segment fichier et porter la bonne signature. Le chargeur de segments peut le reconnaître et marquer `requiresNroRuntime`. **Machine refuse son exécution par défaut**. Le nouveau [profil d'entrée homebrew](NRO_BOOTSTRAP.md), explicitement activé par `--hle-reference --homebrew-entry`, valide un sous-ensemble MOD0/RELA/RELR et fournit un environnement d'entrée. Cela ne correspond pas encore au runtime libnx complet.

La mémoire et les informations d'image sont préparées séparément. Un en-tête, mapping ou profil refusé conserve la session précédente. Un NRO renommé .bin reste reconnu par sa signature. Les fichiers identifiés comme NSO et les extensions de conteneurs NCA/NSP/XCI non pris en charge ne sont pas traités comme des instructions brutes.

## Échantillon reproductible

```powershell
.\tools\build_nro_client.ps1 -Python <chemin_python>
.\build\Debug\Switch2Emu.exe .\tests\programs\nro_client\program.nro 50000 --hle-reference
ctest --test-dir build -C Debug --output-on-failure
```

Le client C réutilise hle_client/work.c et ses vérifications de services, clones, copie et empreinte. Un démarrage original réserve l'en-tête NRO puis branche vers le programme. Le script Python empaquette uniquement ce profil ELF fixe, sans MOD0, relocations ni assets ; il n'est pas présenté comme remplaçant elf2nro.

Le fichier fait **12 288 octets**, avec **12 288 octets de BSS** supplémentaires. Il termine en **14 343 instructions**, avec status=1, sum=55, copied=512 et hash=3505779141. Le banc compare ses sept champs de rapport à ceux de l'ELF correspondant.

Les tests couvrent les droits, la BSS, la pile, les tailles et offsets mal formés, les chevauchements, la troncature, les métadonnées inconnues, le signalement MOD0, la détection par signature et le refus des replis de format.

Le besoin de démarrage libnx et la cible externe choisie sont détaillés dans [COMPATIBILITY_AUDIT.md](COMPATIBILITY_AUDIT.md).

Références publiques étudiées le 25 octobre 2026 : [SwitchBrew NRO](https://switchbrew.org/wiki/NRO), [elf2nro](https://github.com/switchbrew/switch-tools/blob/master/src/elf2nro.c) et [crt0 libnx](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/switch_crt0.s). Le chargeur et l'échantillon sont écrits dans ce projet.
