# Licences et références tierces

La licence GPL-3.0-only à la racine couvre le code propre à Switch2Emu. Elle ne remplace pas les licences des fichiers tiers listés ici. Les programmes invités de référence sont des exécutables distincts du cœur C++ de l'émulateur.

## libnx

Origine : [switchbrew/libnx](https://github.com/switchbrew/libnx/tree/b156b4774fb2df7b93402169509266da7547fc4c), révision `b156b4774fb2df7b93402169509266da7547fc4c`. Licence principale ISC, Copyright 2017-2018 libnx Authors ; les mentions individuelles des sources doivent aussi être conservées.

Concerne les références `tests/programs/libnx_dynamic`, `libnx_crt0`, `libnx_init`, `libnx_sm`, `libnx_app` et le contexte de référence `nro_bootstrap`. Les copies `*.upstream.c` et `*.upstream.s` restent sous leurs conditions d'origine. Chaque dossier concerné contient `LICENSE.libnx.md`. Les ELF/NRO associés peuvent contenir du code libnx. Les scripts `tools/build_libnx_*.ps1` enregistrent les sources utilisées et les manifestes `reference.json` leurs versions et empreintes.

Les mentions des fichiers libnx utilisés dans ces constructions sont rassemblées dans [docs/licenses/LIBNX_SOURCE_NOTICES.txt](docs/licenses/LIBNX_SOURCE_NOTICES.txt).

## newlib et adaptation devkitPro

Les références `libnx_init`, `libnx_sm` et `libnx_app` contiennent une partie de newlib **4.6.0.20260123**, avec le correctif officiel **devkitA64_r29.2**. newlib regroupe plusieurs licences permissives ; ne pas l'étiqueter globalement MIT ou GPL. Les conditions sont conservées dans les fichiers `LICENSE.newlib.txt` de ces trois dossiers et dans les en-têtes des copies `newlib_init.upstream.c`.

Les mentions propres aux sources newlib compilées sont également conservées dans [docs/licenses/NEWLIB_SOURCE_NOTICES.txt](docs/licenses/NEWLIB_SOURCE_NOTICES.txt). Acknowledgement: this software includes software developed by the University of California, Berkeley. Aucun nom de contributeur n'est utilisé pour promouvoir le projet.

Source et correctif : `tools/prepare_newlib_reference.ps1` ; URL et SHA-256 : `tests/programs/libnx_init/reference.json`. Les archives téléchargées restent dans le cache exclu `tools/reference-sources/`.

## Outils de développement externes

- **Zig 0.13.0 / Clang** : compilation des programmes invités. Le téléchargement et l'installation locaux sont exclus du dépôt ; leurs licences propres s'appliquent aux outils.
- **Unicorn 2.1.4** : moteur indépendant utilisé par les générateurs Python de références CPU. Il n'est pas lié au cœur C++ et son installation locale `tools/python-deps/` est exclue. Les vecteurs et résultats générés ne constituent pas une redistribution du moteur.
- **CMake et compilateur C++20** : outils de compilation externes.

Les exemples switchbrew cités dans les documents servent de références de recherche ; la mention d'un exemple n'accorde pas une licence sur son contenu. Les droits de tout futur fichier copié doivent être vérifiés séparément.

## Contenus console

Aucun firmware, jeu ou jeu de clés console n'est nécessaire aux tests fournis. Les ELF/NRO de référence sont des programmes de développement, dont les sources et scripts de construction accompagnent le dépôt. Ne pas ajouter de contenu propriétaire à ces références.
