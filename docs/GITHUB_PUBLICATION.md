# Publier Switch2Emu sur GitHub

## Présentation du dépôt

Description courte à copier dans GitHub :

> Émulateur expérimental en C++20 visant la Nintendo Switch 2. CPU ARM64, chargement ELF/NRO et services HLE en développement. Aucun jeu compatible actuellement.

Licence choisie pour le code propre au projet : **GPL-3.0-only**. Le fichier LICENSE contient le texte intégral de la GNU GPL v3. Les fichiers tiers conservent leurs conditions, détaillées dans THIRD_PARTY_NOTICES.md.

## Contenu à publier

Inclure les sources, tests, programmes de référence, scripts de construction, documentation, CMakeLists.txt, program.bin, .gitignore, .gitattributes, LICENSE et THIRD_PARTY_NOTICES.md. Les ELF/NRO/bin de tests sont intentionnels ; les exclure tous casserait les tests sur une copie neuve.

Trois références binaires dépassent 100 Mio et sont suivies par **Git LFS**, selon .gitattributes : cpu_fp/reference.bin (environ 192 Mio), cpu_simd_arithmetic/reference.bin (122 Mio) et cpu_subnormal/reference.bin (129 Mio). Installer Git LFS et exécuter `git lfs install` avant le premier ajout de fichiers. Vérifier ensuite `git lfs ls-files` : les trois références doivent y apparaître. Ne pas les envoyer par l'interface web comme des fichiers Git ordinaires. Leur stockage et téléchargement utilisent les quotas LFS du compte GitHub.

Exclure build/, out/, les caches, outils téléchargés, installations Python locales et archives de dépendances. Les exclusions de .gitignore ne retirent pas un fichier déjà suivi : contrôler les fichiers préparés avant le premier commit. Ne pas utiliser un envoi du dossier complet sans ce contrôle.

## Créer le dépôt

1. Dans GitHub, créer un dépôt vide, par exemple Switch2Emu. Ne pas faire générer un second README, une autre licence ou un autre .gitignore.
2. Avec GitHub Desktop, ajouter le dossier local comme dépôt ; s'il n'est pas encore un dépôt Git, utiliser l'option proposée pour le créer.
3. Installer/activer Git LFS comme indiqué ci-dessus, vérifier la liste des fichiers et créer le premier commit.
4. Utiliser Publish repository, choisir la visibilité publique et vérifier le dépôt publié.

Une archive préparée pour publication peut aussi être extraite dans un nouveau dossier puis ajoutée à GitHub Desktop. Les dossiers de compilation et caches de la copie de travail restent alors à l'écart.

Après clonage : `git lfs pull` récupère les références. Pour les tests, préférer un clone Git avec LFS à un téléchargement ZIP de GitHub, qui peut contenir des pointeurs au lieu des données.

## État à annoncer

Le projet est un émulateur expérimental ; les homebrews et programmes de test sont ses cibles de validation. Le CPU est partiel, le GPU invité reste à développer et aucun jeu commercial n'est validé. Le parcours applicatif libnx par défaut n'atteint pas encore main. Les validations historiques de libnx Switch ne prouvent pas une compatibilité native Switch 2.

Le rapport technique actuel est [LIBNX_APPLET_STATE.md](LIBNX_APPLET_STATE.md). Une vérification de fichiers et de signatures ne certifie pas à elle seule tous les droits d'auteur ou l'absence de secrets : réexaminer tout ajout futur, notamment les fichiers binaires.
