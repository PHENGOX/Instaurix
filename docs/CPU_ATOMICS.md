# Operations atomiques et exclusives

Palier du 24 octobre 2026. Le CPU reste un interpreteur partiel AArch64.

## Couverture

- LDXR/LDAXR et STXR/STLXR : octet, demi-mot, W et X.
- LDXP/LDAXP et STXP/STLXP : paires W et X.
- LDAR/STLR : octet, demi-mot, W et X.
- CAS, SWP, LDADD, LDCLR, LDEOR, LDSET, LDSMAX, LDSMIN, LDUMAX et LDUMIN : quatre tailles et variantes acquire/release. Les alias sans resultat utilisent le meme encodage avec destination ZR.
- CLREX avec immediat ; DMB/DSB pour les options de domaine/type usuelles ; ISB SY.

Les acces sont limites a la RAM, naturellement alignes (sur la taille totale pour les paires), avec controle des permissions avant tout effet. Les registres W sont etendus par zero, les flags sont preserves. Les chevauchements contraints imprevisibles des exclusives sont refuses ; les combinaisons reservees et CASP restent refuses.

## Modele de reservation

Chaque CPU a une reservation locale, hors du contexte architectural sauvegarde. Elle contient adresse, taille et version d'un granule RAM de 16 octets. Les versions appartiennent a Memory : elles voient aussi les ecritures directes de l'hote, les copies de buffers et les autres CPU qui utilisent la meme RAM. Une ecriture, meme de la meme valeur, invalide la reservation de ce granule ; une modification de permissions ou une remise a zero l'invalide aussi.

Une exclusive store consomme la reservation et retourne Wstatus=0 en cas de succes, 1 sinon. Adresse et taille doivent correspondre a la lecture precedente. CLREX, reset CPU, restauration de contexte, SVC et erreurs d'execution/instruction effacent la reservation. Un simple captureContext ne l'efface pas. Cette politique de changement de contexte evite de transporter une reservation entre threads ; une reprise peut obliger la boucle invitee a recommencer.

L'execution actuelle est sequentielle, sans reordonnancement ni acces differes : acquire/release et les barrieres ne necessitent pas de file memoire supplementaire. Les transferts atomiques sont indivisibles dans ce modele. **Aucune garantie d'execution parallele par plusieurs threads hotes** : un futur ordonnanceur devra serialiser les acces, ou introduire une synchronisation appropriee. Les tests a deux CPU entrelacent leurs pas sur un seul thread hote. Ce palier ne valide pas un modele memoire multicoeur Switch 2.

## Validation et reproduction

Les 206 instructions de `tests/programs/atomic_vectors.S` sont assemblees par Clang 18.1.6, fourni dans Zig 0.13.0. `tools/build_atomic_samples.ps1` regenere le binaire ; il invoque directement l'assembleur Clang avec LSE, car le pilote cc de cette version perd cette option pour les fichiers assembleur.

`cpu_atomic_tests` compare les effets a des attentes mathematiques pour toutes les tailles, variantes d'ordre, operations LSE et CAS sur des valeurs limites. Les tests couvrent aussi exclusives simples/paires, ecritures concurrentes simulees, meme valeur/ABA, copies hote, granules voisins, deux CPU, permissions, MMIO, registres zero/SP, encodages invalides et une boucle reessayant apres contention forcee. Les encodages viennent d'un assembleur independant ; les attentes de ce palier ne sont pas issues d'une comparaison avec une console ou un moteur externe.

```powershell
./tools/build_atomic_samples.ps1
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Un [premier palier FP et Advanced SIMD](CPU_FP_SIMD.md), avec leur etat de contexte, est maintenant realise. Ces familles restent partielles. CCMP/CCMN sont egalement disponibles ; d'autres familles entieres restent absentes.

## References

- [Arm, Synchronization Primitives in A64](https://documentation-service.arm.com/static/68c223238a337a2bc6645c0a), pour les moniteurs et acquire/release.
- [Arm, Instruction Set Reference Guide](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab), pour les instructions et contraintes.
- [Arm, introduction a LSE](https://learn.arm.com/learning-paths/servers-and-cloud-computing/lse/intro/).
