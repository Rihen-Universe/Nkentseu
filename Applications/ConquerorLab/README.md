# ConquerorLab — l'atelier de test de Conqueror

> **Ce que c'est.** Une application NKEditorKit / NKGui / NKCanvas qui permet de
> **tester une règle de jeu en dix minutes au lieu de trois semaines**. Elle
> charge des moteurs de règles et des IA écrits par les stagiaires, les fait
> jouer, et mesure.
>
> **Pourquoi elle existe.** Les règles de Conqueror ne sont pas connues. Plusieurs
> sont ouvertement suspectes (`REGLES_COMPLETES_v2.md` §16) et aucune ne se
> tranche par le débat : il faut dix mille parties. L'atelier est l'instrument qui
> les produit.

---

## 1. Comment on contribue à l'atelier

**Trois dossiers, trois natures de contribution.** C'est tout ce qu'un stagiaire a
besoin de savoir. Il ne manipule **jamais** de DLL et ne lance **jamais** de
commande de build.

| Je veux ajouter… | Je dépose… | Ce qui se passe |
|---|---|---|
| **un moteur de règles** | `Build/ConquerorLab/rules/mes_regles.cpp` | l'atelier détecte → **compile** → charge → le module apparaît dans le panneau **Modules**, sélectionnable |
| **une IA** | `Build/ConquerorLab/ai/mon_ia.cpp` | idem → l'IA apparaît dans la liste des pilotes du panneau **Joueurs** |
| **une grille** | `Build/ConquerorLab/boards/diamant_4j.json` | **aucune compilation** : la grille apparaît dans le panneau **Règles** → *Plateau* → *Grille* |

Les chemins exacts sont affichés en clair dans le panneau **Modules** (règles, IA)
et dans le panneau **Règles** (grilles) : la question « où est-ce que je pose mon
fichier ? » ne doit pas se poser deux fois.

> **📘 Un cours complet existe** : [`Documentation/cours-conqueror/`](../../Documentation/cours-conqueror/README.md)
> — sept chapitres, du contrat aux mesures, avec trois exemples compilables et
> vérifiés. Disponible en **PDF** (`Cours_ConquerorLab.pdf`, 41 pages) et en
> Markdown. C'est par là qu'un stagiaire commence.

### 1.1 Règles et IA — du C++, rien d'autre

Le fichier implémente `NkcRulesVTable` (ou `NkcAIVTable`) et se termine par la
macro d'export. Deux niveaux de lecture :

- `exemples/rules/RegleContratNu.cpp` et `exemples/ai/IAMinimale.cpp` — **les plus
  petits modules qui jouent vraiment**, écrits pour le cours et vérifiés au banc
  d'essai ;
- `modules/rules/ConquerorRulesV2.cpp` et `modules/ai/ConquerorAIRef.cpp` — les
  modules de référence, plus riches.

```
1.  il écrit   Build/ConquerorLab/rules/mes_regles.cpp
2.  il sauvegarde
3.  l'atelier détecte → COMPILE → charge → l'ajoute au menu déroulant
4.  il rejoue une partie, sans avoir quitté l'application
```

En cas d'erreur, **la sortie complète du compilateur** s'affiche dans le panneau
**Modules**. Le binaire produit à côté du `.cpp` est de la tuyauterie invisible.

### 1.2 Grilles — de la donnée **ou** du code, au choix

Deux voies, et la seconde est souvent la bonne :

| Voie | Quand | Comment |
|---|---|---|
| **JSON** | la forme se décrit par une liste de coordonnées ; un designer doit pouvoir l'essayer sans compiler | déposer un `.json` dans `boards/` |
| **C++** | la forme se décrit par une *formule* ; le voisinage n'est pas géométrique ; les cellules n'ont ni la même taille ni la même forme | construire `coords[]` dans `Create`, écrire son propre voisinage, et déclarer `GetCellCenter` / `GetCellShape` (ABI 3) |

**Ce qui a toujours appartenu au module** : la forme du plateau (`coords[]`), le
voisinage (votre code dans `GenerateLegalMoves` — `ConquerorGeometry.h` est une
*commodité*, pas une obligation) et les cases bloquées (`kCellBlocked`).

**Ce qui lui échappait, et ne lui échappe plus** : la projection écran. Elle était
déduite de `NkcTopology`, donc limitée à l'hexagone et au carré — un plateau que
les règles savaient jouer pouvait être impossible à *afficher*. Depuis l'ABI 3,
`GetCellCenter` et `GetCellShape` rendent la main au module. Démonstration :
`exemples/rules/GrilleLibre.cpp`, un plateau **circulaire** à trois anneaux.

### 1.3 Le format JSON, quand on choisit cette voie

Le plateau n'est pas une constante du moteur : c'est un descripteur sérialisable
(REGLES §4). Le format est celui du contrat, lu par `LoadBoardJson` :

```json
{ "topology": "HEX_POINTY",
  "cells":   [[0,0],[1,0],[2,0]],
  "blocked": [[1,0]],
  "starts":  [{"player":0,"q":0,"r":0,"level":0},
              {"player":1,"q":2,"r":0,"level":0}],
  "min_players": 2, "max_players": 2 }
```

Deux facilités pour ne pas partir d'une page blanche :

- au premier lancement, l'atelier **écrit un exemple** dans `boards/`
  (`exemple_plateau_par_defaut.json`) — il est exporté du moteur, donc forcément
  valide ;
- le bouton **« Exporter le plateau courant »** produit le même fichier à tout
  moment : on part d'une grille qui marche, on la modifie.

C'est le **module** qui lit ce JSON, jamais l'atelier. Un moteur de stagiaire qui
accepte un champ supplémentaire le verra sans qu'on touche à l'interface. Si le
module refuse le fichier, le panneau **Règles** le dit, avec le nom du fichier.

**Sur Android et Web**, il n'y a pas de compilateur sur l'appareil : seuls les
modules compilés *dans* l'application sont disponibles. L'atelier reste
pleinement jouable et mesurable ; seul le rechargement à chaud disparaît. Limite
réelle et assumée.

---

## 2. Arborescence

```
ConquerorLab/
├── include/Conqueror/          ← LE CONTRAT (copié dans Stage/Reference/Conqueror/)
│   ├── ConquerorRulesABI.h     ← ce qu'implémente le stagiaire A1
│   ├── ConquerorAIABI.h        ← ce qu'implémente le stagiaire A2
│   └── ConquerorGeometry.h     ← géométrie de grille, entière et inline
├── modules/
│   ├── rules/ConquerorRulesV2.cpp   ← moteur de référence, palier 0
│   └── ai/ConquerorAIRef.cpp        ← IA de référence (random / glouton / minimax)
├── src/ConquerorLab/
│   ├── NkcWinClean.h           ← désamorce les macros <windows.h> (voir §3)
│   ├── NkcModuleCompiler.h     ← .cpp → module chargeable
│   ├── NkcModuleHost.h         ← catalogue, compilation, chargement, hot-reload
│   ├── NkcBoardRender.h        ← projection écran : coord ↔ pixel, cadrage auto
│   ├── NkcDraw.h               ← polygone / anneau / texte (absents de NkGuiDrawList)
│   ├── NkcLabTheme.h           ← LA palette (GitHub Dark Pro), un seul endroit
│   ├── NkcParamSchema.h        ← lecture du schéma JSON de paramètres
│   ├── NkcBoardLibrary.h       ← les grilles : scan de boards/*.json, import/export
│   ├── NkcSession.h            ← la partie : règles, IA threadée, journal, rejeu
│   ├── NkcBatch.h              ← campagne IA-vs-IA multi-thread
│   ├── Nkc{Board,Rules,Players,Modules,Journal,Metrics}Panel.h
│   └── main.cpp                ← shell NKEditorKit (sur le TAS)
├── ConquerorLab.jenga          ← Windows / Linux / macOS / Android / Web
└── tests/NkcAbiHarness.cpp     ← banc d'essai autonome du contrat
```

---

## 3. Décisions d'architecture, et pourquoi

| Décision | Raison |
|---|---|
| **Types Nkentseu partout** (`uint32`, `int8`, `usize`, `float64`) | Décision du studio. Rendue possible parce que l'atelier pilote la compilation et fournit les `-I`. |
| **Toute la pile sous NKCanvas est offerte** aux modules — en-têtes *et* bibliothèques | La version initiale ne donnait que trois `-I` et aucun lien. C'était élégant et trop étroit : un moteur de règles a besoin d'une chaîne, d'un tableau, d'un formatage, d'un journal, et les réécrire à la main est du temps volé au jeu. La frontière est désormais **NKCanvas / NKGui** — tout ce qui est en dessous est disponible, rien de ce qui est au-dessus ne l'est. Un module de règles n'a aucune raison de dessiner. |
| **Le module a sa PROPRE copie de la pile** (édition de liens statique) | Conséquence directe et assumée : un `logger.Infof()` depuis un module n'apparaît pas dans la console de l'atelier, et les allocateurs des deux côtés sont distincts. C'est pourquoi le contrat expose `nkc_rules_set_allocator` — et pourquoi le retour au stagiaire passe par les `NkcEvent`, pas par le journal. |
| **Géométrie entièrement inline et entière** | `NkRound` de NKMath est un symbole *lié* : l'inclure ferait échouer l'édition de liens de tout module. La projection écran vit donc côté atelier, pas dans le contrat. C'est aussi la bonne frontière : la règle ne connaît pas les pixels. |
| **État opaque + vue en lecture seule** | Le module choisit sa représentation interne ; l'IA et l'atelier ne voient qu'une `NkcStateView`. Aucun couplage. |
| **L'IA reçoit les règles par table de pointeurs** | A2 démarre sans attendre A1, et les deux ne peuvent pas diverger sur les règles : il n'en existe qu'une implémentation. |
| **Points de Conquête en dixièmes entiers** | Les coefficients 0,5 / 0,2 / 0,1 en flottant tueraient le rejeu bit-à-bit exigé par A1. |
| **PRNG porté par l'état** | Sans lui, deux clones d'une position piochent dans le même flux global et le rejeu meurt. Inutilisé au palier 0 — Conqueror n'a aucun aléatoire à ce stade. |
| **Copie fantôme Windows** | Sans elle, le `.dll` reste verrouillé et le stagiaire ne peut pas recompiler pendant que l'atelier tourne. |
| **L'IA réfléchit sur SA PROPRE instance de moteur** | `ChooseMove` tourne sur un thread worker. Partager l'instance avec le thread de rendu exposerait le calcul à un paramètre déplacé en cours de route. Le thread reçoit donc une instance privée, synchronisée avant chaque réflexion (plateau + paramètres), et un état transféré par `SerializeState`/`DeserializeState`. Zéro verrou, zéro course. |
| **Les surbrillances sont SIMULÉES, pas déduites** | Pour montrer « quels ennemis vais-je retourner ? », l'atelier clone l'état, joue le coup pour de faux et lit les événements. Il ne déduit rien des règles — donc l'aperçu reste juste même quand A1 change la règle de transformation. |
| **Les vtables sont COPIÉES, pas pointées** | Elles vivent dans un `NkVector` qui réalloue, et dans une DLL qu'un rechargement à chaud peut fermer. La session copie la table à la sélection et se relie explicitement quand le catalogue bouge. |
| **ABI 3 : la géométrie d'affichage rendue au module** | `GetCellCenter` / `GetCellShape`, optionnels (`nullptr` → projection topologique). Le voisinage et la forme du plateau étaient déjà l'affaire du module ; **seule la projection écran lui échappait**, ce qui rendait impossible d'afficher un plateau que ses règles savaient jouer. Les flottants renvoyés sont de la *présentation* : `HashState` ne les voit pas, §17.1 reste entier. |
| **Le projecteur n'a aucun état persistant** | `NkcProjector` est reconstruit à chaque image. Le rendre persistant obligerait à l'invalider quand le module change — le genre d'invalidation qu'on oublie. Coût : deux affectations. |
| **`NkcWinClean.h` avant tout include NKFileSystem** | `<windows.h>` transforme `GetFreeSpace`, `DeleteFile`, `GetCurrentDirectory`… en macros ; elles réécrivent les **déclarations** de NKFileSystem et clang signale une erreur de syntaxe très loin de la cause. Le désamorçage est fait en un seul endroit, comme `NkX11Clean.h` le fait pour Xlib devant NKGui. |
| **La partie avance dans le hook overlay du shell** | Si la boucle de jeu vivait dans `OnUI` du plateau, fermer ce panneau arrêterait la partie — et la campagne de mesure avec elle. |
| **Par défaut : IA contre IA, sur les deux sièges** | Le premier jet mettait « humain contre IA ». Au lancement le trait est au joueur 0, donc l'atelier attendait un clic et **ne simulait rien** : vu de l'extérieur, « la simulation ne marche pas ». L'atelier est d'abord un instrument de mesure — par défaut, il mesure. On repasse un siège en *Humain* d'un clic (bouton « Siège » du plateau, ou panneau Joueurs). |
| **Un atelier immobile dit POURQUOI** | `NkcSession::IdleReason()` alimente la barre du plateau : *au tour du joueur humain*, *l'IA réfléchit*, *rejeu en pause*, *IA introuvable*, *partie terminée*. Une interface qui ne bouge pas et se tait se lit comme une panne. Les échecs de démarrage de réflexion sont tracés (`Fail(...)`), jamais avalés. |
| **Pas de barres d'activité** | Les bandes verticales d'icônes de NKEditorKit servent à basculer entre des *vues* d'IDE. L'atelier n'en a aucune : les garder lui donnait le chrome de NKCode sans en avoir le métier. `NkEditorShell::SetActivityBars(false,false)` — ajouté au socle, **par défaut à `true`**, donc NKCode est inchangé. |
| **Charte GitHub Dark Pro** | Décision de Rihen, 2026-08-06 : remplace la charte teal RIHEN de `HANDOFF §2.1`. L'atelier est un outil de développeur, regardé huit heures par jour à côté d'un éditeur de code ; il doit avoir la même température que lui. |

---

## 4. État vérifié

`tests/NkcAbiHarness.cpp`, exécuté le 2026-08-06 : **16 vérifications, 0 échec**.

```
OK  les deux modules se chargent / symboles exportés / versions d'ABI
OK  plateau hexagonal 6×7 = 42 cases, 2 totems par joueur
OK  l'IA ne produit jamais de coup illégal
OK  la partie se termine d'elle-même            → 31 coups
OK  rejeu déterministe : empreinte identique    → 13735550795990014794
OK  « aucun coup légal » ≡ « joueur bloqué »    (REGLES §13)
OK  bornage des paramètres hors plage
OK  le garde-fou max_tours coupe bien la partie
```

Deux enseignements de ce premier run :

- **Le glouton bat l'aléatoire 29–6.** La position se juge, l'évaluation
  matérielle mord.
- **La partie s'arrête à 31 coups, avec 35 totems sur 42 cases** : elle finit par
  *blocage*, bien avant la saturation. C'est le comportement voulu par la v2.0, et
  l'inverse exact de la boucle dégénérée qu'aurait produite la v1.1.

### Reproduire

```sh
CLANG="C:/msys64/ucrt64/bin/clang++.exe"
R="d:/Projets/2026/Nkentseu/Nkentseu"
INC="-I$R/Applications/ConquerorLab/include -I$R/Kernel/Foundation/NKCore/src -I$R/Kernel/Foundation/NKPlatform/src"

$CLANG -shared -std=c++17 -O2 -fPIC $INC -o ConquerorRulesV2.dll $R/Applications/ConquerorLab/modules/rules/ConquerorRulesV2.cpp
$CLANG -shared -std=c++17 -O2 -fPIC $INC -o ConquerorAIRef.dll   $R/Applications/ConquerorLab/modules/ai/ConquerorAIRef.cpp
$CLANG -std=c++17 -O1 $INC -o abitest.exe $R/Applications/ConquerorLab/tests/NkcAbiHarness.cpp
./abitest.exe
```

*Ré-exécuté le 2026-08-06 après l'ajout de l'interface : **16/16, empreinte
identique**. Les modules n'ont pas bougé, mais le revérifier coûte trente
secondes et évite d'attribuer à l'atelier une régression qui n'existe pas.*

---

## 5. L'interface

Six panneaux, tous dockables (NKEditorKit) :

| Panneau | Ce qu'il fait |
|---|---|
| **Plateau** | rendu + picking + surbrillance des coups légaux + halos rouges des totems qui seraient retournés + anneau du dernier coup + bandeau « CASCADE ×N » + bandeau de score + barre d'occupation |
| **Règles** | **auto-généré** depuis `GetParamsSchemaJson`, groupé par `"group"` (int → champ numérique, bool → case, enum → liste) |
| **Joueurs** | Humain / chaque IA détectée par siège, palier de difficulté, budget, graine, cadence, compte-rendu de la dernière réflexion |
| **Modules** | `.cpp` détectés, statut, et **la sortie complète du compilateur** en cas d'erreur |
| **Journal** | liste des coups avec leur empreinte, curseur de rejeu, copie d'une trace rejouable dans le presse-papiers |
| **Métriques** | campagne IA-vs-IA sur *n* threads NKThreading, winrate par camp, avantage du siège 1, parties coupées par `max_tours`, histogrammes d'usage des actions et de durée |

**Lancer :**

```
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target ConquerorLab --config Release
.\Build\Bin\Release-Windows\ConquerorLab\ConquerorLab.exe
```

---

## 6. Ce qui reste à construire

| Pièce | État |
|---|---|
| Contrat (2 headers ABI) | ✅ écrit et **vérifié par exécution** |
| Géométrie + projection écran | ✅ |
| Compilateur / hôte de modules | ✅ écrits, câblés dans l'application |
| Moteur de règles palier 0 | ✅ compile, joue, déterministe |
| IA de référence | ✅ compile, joue, ne triche pas |
| Banc d'essai | ✅ 16/16 |
| Interface (6 panneaux + shell) | ✅ écrite, **compile en Release-Windows** |
| `ConquerorLab.jenga` | ✅ Windows / Linux / macOS / Android / Web, enregistré dans `Nkentseu.jenga` |
| **Validation à l'écran** | ⏳ **non faite** — l'exécutable n'a pas encore été lancé |
| **Linux / Android** | ⏳ non compilés (blocs écrits, jamais exercés) |

> **Honnêteté sur l'état.** Ce qui est marqué ✅ a été **compilé**, et le banc
> d'essai a été **exécuté**. L'interface, elle, n'a pas encore été vue à l'écran :
> cadrage du plateau, lisibilité des halos, comportement du picking et du rejeu
> restent à valider par un vrai lancement. Points les plus probables à reprendre :
> densité du bandeau de score sur petite fenêtre, et cadence de l'IA à haut
> palier (attendre la fin d'une réflexion en cours bloque la frame le temps que
> le budget expire).

Voir [`HANDOFF.md`](HANDOFF.md) pour reprendre.

---

## 7. Documents de référence

Dans `d:/Projets/2026/Game/Conquerror_PREMIUM/Stage/Reference/Conqueror/` :

- `REGLES_COMPLETES_v2.md` — **les règles, source unique de vérité**
- `NOTE_DESIGN_v2.md` — intention, tensions, ce que la simulation ne dira jamais
- `EQUILIBRAGE.md`, `SPIKE_NOGE.md`, `GDD_premium.md`

Et les deux sujets : `Stage/A1_moteur_regles_spread_fusion_pouvoirs.md`,
`Stage/A2_ia_mcts_multithread.md`.

---

© Studio RIHEN — Projet Conqueror Premium.
