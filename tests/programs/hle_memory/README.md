# Démonstration HLE : tas, interrogation mémoire et thread

Programme C autonome avec entrée assembleur. Il utilise les conventions de référence Horizon pour trois SVC, dans un profil local dont la compatibilité Switch 2 n'est pas établie.

```powershell
.\build\Debug\Switch2Emu.exe .\tests\programs\hle_memory\program.elf 500 --hle-reference
```

Résultat vérifié : **121 instructions**, **X2=55**, fin normale code 0. La valeur X0=123 visible à la fin prouve que le SVC ExitProcess ne l'utilise pas comme code de sortie.

`start.S` alloue 2 Mio avec SetHeapSize, lit TPIDRRO_EL0, appelle `fill_heap` et interroge le tas avec QueryMemory. Le C calcule 55, l'écrit dans le tas et à l'offset 0x180 de la région locale. L'entrée contrôle le type Heap, les droits R/W et le résultat avant de terminer avec SVC 7. Une vérification manquée atteint un SVC inconnu, traité comme erreur terminale.

Le buffer MemoryInfo est à 0x12000 ; les variables C `result` et `last_tls` suivent dans la BSS. Le tas est à 0x200000 et le pointeur local à 0xFEE200 dans ce profil. Ces adresses ne décrivent pas la console réelle.

La compilation reste reproductible avec le [compilateur portable déjà utilisé](../stack_sum/README.md) :

```powershell
.\tools\build_arm64_samples.ps1
```

Le script compile `work.c`, assemble `start.S`, effectue le lien avec `link.ld` et produit `work.generated.s` pour inspection. Les tests utilisent aussi les encodages assemblés indépendamment de `tests/programs/hle_vectors.S`.

| Artefact | SHA-256 |
| --- | --- |
| hle_memory/program.elf | a5f79dfcd9825e725a6be21aa5daa6680d95668601c8583be3654d2c1b733ccf |
| hle_vectors.bin | 8c491b2579665c1b2aacff7b182a20a35ca8085fa6ba85b966b1e0a75f6aac8d |

Voir le [périmètre HLE et les références](../../../docs/HLE_RUNTIME.md). Ce programme ne teste ni handles, ni IPC, ni exécution multithread.
