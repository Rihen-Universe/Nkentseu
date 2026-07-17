# NkUIDesign — Spécification de l'interface (version humaine)

Version 0.2 — description fenêtre par fenêtre, zone par zone, élément par
élément, du lancement de l'application jusqu'à la création d'interactions
(code ou node blueprint).

---

## 0. Carte de navigation

```
Launcher
  ├─ Nouveau projet (dialog) ──────────────┐
  ├─ Ouvrir projet / Importer .nkgui ──────┤
  └─ Nouveau via IA (dialog) ──────────────┤
                                            ▼
                                  Fenêtre principale (Éditeur)
                                   ├─ Barre de menus + Barre d'outils
                                   ├─ Panneau Structure (gauche)
                                   ├─ Canvas (centre)
                                   ├─ Panneau Inspecteur (droite)
                                   ├─ Panneau Palette de widgets (gauche/onglet)
                                   ├─ Panneau Assets (gauche/onglet)
                                   ├─ Panneau IA (droite/onglet ou flottant)
                                   ├─ Panneau Behavior (bas, extensible plein écran)
                                   ├─ Barre de statut (bas)
                                   ├─ Fenêtre Gestionnaire de callbacks (secondaire)
                                   ├─ Fenêtre Aperçu / Test interactif (secondaire)
                                   ├─ Dialog Export (modal)
                                   └─ Fenêtre Préférences (secondaire)
```

---

## 1. Launcher

**But** : point d'entrée, avant tout projet ouvert.

**Zones :**
- **En-tête** : logo NkUIDesign, numéro de version, barre de recherche des
  projets récents.
- **Colonne gauche (actions)** :
  - Bouton primaire « Nouveau projet »
  - Bouton « Nouveau via IA » (icône étincelle)
  - Bouton « Ouvrir un projet »
  - Bouton « Importer un `.nkgui` »
  - Lien « Documentation »
- **Zone centrale** : grille de vignettes des **projets récents** (aperçu
  miniature du dernier écran édité, nom, date de dernière modification, clic
  = ouverture, clic droit = épingler/supprimer de la liste).
- **Bas de fenêtre** : sélecteur de compte/espace de travail (si collaboratif),
  bouton Préférences.

**États** : liste vide (première utilisation) → message d'accueil + mise en
avant de « Nouveau via IA » et « Nouveau projet ».

---

## 2. Dialog « Nouveau projet »

**Zones :**
- Champ **Nom du projet**.
- Champ **Emplacement** (chemin + bouton parcourir).
- Sélecteur **Gabarit** : Vierge / Panneau d'outils / Fenêtre de dialogue /
  HUD de jeu / Dashboard (vignettes visuelles).
- **Taille de canvas de départ** (presets + valeur libre en px).
- Sélecteur **Thème** : reprend directement les couleurs par défaut du moteur
  (`NkGuiTheme`) — presets Sombre (par défaut) / Clair / Personnalisé.
- Boutons **Annuler** / **Créer**.

---

## 3. Dialog « Nouveau projet via IA »

**Zones :**
- Grand champ de texte : « Décris l'interface ou l'application que tu veux
  créer » (placeholder avec exemples).
- Zone de dépôt optionnelle (image de référence, capture d'écran, palette de
  couleurs).
- Options : nombre d'écrans à générer, plateforme cible (Desktop outil /
  HUD jeu / Dashboard), style (Minimal / Dense / Ludique).
- Bouton **Générer un aperçu** → bascule vers un écran de résultats
  (miniatures de chaque écran généré, cases à cocher pour choisir lesquels
  importer) avant tout import réel dans le projet.
- Boutons **Régénérer** / **Importer la sélection** / **Annuler**.

---

## 4. Fenêtre principale (Éditeur)

### 4.1 Barre de menus (tout en haut)

`Fichier` · `Édition` · `Affichage` · `Widget` · `Behavior` · `IA` ·
`Fenêtre` · `Aide`

- **Fichier** : Nouveau, Ouvrir, Enregistrer, Enregistrer sous, Exporter…,
  Importer, Récents, Quitter.
- **Édition** : Annuler/Rétablir (unifié Design+Structure+Behavior),
  Couper/Copier/Coller, Dupliquer, Rechercher/Remplacer (dans le code
  Behavior), Préférences.
- **Affichage** : afficher/masquer chaque panneau, réinitialiser la
  disposition, zoom canvas, grille/repères.
- **Widget** : Promouvoir la sélection, Rétrograder, Renommer, Grouper.
- **Behavior** : Nouveau comportement (Code / Node Graph), Ouvrir le
  Gestionnaire de callbacks, Convertir Code⇄Node Graph.
- **IA** : Générer depuis un prompt, Historique des générations,
  Configuration du fournisseur IA.
- **Fenêtre** : Aperçu/Test, dispositions enregistrées.
- **Aide** : Documentation, Raccourcis clavier, À propos.

### 4.2 Barre d'outils (sous la barre de menus)

De gauche à droite :
1. Outils de sélection/dessin : **Sélection**, **Cadre/Artboard**,
   **Rectangle**, **Ellipse**, **Texte**, **Image**, **Tracé** (pen tool).
2. Séparateur.
3. Sélecteur de **mode d'édition** : `Design` | `Structure` | `Behavior`
   (bascule le focus des panneaux, mais tous restent accessibles).
4. Séparateur.
5. Contrôles d'**alignement/distribution** (actifs si sélection multiple).
6. Séparateur.
7. **Zoom** (pourcentage + boutons -/+/ajuster).
8. Bouton **Aperçu/Test** (ouvre la fenêtre Aperçu).
9. Bouton **Générer avec l'IA** (icône étincelle, ouvre le panneau IA
   contextuel).

### 4.3 Panneau Structure (colonne gauche, onglet 1/2 avec Palette)

- **Barre de recherche** filtrant l'arbre.
- **Arbre hiérarchique** : chaque ligne = un élément (forme ou widget).
  - Icône de rôle à gauche (badge distinct pour chaque type de widget ;
    icône neutre « calque » pour une forme non promue).
  - Nom éditable en double-clic.
  - Icône œil (visibilité) et cadenas (verrouillage) à droite, apparaissant
    au survol.
  - Glisser-déposer pour réordonner/re-parenter.
  - Clic droit → menu contextuel : **Promouvoir en…** (sous-menu = catalogue
    de widgets), **Rétrograder en forme**, Dupliquer, Supprimer, Grouper,
    Créer un contrôleur à partir de ce groupe.
- Élément sélectionné surligné, synchronisé avec la sélection sur le Canvas
  et l'Inspecteur.

### 4.4 Panneau Palette de widgets (colonne gauche, onglet 2/2 avec Structure)

- Recherche.
- Liste groupée par catégorie (Boutons & actions, Saisie, Affichage/Data-viz,
  Conteneurs, Fenêtres & Dock, Navigation), une entrée par rôle du catalogue
  (§8 du document 2), avec icône + nom.
- Glisser un élément de la palette directement sur le Canvas crée le widget
  déjà promu, avec ses propriétés par défaut pré-remplies dans l'Inspecteur.

### 4.5 Panneau Assets (colonne gauche, onglet 3, ou tiroir séparé)

- Bibliothèque d'images/icônes/polices du projet, import par glisser-déposer,
  aperçu miniature, recherche, tags.

### 4.6 Canvas (centre)

- Zone de dessin/mise en page principale, règles horizontales/verticales,
  grille configurable, guides d'alignement magnétiques.
- Multi-sélection (rectangle de sélection, Maj+clic), poignées de
  redimensionnement/rotation.
- Cadres (« Artboards ») représentant chaque écran/fenêtre du projet,
  nommés, organisables côte à côte (vue « plan large » comme sur Figma).
- Survol d'un widget promu affiche une bulle discrète avec son rôle et son
  nombre d'événements liés (icône éclair avec compteur).
- **Badge « Généré par IA »** en haut à gauche d'un élément tant qu'il n'a
  pas été modifié manuellement (survol → bouton « Accepter » pour l'effacer
  explicitement, ou disparition automatique à la première édition).

### 4.7 Panneau Inspecteur (colonne droite)

Trois onglets, actifs selon la sélection :

**Onglet Design**
- Position (X, Y), Taille (L, H), Rotation.
- Remplissage (couleur/dégradé), Bordure (couleur, épaisseur, arrondi).
- Typographie (police, taille, couleur, alignement) si texte.
- Contraintes de layout (si l'élément est dans un conteneur VBox/HBox/Grid) :
  ancrage, poids flex, marge.

**Onglet Widget** (visible seulement si l'élément est promu)
- Rôle affiché en en-tête (avec bouton « Rétrograder »).
- Formulaire dynamique selon le rôle : ex. pour `SliderFloat` → champs
  **Variable liée (bind)**, **Min**, **Max** ; pour `Table` → éditeur de
  colonnes ; pour `Button` → case à cocher flags (Repeat…).
- Les flags apparaissent en cases à cocher lisibles (pas de bitmask brut).

**Onglet Behavior**
- Liste des **événements disponibles** pour ce rôle (catalogue A.8/§9), une
  ligne par événement :
  - Pastille de statut : gris = non défini, bleu = Callback direct, violet =
    Behavior (code), orange = Behavior (node graph).
  - Bouton **+** pour définir : propose immédiatement 3 choix (Callback
    direct / Ouvrir éditeur Code / Ouvrir éditeur Node Graph).
  - Clic sur une ligne déjà définie → ouvre le panneau Behavior correspondant
    en bas.

### 4.8 Panneau IA (droite, onglet ou flottant contextuel)

- Champ prompt + bouton Générer.
- Contexte affiché : « s'applique à : [sélection actuelle] » ou « nouvel
  élément sur le canvas ».
- Historique des générations de la session (miniatures + prompt utilisé),
  chaque entrée : boutons **Réappliquer**, **Variante**, **Rejeter**.
- Aperçu en incrustation semi-transparente sur le Canvas avant validation.

### 4.9 Panneau Behavior (bas, dockable, extensible en plein écran)

Ouvert quand un événement est en cours d'édition. Deux sous-onglets :

**Sous-onglet Code**
- Éditeur de texte avec coloration syntaxique (thème repris de
  `NkGuiSyntax` du moteur : mots-clés en bleu, chaînes en orange clair,
  types en turquoise, etc.).
- Barre latérale : liste des variables locales disponibles (paramètres de
  l'événement) et callbacks déclarés (autocomplétion `Callback "..."`).
- Panneau d'erreurs en bas de l'éditeur (ligne, message).

**Sous-onglet Node Graph**
- Canvas infini avec grille, zoom/pan, minimap en coin.
- Palette de nœuds à gauche (catégories du §6.2 du document 2 : Événement,
  Flux, Donnée, Opérateur, Action), recherche.
- Clic droit sur le canvas → menu contextuel de création de nœud (recherche
  incrémentale, façon Unreal Blueprint).
- Chaque nœud : en-tête coloré par catégorie, pins d'exécution (haut/bas,
  triangle blanc) et pins de données (gauche/droite, couleur par type).
- Câbles courbes entre pins compatibles ; glisser depuis un pin vide ouvre
  automatiquement le menu de nœuds compatibles.
- Panneau **Variables** (liste des variables du graphe, type, valeur par
  défaut).
- Bouton **Mode debug** : surligne en direct le chemin d'exécution pendant
  un test (fenêtre Aperçu).
- Bouton **Convertir en Code** (si compatible) / **Convertir en Node Graph**
  côté Code.

### 4.10 Barre de statut (tout en bas)

Coordonnées du curseur, zoom courant, nom + id de l'élément sélectionné,
compteur d'avertissements de validation (clic → ouvre le rapport), indicateur
d'autosave.

---

## 5. Fenêtre secondaire — Gestionnaire de callbacks / contrôleurs

- Table triable : Nom, Type (`Callback` libre / `Controller.Méthode`),
  Signature, Nombre d'éléments qui l'utilisent, Statut (« lié en test » /
  « jamais lié »).
- Clic sur une ligne → surligne tous les widgets du projet qui y sont
  reliés (sur le Canvas et dans l'arbre Structure).
- Bouton **Nouveau contrôleur**, **Nouveau callback libre**.
- Bouton **Lier un mock pour le test** (ouvre un petit éditeur pour définir
  une implémentation factice utilisée uniquement dans la fenêtre Aperçu).

---

## 6. Fenêtre secondaire — Aperçu / Test interactif

- Rendu plein cadre du ou des écrans du projet, via le moteur NKGui réel.
- Barre d'outils : presets de taille (Desktop / Tablette / HUD 16:9…),
  bouton **Rejouer**, bouton **Console** (affiche les callbacks déclenchés en
  direct avec leurs arguments, utile puisque aucune vraie implémentation
  n'existe encore).
- Interaction complète possible (clic, saisie, slider…) exactement comme le
  rendu final.

---

## 7. Dialog Export

- Choix du fichier de sortie (chemin, nom).
- Case à cocher **Découper en plusieurs fichiers** (par écran / par
  contrôleur), avec aperçu de l'arborescence générée (`include`).
- **Rapport de validation** listant les erreurs/avertissements du document 2
  §12, bloquant l'export tant que des erreurs `E-*` existent (les
  avertissements `W-*` n'empêchent pas l'export mais sont listés).
- Boutons **Annuler** / **Exporter**.

---

## 8. Fenêtre Préférences

Onglets : **Général** (langue, autosave), **Apparence** (thème de l'app),
**Raccourcis clavier**, **IA** (fournisseur, clé API, modèle, quota),
**Canvas** (grille/snap par défaut).

---

*Voir aussi : Document 1 (spécification fonctionnelle), Document 2 (langage),
Document 4 (brief pour génération de maquettes via Banani).*
