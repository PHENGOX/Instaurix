# Client graphique C ARM64

Sources originales, compilation Clang 18.1.6 via Zig 0.13.0, freestanding O1. Reconstruction : `./tools/build_hle_graphics.ps1`. Les wrappers et le démarrage viennent de hle_client/start.S ; le placement ELF de hle_memory.

Le client obtient emu:fs et emu:disp via sm:, lit les 272 octets de root/sprite.txt et les vérifie. Il construit un framebuffer RGB de 160 × 96 avec un fond étoilé et le motif central, calcule son empreinte, le présente par IPC puis ferme ses sessions.

Rapport à 0x12000 : status, loaded, presented, hash, chacun u64. Succès : **1, 272, 46080, 4044961488**. Parcours : **614 588 instructions**. Un code de sortie normal seul ne valide pas le rapport.

ELF SHA-256 : `8CB689A7E5F5701B004163E44316E25B3CE348823A95A6A628EDCC28DA9D4A9C`.

Voir [HLE_GRAPHICS.md](../../../docs/HLE_GRAPHICS.md) pour la commande d'export et les limites. Ce client utilise un service de framebuffer local ; il ne soumet aucune commande GPU native.
