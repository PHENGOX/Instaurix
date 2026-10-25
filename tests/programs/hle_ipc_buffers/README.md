# Démonstration des buffers IPC

ELF ARM64 original assemblé avec Clang 18.1.6 via Zig 0.13.0. Aucun fichier Nintendo. Reconstruction depuis la racine : `./tools/build_hle_ipc_buffers.ps1`. Le placement ELF est celui de hle_memory.

Le programme remplit 512 octets à 0x12100 avec deux répétitions de 0..255, demande à `emu:demo` de les copier à 0x12400 par descripteurs Send/Receive, puis compare chaque octet. Il enregistre la taille copiée (512) à 0x12000 et le marqueur de réussite (1) à 0x12008, ferme la session et quitte. Le marqueur vaut 99 en cas d'échec détecté.

Le parcours réussi compte **5 677 instructions**. La requête a un en-tête HIPC 0x01100004, RawCount=8, deux descripteurs de 12 octets, puis SFCI à l'offset 32 et commande 2.

ELF SHA-256 : `7472F0E085C8D4564B701140813D597CAFBD4AA75B4AB55CD5E9480905DF527F`.

Limites et tests détaillés dans [HLE_IPC_BUFFERS.md](../../../docs/HLE_IPC_BUFFERS.md).
