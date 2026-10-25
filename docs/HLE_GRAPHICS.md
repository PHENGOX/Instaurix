# Première image de développement

Étape du 25 octobre 2026 : un client C ARM64 lit un motif par emu:fs, calcule une image dans sa mémoire et la présente au service local **emu:disp**, obtenu via sm:. L'hôte conserve les pixels et peut les exporter en PNG. Cette étape valide un framebuffer de développement. Le GPU invité, ses shaders, ses commandes natives et le backend Vulkan restent absents.

## Service de présentation

| Élément | Contrat local |
| --- | --- |
| Service | Nom de 8 octets `emu:disp` |
| Commande | 0 Present |
| Paramètres CMIF | largeur u32, hauteur u32 |
| Entrée | Un descripteur Send Normal contenant les pixels RGB8 contigus |
| Disposition | Lignes de haut en bas, pixels de gauche à droite, R puis G puis B, sans alpha ni padding |
| Réponse | SFCO avec nombre d'octets présentés en u64 |

Les dimensions vont de 1 à 256 ; largeur × hauteur × 3 doit rester inférieur ou égal à 64 Kio et correspondre exactement à la taille Send. Les buffers Receive et paramètres supplémentaires sont refusés. Le contrôle mémoire commun vérifie lecture RAM, limites, modes et absence de MMIO.

Les pixels sont copiés dans un nouvel objet hôte immuable. La nouvelle image n'est publiée qu'après écriture complète de la réponse IPC. Une requête invalide, un buffer interdit ou une réponse non inscriptible laisse la dernière image et son compteur inchangés. Modifier ensuite les pixels invités ne modifie pas une image déjà capturée.

`process().frame()` expose la dernière image et `process().frameCount()` le nombre de présentations réussies. L'image reste disponible après fermeture du service et fin du programme. Un chargement réussi réinitialise image et compteur ; un chargement échoué les conserve. Les sessions utilisent les contrôles de clonage et fermeture déjà présents.

## Export PNG

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_graphics\program.elf 1000000 --hle-reference --read-root .\tests\programs\hle_graphics\root --frame-out .\build\first-frame.png
```

L'export est demandé explicitement par `--frame-out`. Il exige le mode HLE et une image effectivement présentée. Il est effectué après l'exécution ; aucun chemin de sortie n'est fourni par le programme invité. Un budget insuffisant avant la première présentation produit une erreur d'export, sans fichier image factice.

Le code d'export écrit un PNG RGB 8 bits, sans compression des blocs DEFLATE, avec CRC et Adler-32. Il ne nécessite aucune bibliothèque graphique externe. Le PNG ne constitue pas encore une fenêtre d'affichage ni une boucle de présentation en temps réel.

## Démonstration

La ressource sprite.txt contient 16 lignes de 16 caractères plus LF, soit 272 octets. Le client C vérifie le motif, produit un fond étoilé et agrandit le motif par blocs de 4 × 4 pixels au centre d'une image **160 × 96**. Tous les pixels sont calculés par le code ARM64 invité.

Le parcours termine en **614 588 instructions**. Le rapport à 0x12000 contient quatre champs u64 : status=1, loaded=272, presented=46080, hash=4044961488. Le compteur de présentation vaut 1.

Le banc compare chacun des pixels à un calcul hôte indépendant, vérifie l'empreinte et le résultat avec des budgets de 127 instructions. Il contrôle aussi les dimensions et tailles invalides, les sources MMIO, les réponses interdites, l'immuabilité des captures, le remplacement d'image et le rechargement.

Les PNG de la démonstration et d'un cas nécessitant deux blocs DEFLATE ont également été décodés et vérifiés avec Pillow, indépendamment de l'encodeur du projet.

Le jalon d'animation et de présentation continue est réalisé dans [HLE_ANIMATION.md](HLE_ANIMATION.md). Prochaine étape : entrées clavier pour une démo interactive. L'interface graphique de développement reste distincte du futur décodage des commandes GPU natives Switch 2.
