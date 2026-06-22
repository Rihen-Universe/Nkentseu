# NKCode — Roadmap

> Feuille de route par phases. Esprit : **from scratch, incrémental, un jalon observable par
> phase**. On bâtit d'abord la valeur la plus sûre (éditeur texte + build Jenga), puis les couches
> visuelles, puis le polissage. Cible d'itération : desktop (Windows/Linux/macOS) d'abord, le moteur
> étant déjà cross-plateforme.

Légende : ⬜ à faire · 🟡 en cours · ✅ fait.

---

## Phase 0 — Mise en place
- ⬜ `NKCode.jenga` (windowedapp, dépendances NKUI/NKCanvas/NKFont/NKEvent/NKFileSystem…).
- ⬜ Coquille minimale qui ouvre une fenêtre NKUI (réutiliser le pattern de Nkoung).
- ⬜ Namespace `nkentseu::nkcode`, arborescence `src/NKCode/`.

## Phase 1 — La coquille (Shell)
- ⬜ Boucle app + thème (réutiliser `NkoungUIColor`-like).
- ⬜ **Layout / docking** des panneaux (éditeur, explorateur, sortie).
- ⬜ **Palette de commandes** (Ctrl+Maj+P) + raccourcis.
- 🎯 **Jalon** : une fenêtre IDE avec panneaux dockables et une palette de commandes.

## Phase 2 — L'éditeur de texte (Editor)
- ⬜ Tampon de texte (lignes, insertion/suppression, undo/redo).
- ⬜ Rendu monospace (NKFont) + gouttière (numéros de ligne) + curseur/sélection.
- ⬜ **Coloration syntaxique** (tokenizer + thème) ; recherche/remplacement.
- ⬜ Ouvrir/sauver des fichiers (NKFileSystem) ; onglets.
- 🎯 **Jalon** : éditer et sauver un fichier `.cpp` avec coloration.

## Phase 3 — Intégration Jenga (Project)
- ⬜ **Créer un projet** depuis un modèle (génère le `.jenga` + l'arbo).
- ⬜ Explorateur de fichiers du workspace.
- ⬜ **Build / Run** via la CLI Jenga + **panneau de sortie** (parse erreurs → clic = aller à la ligne).
- 🎯 **Jalon visible** : créer, éditer, **builder et lancer** un projet depuis NKCode.

## Phase 4 — Le moteur de graphe (Graph)
- ⬜ Modèle **nœud / broche / lien** + undo/redo.
- ⬜ **Canvas** : pan/zoom, sélection, **tirer un lien** entre broches, déplacer des nœuds.
- ⬜ **Sérialisation** des graphes (NKSerialization / NKReflection).
- 🎯 **Jalon** : poser des nœuds, les relier, sauver/recharger le graphe.

## Phase 5 — Blueprint (nœuds typés)
- ⬜ **Broches typées** + règles de connexion ; **flux d'exécution** (exec) + **flux de données**.
- ⬜ Palette de nœuds — idéalement **auto-générée depuis NKReflection** (fonctions/types du moteur).
- ⬜ Nœuds de base (variables, branches, boucles, appels de fonction, événements).
- 🎯 **Jalon** : un graphe Blueprint cohérent et typé.

## Phase 6 — Codegen (le visuel s'exécute)
- ⬜ **Compilation** Blueprint → code texte (C++ ou script).
- ⬜ Brancher le code généré dans un projet Jenga → **build & run**.
- 🎯 **Jalon « waouh »** : un graphe Blueprint **se compile et s'exécute** depuis NKCode.

## Phase 7 — UIBuilder (interfaces par glisser-déposer)
- ⬜ **Palette de widgets** + **canvas de conception** (poser/déplacer/redimensionner, guides, magnétisme).
- ⬜ **Arbre de hiérarchie** + **inspecteur de propriétés** (via **NKReflection**).
- ⬜ **Layout responsive** (ancres, flex/grille, safe-area) ; aperçu fidèle (NKUI = WYSIWYG).
- ⬜ **Sérialisation** `.nkui` (NKSerialization) + **codegen** vers NKUI (ou chargement runtime).
- ⬜ **Liaison événements → logique** (Graph/Blueprint/Blocks ou code).
- 🎯 **Jalon** : dessiner un écran à la souris, le sauver, le générer/charger et brancher un bouton.

## Phase 8 — Blocks (façon Scratch)
- ⬜ **Blocs emboîtables** (snapping) + catégories + palette, sur le substrat Graph.
- ⬜ Codegen Blocks → script lisible.
- 🎯 **Jalon** : un programme par blocs qui tourne.

## Phase 9 — Polissage & au-delà
- ⬜ Multi-curseurs, repli de code, minimap, thèmes.
- ⬜ **Complétion / LSP**, diagnostics, go-to-définition.
- ⬜ **Débogueur** (points d'arrêt) intégré.
- ⬜ Système d'**extensions** ; sens inverse **texte → graphe** (parsing).
- ⬜ Portage tactile/web (le moteur le permet).

## Phase 10 — Extensions (NKCode devient une plateforme)
- ⬜ **API d'extension** + **points de contribution** (commandes, panneaux, langages, **nœuds**, thèmes).
- ⬜ **Chargeur** local : natif (DLL) + scripté (runtime Python embarqué) ; manifeste + cycle de vie.
- ⬜ **Packages de projet** : recherche/installation de dépendances **via Jenga**.
- ⬜ Plus tard : registre / marketplace + sandboxing.
- 🎯 **Jalon** : une extension tierce ajoute une commande et un nœud Blueprint sans toucher au cœur.

## Phase 11 — Agents (l'IA de dev dans l'IDE)
- ⬜ **Assistant** : panneau de chat + boucle d'outils (lire/écrire fichiers, build Jenga, recherche)
  + diff/aperçu avant application. Connecteur **LLM externe** (via NKNetwork).
- ⬜ **Sous-agents** : agents spécialisés (revue, tests, refactor) en parallèle + orchestrateur + fusion.
- ⬜ **Orchestration visuelle** : nœuds agent/outil/condition/boucle sur le substrat **Graph** (+ équivalent texte).
- ⏳ Plus tard : **modèles LOCAUX** via **NKAI/NKInfer** (assistant 100 % local) ; permissions/garde-fous fins.
- 🎯 **Jalon** : décrire une tâche → l'assistant édite, build et corrige ; un pipeline d'agents câblé visuellement s'exécute.

---

[Architecture](ARCHITECTURE.md) · [README](README.md)
