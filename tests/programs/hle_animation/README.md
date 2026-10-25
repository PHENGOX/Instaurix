# Client C animé

Client original dérivé de hle_graphics, compilé par Clang 18.1.6 via Zig 0.13.0 en ARM64 freestanding O1. Reconstruction : `./tools/build_hle_animation.ps1`. Le client ajoute un wrapper SleepThread et réutilise le montage `../hle_graphics/root`.

Le programme lit et vérifie sprite.txt, produit huit images 160 × 96, déplace le motif horizontalement et change les étoiles. Entre deux images, SleepThread attend 100 000 000 ns virtuelles. Il n'y a pas de sommeil après la huitième présentation.

Succès : **4 882 772 instructions**, **8 images**, **700 000 000 ns virtuelles**. Rapport à 0x12000 : status=1, loaded=272, presented=8, hash de la dernière image ; quatre champs u64.

ELF SHA-256 : `AEA26DC51A3A52CF95132016692ED5E74BED3B518BE993E04EC81898AECFC5C8`.

La référence hôte vérifie chaque pixel, temps et nombre cumulé d'instructions. Voir [HLE_ANIMATION.md](../../../docs/HLE_ANIMATION.md) pour --window et --frames-out. Il s'agit d'un affichage de développement, sans commandes GPU natives.
