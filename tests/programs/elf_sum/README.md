# Démonstration C en ELF ARM64

Cette cible vérifie le chargement de segments et les droits mémoire. Elle n'utilise pas une ABI Switch 2.

| Plage | Contenu | Droits |
| --- | --- | --- |
| Page 0x10000 | Entrée assembleur et fonction C | R/X |
| Page 0x11000 | Constante attendue : 55 | R |
| Page 0x12000 | `initial=10`, puis BSS `output` et `zero_probe` | R/W |
| Derniers 64 Kio de RAM | Pile | R/W |
| Page avant la pile | Garde | Aucun |

`start.S` vérifie que `zero_probe` est nul, appelle `compute_sum`, lit son résultat puis le compare à la constante. Le résultat 55 est écrit à **0x12008** et reste dans **X2**. La pile est restaurée. `SVC #0xF000` termine le test avec le code 0 ; un résultat inattendu demande un code 1. Ce protocole est désactivé par défaut et reste distinct des services d'une console.

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\elf_sum\program.elf 500 --test-protocol
```

Sortie vérifiée : 94 instructions, X2=55, fin normale avec code 0.

Reproduction avec le compilateur portable Zig 0.13.0 déjà utilisé pour les tests CPU :

```powershell
.\tools\build_arm64_samples.ps1
```

Le script compile `sum.c`, assemble `start.S` et effectue le lien selon `link.ld`, avec des segments PT_LOAD distincts. Les commandes et options exactes restent dans le script ; aucun téléchargement n'est effectué à la compilation habituelle de l'émulateur.

Empreinte SHA-256 du fichier ELF vérifié :

```text
ff469000d206386e234079ddd10d73e34696df6075766098e33266a2dc2e7e2c
```

Le code, les données initialisées et la BSS sont examinés par `elf_loader_tests`, qui teste aussi les accès interdits et des fichiers invalides. Voir le [périmètre et les restrictions du chargeur](../../../docs/ELF_LOADER.md). La chaîne de compilation portable est décrite dans le [guide du premier programme C](../stack_sum/README.md).
