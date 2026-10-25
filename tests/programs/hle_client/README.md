# Client HLE compilé en C

Sources originales : work.c et start.S. Compilation Clang 18.1.6 via Zig 0.13.0, ARM64 freestanding O1, sans libc ni SDK Nintendo/libnx. work.generated.s permet d'inspecter les instructions produites.

Reconstruction depuis la racine : `./tools/build_hle_client.ps1`. Le placement ELF est celui de hle_memory.

Le client ouvre et clone sm:, initialise l'objet partagé, obtient et clone emu:demo, interroge le buffer de pointeurs, calcule 55, copie et vérifie 512 octets, lit le temps virtuel et ferme ses sessions. Les wrappers assembleur ne contiennent pas la logique de vérification.

Le rapport commence à 0x12000 : sept champs de 64 bits status (+0), sum (+8), copied (+16), hash (+24), pointer_size (+32), time (+40), flags (+48). Succès : 1, 55, 512, 3505779141, 0, temps injecté, 7. Nombre d'instructions : **14 342**. Les statuts intermédiaires identifient la phase où un échec a été détecté.

ELF SHA-256 : `33C49812E24B7BB1835C49B267E7FD8C75842AD01C74A1ACC411F1283070BFCC`.

Détails dans [HLE_CLIENT.md](../../../docs/HLE_CLIENT.md).
