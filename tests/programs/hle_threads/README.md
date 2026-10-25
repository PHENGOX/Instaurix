# Démonstration des deux threads HLE

Programme ARM64 original assemblé avec Clang 18.1.6 via Zig 0.13.0. Aucun fichier Nintendo. Reconstruction depuis la racine : `./tools/build_hle_threads.ps1`.

Le principal crée un événement et un second thread. Il attend l'événement puis la fin du thread. Le second écrit 42, signale l'événement, effectue une boucle, écrit 55 puis quitte. Le principal vérifie 55 et quitte aussi. Le programme parcourt 367 instructions dans son mode normal.

La zone R/W à 0x12000 contient les deux handles d'événement (+0, +4), le handle du thread (+8), le résultat (+12), puis les marqueurs de registres et de TLS. Les champs initialement nuls à +96 et +100 permettent au test hôte de demander respectivement un principal en boucle active et un second thread qui attend lui aussi l'événement avant de calculer.

Les marqueurs entiers, vecteurs, registres TLS et la pile distincte vérifient la conservation du contexte. Le banc C++ contrôle également FPCR/FPSR, les gardes de pile et la stabilité de l'ordonnancement avec un budget d'une instruction.

ELF SHA-256 : `B8969BED87ADBE2D2C5BAE75491E9BFE8EF68EB359F2820CBC7F621EB9532CEB`.

Le script réutilise le placement ELF de hle_memory. Détails et limites dans [HLE_THREADS.md](../../../docs/HLE_THREADS.md).
