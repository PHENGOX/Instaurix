# Animation et présentation continue

Étape du 25 octobre 2026 : un client C ARM64 présente huit images successives et dort entre les images. Le lancement peut afficher chaque présentation dans une fenêtre Windows ou exporter une séquence PNG horodatée. Le service reste le framebuffer local emu:disp ; le GPU natif et Vulkan restent absents.

## Frontière d'image

`Machine::run(budget, true)` retourne **Stop::Frame** après une présentation réussie. Le SVC, le compteur d'instructions, le minuteur de développement et le quantum sont traités avant l'arrêt. La reprise continue à l'instruction suivante. Le mode normal `run(budget)` conserve son comportement antérieur.

Le lecteur récupère ainsi chaque image avant que la suivante la remplace. Le nombre total d'instructions et les points de présentation ne dépendent pas du découpage des budgets. L'horloge virtuelle reste pilotée explicitement : quand les threads sont bloqués sur un délai, le lecteur avance jusqu'à l'échéance suivante.

## Fenêtre Windows

```powershell
.\build\Release\Switch2Emu.exe .\tests\programs\hle_animation\program.elf 10000000 --hle-reference --read-root .\tests\programs\hle_graphics\root --window
```

La fenêtre native conserve le rapport largeur/hauteur et agrandit les pixels avec des bandes noires si nécessaire. Elle convertit les captures RGB en DIB pour GDI et ne nécessite pas de bibliothèque graphique externe. La boucle traite les messages entre des budgets de 4 096 instructions et pendant les pauses ; fermer la fenêtre interrompt l'exécution proprement.

Les pauses virtuelles sont également attendues côté horloge réelle avec traitement des messages. Le temps de calcul et de dessin s'y ajoute : il ne s'agit pas d'une garantie de cadence ou d'une calibration du CPU. La fenêtre reste ouverte sur la dernière image après la fin normale jusqu'à sa fermeture. Une limite d'instructions atteinte arrête la session.

La fenêtre est réservée à Windows. Les autres fonctions du moteur et l'export de séquence restent indépendants de cette interface. Le test automatisé de cycle de vie crée la fenêtre cachée, présente deux images, traite les messages et ferme/réouvre la fenêtre ; il ne constitue pas une inspection visuelle d'une fenêtre affichée.

## Export sans fenêtre

```powershell
.\build\Release\Switch2Emu.exe .\tests\programs\hle_animation\program.elf 10000000 --hle-reference --read-root .\tests\programs\hle_graphics\root --frames-out .\build\animation --frame-out .\build\animation-last.png
```

`--frames-out` écrit frame-000001.png, etc., et **frames.csv**, avec numéro, temps virtuel en ns, nombre cumulé d'instructions, dimensions et empreinte RGB FNV-1a. Le journal définit les images de ce lancement ; d'éventuels anciens fichiers non listés ne font pas partie de la séquence. Le dossier est créé si nécessaire. Les erreurs d'export sont signalées, sans poursuivre une capture présentée comme complète.

Sans fenêtre, les délais virtuels avancent sans pause réelle. `--frame-out` continue d'exporter seulement la dernière capture. Toutes ces options exigent le mode HLE. Le budget total d'instructions reste strict, y compris avec les arrêts intermédiaires aux images.

## Démonstration et validation

Le client réutilise sprite.txt via le montage de fichiers. Le vaisseau se déplace horizontalement et les étoiles changent à chaque présentation. Il émet **8 images**, aux temps **0, 100, …, 700 ms virtuelles**, puis termine en **4 882 772 instructions**. Le rapport conserve status=1, loaded=272, presented=8 et l'empreinte de la dernière image.

Les tests vérifient chaque pixel des huit captures, leur ordre, leur temps et leur frontière d'instruction. Les mêmes captures sont obtenues avec un budget élevé ou des tranches de 997 instructions. Les anciens tests restent actifs. Les PNG exportés ont aussi été décodés indépendamment et comparés aux empreintes de frames.csv.

Un aperçu GIF peut être dérivé des PNG pour consultation ; ses couleurs indexées et son bouclage ne remplacent pas les captures RGB de référence. L'aperçu de cette étape est dans build/animation-preview.gif.

Les entrées clavier et la démo interactive sont réalisées dans [HLE_INPUT.md](HLE_INPUT.md). Prochaine étape : confronter ce socle à une cible applicative plus complète. Les interfaces de manette et de GPU attendues par les jeux Switch 2 restent à étudier et implémenter.
