# Client C de lecture de fichiers

Sources originales, ARM64 freestanding O1, compilation Clang 18.1.6 via Zig 0.13.0. Reconstruction : `./tools/build_hle_files.ps1`. Le démarrage et les wrappers SVC viennent de hle_client/start.S ; le placement ELF de hle_memory.

Le dossier root contient seulement sample.txt, 30 octets avec LF final. Le client obtient emu:fs via sm:, ouvre ce fichier, interroge sa taille, vérifie son contenu et une empreinte FNV-1a, clone sa session puis contrôle une lecture partielle et EOF. Les sessions sont fermées au fur et à mesure.

Lancement : `Switch2Emu program.elf 50000 --hle-reference --read-root root`, depuis ce dossier avec un chemin correct vers l'exécutable.

Rapport à 0x12000 : six champs u64 status, size, read, hash, partial, eof. Valeurs de succès : **1, 30, 30, 530586646, 9, 0**. Nombre d'instructions : **4 006**. Une fin normale seule ne prouve pas le succès : le banc vérifie le rapport.

ELF SHA-256 : `BE2CE87106895290C85AEE8EDB0CC84ED5BFE209C84DFE4ACA7F9635138BD0A4`.

Détails et limites dans [HLE_FILES.md](../../../docs/HLE_FILES.md).
