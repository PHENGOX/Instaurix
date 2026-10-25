# Démonstration des attentes à durée finie

ELF ARM64 original, assemblé par Clang 18.1.6 via Zig 0.13.0. Aucun fichier Nintendo. Reconstruction : `./tools/build_hle_time.ps1` depuis la racine. Le placement ELF est celui de hle_memory.

Le principal crée un événement et un thread, attend l'événement, puis attend la fin du thread. Le second dort 1 000 ns, signale, dort 500 ns et quitte. Le lanceur console avance explicitement le temps pendant les périodes où tous les threads sont bloqués. Résultat : **43 instructions**, **1 500 ns virtuelles**, valeur **55** à 0x1200C.

La zone de données à 0x12000 contient les handles aux offsets 0/4/8, le résultat final à +12, le résultat de l'attente à +16, X1 après l'attente à +24, le délai du principal à +32 (2 000 ns), le sommeil du second à +40 (1 000 ns), et les ticks observés à +48/+56. Modifier le délai principal en 500 ns déclenche un TimedOut sans empêcher la fin normale après le join.

ELF SHA-256 : `0C2800208D3DEE4A33EFB29699C506ACF88BBAA554FA7B3367C73F290BA553FE`.

Le banc hle_time_tests vérifie le parcours complet et les limites du moteur de temps. Détails dans [HLE_TIME.md](../../../docs/HLE_TIME.md).
