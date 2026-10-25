# Client C empaqueté en NRO0 statique

Échantillon original du projet, sans binaire Nintendo. work.c vient de hle_client ; start.S réserve les 0x80 octets d'en-tête et une BSS supplémentaire. Compilation Clang 18.1.6 via Zig 0.13.0, puis empaquetage par tools/package_nro_reference.py.

Reconstruction : `./tools/build_nro_client.ps1 -Python <chemin_python>`. Le script conserve un ELF correspondant pour la comparaison du parcours.

Le NRO a 12 288 octets de fichier et 12 288 octets de BSS. Il termine en **14 343 instructions**, avec le même rapport C que son ELF : 1, 55, 512, 3505779141, 0, temps virtuel, 7 à partir de 0x12000 (champs u64).

NRO SHA-256 : `A34BF47CD5EE27E274AB897609613CAF945568ADA0441C321A15FD41BC7E2092`.

Cet échantillon sans MOD0 utilise le profil HLE local. Il ne valide ni l'ABI de démarrage libnx ni le mode natif Switch 2. Voir [NRO_LOADER.md](../../../docs/NRO_LOADER.md).
