# Démonstration du gestionnaire de services

ELF ARM64 original, assemblé avec Clang 18.1.6 via Zig 0.13.0. Aucun fichier Nintendo. Reconstruction depuis la racine : `./tools/build_hle_sm.ps1`. Le placement ELF est celui de hle_memory.

Le programme ouvre sm:, envoie RegisterClient avec SendPid, demande les huit octets du nom emu:demo et récupère un handle Move dans la réponse. Il ferme la connexion sm:, utilise le nouveau handle pour calculer 20+35, vérifie 55, ferme la session et quitte.

Résultat en **85 instructions**. À 0x12000 : handle du service obtenu (+0), ancien handle sm: (+4), résultat 55 (+8), marqueur de validation 1 (+16). Le marqueur vaut 99 en cas d'échec détecté.

ELF SHA-256 : `4BB27A6CD9AEF3F5EFD2E02AD05F6DBCF9E05975D4E5381F1FAEF651CE8DAB48`.

Le service est un service local de démonstration. Les limites du gestionnaire, du PID et du transfert de handle sont détaillées dans [HLE_SM.md](../../../docs/HLE_SM.md).
