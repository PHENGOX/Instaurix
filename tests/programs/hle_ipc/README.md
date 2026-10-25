# Démonstration IPC à deux threads

Programme ARM64 original, assemblé avec Clang 18.1.6 via Zig 0.13.0. Aucun fichier Nintendo. Reconstruction depuis la racine : `./tools/build_hle_ipc.ps1`. Le script réutilise le placement ELF de hle_memory.

Le principal ouvre `emu:demo` avec ConnectToNamedPort, construit une requête HIPC/CMIF dans sa région locale, calcule 20+35, puis crée un second thread. Celui-ci utilise la même session pour calculer 44+55 dans son propre buffer. Le principal attend la fin du thread, contrôle les résultats et l'isolation des buffers, lit le temps virtuel puis ferme la session.

Le programme termine en **116 instructions**. À 0x12000 : résultat principal 55 (+0), résultat secondaire 99 (+4), handle du thread (+8), temps virtuel (+16), marqueur de validation 1 (+24). Le marqueur vaut 99 en cas d'échec détecté.

ELF SHA-256 : `F5722EE5D1C337C258EBE3989BC962738508F75BE8F416E41FEA1560B29FC2DD`.

Le port et ses commandes sont un service de démonstration local. Le format est un sous-ensemble sans descripteurs ni transferts de handles ; voir [HLE_IPC.md](../../../docs/HLE_IPC.md).
