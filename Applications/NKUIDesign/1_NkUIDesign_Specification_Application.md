# NkUIDesign — Spécification de l'application

Version 0.2 — remplace le nom de travail « NKDesign ». Application de design
d'interfaces pour le framework **NKGui**, produisant des fichiers `.nkgui`
(design + structure widgets + comportements) directement exploitables par
l'application NKGui finale, sans réécriture de code UI.

---

## 1. Positionnement

NkUIDesign combine trois métiers habituellement séparés :

1. **Design visuel libre** (façon Figma/Lunacy) — formes, texte, mise en page.
2. **Édition de composants d'interface réels** (façon éditeur de formulaires) —
   promotion d'une forme en widget NKGui avec ses propriétés fonctionnelles.
3. **Programmation visuelle du comportement** (façon Blueprint Unreal) —
   définition des réactions aux interactions, sans écrire de C++, tout en
   restant strictement compatible avec un branchement C++ ultérieur.

Le tout **assisté par génération IA à la demande**, sans jamais devenir une
boîte noire : tout ce que l'IA produit est un design normal, éditable comme
n'importe quel élément créé à la main.

---

## 2. Utilisateurs cibles

- Développeurs Nkentseu voulant prototyper une UI (NKCode, Nogee, Nkoung...)
  sans écrire d'appels `nkgui::` un par un.
- Designers UI/UX qui définissent l'apparence ET une partie du comportement
  sans dépendre en permanence d'un développeur.
- Combinaison des deux : le design + les interactions de base sont posés dans
  NkUIDesign, le développeur ne fait que « brancher » la logique métier réelle
  (accès disque, réseau, moteur de jeu, etc.) via des callbacks nommés.

---

## 3. Principes directeurs

- **Non-destructif** : promouvoir/rétrograder un élément entre « forme
  décorative » et « widget » ne perd jamais l'information visuelle.
- **Contrat, pas implémentation** : tout comportement défini dans l'app décrit
  *quoi* déclencher (nom + signature), jamais *comment* c'est implémenté côté
  moteur/jeu — cf. document 2 (langage).
- **Un seul modèle de vérité** : Design (formes), Structure (widgets),
  Comportement (code/nodes) sont trois vues d'un même document, jamais trois
  fichiers désynchronisés.
- **IA = accélérateur, jamais un mode à part** : un écran généré par IA
  atterrit comme des calques/widgets/comportements normaux, immédiatement
  éditables à la souris, au clavier, ou en Behavior — aucune notion de
  « verrou IA » ni de re-génération obligatoire pour modifier un détail.
- **Fidélité au runtime** : l'aperçu dans l'app doit rendre exactement ce que
  rendra NKGui en production (idéalement en réutilisant NKGui lui-même comme
  moteur de rendu de l'éditeur — *dogfooding*).

---

## 4. Modules fonctionnels

### 4.1 Launcher
Écran d'accueil : projets récents, nouveau projet (vierge / gabarit / via IA),
ouverture de fichier, préférences, documentation.

### 4.2 Éditeur — Canvas (Design)
Outils de dessin vectoriel (rectangle, ellipse, texte, image, tracé,
cadre/artboard), groupement, alignement/distribution, grille & snapping,
gestion de calques, conteneurs de layout live (VBox/HBox/Grid/Flow/Stack —
mêmes primitives que le moteur), zoom/pan, règles.

### 4.3 Structure (arbre + promotion)
Arbre hiérarchique de tous les éléments (formes ou widgets), drag & drop pour
réorganiser, icônes de rôle, action « Promouvoir en… » / « Rétrograder »,
recherche/filtre.

### 4.4 Inspecteur
Panneau contextuel à onglets : **Design** (position/taille/fond/bordure/
typo), **Widget** (propriétés spécifiques au rôle : bind, min/max, flags),
**Behavior** (liste des événements disponibles + statut lié/non lié).

### 4.5 Palette de composants
Liste des rôles disponibles = catalogue exact des widgets NKGui, générée
automatiquement depuis la signature des fonctions du moteur (jamais
maintenue à la main → zéro dérive design/moteur).

### 4.6 Éditeur de comportement (Behavior)
Deux vues synchronisées sur la même donnée :
- **Code** : éditeur texte avec coloration syntaxique (réutilise le thème
  `NkGuiSyntax` déjà défini dans le moteur, pour une cohérence visuelle totale
  entre l'IDE de code Nkentseu et NkUIDesign), autocomplétion des callbacks
  déclarés, panneau d'erreurs.
- **Node Graph** : canvas infini de nœuds Événement → Logique → Action/
  Callback, avec pins d'exécution et de données typées, minimap, recherche de
  nœuds, mode debug (surbrillance du chemin exécuté en test).
- Bouton de conversion Code ⇄ Node Graph tant que le contenu reste dans le
  sous-ensemble représentable par les deux (au-delà : lecture seule sur l'une
  des deux vues, avec message explicite).

### 4.7 Gestionnaire de callbacks / contrôleurs
Vue centralisée de tous les callbacks et contrôleurs déclarés dans le projet
(nom, signature, éléments qui les utilisent, statut « lié en test » /
« jamais lié »).

### 4.8 Génération par IA
Points d'entrée multiples (voir §6), toujours avec **aperçu avant
application** et **application = édition normale** (rien n'est figé après
génération).

### 4.9 Aperçu / Test interactif
Rendu live via NKGui, presets de taille de fenêtre/viewport, mode « test » où
les interactions déclenchent réellement les callbacks (avec des implémentations
factices journalisées dans une console), pour valider le comportement avant
d'écrire une ligne de C++.

### 4.10 Export / Validation
Génère le(s) fichier(s) `.nkgui`, rapport de validation (callbacks orphelins,
bindings incomplets, ids dupliqués), choix du découpage en fichiers inclus
(`include`) pour les gros projets.

### 4.11 Préférences
Thème de l'application, raccourcis clavier, autosave, configuration du
fournisseur IA (clé API, modèle, quota), grille/snap par défaut.

---

## 5. Architecture logicielle (haut niveau)

```
NkUIDesign (application)
 ├─ Core Document Model     (Geometry / WidgetTree / Behaviors / Contracts)
 ├─ Canvas Engine            (rendu design — idéalement = NKGui en mode retenu)
 ├─ Behavior Compiler        (Code ⇄ NodeGraph ⇄ IR commune)
 ├─ Parser/Serializer .nkgui (identique au parseur runtime du moteur —
 │                             partagé en librairie pour garantir le round-trip)
 ├─ AI Generation Service    (prompt → Document partiel, jamais direct au fichier
 │                             final sans passage par le modèle de document)
 └─ Export/Validation
```

Le parseur/sérialiseur `.nkgui` est **partagé** entre NkUIDesign et le
runtime NKGui (même librairie C++, compilée dans les deux) : aucune
divergence possible entre ce que l'éditeur écrit et ce que le moteur sait
lire.

---

## 6. Génération par IA — spécification fonctionnelle

### 6.1 Points d'entrée

| Contexte | Ce que génère l'IA | Résultat |
|---|---|---|
| Launcher — « Nouveau projet via IA » | Description textuelle du produit → un ou plusieurs écrans complets (Design + Structure) | Nouveau projet, tout éditable |
| Canvas — sélection vide / cadre | Prompt local (« un formulaire de connexion ») | Nouveau groupe de calques + widgets insérés à l'endroit choisi |
| Canvas — élément(s) sélectionné(s) | « Rends ce panneau plus dense » / « décline en version mobile » | Variante appliquée en aperçu, à valider ou rejeter |
| Behavior — widget sélectionné | « Ce bouton doit réinitialiser les 3 champs au-dessus » | Squelette de Node Graph proposé (nœuds + câblage), callbacks nommés proposés par convention (`ResetX`, `ResetY`…), jamais d'implémentation cachée |

### 6.2 Garanties (non négociables)

- **Aperçu avant application** : rien n'est écrit dans le document tant que
  l'utilisateur n'a pas validé.
- **Sortie = données normales du modèle** : l'IA ne produit jamais un format
  parallèle ; elle remplit directement des nœuds `Geometry`/`WidgetTree`/
  `Behavior` standards. Donc aucune limitation d'édition après coup.
- **Traçabilité légère, non bloquante** : un badge « Généré par IA » est posé
  en métadonnée (visible dans l'inspecteur), purement informatif, supprimé
  automatiquement dès qu'un élément est modifié manuellement.
- **Callbacks jamais inventés côté implémentation** : l'IA peut proposer des
  *noms* de callbacks et une structure de Node Graph, jamais un binding réel
  vers du code externe — cela reste toujours la responsabilité du
  développeur C++ (cf. document 2, §contrats).

---

## 7. Exigences non fonctionnelles

- Round-trip fidèle (export puis réimport reproduit exactement Design +
  Structure + Behavior).
- Undo/redo unifié sur les trois vues (une action Behavior est annulable
  comme une action Design).
- Autosave + historique de versions locales.
- Performance : canvas fluide avec plusieurs centaines d'éléments, Node Graph
  fluide avec plusieurs centaines de nœuds (culling/minimap).
- Accessibilité de l'éditeur lui-même (contraste, navigation clavier) —
  cohérent avec l'exigence de qualité qu'on impose aux UI produites.

---

## 8. Feuille de route (alignée sur les phases du moteur NKGui)

| Étape | Livrable |
|---|---|
| 1 | Modèle de document + parseur/sérialiseur `.nkgui` partagé avec le runtime |
| 2 | Canvas + Structure (design libre + promotion en widget) |
| 3 | Inspecteur (Design/Widget) + Palette générée depuis le catalogue moteur |
| 4 | Éditeur Behavior — Code |
| 5 | Éditeur Behavior — Node Graph + conversion Code⇄Node |
| 6 | Gestionnaire de callbacks/contrôleurs + Preview/Test interactif |
| 7 | Génération IA (tous points d'entrée du §6) |
| 8 | Export/Validation + découpage multi-fichiers |

---

*Voir aussi : Document 2 (langage de description et de node blueprint),
Document 3 (interface humaine, fenêtre par fenêtre), Document 4 (brief pour
génération de maquettes via Banani).*
