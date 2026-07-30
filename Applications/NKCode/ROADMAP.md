# NKCode — Roadmap

> Feuille de route par phases. Esprit : **from scratch, incrémental, un jalon observable par
> phase**. On bâtit d'abord la valeur la plus sûre (éditeur texte + build Jenga), puis les couches
> visuelles, puis le polissage. Cible d'itération : desktop (Windows/Linux/macOS) d'abord, le moteur
> étant déjà cross-plateforme.

Légende : ✅ fait · 🟡 partiel · ⬜ à faire.

---

## 🧩 Widgets réutilisables — OÙ ça vit (cartographie, décidée 2026-07-12)

**Constat** : plusieurs widgets ont été RÉIMPLÉMENTÉS à l'app (bugs de traversée
d'événements, duplication : DEUX sélecteurs de dossier). Le moteur possède déjà
les PRIMITIVES. Règle : **ne plus réimplémenter — réutiliser/consolider**.

### Ce qui existe DÉJÀ dans NKGui (`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h`) — primitives bas niveau, thémées :
- **Menus/popups** : `BeginPopupMenu`/`EndPopupMenu`, `OpenPopupAt`, `MenuItem`,
  `BeginMenu`, `Separator`, `BeginCombo`. Pile de popups avec **occlusion d'input**
  (`popupRects`/`popupDepth`, respectée par `ItemHoverable`).
- **Champs texte** : `InputText`, `InputTextEx`, `InputTextMultiline`.
- **Listes** : `Selectable`, `SelectableEditable`, `ListBox`.
- Thème = `NkGuiTheme` (couleurs) + `NkGuiSyntax` → **personnalisable** par l'utilisateur.

### Réimplémentations APP à retirer/migrer (dette) :
- `NkCtxMenuDraw` (`Editor/NkTextDraw.h`) → menu ad hoc, NE registre PAS dans la
  pile de popups → **traverse les événements**. À remplacer par un MANAGER de menu.
- `NkOwEdit`/`NkOwEditA` (`Shell/NkOpenWs.h`) → champs texte ad hoc → utiliser `InputText*`.
- **DEUX sélecteurs de dossier** : `NkOpenWsPanel` (`Shell/NkOpenWs.h`, launcher) +
  picker `pickerTree` (`Shell/Dialogs.h`, SaveAs). À UNIFIER en un seul.

### DÉCISION — où vivent les OUTILS réutilisables (managers) :
- **NKEditorKit** (`Engine/NKEditorKit/`) : gestionnaires qui doivent être dessinés
  AU-DESSUS des panneaux et gérer l'occlusion cross-panneaux (rôle du shell) :
  - `NkPopupManager` — menu contextuel/déroulant demandé par un panneau, dessiné
    par le shell APRÈS tous les panneaux, input réel, occlusion via le mécanisme
    modal existant. **1 seul menu, thémé, personnalisable.**
  - `NkModalManager` / fenêtres FLOTTANTES ou ANCRÉES (non modales) — cadre
    déplaçable réutilisable (barre de titre + ✕ + drag), au choix flottant/docké.
  - `NkFilePicker` — sélecteur de fichier ET dossier UNIQUE (mode file/folder),
    bâti sur les primitives NKGui, présentable en fenêtre flottante ou panneau
    docké. Absorbe `NkOpenWsPanel` + le picker SaveAs (a besoin de NKFileSystem,
    que NKGui n'a PAS → ne peut PAS vivre dans NKGui).
- **NKGui** : reste la couche de PRIMITIVES (menus/texte/listes/occlusion). Les
  managers NKEditorKit l'utilisent pour dessiner.

**Personnalisation** = uniquement design + couleurs (thème NKGui + hooks de style
des managers) ; la logique est unique et partagée.

**Plan de migration (phasé, faible risque)** : (1) `NkPopupManager` shell-level +
migrer le menu de l'explorateur (corrige la traversée) ; (2) `NkFilePicker` unifié
(remplace NkRootPicker + les 2 pickers) ; (3) champs texte → `InputText*` ;
(4) retrait de la dette (`NkCtxMenuDraw`, `NkOwEdit`).

### ✅ Dette traitée (2026-07-12) :
- **`NkCtxMenu` + `NkCtxMenuDraw` → `Engine/NKEditorKit/src/NKEditorKit/NkEditorContextMenu.h`**
  (commit `4657771`). Widget moteur réutilisable (scroll V/H, sous-menus, thème,
  **occlusion « modal léger »** : consomme le clic quand la souris est dedans — la
  « traversée d'événements » était déjà réglée). 8 appelants NKCode ré-exportés.
- **`NkDirBrowserState` → `NkDirBrowser.h`** (commit `2c06612`) : cœur de navigation
  du launcher (curDir + historique + dossiers connus) ; `NkOpenWsState` en dérive.
- **Retrait dette morte** (commit `c651b9b`) : picker dormant du shell + `NkRootPicker.h`.
- **⏳ `NkOwEdit` — À FUSIONNER (tâche dédiée)** : champ PLUS riche que
  `NkOverlayTextField` (masquage mot de passe, `leftPad` icône, caret par-champ, menu
  contextuel clic-droit `NkTxtMenu` intégré) et couplé à `NkUi`. Le remplacer
  régresserait des features → il faut FUSIONNER les deux en un seul champ moteur
  (porter hors `NkUi`, absorber masked/leftPad + menu). Mini-projet à part, non fait.

### ✅ AVANCEMENT (2026-07-12) — briques posées dans NKEditorKit :
- **`Engine/NKEditorKit/src/NKEditorKit/NkEditorTextField.h`** — `NkOverlayTextField`
  (champ mono-ligne complet : caret, sélection, copier/couper/coller, double-clic)
  DÉPLACÉ de l'app → widget moteur réutilisable. NkTextDraw.h le ré-exporte pour
  les 23 appelants historiques. Commit `3b6cd29`.
- **`Engine/NKEditorKit/src/NKEditorKit/NkFilePicker.h`** — `NkFilePickerState` =
  **cœur RÉUTILISABLE du picker** (extraction phase 1) : ÉTAT (arborescence, chemin,
  scroll, fenêtre déplaçable, menu/renommage) + NAVIGATION filesystem (BuildPickerTree,
  TogglePickerNode, PickerCreateFolder, PickBeginRename/Commit, PickDelete,
  OpenPickerBase, ScanPickerFiles, PickerGoto/Up/Enter/Cancel). **Zéro dépendance
  NkCodeState** (comparaison de chemins `PathIsAncestor` inlinée). `NkCodeDialogs`
  en **dérive** (`: public NkFilePickerState`) et SPÉCIALISE : le RENDU
  (`DrawFolderPicker`), les ACTIONS de confirmation (`PickerConfirm` → DoLoad /
  wsDir / loadDir / pickedFolder), l'assistant de **scaffolding C++** (`newFile`,
  `scafKind`, `DoScaffoldCreate`, `GenCode`) — 100 % NKCode. Logique DÉPLACÉE (pas
  réécrite) → comportement identique. Commit `38b21a6`.
- **✅ PHASE 2 (2026-07-12, commit `a205f03`)** — le RENDU est monté dans le moteur :
  `NkDrawFilePicker(ctx, NkFilePickerState&, NkFilePickerStyle&)` dans
  `NkFilePicker.h`. Frame modal + barre de titre déplaçable + champ chemin + arbre +
  scrollbars V/H + création de dossier + liste de fichiers + menu contextuel
  (nouveau/renommer/supprimer) — **tout générique**. Confirmation par **RÉSULTAT**
  (`fp.pickerConfirmed`/`pickerCancelled` + `pickerResult*`) : l'app poll et route.
  **`NkFilePickerStyle`** = TOUTES les couleurs → personnalisation = design/couleurs
  (l'app passe scrollbars theme-aware). Spécialisation NKCode via **surcharges
  virtuelles** (`NkFilePickerState` est polymorphe) : `PickerWindowHeight`,
  `PickerBottomReserve`, `PickerExtraHeight`, `DrawPickerExtra` (assistant scaffolding
  C++ = SEUL morceau NKCode restant, dessiné dans la région app), `PickerConfirmLabel`,
  `PickerConfirmEnabled`, `PickerClearExtraFocus`. NKCode `DrawFolderPicker` = wrapper
  ~25 lignes (style + routage `DoLoad`/`DoSaveHere`/`DoScaffoldCreate`/`RoutePickerResult`).
- **RESTE (phase 3)** : absorber `NkOpenWsPanel` (couche workspace .jenga restant côté
  NKCode) dans le picker moteur ; factoriser le cadre modal déplaçable (barre de titre
  + drag, dupliqué) en widget `NkModalFrame` ; retirer la dette (`NkRootPicker`,
  `NkCtxMenuDraw`, `NkOwEdit`).

> ### 📣 RÈGLE PERMANENTE — Communiquer CHAQUE évolution (depuis 2026-07-05)
> Toute évolution notable de NKCode (feature livrée, jalon, fix visible) doit produire,
> **en plus du code** : (1) des **publications réseaux** (LinkedIn FR+EN, X, Facebook,
> Instagram, TikTok) avec **captures image/vidéo réelles** (architecture, raisonnement,
> rendu réel, résultats), et (2) un **article scientifique** publiable. Tout va dans
> `D:\Rihen\Rodolf\Publications\NN_AAAA-MM-JJ_sujet/`, en suivant **strictement**
> `D:\Rihen\Rodolf\CLAUDE.md` (ton **humble/en demande d'aide**, garde-fous honnêteté,
> charte Rihen). Cf. règle globale dans le `CLAUDE.md` du dépôt.
> Fait à ce jour : **diagnostics temps réel + grisage préproc + Ctrl+clic** →
> `Publications/09_2026-07-05_nkcode-diagnostics/`.

---

## ✅ Déjà implémenté (au 27 juin 2026)
IDE **fonctionnel**, bâti sur **NKGui** (réécriture UI) + **NKEditorKit** :
- ✅ **Fenêtre sans décoration OS** (borderless dès le lancement) + **barre de titre custom**
      (logo + menus + infos centrées + min/max/close) + redimensionnement par les bords + bordure.
- ✅ **Docking** des panneaux (dock de bord enveloppant la racine → bas pleine largeur,
      droite pleine hauteur ; nœud à 1 panneau sans barre d'onglets) + **palette de commandes** + raccourcis.
- ✅ **Barre d'outils façon Visual Studio** (centrée) : Construire / Démarrer + sélecteur de
      **tous** les projets du workspace + config (Debug/Release) + plateforme (+ appareil mobile).
- ✅ **Éditeur de code** : tampon par lignes, curseur/sélection souris+clavier, scroll V/H, interligne,
      **coloration** (C/C++, Python, NKSL, Markdown), **formatage** (Ctrl+L), **onglets** de fichiers.
- ✅ **Explorateur** en arbre repliable (charge la racine Nkentseu).
- ✅ **Build + Run Jenga** asynchrones + **panneau Sortie** + **Terminal** intégré réaliste.
- ✅ **Activity bar** + **footer** (Ln/Col, langage).

Reste (par phases ci-dessous) : undo/redo, recherche/remplacement, créer un projet, parse d'erreurs,
puis **Graph → Blueprint → Codegen → UIBuilder → Blocks → Extensions → Agents**.
Détail technique granulaire : `Kernel/Runtime/NKUI/ROADMAP_UI_REWRITE.private.md`.

> ### 🧭 ORDRE VALIDÉ PAR RIHEN (2026-07-11) — éditeur puis systèmes
> 1. **Lot final éditeur (sans dépendance)** : word wrap (Alt+Z), **recherche workspace**
>    (Ctrl+Maj+F, panneau résultats + remplacement multi-fichiers), **quick fix** (Ctrl+.)
>    sur les diagnostics nommés (« expected ';' », include manquant) ; option : split éditeur,
>    F2 renommage textuel (aperçu avant application).
> 2. **Système LSP clangd** (JSON-RPC + processus + synchro buffers) → retour éditeur :
>    références/renommage SÉMANTIQUES, refactorings, hover/complétion exacts (templates).
>    Le compile-first actuel reste le REPLI sans clangd.
> 3. **Système débogueur** (gdb/lldb, MI ou DAP) → retour éditeur : breakpoints FONCTIONNELS,
>    ligne d'exécution, valeurs inline, pas-à-pas (la commande `jenga gdb/debug` existe).
> 4. **Service IA de complétion** (étendre NkAi : async/stream) → ghost text + [IA : expliquer].
> 5. **Runner de tests Unitest** (découverte/exécution/parse) → gouttière ▶ + panneau résultats.
> 6. **Terminal** : shell par défaut configurable + profils (système PTY).
> 7. **Installateur d'outils INTÉGRÉ** (souhait Rihen 2026-07-12) : au premier lancement ou à la
>    demande, NKCode DÉTECTE ce qui manque (compilateur, clangd, SDK/NDK, emsdk, JDK, débogueur…)
>    et propose l'installation AUTOMATIQUE **au choix de l'utilisateur**, par cas d'usage
>    (ex. « C++ desktop » → clang+clangd+gdb ; « Android » → SDK/NDK/JDK). S'appuie sur les
>    gestionnaires existants (pacman msys2, winget, sdkmanager) + la page wiki Jenga
>    « Installation des outils ». Précédent concret : clangd installé via pacman pour le LSP.

---

## Phase 0 — Mise en place ✅
- ✅ `NKCode.jenga` (windowedapp, dépendances NKEditorKit/NKGui/NKCanvas/NKFont/NKEvent/NKFileSystem…).
- ✅ Coquille minimale qui ouvre une fenêtre (via **NKEditorKit** sur **NKGui**).
- ✅ Namespace `nkentseu::nkcode`, arborescence `src/NKCode/` (Editor/Project/Shell + dossiers à venir).

## Phase 1 — La coquille (Shell) ✅
- ✅ Boucle app + thème (**VSCode Dark+**).
- ✅ **Layout / docking** des panneaux (éditeur, explorateur, sortie, terminal) — dock de bord externe
      enveloppant la racine ; nœud à 1 panneau sans barre d'onglets.
- ✅ **Palette de commandes** (Ctrl+P) + raccourcis (Ctrl+B/R/S/L/Q…).
- ✅ 🎯 **Jalon** : fenêtre IDE avec panneaux dockables et palette de commandes.

## Phase 2 — L'éditeur de texte (Editor) 🟡
- ✅ Tampon de texte + **undo/redo PAR FICHIER** (snapshots coalescés, Ctrl+Z / Ctrl+Maj+Z / Ctrl+Y).
- ✅ Rendu + gouttière (numéros de ligne) + curseur/sélection + interligne (police proportionnelle,
      pas encore monospace).
- ✅ **Coloration syntaxique** (C/C++, Python, NKSL, Markdown) + **recherche/remplacement** (Ctrl+F/H).
- ✅ **Coloration sémantique** (types/fonctions) niveau **fichier + projet** (index async).
- ✅ **Diagnostics C/C++ temps réel** (juil. 2026) — **compile-first, sans clangd** : compile le buffer
      via le **compilateur cible** (`-fsyntax-only`, fichier temp frère, débounce ~0,6 s, sans save),
      avec les flags **par projet** d'une base **`.jenga/compileflags.jcdb`** (commande Jenga
      `compile-flags`, régénérée au reload d'un `.jenga`). Rendu : **souligné rouge ondulé** +
      **marqueur gouttière** (numéro rouge + pastille) + message **Error-Lens** en fin de ligne.
- ✅ **Grisage des branches préprocesseur inactives** (`#if/#else` non pris atténués) via l'ensemble
      **effectif** des macros du compilateur (`-dM -E`, cache par projet) — branche active nette.
- ✅ **Ctrl+clic navigation** : ouvrir un `#include` (sync) / **aller à la définition** (sur un **thread**,
      **barre de progression** + **liste de toutes les occurrences** façon VSCode ; scan borné anti-freeze).
- ✅ **Autocomplétion** (popup façon VSCode) : symboles fichier + projet + mots-clés du langage, filtrés
      par préfixe ; ↑↓ naviguer, Tab/Entrée accepter, Échap fermer. ✅ **Contextuelle** (`.`/`->`/`::`)
      en 2 temps : heuristique instantanée (tables workspace) + **compilateur réel** (`-code-completion-at`
      + préambule **PCH** façon clangd) quand il répond.
- ✅ **Zoom éditeur** : Ctrl+molette / Ctrl+= / Ctrl+- **par onglet** (taille propre à chaque fichier,
      persistée en session) + **terminal** (zoom au survol, atlas séparé) + **cache d'atlas par taille**
      (revenir sur un onglet zoomé = instantané, plus de « saut » de taille).
- ✅ Ouvrir/sauver des fichiers (NKFileSystem) ; **onglets** custom (point modifié, fermeture).
- ✅ **Éditeur avancé (PRs #31→#35, juil. 2026)** : repli de code (fold) · **minimap** (sliding-window,
      par-caractère) · **hover documentation** (prototypes complets, macros AVEC expansion des arguments,
      genres réels struct/class/union/enum/namespace, cartes erreurs/warnings, scroll/glisser, boutons
      [Aller à la définition][Copier][Références], **Ctrl+K Ctrl+I au clavier**) · **F8/Maj+F8** diag
      suivant/précédent · **Ctrl+G** aller à la ligne · **F12 / Maj+F12 références** (occurrences hors
      commentaires/chaînes) · **multi-carets** (Ctrl+D, Ctrl+Maj+L toutes occurrences) · aide aux
      paramètres (Ctrl+Maj+Espace) · chords **Ctrl+K** (0/J/I, guide au footer) · onglets **MRU**
      (Ctrl+Tab), Ctrl+W, **Ctrl+Maj+T rouvrir**, drag réordonner · marques scrollbar (diags/recherche/
      breakpoints) · caret clignotant · surbrillance des occurrences de la sélection · **diagnostics
      MULTI-PASSES** (« expected ';' » → relance sur copie patchée : 1 erreur avant, 16 révélées sur le
      cas réel) · point « modifié » fidèle à l'undo (hash vs état sauvegardé) · antislash ISO ·
      Ctrl+Maj+O panneau Structure (DockFocusWindow NkGui) · session/hot-exit · git gutter · zoom.
- ✅ 🎯 **Jalon** : éditer et sauver un fichier `.cpp`/`.md` avec coloration.

## Phase 3 — Intégration Jenga (Project) 🟡
- ⬜ **Créer un projet** depuis un modèle (génère le `.jenga` + l'arbo).
- ✅ Explorateur de fichiers du workspace (**arbre repliable**, charge la racine Nkentseu).
- 🟡 **Build / Run** via la CLI Jenga (ASYNC, sélecteur de **tous** les projets, config, plateforme)
      + **panneau de sortie** **faits** ; **parse erreurs → clic = aller à la ligne** à faire.
- ✅ 🎯 **Jalon visible** : éditer, **builder et lancer** un projet depuis NKCode.

## Phase 4 — Le moteur de graphe (Graph) ⬜  ← PROCHAIN

> ### ⚠️ COORDINATION INTER-AGENTS — DÉCISION VALIDÉE PAR RIHEN (2026-07-09), À LIRE AVANT DE CODER LA PHASE 4
> Le substrat de graphe de nodes est désormais **UNIQUE et partagé** dans tout l'écosystème :
> spec de référence → **`Kernel/Runtime/NKGraph/ROADMAP.md`** (architecture 3 couches).
> Consommateurs prévus : NKCode (Blueprint/Blocks/Agents), graphe de **matériaux** (NKRenderer
> Phase T.2), graphe **VFX** (Noge), **procédural** (Kernel/AI), **anim graphs** (NkAnima M2).
> Concrètement pour cette Phase 4 — **ne PAS construire le substrat dans `src/NKCode/Graph/`** :
> 1. Le **modèle** (nœud/broche typée/lien, tri topologique, sérialisation `.nkgraph`,
>    undo/redo) s'implémente dans **`Kernel/Runtime/NKGraph`** (deps : Foundation +
>    NKSerialization/NKReflection SEULEMENT, zéro type métier dans le cœur).
> 2. Le **canvas** (pan/zoom, fils, sélection, recherche) s'implémente comme **widget
>    réutilisable dans NKEditorKit** (NKCode en est déjà client).
> 3. NKCode garde chez lui ce qui lui est propre : **bibliothèques de nœuds** Blueprint/Blocks,
>    palette auto-générée NKReflection (Phase 5), **codegen/VM** (Phase 6), Agents (nœuds).
> Bénéfice direct : les jalons Phase 4/5 restent identiques, mais le travail sert aussi les
> 4 autres consommateurs (et l'API sera généralisée par le 2e consommateur — règle des deux).
> En cas de doute ou de friction d'API : en parler à Rihen AVANT de diverger.

- ⬜ Modèle **nœud / broche / lien** + undo/redo → **dans `Kernel/Runtime/NKGraph`** (cf. note).
- ⬜ **Canvas** : pan/zoom, sélection, **tirer un lien** entre broches, déplacer des nœuds
      → **widget NKEditorKit** (cf. note).
- ⬜ **Sérialisation** des graphes (NKSerialization / NKReflection) → format **`.nkgraph`** (NKGraph).
- ⬜ 🎯 **Jalon** : poser des nœuds, les relier, sauver/recharger le graphe (inchangé — mais le
      moteur vit dans NKGraph/NKEditorKit, NKCode est son 1er client).

## Phase 5 — Blueprint (nœuds typés) ⬜
- ⬜ **Broches typées** + règles de connexion ; **flux d'exécution** (exec) + **flux de données**.
- ⬜ Palette de nœuds — idéalement **auto-générée depuis NKReflection** (fonctions/types du moteur).
- ⬜ Nœuds de base (variables, branches, boucles, appels de fonction, événements).
- ⬜ 🎯 **Jalon** : un graphe Blueprint cohérent et typé.

## Phase 6 — Codegen (le visuel s'exécute) ⬜
- ⬜ **Compilation** Blueprint → code texte (C++ ou script).
- ⬜ Brancher le code généré dans un projet Jenga → **build & run**.
- ⬜ 🎯 **Jalon « waouh »** : un graphe Blueprint **se compile et s'exécute** depuis NKCode.

## Phase 7 — UIBuilder (interfaces par glisser-déposer) ⬜
- ⬜ **Palette de widgets** + **canvas de conception** (poser/déplacer/redimensionner, guides, magnétisme).
- ⬜ **Arbre de hiérarchie** + **inspecteur de propriétés** (via **NKReflection**).
- ⬜ **Layout responsive** (ancres, flex/grille, safe-area) ; aperçu fidèle (NKGui = WYSIWYG).
- ⬜ **Sérialisation** `.nkui` (NKSerialization) + **codegen** vers NKGui (ou chargement runtime).
- ⬜ **Liaison événements → logique** (Graph/Blueprint/Blocks ou code).
- ⬜ 🎯 **Jalon** : dessiner un écran à la souris, le sauver, le générer/charger et brancher un bouton.

## Phase 8 — Blocks (façon Scratch) ⬜
- ⬜ **Blocs emboîtables** (snapping) + catégories + palette, sur le substrat Graph.
- ⬜ Codegen Blocks → script lisible.
- ⬜ 🎯 **Jalon** : un programme par blocs qui tourne.

## Phase 9 — Polissage & au-delà 🟡
- ✅ Multi-curseurs, repli de code, minimap (PRs #31-35) ; thèmes (**VSCode Dark+ fait**).
- ⬜ **Zoom global de l'UI** (explorateur, panneaux, chrome) au survol des zones **non-code** — met à
      l'échelle la police d'interface (le zoom code/terminal au survol est déjà fait, cf. Phase 2).
- 🟡 **LSP clangd** = système n°2 de l'ORDRE VALIDÉ (cf. encart) ; complétion/diagnostics/go-to-def compile-first FAITS.
- ⬜ **Débogueur** (points d'arrêt) intégré = système n°3 de l'ORDRE VALIDÉ (gdb/lldb, MI/DAP).
- ⬜ Système d'**extensions** ; sens inverse **texte → graphe** (parsing).
- ⬜ Portage tactile/web (le moteur le permet).
- ⬜ **Onglets sur PLUSIEURS RANGÉES** (option de préférences, façon Visual Studio) — le
      débordement scroll+▾ est fait (12 juil.) ; le mode multi-rangées reste à offrir en option.
- 🟡 **Internationalisation (i18n)** : document de traductions multi-langue (démarré
  côté Paramètres › Général) ; 5 langues majeures + Ghomala' (bamiléké).

## Phase 12 — Intégration Jenga « zéro-dépendance » (in-process) 🟢 ← quasi terminée (30 juil 2026)
> Objectif : **Jenga totalement intégré et fiable** dans NKCode, sans imposer
> l'installation de Python à l'utilisateur, **en gardant le DSL Python**.
> **Décision Rihen (21 juil)** : Windows d'abord jusqu'au bout (le vrai cas
> bloquant pour un testeur externe), puis extension Linux/macOS avec le même
> design une fois la mécanique prouvée.
- ✅ **Étape 1 (Windows)** : pybind11 v2.13.6 vendorisé (`Externals/Libs/pybind11/`,
  header-only) + CPython 3.12.7 embeddable vendorisé (`Externals/Libs/PythonEmbed/` :
  `runtime/` = distribution embeddable officielle à copier près de NKCode.exe,
  `sdk/` = Python.h + libs de lien, jamais distribués). Câblé dans `NKCode.jenga`
  (includedirs/libdirs/links, même patron que le bloc Vulkan) — build/lien OK.
  ⚠️ Piège résolu : la `python312.lib` officielle est au format MSVC ; pour la
  toolchain clang-mingw il a fallu générer `libpython312.a` via `objdump`
  (exports du DLL) + `dlltool` (le `.def` et le `.a` sont dans `sdk/libs/`).
  ⚠️ Piège résolu : dans un `.jenga`, `__file__` est RELATIF et le loader a déjà
  fait chdir() dans le dossier du fichier → utiliser `os.getcwd()`, jamais
  `os.path.abspath(__file__)` (chemin doublé sinon).
- ✅ **Étape 2** : `Jenga/Core/Embed.py` — API programmatique structurée
  (build/clean/test/info/compile_flags + dataclasses de résultat + sink de
  progression accroché dans `Utils/Reporter.py` : `BuildLogger.SetTotal/
  LogCompile/LogLink`, `BuildCoordinator.PrintHeader/MarkProjectBuilt`).
  (PR Jenga #13.) ⚠️ Piège : `from ..Utils import Reporter` renvoie la CLASSE
  (re-export) → importer `from ..Utils.Reporter import SetBuildSink`.
- ✅ **Étape 3** : `NkEmbeddedJenga.h/.cpp` (NKCode) — thread worker unique
  possédant l'interpréteur (`pybind11::scoped_interpreter`, init paresseuse,
  finalize sur le même thread), `PyConfig` isolé (`isolated=1`,
  `home=<exe>/tools/python-embed`, jamais le Python système), surface
  compatible `NkProcess` (Start/Running/Done/Drain). Sink = `SimpleNamespace`
  + `py::cpp_function` (pas besoin de `PYBIND11_EMBEDDED_MODULE`).
  ⚠️ Piège : en Debug, `_DEBUG` bascule `pyconfig.h` sur l'ABI Py_DEBUG →
  `#undef _DEBUG` avant les includes Python.
- ✅ **Étape 4** : activation AUTOMATIQUE quand `tools/` est présent à côté de
  l'exe (`HasProdTools`), variable `NKCODE_EMBEDDED_JENGA` = `0`/`1` pour
  forcer. `DoRun()` reste sur sous-processus (on ne peut pas exécuter un exe
  natif dans l'interpréteur) ; l'onglet terminal garde le vrai CLI.
- ✅ **Étape 5** : packaging `scripts/MakeNkCodeDist.py` (runtime →
  `<exe>/tools/python-embed/`, arbre `Jenga/` → `<exe>/tools/jenga-src/`,
  llvm-mingw téléchargé + mis en cache) et **installeur Inno Setup**
  (`--installer`) : FR+EN, `PrivilegesRequired=lowest` (installation par
  utilisateur, **aucun UAC**), raccourcis, désinstalleur, entrée « Programmes
  et fonctionnalités ». Version lue dans la source unique
  `NkUi.h::NkCodeVersion()`. Vérifié : setup 24 Mo (`--skip-compiler`),
  installation silencieuse, exe installé lancé depuis un CWD étranger.
- 🐛 **Cause racine des retours bêta #9/#10 corrigée (30 juil)** : `ExeDir()`
  était **déduit de `argv[0]`** — qui n'a pas de dossier si l'exe est lancé via
  le PATH, et est relatif s'il est lancé depuis un autre dossier. `tools/`
  n'était alors pas trouvé → mode embarqué **désactivé** → repli sur un `jenga`
  du PATH, absent chez un testeur sans Python. Remplacé par
  `NkPath::GetExecutableDirectory()` (API OS, déjà multi-plateforme), calculé en
  tête de `main` ; polices/textures/`icons.cfg` ont aussi reçu un candidat
  relatif à l'exécutable. **Règle** : jamais de chemin de ressource déduit de
  `argv[0]` ou du CWD dans une application distribuée.
  Un **diagnostic de démarrage** (panneau Sortie) affiche désormais le dossier
  de l'exécutable et « Jenga EMBARQUÉ actif » / « INACTIF : raison ».
- 🐛 **2ᵉ cause de #9/#10 : toutes les commandes ne passaient PAS par
  l'interpréteur embarqué (30 juil)** — seuls *Construire* et *Recompiler* le
  faisaient. `info` (donc **la liste des projets**), `clean`, `test`, `run`,
  `compile-flags` et le clonage d'exemples repartaient en sous-processus sur un
  `jenga` du PATH. Sans Python, le workspace s'ouvrait **sans aucun projet** :
  plus rien n'était déclenchable, d'où « pas utilisable avec les boutons
  dédiés ». Corrigé côté Jenga par `Embed.RunCommand(argv, sink)` qui délègue au
  **même dispatcher que la CLI** (`Jenga.Commands.execute_command`) — donc
  aucune liste à maintenir et aucune commande ne peut être oubliée (PR Jenga
  #16) ; côté NKCode, `ParseJengaCmd` route les commandes connues vers leurs
  entrées dédiées et **tout le reste** vers un `cli` générique. `jenga run`
  transmet en plus des **arguments** à l'exécutable (champ *Arguments* de la
  barre d'outils, mémorisé par workspace) et son chemin embarqué est décomposé
  en 3 étapes, le worker n'ayant **qu'un créneau** (globals de `Core/Api.py`
  non réentrants). Un **shim `tools/jenga`** rend enfin la commande utilisable
  dans le terminal intégré sans Python — via le mécanisme `._pth` de CPython
  embeddable, car `PYTHONPATH` et les variables d'environnement sont ignorés
  dès qu'un `._pth` est présent (et `-I` les ignore aussi).
- 🐛 **3ᵉ cause de #9/#10 : `import Jenga` était CASSÉ dans la distribution
  (30 juil)** — `Unitest` était exclu du filtre de copie de
  `scripts/MakeNkCodeDist.py`, alors que `Jenga/__init__.py` fait
  `from . import Unitest`. **Aucun** `import Jenga` ne fonctionnait dans
  l'archive publiée. Invisible en développement (le dépôt complet est là).
  **Règle** : un filtre d'empaquetage doit être validé par un `import` réel
  depuis la distribution, avec un environnement vidé des variables Python.
- ⬜ **Étape 6 (multi-plateforme)** : Linux/macOS — pas d'équivalent officiel du
  package embeddable hors Windows ; approche à trancher (python-build-standalone
  vendorisé, ou repli détection Python système avec `python3-dev`).
- ⬜ (Optionnel, côté Jenga) **cœur natif C++** pour le graphe de build/cache, le
  `.jenga` restant le frontend Python via l'interpréteur embarqué.
- ✅ **Chaîne complète vérifiée en environnement neutralisé (30 juil)** : la
  distribution a été exercée avec **le seul** Python embarqué, dans un
  processus dont l'environnement est **vidé** (aucune variable `PYTHON*`) et le
  `PATH` réduit à `System32` + `tools/compilers/llvm-mingw/bin` + `tools/` —
  soit exactement ce que `NkEmbeddedJenga::Configure` donne au terminal
  intégré. `where python` ne trouve rien, et pourtant : `import Jenga` (2.0.9,
  `sys.prefix` = `tools/python-embed`), `import Jenga.Core.Embed`,
  `python -m Jenga --version`, le shim `tools/jenga.cmd`, `jenga info`, puis
  **`jenga build` sur un workspace sans `usetoolchain()`** — le compilateur
  embarqué est bien **détecté** (`Toolchain: clang-mingw`), `BUILD COMPLETED`,
  l'exe produit s'exécute et **reçoit ses arguments**. Bancs de test :
  `TestDistSansPython.ps1` / `TestBuildSansPython.ps1`.
- 🎯 **Jalon restant** : test sur une machine/VM **réellement** sans Python ni
  compilateur (registre, DLL système, `App Paths`…). L'essai ci-dessus élimine
  la contamination par l'environnement et le `PATH`, mais pas ce qu'une
  installation Python laisse ailleurs dans le système. (La bêta.1 a été
  téléchargée 15 fois, mais avec le bug `argv[0]` ci-dessus.)
  (Voir aussi Jenga `ROADMAP.md` § 6.5.)

## Phase 13 — Mises à jour in-app (NKCode, Jenga, outils embarqués) 🟡 ← 1re tranche faite (30 juil 2026)
> Demande Rihen (21 juil 2026) : l'utilisateur doit être **notifié** quand une
> mise à jour existe (NKCode lui-même, Jenga embarqué, runtime Python, futurs
> compilateurs bundlés) ; s'il **accepte**, on met à jour **sans réinstaller**
> l'application complète, puis on **redémarre** NKCode proprement.
>
> **Choix d'implémentation** : on ne remplace **pas** les fichiers à la main.
> NKCode étant distribué avec un vrai installeur Inno Setup (Phase 12), on
> télécharge le nouveau `setup.exe` et on le lance : Inno reconnaît son `AppId`,
> met à jour **en place** (sans désinstallation) puis relance NKCode via sa
> section `[Run]`. C'est le patron des applications de bureau réelles et c'est
> bien plus sûr qu'un remplacement à chaud (l'exe et les DLL sont **verrouillés**
> par le processus en cours ; une coupure laisserait une installation partielle).
- ✅ **Vérification de version distante** (`Shell/NkUpdate.h`) : appel à l'API
  GitHub Releases du dépôt public `Rihen-Universe/NKCode-Beta` via `curl` lancé
  par `NkProcess` → **asynchrone**, l'interface ne gèle jamais, `-m 20` borne
  l'attente (machine hors ligne = échec silencieux, pas d'état bloqué).
- ✅ **Comparaison de versions tolérante** (`CompareVersions`) : gère
  `v0.1.0-beta`, `0.1.0-beta.2`, `1.2` ; une version finale est considérée plus
  récente qu'une pré-version de mêmes nombres. Version locale lue dans la source
  unique `NkUi.h::NkCodeVersion()`.
- ✅ **Vérification automatique une fois par session** + entrée de menu
  **Aide → Rechercher les mises à jour** qui porte l'état (« Recherche… »,
  « Version X disponible », « NKCode est à jour (0.1.0-beta) », ou l'erreur).
  Un clic quand une version existe lance téléchargement puis installation.
- ✅ **Téléchargement + lancement de l'installeur** (`/SILENT /NORESTART`) puis
  `RequestClose()` : NKCode se ferme pour libérer ses fichiers, Inno met à jour
  et relance. La session (onglets, contenu non sauvegardé) est déjà persistée
  dans `.nkcode/`, donc rien n'est perdu.
- ⬜ **Notification visuelle** plus visible qu'une entrée de menu (bandeau ou
  `NkModal` « Mise à jour disponible — Mettre à jour / Plus tard ») + affichage
  des notes de version récupérées avec la release.
- ⬜ **Mise à jour granulaire des composants** `tools/*` (Jenga, python-embed)
  sans re-livrer NKCode.exe : versionnage indépendant (amorcé côté
  `Externals/Libs/PythonEmbed/VERSION`, à faire pour `tools/jenga-src/`) — utile
  pour pousser un correctif Jenga seul.
- ⬜ Multi-plateforme : équivalent Linux/macOS (paquet système ou AppImage
  auto-update), à traiter avec l'étape 6 de la Phase 12.
- 🎯 **Jalon** : un testeur reçoit la notification, clique Accepter, NKCode
  redémarre à jour — sans réinstallation manuelle. *(Chaîne implémentée ;
  vérifiable de bout en bout seulement après publication de deux releases
  successives portant un installeur.)*

## Phase 10 — Extensions (NKCode devient une plateforme) ⬜
- ⬜ **API d'extension** + **points de contribution** (commandes, panneaux, langages, **nœuds**, thèmes).
- ⬜ **Chargeur** local : natif (DLL) + scripté (runtime Python embarqué) ; manifeste + cycle de vie.
- ⬜ **Packages de projet** : recherche/installation de dépendances **via Jenga**.
- ⬜ Plus tard : registre / marketplace + sandboxing.
- ⬜ 🎯 **Jalon** : une extension tierce ajoute une commande et un nœud Blueprint sans toucher au cœur.

## Phase 11 — Agents (l'IA de dev dans l'IDE) 🟡
- ✅ **Assistant Claude Code** : panneau de chat + CLI réel en sous-processus (NkPipeProc,
      mémoire/outils/permissions natifs), permissions interactives (accepter/refuser en direct),
      journal IDE (dernières interactions) transmis au contexte, vérification en fond des
      fichiers modifiés (git status + repli scan disque si git absent).
- ✅ **Compte & Usage** : requêtes réelles (`claude auth status`, `/usage`), barres Session/Semaine,
      coût par modèle persistant, bascule Jour/Semaine, calcul dès l'entrée dans le panneau.
- ✅ **Assistant général** (API directe) : génération/revue de code câblées.
- ⬜ **Comparaison des 4 panneaux d'agents** (Claude Code / Assistant général / Codex / NkAI) —
      en attente du retour de Rihen.
- ⬜ **Codex/OpenAI** et **« IA maison » (NkAI)** : juste des messages « bientôt disponible »
      pour l'instant, pas encore câblés (contrairement à Claude Code et l'Assistant général).
- ⬜ **Agents capables de cliquer/interagir avec les fenêtres de l'IDE** (computer-use) —
      explicitement repoussé à plus tard par Rihen.
- ⬜ **Bloc « Skills, subagents, plugins, MCP servers »** dans Compte & Usage — pas ajouté
      volontairement : cette donnée n'existe pas dans la réponse texte brute du CLI (`/usage`),
      l'inventer violerait la règle du projet de ne jamais afficher une valeur non reçue réellement.
- ⬜ **Sous-agents** : agents spécialisés (revue, tests, refactor) en parallèle + orchestrateur + fusion.
- ⬜ **Orchestration visuelle** : nœuds agent/outil/condition/boucle sur le substrat **Graph** (+ équivalent texte).
- ⬜ Plus tard : **modèles LOCAUX** via **NKAI/NKInfer** (assistant 100 % local) ; permissions/garde-fous fins.
- ⬜ 🎯 **Jalon** : décrire une tâche → l'assistant édite, build et corrige ; un pipeline d'agents câblé visuellement s'exécute.

---

## Backlog — demandes de Rihen à traiter plus tard

- ⬜ **Saisie du chat IA sur le modèle de l'éditeur** (30 juil 2026). Le brouillon
  de chat est un tableau C de taille fixe, imposé par le widget NKGui
  `InputTextMultiline` (convention ImGui : il écrit dans un tampon fourni par
  l'appelant). L'éditeur de code, lui, n'a aucune borne : stockage
  `NkVector<NkVector<char>>` et rendu de la seule fenêtre visible
  (`firstVis = (scrollY - topPad) / lineH`, `lastVis = firstVis + viewH / lineH + 2`
  — `NkCodeEditor.h`). **Remarque de Rihen** : la saisie du chat devrait faire
  pareil — texte illimité, on ne dessine que ce que le défilement montre. Cela
  demande soit un widget NKGui acceptant un stockage dynamique, soit la
  réutilisation du composant éditeur dans le champ de chat. Palliatif en place :
  les prompts composés ne transitent plus par le tampon (voir `mOut` dans
  `NkAiPanel.h`), tampon porté à 64 Ko, compteur de caractères visible.
- ⬜ **Afficheur PDF en LECTURE** (30 juil 2026, demande de Rihen ; l'édition
  ne l'intéresse pas, la lecture si). Rien n'existe aujourd'hui — vérifié, la
  seule occurrence de « pdf » dans NKCode est un commentaire de mise en page
  Markdown. **Priorité : après la bêta.2** (décision Rihen).

  **Ce qu'on possède déjà** — l'inventaire change complètement l'estimation :

  | Brique | État |
  |---|---|
  | `FlateDecode` (filtre de flux dominant) | ✅ `NkDeflate::Decompress` (codec PNG) |
  | `DCTDecode` (images) | ✅ codec JPEG de NKImage |
  | Polices embarquées | ✅ `NkFontParser` : TrueType `glyf` **et** CFF/Type 2 charstrings (interpréteur intégré), cmap 4 et 12 |
  | Rastérisation de contours | ✅ `NkFontRasterizer` (Bézier quadratiques et cubiques) |

  Le moteur de police — que j'avais annoncé comme l'obstacle principal — est
  donc **déjà là**, et c'était la brique la plus coûteuse.

  **Ce qui reste à écrire** : lexer/parseur d'objets PDF, tables `xref`
  **y compris xref streams et object streams** (PDF ≥ 1.5, majoritaires
  aujourd'hui), arbre de pages, interpréteur de flux de contenu (opérateurs
  graphiques `q/Q/cm/re/m/l/c/f/S` + texte `BT/ET/Tf/Td/Tm/Tj/TJ`), pont
  « `FontFile2`/`FontFile3` → `NkFontParser` », encodages (`Differences`) et
  CMaps CID→GID, remplissage de chemins (winding non-zero et even-odd) et
  clipping.

  **Risques identifiés** (les parties non bornées) : les encodages/CMaps, les
  espaces colorimétriques, la transparence, et surtout les **PDF réels mal
  formés** — un afficheur qui échoue sur certains fichiers peut être pire que
  pas d'afficheur. Prévoir un repli explicite « ce PDF n'est pas affichable »
  plutôt qu'une page blanche.

- ⬜ **Afficheurs Word / Excel / PowerPoint** — reportés, l'intérêt porte
  d'abord sur le PDF. Ce sont des archives ZIP d'XML (OOXML) : la lecture seule
  est atteignable, l'édition non à court terme.

---

> **Note** : le **viewport 3D** n'est PAS dans NKCode — c'est une **démo autonome** dédiée
> (`Applications/NKViewportDemo`).

[Architecture](ARCHITECTURE.md) · [README](README.md)
