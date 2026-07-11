# NKCode — Roadmap

> Feuille de route par phases. Esprit : **from scratch, incrémental, un jalon observable par
> phase**. On bâtit d'abord la valeur la plus sûre (éditeur texte + build Jenga), puis les couches
> visuelles, puis le polissage. Cible d'itération : desktop (Windows/Linux/macOS) d'abord, le moteur
> étant déjà cross-plateforme.

Légende : ✅ fait · 🟡 partiel · ⬜ à faire.

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

## Phase 12 — Intégration Jenga « zéro-dépendance » (in-process) ⬜
> Objectif : **Jenga totalement intégré et fiable** dans NKCode, sans imposer
> l'installation de Python à l'utilisateur, **en gardant le DSL Python**.
- ⬜ **Embarquer libpython dans NKCode** (in-process) : exécuter Jenga **dans**
  l'IDE via `libpython`/`pybind`, **sans spawn de `jenga.exe`** → plus rapide,
  ultra-fiable, remontée directe des erreurs/logs (fin des popen fragiles).
- ⬜ **CPython embeddable** bundlé (~10–15 Mo) : zéro install côté utilisateur,
  plus de conflits de version.
- ⬜ (Optionnel, côté Jenga) **cœur natif C++** pour le graphe de build/cache, le
  `.jenga` restant le frontend Python via l'interpréteur embarqué.
- 🎯 **Jalon** : `Construire` lance Jenga in-process, aucun Python système requis.
  (Voir aussi Jenga `ROADMAP.md` § 6.5.)

## Phase 10 — Extensions (NKCode devient une plateforme) ⬜
- ⬜ **API d'extension** + **points de contribution** (commandes, panneaux, langages, **nœuds**, thèmes).
- ⬜ **Chargeur** local : natif (DLL) + scripté (runtime Python embarqué) ; manifeste + cycle de vie.
- ⬜ **Packages de projet** : recherche/installation de dépendances **via Jenga**.
- ⬜ Plus tard : registre / marketplace + sandboxing.
- ⬜ 🎯 **Jalon** : une extension tierce ajoute une commande et un nœud Blueprint sans toucher au cœur.

## Phase 11 — Agents (l'IA de dev dans l'IDE) ⬜
- ⬜ **Assistant** : panneau de chat + boucle d'outils (lire/écrire fichiers, build Jenga, recherche)
      + diff/aperçu avant application. Connecteur **LLM externe** (via NKNetwork).
- ⬜ **Sous-agents** : agents spécialisés (revue, tests, refactor) en parallèle + orchestrateur + fusion.
- ⬜ **Orchestration visuelle** : nœuds agent/outil/condition/boucle sur le substrat **Graph** (+ équivalent texte).
- ⬜ Plus tard : **modèles LOCAUX** via **NKAI/NKInfer** (assistant 100 % local) ; permissions/garde-fous fins.
- ⬜ 🎯 **Jalon** : décrire une tâche → l'assistant édite, build et corrige ; un pipeline d'agents câblé visuellement s'exécute.

---

> **Note** : le **viewport 3D** n'est PAS dans NKCode — c'est une **démo autonome** dédiée
> (`Applications/NKViewportDemo`).

[Architecture](ARCHITECTURE.md) · [README](README.md)
