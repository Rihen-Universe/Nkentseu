# HANDOFF — reprendre ConquerorLab

Ce fichier contient **le prompt à copier-coller** dans une nouvelle session, plus
la direction artistique à respecter.

> **Mise à jour 2026-08-06 — l'interface est écrite.** Les six panneaux, la
> projection écran, la session de partie, la campagne threadée, `main.cpp` et le
> `.jenga` existent et **compilent en Release-Windows** ; l'application est
> enregistrée dans `Nkentseu.jenga`. Le banc d'essai est toujours à **16/16**,
> même empreinte. **Ce qui n'a pas été fait : lancer l'exécutable.** Utilise donc
> le prompt du §1bis, pas celui du §1 (conservé comme trace de la commande
> d'origine).

---

## 1bis. Le prompt à donner MAINTENANT

```
Reprends ConquerorLab, dans
d:\Projets\2026\Nkentseu\Nkentseu\Applications\ConquerorLab\

LIS D'ABORD, DANS CET ORDRE :
1. Applications/ConquerorLab/README.md        — état, architecture, décisions
2. Applications/ConquerorLab/HANDOFF.md       — ce fichier : la suite + la DA
3. Applications/ConquerorLab/include/Conqueror/ConquerorRulesABI.h
4. Applications/ConquerorLab/include/Conqueror/ConquerorAIABI.h
5. d:\Projets\2026\Game\Conquerror_PREMIUM\Stage\Reference\Conqueror\REGLES_COMPLETES_v2.md
6. Documentation/cours/md/03-nkgui.md   (API NkGui : identité, occlusion, layout)

CE QUI EXISTE ET COMPILE (ne pas réécrire) :
- Contrat (2 headers ABI), géométrie, moteur de règles palier 0, IA de référence,
  compilateur et hôte de modules, banc d'essai à 16/16.
- Projection écran (NkcBoardRender.h), palette (NkcLabTheme.h), primitives de
  dessin (NkcDraw.h), lecture du schéma (NkcParamSchema.h).
- Session de partie (NkcSession.h) : règles + IA sur thread worker + journal +
  rejeu + aperçu des coups PAR SIMULATION.
- Campagne IA-vs-IA multi-thread (NkcBatch.h).
- Six panneaux : Plateau, Règles, Joueurs, Modules, Journal, Métriques.
- main.cpp + ConquerorLab.jenga (Win/Linux/macOS/Android/Web), enregistré dans
  Nkentseu.jenga.

CE QUI RESTE :
a) LANCER ET REGARDER. C'est la tâche principale.
     cd D:\Projets\2026\Nkentseu\Nkentseu
     jenga build --target ConquerorLab --config Release
     .\Build\Bin\Release-Windows\ConquerorLab\ConquerorLab.exe
   À vérifier dans l'ordre : le plateau se cadre et se lit ; un clic sélectionne
   un totem et montre ses destinations ; survoler une destination allume les
   halos rouges ; l'IA joue ; le journal rejoue ; le panneau Règles reflète les
   valeurs bornées par le module ; une campagne de 200 parties se termine.

b) CORRIGER CE QUI SE VOIT. Points déjà identifiés comme fragiles :
   - densité du bandeau de score sur fenêtre étroite ;
   - à haut palier, attendre la fin d'une réflexion en cours bloque la frame le
     temps que le budget expire (NkcSession::WaitThinking) ;
   - le repli automatique des panneaux sous 900 px n'a jamais été vu à l'œuvre.

c) VÉRIFIER LES AUTRES CIBLES : jenga build --platform linux, puis android.
   Le bloc Android est écrit mais n'a jamais été exercé.

d) Quand tout tient : déposer un .cpp volontairement cassé dans
   Build/ConquerorLab/rules/ et vérifier que l'erreur de compilation s'affiche
   bien dans le panneau Modules — c'est le seul retour que le stagiaire aura.

RÈGLES DE TRAVAIL NON NÉGOCIABLES :
- Types Nkentseu partout (uint32, int8, usize, float32, float64) — jamais uint32_t.
- Tout ce qui existe dans Nkentseu vient de Nkentseu (NkString, NkVector,
  NkUniquePtr, NkFile, NkDirectory, math::). stdint/STL uniquement si Nkentseu
  ne le définit pas.
- NkGui : écrire dans ctx.DL(), jamais ctx.dl. Picking par ctx.InputHits/ClickIn,
  jamais NkGuiRectContains brut. Une seule découpe pour tout le plateau.
- Inclure ConquerorLab/NkcWinClean.h AVANT tout header NKFileSystem : <windows.h>
  transforme GetFreeSpace/DeleteFile/GetCurrentDirectory en macros qui réécrivent
  les déclarations du moteur, et l'erreur apparaît très loin de la cause.
- L'application ne doit RIEN savoir des règles : tout passe par la vtable. Pour
  montrer ce qu'un coup ferait, on SIMULE (clone + ApplyMove + événements), on ne
  déduit jamais.
- Cible : PC (Windows/Linux) ET Android, tous deux fonctionnels.

L'INTERFACE DOIT ÊTRE BELLE. La direction artistique est dans HANDOFF.md §2 :
respecte-la, ne produis pas une UI grise par défaut.
```

---

## 1. Le prompt d'origine (archive — la construction est faite)

> Conservé pour mémoire : c'est la commande qui a produit l'atelier.
>
> **Deux affirmations y sont devenues fausses**, et il est plus honnête de le
> signaler que de réécrire l'histoire :
>
> - « les modules compilent avec 3 -I seulement, sans lier le moteur » — ils ont
>   désormais **toute la pile sous NKCanvas**, liée automatiquement ;
> - « `NkRound` est un symbole lié, l'inclure casserait l'édition de liens » —
>   NKMath est maintenant disponible aux modules. La projection écran reste
>   pourtant côté atelier, mais pour la **bonne** raison : une règle de jeu ne
>   connaît pas les pixels.

```
Reprends le développement de ConquerorLab, dans
d:\Projets\2026\Nkentseu\Nkentseu\Applications\ConquerorLab\

LIS D'ABORD, DANS CET ORDRE :
1. Applications/ConquerorLab/README.md        — état, architecture, décisions
2. Applications/ConquerorLab/HANDOFF.md       — ce fichier : la suite + la DA
3. Applications/ConquerorLab/include/Conqueror/ConquerorRulesABI.h
4. Applications/ConquerorLab/include/Conqueror/ConquerorAIABI.h
5. d:\Projets\2026\Game\Conquerror_PREMIUM\Stage\Reference\Conqueror\REGLES_COMPLETES_v2.md
6. Documentation/cours/md/03-nkgui.md et 02-nkcanvas.md   (API NkGui / NkCanvas)

CE QUI EXISTE ET EST VÉRIFIÉ (ne pas refaire) :
- Les deux headers ABI, la géométrie, le moteur de règles palier 0, l'IA de
  référence, le compilateur de modules, l'hôte de modules, le banc d'essai.
- Le banc d'essai passe 16/16 : chargement, partie complète, rejeu déterministe,
  équivalence blocage/absence de coup légal, bornage des paramètres.
- Les modules compilent avec 3 -I seulement, sans lier le moteur.

CE QU'IL RESTE À ÉCRIRE :
a) src/ConquerorLab/NkcBoardRender.h
   Projection écran retirée de la géométrie (elle en a été sortie exprès : NkRound
   de NKMath est un symbole lié, l'inclure dans le contrat casserait l'édition de
   liens des modules). Y mettre CoordToPixel, PixelToCoord (arrondi cube pour
   l'hex), CellPolygon, et le calcul de cadrage automatique du plateau.

b) src/ConquerorLab/ — les panneaux NKEditorKit (dériver NkEditorPanel, implémenter
   OnUI(NkEditorFrameContext&)) :
   - Plateau      : rendu + picking + surbrillance des coups légaux + dernier coup
   - Règles       : panneau AUTO-GÉNÉRÉ depuis GetParamsSchemaJson (int → DragInt,
                    bool → Checkbox, enum → BeginCombo), groupé par "group"
   - Joueurs      : combo Humain / chaque IA détectée, par joueur, + palier de difficulté
   - Modules      : liste des .cpp détectés, statut, et LA SORTIE DU COMPILATEUR
                    en cas d'erreur (c'est le seul retour que le stagiaire aura)
   - Journal      : liste des coups + curseur de rejeu
   - Métriques    : lanceur de batch IA-vs-IA sur thread NKThreading, avec
                    PlotHistogram de l'usage des actions et winrate
   c) src/ConquerorLab/main.cpp — NkEditorShell, alloué SUR LE TAS
     (memory::NkMakeUnique — le shell fait > 1 Mo, sur la pile c'est un stack overflow)

d) ConquerorLab.jenga — modèle : Applications/NKEditorKitDemo/NKEditorKitDemo.jenga
   pour la structure, et Applications/Mou/Mou.jenga pour le bloc system:Android.
   Compiler modules/rules/*.cpp et modules/ai/*.cpp DANS l'app avec
   -DNKC_RULES_STATIC=1 et -DNKC_AI_STATIC=1 (indispensable sur Android).
   Dépendances : NKEditorKit, NKGui, NKCanvas, NKImage, NKFont, NKWindow, NKEvent,
   NKFileSystem, NKThreading, NKContainers, NKMemory, NKCore, NKMath, NKLogger, NKTime.

e) Vérifier que ça compile et tourne. Le README documente comment reproduire le
   banc d'essai.

RÈGLES DE TRAVAIL NON NÉGOCIABLES :
- Types Nkentseu partout (uint32, int8, usize, float32, float64) — jamais uint32_t.
- Tout ce qui existe dans Nkentseu vient de Nkentseu (NkString, NkVector,
  NkUniquePtr, NkFile, NkDirectory, math::). stdint/STL uniquement si Nkentseu
  ne le définit pas.
- NkGui : écrire dans ctx.DL(), jamais ctx.dl. Picking par ctx.InputHits/ClickIn,
  jamais NkGuiRectContains brut. PushId par cellule. Une seule découpe pour tout
  le plateau (chaque SetClip coûte un draw call).
- L'application ne doit RIEN savoir des règles : tout passe par la vtable.
- Cible : PC (Windows/Linux) ET Android, tous deux fonctionnels.

L'INTERFACE DOIT ÊTRE BELLE. La direction artistique est dans HANDOFF.md §2 :
respecte-la, ne produis pas une UI grise par défaut.
```

---

## 2. Direction artistique — l'interface doit être belle

C'est une exigence, pas un souhait. L'atelier sera regardé tous les jours pendant
huit semaines par deux stagiaires, et montré à des tiers.

> ### ⚠️ §2.1 EST PÉRIMÉ — charte remplacée le 2026-08-06
>
> **Décision de Rihen : l'atelier passe en GitHub Dark Pro**, pas en teal RIHEN.
> Raison : c'est un outil de développeur, regardé à côté d'un éditeur de code ; il
> doit avoir la même température que lui, pas celle d'une plaquette de studio.
> La palette vit dans `src/ConquerorLab/NkcLabTheme.h` — un seul endroit, comme
> l'exige §2.4, qui reste valable.
>
> | Rôle | Couleur |
> |---|---|
> | fond application | `#0D1117` |
> | surfaces / panneaux | `#161B22` |
> | creux (pistes, barre d'onglets) | `#010409` |
> | bouton | `#21262D` · survol `#30363D` · pressé `#1F6FEB` |
> | bordure | `#30363D` |
> | texte | `#E6EDF3` · atténué `#8B949E` |
> | accent | `#58A6FF` |
> | joueurs 0-3 | `#58A6FF` · `#DB6D28` · `#3FB950` · `#A371F7` |
> | coup légal | `#3FB950` · menace `#F85149` · dernier coup `#D29922` |
>
> **Deuxième décision du même jour : pas de barres d'activité.** Les bandes
> verticales d'icônes de NKEditorKit donnaient à l'atelier le chrome de NKCode
> sans qu'il en ait le métier. `SetActivityBars(false, false)` a été ajouté au
> socle (par défaut `true`, donc NKCode est inchangé).
>
> Tout le reste de §2 — le plateau est le héros, lisibilité de l'état, ce qu'il ne
> faut pas faire, adaptation Android — **reste en vigueur**.

### 2.1 Palette — charte RIHEN *(archive — remplacée, voir l'encadré ci-dessus)*

Reprise des documents PDF du studio. Écraser `ctx.theme` après `Init` :

| Rôle | Couleur | Usage |
|---|---|---|
| `bgPrimary` | `#0B2229` | fond de l'application, teal très sombre |
| `panel` | `#12313A` | fond des panneaux |
| `header` | `#17404B` | barres de titre |
| `button` | `#1D4E5A` | boutons au repos |
| `buttonHover` | `#276A79` | survol |
| `buttonActive` | `#E8973F` | **accent orange RIHEN** — pressé, sélection |
| `accent` | `#E8973F` | filets, valeurs mises en avant |
| `border` | `#1F5A68` | bordures |
| `text` | `#E8F1F3` | texte principal |
| `textDisabled` | `#6E8C95` | texte inactif |
| `track` | `#0A1D23` | fonds de sliders, zones creuses |

`rounding = 6`, `framePadX = 12`, `framePadY = 7`.

### 2.2 Le plateau est le héros

Il occupe le centre, il est grand, et il est le seul élément coloré vivement.

- **Cadrage automatique** : calculer la boîte englobante des cases et zoomer pour
  remplir le panneau avec une marge de 8 %. Recalculer au redimensionnement.
- **Cellule vide** : contour `#1F5A68` 1,5 px, remplissage `#0E2A32`.
- **Cellule bloquée** : remplissage `#071619`, contour éteint.
- **Totem joueur 0** : `#4FB3C7` (cyan). **Joueur 1** : `#E8973F` (orange).
  Joueurs 2 et 3 : `#8FCB6D` (vert) et `#C77DD4` (violet). Ces quatre teintes se
  distinguent aussi en niveaux de gris — vérifie-le.
- **Niveau du totem** : le disque grossit légèrement (N0 = 0,52 × rayon, +0,06 par
  niveau) et gagne un liseré clair. Ne jamais coder le niveau par la seule couleur.
- **Coups légaux** : quand une source est sélectionnée, les destinations reçoivent
  un anneau `#8FCB6D` à 60 % d'alpha. Les ennemis qui seraient retournés reçoivent
  un halo rouge `#E86A5A` — **c'est la lecture tactique centrale du jeu**
  (« quelle surface de contact suis-je en train d'offrir ? », NOTE_DESIGN §2).
- **Dernier coup** : anneau orange qui pulse ~1,2 s puis s'éteint.
- **Cascade** : quand 2 totems ou plus basculent d'un coup, afficher « CASCADE ×N »
  en gros, centré, qui monte et s'efface en 900 ms. C'est le sommet émotionnel du
  jeu (PXG 1) — il doit se voir.

### 2.3 Lisibilité de l'état

- Un **bandeau de score** en haut du plateau : une tuile par joueur, pastille de
  couleur + nombre de totems en gros + énergie et PC en petit. Le joueur au trait
  a sa tuile soulignée d'un filet orange.
- Barre de progression de l'occupation du plateau (cases prises / 42).
- Quand la partie est finie : bandeau centré, sobre, « Vainqueur : Joueur 1 » ou
  « Match nul », avec le décompte.

### 2.4 Ce qu'il ne faut pas faire

- Pas d'emoji dans l'interface.
- Pas de couleur en dur dans le code des panneaux : tout passe par `ctx.theme`
  ou par une petite palette nommée déclarée en un seul endroit.
- Pas de texte brut là où une tuile ou une jauge dit mieux (les métriques
  surtout : `PlotHistogram` et `PlotLines` existent, s'en servir).
- Pas de panneau qui déborde : `CurrentClip()` donne le visible,
  `AvailHeight()` donne le contenu (≈ 1e9 en zone défilable) — toujours
  intersecter les deux, sinon la barre de défilement part à un million de pixels.

### 2.5 Adaptation Android

- `SetUiScale(ctx, s)` selon la densité, **et recharger la police** à
  `taille × s` — sinon la mise en page grossit et le texte reste petit.
- Cibles tactiles ≥ 44 px logiques : sur téléphone, les cases hexagonales doivent
  avoir un rayon minimal, quitte à rendre le plateau défilable.
- Panneaux latéraux repliés par défaut en dessous de 900 px de large ; le plateau
  garde la totalité de l'écran.

---

## 3. Ordre de travail — état au 2026-08-06

1. ~~`NkcBoardRender.h` + panneau Plateau~~ — **écrit**
2. ~~Panneau Modules (statut + erreurs de compilation)~~ — **écrit**
3. ~~Panneau Règles auto-généré~~ — **écrit**
4. ~~Panneau Joueurs (humain vs IA)~~ — **écrit**
5. ~~Journal + rejeu~~ — **écrit**
6. ~~Métriques + batch threadé~~ — **écrit**
7. ~~`.jenga`~~ — **écrit, compile en Release-Windows**
8. **Valider à l'écran** — ⏳ pas encore fait, c'est la suite immédiate
9. **Vérifier Linux, puis Android** — ⏳ blocs écrits, jamais exercés

### Ce que la DA §2.5 attend encore d'Android

Le rayon minimal des cases est en place (`FitBoard(..., minCell)`), et les
panneaux se replient sous 900 px. Restent à faire, **quand un appareil sera sous
la main** : l'appel à `SetUiScale` avec rechargement de la police à `taille × s`
(le shell gère son DPI seul, ses polices sont privées — il faudra passer par
`NkFontPrefs` plutôt que par `SetUiScale` brut), et le plateau défilable quand le
rayon minimal fait déborder la grille.

---

© Studio RIHEN — Projet Conqueror Premium.
