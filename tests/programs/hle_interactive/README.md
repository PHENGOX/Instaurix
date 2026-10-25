# Client C interactif

Client original dérivé de hle_animation, compilé par Clang 18.1.6 via Zig 0.13.0, ARM64 freestanding O1. Reconstruction : `./tools/build_hle_interactive.ps1`. Le démarrage et SleepThread viennent de hle_animation/start.S ; les ressources de hle_graphics/root.

Le programme ouvre emu:fs, emu:disp et emu:pad via sm:, charge le sprite et lit les boutons avant chaque image. Il applique des pas de quatre pixels, annule les directions opposées et borne le sprite dans l'écran. Échap permet de quitter avant une présentation. Sans entrée, le sprite reste à sa position initiale.

Le rapport à 0x12000 contient status, loaded, presented, hash, x, y, max_frames, tous u64. Valeurs initiales : 99, 0, 0, 0, 48, 16, 60. Le banc modifie max_frames pour ses scénarios ; le nombre d'instructions et l'empreinte dépendent des entrées.

ELF SHA-256 : `69949E93E913C07DEF4AEFA3937127983FA9EACEAFD99E00C6B5907A84E5120B`.

Lancement et limites dans [HLE_INPUT.md](../../../docs/HLE_INPUT.md).
