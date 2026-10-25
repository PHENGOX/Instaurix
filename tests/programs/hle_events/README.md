# Événement HLE : attendre et reprendre

Programme ARM64 original assemblé avec Clang 18.1.6 via Zig 0.13.0. Aucun code ni fichier Nintendo. Reconstruction depuis la racine : `./tools/build_hle_events.ps1`.

Le programme crée un événement, publie ses handles à 0x12000 et 0x12004, puis attend son signal. Le test hôte signale le handle d'écriture et reprend l'exécution. Le programme vérifie le retour et l'index, réinitialise l'événement, ferme les deux handles, écrit 55 à 0x12008 et quitte. Un échec écrit 99.

Le lancement avec `--hle-reference` affiche l'attente. Le programme a besoin d'un signal hôte pour finir ; le test `hle_events_suspend_resume` pilote ce parcours complet. Le CLI ne fournit pas encore de commande de signal.

ELF SHA-256 : `E3922FA1CC07DEA10A33CFEAB5D6D359581461FF9E1A6F3B74BAB44E16EBADE1`.

Le script réutilise le placement ELF du programme hle_memory. Les conventions de registres suivent les références Horizon publiques ; aucune validation native Switch 2 n'est revendiquée.
