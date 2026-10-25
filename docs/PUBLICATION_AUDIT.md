# Préparation GitHub — 25 octobre 2026

## Périmètre

Contrôle des fichiers sélectionnés pour publication selon .gitignore, des dépendances documentées, des licences déjà présentes et des programmes de référence utilisés par CMake. Ce contrôle ne constitue pas une certification juridique de chaque ligne de code ni une garantie exhaustive d'absence de secrets.

## Résultats

- Licence du code propre au projet : GPL-3.0-only, texte intégral dans LICENSE. Le texte provient de COPYING3 de l'archive newlib vérifiée localement ; il s'agit du texte GNU GPL v3, sans modification.
- Mentions ISC de libnx et licences multiples de newlib conservées dans les dossiers de référence. Ajout de THIRD_PARTY_NOTICES.md et des recueils de mentions sources/en-têtes dans docs/licenses/.
- Aucun fichier de jeu XCI/NSP/NCA, firmware identifié par son emplacement, jeu de clés console, exécutable hôte ou outil téléchargé parmi les fichiers sélectionnés. Les programmes ELF/NRO et vecteurs binaires de tests sont conservés avec leurs sources, générateurs et manifestes disponibles.
- Aucune correspondance aux signatures recherchées de clés privées, jetons GitHub, identifiants AWS ou jetons Slack parmi les fichiers sélectionnés. Ce contrôle ne détecte pas toutes les formes de secrets.
- Les fichiers et répertoires de référence explicitement utilisés par CMake sont présents dans la sélection.
- Trois fichiers CPU de plus de 100 Mio nécessitent Git LFS. .gitattributes définit leur suivi ; la génération de pointeurs LFS a été vérifiée localement, avec correspondance SHA-256 et reconnaissance par `git lfs ls-files`.
- Les attributs Git préservent les octets des fichiers pour ne pas modifier les sources et références dont les empreintes sont vérifiées.
- Une copie extraite de l'archive a été configurée et compilée en Release sans les caches de la copie de travail (`BUILD_TESTING=OFF`). Le programme de base produit X0=5, X1=7, X2=12. Le test libnx applicatif existant exécuté sur le NRO extrait retrouve 7 428 instructions, 35 IPC et zéro échec. La préparation ne modifie pas le code de l'émulateur ; les 60 suites Debug/Release du palier précédent restent la dernière validation complète.

La copie de travail principale n'a pas été initialisée comme dépôt Git. Un dépôt temporaire sous build/ a servi à vérifier les attributs et pointeurs LFS. Aucun commit, dépôt distant ou envoi GitHub n'a été créé.

## Livrables locaux

`build/Switch2Emu-github.zip` contient les fichiers destinés à publication, sous un dossier Switch2Emu, sans les caches ni les compilations. L'archive contient les données LFS réelles ; Git doit les transformer en pointeurs lors de l'ajout, après activation de Git LFS.

`build/github-publication-audit.json` contient l'inventaire, les tailles, les empreintes SHA-256 et le résultat du contrôle automatisé. Ces deux fichiers de préparation restent exclus du dépôt.

## Étape utilisateur

Extraire l'archive et suivre [GITHUB_PUBLICATION.md](GITHUB_PUBLICATION.md). Activer Git LFS avant le premier ajout et vérifier les trois références. La création du dépôt distant et son premier envoi restent à effectuer.
