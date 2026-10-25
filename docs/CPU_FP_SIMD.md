# Paliers flottants et Advanced SIMD

Etat du 24 octobre 2026. Ce palier ajoute des instructions utiles aux programmes compiles et leur etat architectural. Il ne constitue pas une implementation complete de FP/Advanced SIMD ni une validation de compatibilite Switch 2.

## Etat du CPU et du thread

Les 32 registres V0-V31 de 128 bits sont representes par deux moities entieres de 64 bits. Leurs vues B/H/S/D/Q partagent les memes bits. Les ecritures scalaires prises en charge effacent les bits superieurs ; une ecriture INS ne remplace que la voie selectionnee. V31 est un registre vectoriel normal, distinct de XZR/SP.

`CPU::Context` inclut tous les V, FPCR et FPSR. Reset et chargement reussi les remettent a zero. Capture/restauration, checkpoints de pause, sortie et erreur les conservent ; un chargement echoue conserve aussi le contexte precedent. Les transferts MRS/MSR FPCR/FPSR sont disponibles pour les champs exposes par le profil local (FPCR masque `0x07C89F00`, FPSR `0x0800009F`). Les autres bits sont RAZ/WI ; les extensions plus recentes ne sont pas annoncees.

## Instructions couvertes

| Famille | Sous-ensemble de ce palier |
| --- | --- |
| FP scalaire S/D | FADD, FSUB, FMUL, FDIV, FSQRT ; FMADD, FMSUB, FNMADD, FNMSUB avec un seul arrondi |
| FP vectoriel 2S/4S/2D | FADD, FSUB, FMUL, FDIV, FSQRT ; FMLA/FMLS fusionnes ; FABS/FNEG |
| Conversions | SCVTF/UCVTF W/X vers S/D ; FCVTNS/NU, AS/AU, PS/PU, MS/MU, ZS/ZU S/D vers W/X ; FCVT S vers D et D vers S |
| Conversions vectorielles | SCVTF/UCVTF et FCVTNS/NU, AS/AU, PS/PU, MS/MU, ZS/ZU sur les elements 32/64 bits des formes 2S/4S/2D |
| Arrondis flottants | FRINTN/A/P/M/Z/I/X scalaires S/D et vectoriels 2S/4S/2D |
| Extrema flottants | FMAX/FMIN et FMAXNM/FMINNM scalaires S/D et vectoriels 2S/4S/2D |
| Reductions flottantes | FADDP et FMAXP/FMINP/FMAXNMP/FMINNMP vectoriels et vers un scalaire ; FMAXV/FMINV/FMAXNMV/FMINNMV sur 4S |
| Deplacements et signes | FMOV entre registres S/D, entre W/S et X/D, et immediat ; FABS, FNEG |
| Comparaisons et selection | FCMP/FCMPE avec registre ou zero ; FCSEL selon les conditions NZCV |
| Entiers vectoriels | ADD/SUB par voies 8/16/32 bits sur 64/128 bits, et 64 bits sur 128 bits ; debordement modulo la largeur |
| Multiplications et extrema | MUL/MLA/MLS et SMAX/SMIN/UMAX/UMIN sur elements 8/16/32 bits, vecteurs 64/128 bits |
| Arithmetique SIMD supplementaire | Saturation SQADD/UQADD/SQSUB/UQSUB/SQABS/SQNEG ; retrecissement SQXTN/UQXTN/SQXTUN ; decalages variables SSHL/USHL et variantes arrondies/saturantes ; produits SMULL/UMULL et accumulations SMLAL/UMLAL/SMLSL/UMLSL, variantes 2 |
| Comparaisons vectorielles | CMEQ, CMGT/CMGE, CMHI/CMHS et CMTST entre registres ; elements 8/16/32 bits sur 64/128 bits et 64 bits sur 128 bits |
| Reductions entieres | ADDP vectoriel ; ADDV, SMAXV/SMINV/UMAXV/UMINV sur les formes 8B/16B/4H/8H/4S |
| Permutations et bits | ZIP1/2, UZP1/2, TRN1/2 ; EXT par octets ; TBL/TBX sur tables de 1 a 4 registres ; REV16/32/64 selon la taille valide ; CNT et NOT par octets |
| Decalages vectoriels | SHL immediat, SSHR/USHR immediats, y compris decalage droit de toute la largeur d'un element |
| Logique vectorielle | AND, BIC, ORR, ORN, EOR, BSL, BIT, BIF sur 8B/16B |
| Copies de voies | DUP depuis GPR ou element vectoriel ; UMOV/SMOV vers GPR ; INS depuis GPR ou element vectoriel |
| Memoire | LDR/STR et LDUR/STUR B/H/S/D/Q ; offsets immediats ou registre et pre/post-index ; lectures litterales S/D/Q ; LDP/STP S/D/Q avec offset et pre/post-index |
| Memoire structuree | LD1/ST1 vers 1 a 4 registres ; LD2/ST2, LD3/ST3, LD4/ST4 entrelaces/desentrelaces ; formes multiple structures sans offset et post-index immediat/registre |

Les transferts memoire sont limites a la RAM et verifient permissions et plage complete avant tout effet. Les ecritures simples, par paires et structurees utilisent la copie transactionnelle existante et participent aux invalidations des reservations exclusives. La base entiere et la destination vectorielle peuvent avoir le meme numero puisqu'elles appartiennent a des banques distinctes. Les paires non temporelles, transferts structures vers une voie unique et chargements repliques LDnR ne sont pas ajoutes.

Les formes multiple structures LD1/ST1 transfèrent des blocs consecutifs ; LD2/3/4 et ST2/3/4 desentrelacent/entrelacent les elements de 8/16/32/64 bits. Les formes 1D sont reservees a LD1/ST1. Les listes de registres peuvent passer de V31 a V0. Les chargements de 64 bits effacent la partie superieure de chaque registre. La mise a jour post-index utilise la taille totale ou la valeur initiale du registre d'offset ; elle n'est appliquee qu'apres un transfert reussi. Les erreurs de permissions, MMIO, limites, alignement SP ou debordement d'adresse sont refusees avant toute modification.

TBL/TBX lisent une table de 16 a 64 octets dans des registres consecutifs, avec retour de V31 a V0. TBL remplace un indice hors table par zero ; TBX conserve l'octet correspondant de l'ancienne destination. Les tables, indices et destinations peuvent se chevaucher sans affecter les lectures de l'instruction. Les formes 8B effacent la moitie superieure. SMOV etend le signe d'un element B/H vers W/X ou S vers X ; une destination W efface les bits superieurs de X. INS ne remplace que la voie selectionnee.

## Calcul flottant : comportement et limites

Les operations arithmetiques utilisent maintenant un [moteur flottant logiciel](CPU_SOFTFLOAT.md), sans calcul flottant hote. Les quatre modes d'arrondi FPCR, Default NaN et FZ sont pris en charge. Le moteur traite les sous-normaux, NaN silencieux/signalisants, leur priorite, les zeros signes et les flags cumulatifs IOC/DZC/OFC/UFC/IXC/IDC de FPSR. FCMP/FCMPE affectent NZCV ; les autres operations couvertes preservent les flags entiers.

L'environnement flottant hote n'est pas modifie. Les erreurs d'encodage ou de mode non couvert ne laissent pas de modification partielle dans l'etat invite.

Les conversions entier/flottant sont realisees par calcul entier sur les bits, sans conversion C++ hors limites. SCVTF/UCVTF utilisent l'arrondi FPCR. Les conversions vers entier utilisent l'arrondi encode dans leur instruction : plus proche pair, plus proche avec egalites loin de zero, vers plus/moins l'infini ou vers zero. Un depassement ou un infini sature a la borne du type ; NaN donne zero ; FPSR.IOC est alors pose sans ajouter IXC. Les conversions valides inexactes posent IXC. Ces conversions vers entier traitent aussi les entrees sous-normales, sans activer FZ. Les ecritures W effacent la moitie haute de X, meme pour un resultat signe negatif.

FCVT S/D preserve les NaN selon DN, leur signe et les bits de charge utile conservables, et signale les NaN signalisants. Elargissement et retrecissement acceptent les sous-normaux et traitent underflow, arrondi et FZ. Les variantes fixed-point a facteur d'echelle ne sont pas ajoutees.

Les calculs fusionnes effectuent le produit et l'addition en entiers exacts avant un arrondi unique. Le moteur applique les signes propres aux quatre instructions et traite les trois NaN et le cas invalide zero multiplie par infini avec un addend NaN. Les tests distinguent le resultat fusionne d'un arrondi intermediaire et couvrent l'annulation d'un produit qui depasserait seul la plage flottante.

Les calculs flottants vectoriels reutilisent le meme traitement arithmetique que les scalaires, element par element. FMLA/FMLS utilisent la destination initiale comme accumulateur et conservent un seul arrondi par element. Les sources et l'accumulateur sont captures avant tout calcul pour permettre les alias de registres. Les flags FPSR de chaque element sont cumules localement ; destination et FPSR ne sont valides qu'apres le succes de tous les elements. Une erreur tardive ne laisse aucun effet partiel. Les formes 2S effacent les 64 bits superieurs et n'examinent pas les elements inactifs. FABS/FNEG manipulent les bits sans appliquer les limites propres a l'arithmetique.

Les reductions FADDP additionnent les elements adjacents dans l'ordre architectural, sans remplacer cette operation par une somme C++ reassociee. Les extrema par paires et sur 4S respectent l'ordre de combinaison et le traitement des NaN. Une reduction invalide ou non couverte ne laisse pas de flags produits par les paires precedentes.

Les conversions vectorielles utilisent les memes fonctions entieres que les conversions scalaires : plage signee/non signee, arrondi, saturation et flags restent identiques element par element. Elles conservent la largeur des elements, ne sont pas des conversions S/D et n'ajoutent pas FCVTL/FCVTN.

FRINTN choisit le plus proche pair ; FRINTA eloigne les egalites de zero ; P/M/Z arrondissent vers plus/moins l'infini ou zero. FRINTI et FRINTX suivent FPCR.RMode ; seul FRINTX ajoute IXC lorsqu'un resultat numerique change. Tous conservent les zeros signes et traitent les NaN signalisants. Ces arrondis sont calcules sur les bits et acceptent les entrees sous-normales dans les modes couverts. Ils ne resolvent pas l'underflow des additions ou multiplications.

FMAX/FMIN propagent les NaN avec priorite aux signalisants. Les variantes NM choisissent le nombre lorsqu'un seul operande est un NaN silencieux ; un NaN signalisant genere IOC et un NaN silencieux en sortie. DN determine la charge utile conservee. Les extrema distinguent -0 de +0, selectionnent les sous-normaux en FZ=0 et appliquent la mise a zero des entrees avec IDC en FZ=1.

Les anciens refus des sous-normaux, de l'underflow et de FZ sont leves. Le profil detecte la petitesse avant arrondi, distingue un sous-normal exact d'un resultat inexact et applique FZ aux entrees/sorties. Les exceptions flottantes piegees, FZ16/AHP et les nouvelles extensions Alternate Handling restent refuses avec un diagnostic.

La [saturation, les decalages variables et les multiplications elargies](CPU_SIMD_ARITHMETIC.md) sont ajoutes dans un palier separe. La saturation modifie uniquement le bit cumulatif FPSR.QC ; les autres flags sont preserves. Restent notamment absents : FP16, conversions fixed-point, FCVTL/FCVTN et plusieurs autres familles. Les corpus testes ne prouvent pas une conformite exhaustive.

## Validation

- **1 491 variantes, 96 400 cas independants** : encodages assembles avec Clang 18.1.6 via Zig 0.13.0, attendus calcules avec Unicorn 2.1.4, graine `0xF0642026`. Les 983 variantes precedentes sont conservees. Les 508 variantes de transferts structures, tables et copies d'elements ajoutent 10 944 cas, incluant les registres circulaires, mises a jour de base, index hors table, extensions de signe et chevauchements. Le corpus conserve les calculs flottants avec elements heterogenes, valeurs limites, quatre arrondis FPCR, DN, NaN et aliases source/destination/accumulateur.
- Le corpus supplementaire `cpu_subnormal` ajoute **64 896 cas sur 129 variantes de test**, avec sous-normaux, petitesse avant arrondi, FZ, flags et valeurs aleatoires. Voir [le bilan logiciel](CPU_SOFTFLOAT.md).
- Le corpus `cpu_simd_arithmetic` ajoute **61 056 cas sur 477 variantes** pour saturation/QC, retrecissements, decalages variables et produits elargis. Voir [ce palier](CPU_SIMD_ARITHMETIC.md).
- Comparaison de X0-X30, SP, NZCV, des 32 registres V complets, de FPCR/FPSR et d'une fenetre RAM de 512 octets. Inclut aliases source/destination, V31/XZR, modes d'arrondi, NaN, infinis, zeros signes, inexact/overflow et chemins memoire.
- Verification de la preservation de l'environnement flottant hote, y compris quand son arrondi est different de celui de l'invite et FTZ/DAZ sont actifs.
- Le fichier `reference-layout.txt`, produit hors des boucles d'entrees, decrit l'instruction et la longueur de chaque bloc. Le lecteur C++ verifie ces informations et les indices d'echantillon avant toute comparaison CPU. Ce controle a ete ajoute apres detection et correction d'une variable d'indice reutilisee dans le generateur des nouveaux cas.
- Tests directs : contexte, reset, pauses/sortie/erreurs du processus, rechargements, sous-normaux et modes refuses, encodages invalides, limites RAM, permissions, paires et MMIO.
- Un meme programme C float/double est compile pour ARM64 et pour l'hote. Resultats binaires identiques : simple `0x40B8B8B0`, double `0xC026C80000000000`. Son executable ELF conserve les chaines precedentes et ajoute **192 octets** pour saturation, retrecissements, decalages variables, produits elargis et instantanes QC. Le programme termine normalement en **343 instructions** (308 avant cette extension).
- La suite complete comprend maintenant **24 tests CTest**, tous reussis en **Debug et Release** avec MSVC. Les references entieres et les tests atomiques precedents sont conserves.

Unicorn est une reference independante, pas le materiel cible. Le corpus reste fini. Le manifeste `tests/programs/cpu_fp/reference.json` contient les instructions, la graine, les versions et l'empreinte SHA-256 du corpus binaire.

## Reproduction

```powershell
./tools/build_fp_reference.ps1 -Python /chemin/vers/python.exe
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

La regeneration demande Unicorn 2.1.4 dans `tools/python-deps`, ou dans l'environnement Python choisi, et Zig 0.13.0 (chemin remplacable par `-Zig`). Elle n'est pas necessaire pour lancer les tests : les artefacts sont inclus. Le script invoque directement les entrees Clang du compilateur pour assembler/generer le code et utilise des caches de travail sous `build`.

L'option `-SamplesOnly` recompile uniquement le programme ELF, sans regenerer le corpus independant.

Prochaine priorite CPU : consolidation sur plusieurs programmes C/C++ compiles plus varies, comparaison native et traitement des instructions manquantes constatees, avant l'ordonnancement de plusieurs threads. L'IPC reste differe pendant cette consolidation.

## References

- [Arm, guide de reference A64](https://documentation-service.arm.com/static/6245c734b059dc5ff9a8bdab).
- [Microsoft, comportement flottant strict](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170).
- [Microsoft, sauvegarde de l'environnement flottant avec feholdexcept](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/feholdexcept2?view=msvc-170).
- [Unicorn, moteur independant utilise pour les references](https://github.com/unicorn-engine/unicorn).
