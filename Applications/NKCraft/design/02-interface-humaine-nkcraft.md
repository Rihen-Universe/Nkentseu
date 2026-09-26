# NKCraft — l'interface, pour un humain

### Document 2 — Spécification d'interface côté humain

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> **`docs/UI_SPEC.md` reste la référence détaillée** (disposition relevée sur les
> captures d'UE5, palette imposée, panneau Modificateurs, quatre chemins d'accès,
> panneau T, éditeur de nœuds). Ce document la **resitue dans la famille** et
> ajoute ce qu'elle ne couvrait pas : espaces de travail, sculpture, UV,
> textures, générateurs, IA, et les couleurs d'information communes.
> Le **03** le traduit pour l'agent. **Si 02 et 03 se contredisent, 02 gagne ;
> si 02 et UI_SPEC se contredisent sur la disposition ou les gestes, UI_SPEC gagne.**

---

## 1. Le style

> **Apparence d'Unreal Engine 5. Gestes de Blender. La facilité gagne.**
> (UI_SPEC §0)

### 1.1 La palette imposée — qui devient celle de la famille

Rihen a imposé six couleurs pour le modeleur (UI_SPEC §10bis). **Elles deviennent
les valeurs de référence du thème « UE5 Rihen » pour les cinq applications**
(NkAnima, NKScena, Nogee, PV3DE, NKCraft) :

| jeton | Sombre | Clair | rôle |
|---|---|---|---|
| `@fond.app` | `#141414` | `#F5F5F5` | fond de fenêtre |
| `@fond.panneau` | `#212121` | `#FFFFFF` | fond des panneaux |
| `@fond.entete` | `#2B2B2B` | `#EAEAEA` | en-têtes, barres d'outils, en-têtes de section |
| `@accent` (bleu d'état) | `#1177D1` | `#0E5FA6` | **l'état de l'interface** : focus, onglet actif, bouton actif |
| `@selection.3d` (ambre) | `#F2980E` | `#C97A08` | **la sélection 3D** — objets, sommets, arêtes, faces sélectionnés |
| `@selection.actif` | `#FFFFFF` | `#1A1A1A` | l'élément **actif** (distinct de la sélection) |
| `@selection.aucune` | `#000000` | `#000000` | contour de ce qui n'est pas sélectionné |
| `@noeud.donnees` / `@noeud.donnees_survol` | `#0A545E` / `#095461` | idem | en-tête des nœuds de données (UI_SPEC §10bis.3) |

> **La règle qui compte** (UI_SPEC §10bis.2) : **le bleu est l'état de
> l'interface, l'ambre est la sélection 3D.** Deux familles, pas deux nuances.

⚠️ **Un conflit à trancher dans la famille** : NkAnimaEditor écrit déjà son
« bouton qui écrit une clé » en `#F79A28` (charte Rihen), à 5 points de l'ambre
`#F2980E`. UI_SPEC a posé la règle *« un seul orange »*. **Proposition** :
`@info.ecrit` **prend la valeur de l'ambre `#F2980E`** ; la confusion avec la
sélection 3D n'est pas possible, l'un étant un **bouton** et l'autre un **objet
dans la vue**. Décision de Rodolf.

### 1.2 Les couleurs d'information propres à NKCraft

| jeton | sens | couleur | pourquoi |
|---|---|---|---|
| `@axe.x/y/z` | axes | rouge / vert / bleu | convention |
| `@lumiere` | une lumière dans la vue | jaune (clair si active) | UI_SPEC §3.4 |
| `@sculpt.masque` | zone masquée en sculpture | gris sombre sur la surface | on ne sculpte pas ce qui est masqué — il faut le voir |
| `@uv.etirement` | étirement des UV | bleu (compressé) → vert (juste) → rouge (étiré) | un dépliage se juge à l'œil |
| `@uv.couture` | couture marquée | rouge | convention Blender |
| `@type.maillage` `.materiau` `.texture` `.animation` `.niveau`… | **bande de type** des cartes d'assets | une teinte par type | UI_SPEC §2.2 (déjà livré) |
| `@info.apercu` | **proposition de l'IA non appliquée** | violet hachuré | commun à la famille |
| `@info.alerte` / `@info.erreur` | à regarder / cassé | ambre / rouge | communs |

📌 **Jamais la couleur seule** (règle commune) : sélection = contour **et**
couleur ; actif = contour **plus épais**.

---

## 2. La coquille

**La disposition d'UI_SPEC §2.2 fait foi** : hiérarchie **à gauche et seule de ce
côté** (divergence assumée avec UE5), Propriétés au-dessus de Détails à droite,
barre de vue **dans** la vue, navigateur de projet en bas sur toute la largeur,
barre d'état.

Le panneau de droite porte les **quatre pastilles** de la maquette Banani (ROADMAP
§1) : **Modèle** (`box`), **Rendu** (`sun`), **Scène** (`layers`), **Modificateur**
(`sliders-horizontal`). Une seule active ; recliquer l'active replie le panneau.

### 2.1 La barre d'outils

`[💾] │ [Espace ▾] [Mode ▾] │ [+ Ajouter ▾] [Modificateur ▾] │ … │ [✦ Assistant] [● n] │ [⚙ Réglages ▾]`

- **Espace** (§3) — le sélecteur commun à la famille ;
- **Mode** — le mode d'interaction de la vue (§4), comme Blender ;
- **+ Ajouter** et **Modificateur** — deux des « cinq actions sans menu » ;
- **Assistant** et le compteur de propositions en attente (violet).

### 2.2 La barre de vue (dans la vue)

À gauche, en pilule : ☰ · Perspective ▾ · Éclairé ▾ · Afficher ▾. À droite :
les outils de transformation, le repère (global / local / normal), le **pivot**,
les **aimantations** (grille, angle, échelle) **toujours visibles**, la vitesse
de caméra. En mode Édition : **sommet / arête / face** (1 / 2 / 3). En mode
Sculpture : **symétrie X / Y / Z** et **dyntopo / multirésolution**.

### 2.3 Les quatre chemins d'accès (UI_SPEC §9bis), plus un

Menus de la vue · clic droit filtré par le mode · **palette de recherche** (F3) ·
**dernière opération** (en bas à gauche de la vue, F9) · **panneau T** (à gauche
de la vue, repliable, ouvert d'office en Sculpture). **Chaque commande affiche
son raccourci partout où elle apparaît.**

---

## 3. Les espaces de travail

| espace | pour | disposition | mode par défaut |
|---|---|---|---|
| **Modélisation** | objet, édition, modificateurs | la disposition d'UI_SPEC §2.2 | Objet |
| **Sculpture** | sculpter | vue large, panneau T ouvert, Détails réduit aux réglages de brosse, navigateur replié | Sculpture |
| **UV** | déplier, coudre, empaqueter | vue 3D **et** éditeur d'UV côte à côte | Édition (faces) |
| **Matière** | matériaux et textures | vue (aperçu matière), **graphe de matériau** en bas, navigateur des matières | Objet |
| **Peinture** | peindre les textures | vue, panneau T (pinceaux), **calques de texture** à droite | Peinture de texture |
| **Nœuds** | modélisation procédurale, générateurs | vue, **graphe de modélisation** en bas | Objet |
| **Présentation** | lumières, ciel, caméras, captures, tour | la pastille **Rendu** ouverte, caméra active | Objet |

Changer d'espace **ne change pas la scène** : c'est la même, vue autrement.

---

## 4. Les modes de la vue

| mode | touche | ce qu'on manipule | panneau T |
|---|---|---|---|
| **Objet** | Tab (bascule) | objets | sélection, curseur, déplacer, tourner, échelle |
| **Édition** | Tab | sommets / arêtes / faces (1 / 2 / 3) | + extruder, biseauter, insérer, couteau, boucle, glisser, lisser, poinçonner |
| **Sculpture** | Ctrl+Tab ▸ | les sommets, par brosses | liste des brosses + réglages de la brosse |
| **Sculpture 2.5D** | Ctrl+Tab ▸ | le G-buffer pixol | idem (brosses pixol) |
| **Volume** | Ctrl+Tab ▸ | les voxels | brosses de volume, booléens |
| **Peinture de texture** | Ctrl+Tab ▸ | les texels | pinceaux, pochoirs, calque actif |
| **Retopologie** | Ctrl+Tab ▸ | des quads dessinés sur une surface | outils de retopologie |

**Ctrl+Tab** ouvre un **menu circulaire** des modes (Blender). Le mode s'affiche
aussi en toutes lettres dans le HUD de la vue : *« rien d'important n'existe
seulement dans un journal »*.

---

## 5. Les panneaux, par espace

### 5.1 Propriétés et Détails (pastille Modèle)

L'ordre de la maquette (ROADMAP §1) : **Transformation** (Position / Rotation /
Échelle, chacune avec verrou, réinitialiser, copier) · **Dimensions** ·
**Relations** (parent / enfants) · **Matériaux** · **Groupes de sommets** ·
**Shape keys** · **Maps UV** · **Attributs de couleur** · **Attributs** · **Espace
texture** · **Données géométriques** (sommets / faces / arêtes).

**Tout ce qui peut être engendré l'est** : les paramètres des modificateurs depuis
`NkModParam` (UI_SPEC §7 — *ajouter un 18ᵉ modificateur ne doit demander aucune
ligne d'interface*), et de même les réglages d'outils, de brosses, de nœuds.

### 5.2 Pastille Modificateur

Le panneau d'UI_SPEC §7 : pile, activer, dupliquer, retirer, **Appliquer**
(destructif, et il le dit), `id` visible, ↑↓ et glisser.

### 5.3 Pastilles Rendu et Scène

Rendu : ambiance, brouillard, ombres, ciel, **capture** (source, résolution,
chemin, format) — livrés. Scène : unités, grille, collections, caméra active,
**webcam en incrustation** et enregistrement (ROADMAP, « les trois usages »).

### 5.4 Sculpture

- **Panneau T** : les brosses en **vignettes avec leur nom** (tout icône a un
  mot), puis Rayon (F), Force (Maj+F), **Courbe d'atténuation** (cinq profils en
  vignettes), Espacement, Symétrie X/Y/Z, **Masque** (peindre, inverser, effacer,
  flouter).
- **HUD** : le rayon de la brosse suit le curseur ; un cercle intérieur montre
  la force.
- **Multirésolution** : niveaux (monter, descendre, **appliquer**), et la
  subdivision courante dans la barre de vue.

### 5.5 UV

- **Éditeur d'UV** à côté de la vue 3D : îlots, sélection synchronisée avec la
  vue, **damier** ou **carte d'étirement** (`@uv.etirement`).
- Actions : **Marquer / effacer une couture** (arêtes rouges), **Déplier**,
  **Déplier automatiquement**, **Empaqueter** (marge), **Redresser**, **Aligner**.
- **Jeux d'UV** : liste, ajouter, renommer, choisir l'actif.

### 5.6 Matière

- **Graphe de matériau** en bas (même éditeur que Nogee : même navigation,
  mêmes raccourcis).
- **Aperçu** de la matière sur l'objet dans la vue, et sur une sphère dans le
  Détails.
- **Bibliothèque de matières** (Métal, Bois, Pierre, Peau, Tissus — **wax,
  kente, ndop, bogolan** —, Plastique, Verre, Émissif) : glisser sur un objet ou
  un sous-mesh.
- **Matériaux par sous-mesh** : la liste dans la pastille Modèle ▸ Matériaux,
  avec « Assigner à la sélection » en mode Édition.

### 5.7 Peinture

- **Calques** de texture (comme un logiciel de dessin) : couleur, rugosité,
  métal, normales, émission ; opacité, mode de fusion, **masque de calque**.
- Pinceaux (panneau T) : couleur, taille, force, **pochoir**, **projection**
  depuis la caméra.
- **Cuire** : une fenêtre — source (haute définition), cible (basse), cartes à
  produire (normales, occlusion, courbure, épaisseur, ID), taille, marge,
  **Cuire**.

### 5.8 Nœuds et générateurs

- **Graphe de modélisation** : nœuds qui **émettent des commandes** (UI_SPEC
  §10ter pour l'anatomie des nœuds).
- **Générateurs** (Personnage, Végétation, Rochers, Bâtiments) : un générateur
  s'ouvre comme **un panneau de curseurs** (avec « Voir le graphe » pour ceux qui
  veulent), et chaque réglage **montre son effet en direct**. **Figer en modèle**
  termine le générateur : le résultat devient un modèle ordinaire.
- **Personnage** : curseurs regroupés en catégories (Corps, Visage ▸ yeux, nez,
  bouche, oreilles, mâchoire…, Peau, Cheveux de base) ; **préréglages de
  morphologie** couvrant toutes les origines ; **symétrie** et **asymétrie**
  (un visage réel n'est pas symétrique).

### 5.9 L'Assistant

Le panneau Assistant commun à la famille (NkAnima 08 §5.5), avec deux onglets :

- **Faire** (IA opératrice) : « chanfreine les arêtes du dessus », « ajoute trois
  marches » → **cartes de proposition** : la **liste des commandes**, **Aperçu**
  (violet hachuré dans la vue), **Appliquer**, **Rejeter** ; **Défaire ce
  prompt** (un geste, pas trente).
- **Créer** (IA génératrice) : une **image** ou une **phrase** → une génération
  (aperçu tournant), **Importer** (une commande au journal) ; les générations
  passées restent listées.
- **Historique** : chaque prompt, sa réponse, et **les commandes qu'il a
  produites** (clic → sélection de ce qu'elles ont créé).

---

## 6. Raccourcis

**Ceux de Blender** (SPECIFICATION §3, UI_SPEC §9), servis par `NkShortcutTable`
(aucun raccourci en dur) :

| touche | effet |
|---|---|
| G / R / S (+ X, Y, Z) | déplacer / tourner / échelle (sur un axe) |
| Tab | objet ↔ édition |
| Ctrl+Tab | menu circulaire des modes |
| 1 / 2 / 3 (édition) | sommet / arête / face |
| E · I · K · Ctrl+R · Ctrl+B | extruder · insérer · couteau · boucle · biseauter |
| A / Alt+A | tout / rien |
| B / C | sélection en boîte / cercle |
| Alt+clic | boucle d'arêtes |
| . / , | pivot / orientation (menus circulaires) |
| Maj+A | ajouter |
| F3 | palette de recherche |
| F9 | dernière opération |
| T / N | panneau T / panneau latéral |
| F / Maj+F (sculpture) | rayon / force de la brosse |
| U (édition) | menu UV |
| Ctrl+K | Assistant (commun à la famille) |
| Ctrl+Espace | navigateur de projet (commun à la famille) |

⚠️ **Tout sélectionner = A** ici comme dans NkAnima ; **Ctrl+A** dans Nogee (gestes
UE5). Cette différence est **voulue** (gestes de Blender dans les ateliers,
d'UE5 dans l'éditeur de jeu) mais reste à confirmer par Rodolf (question posée
avec le doc 05 de Nogee).

---

## 7. Toujours vrai

- **Tout icône a un mot** ; **le rare est replié** ; **les états vides parlent** ;
  **cinq actions sans menu** ; **une information, un endroit** (UI_SPEC §0).
- **Chaque commande montre son raccourci** partout où elle apparaît.
- **Le mode courant est écrit** dans la vue.
- **Une proposition de l'IA est violette et hachurée**, et rien n'est appliqué
  sans « Appliquer ».
- **« Appliquer » un modificateur dit qu'il est destructif.**
- **Une action indisponible est grisée avec sa raison.**
- **Fermer un onglet ne supprime rien.**

---

## 8. Les menus

La barre de menus d'UI_SPEC §2.2 (**Fichier · Édition · Fenêtre · Outils ·
Sélection · Objet · Aide**) — les 19 entrées mesurées le 26/09 **restent**, avec
leurs identifiants — complétée :

| menu | contenu |
|---|---|
| **Fichier** | Nouveau projet · Ouvrir · Récents ▸ · Enregistrer · Enregistrer sous · Tout enregistrer · Importer ▸ (OBJ, glTF, FBX, DAE, PLY, STL, USDA) · Exporter ▸ · Quitter |
| **Édition** | Annuler · Refaire · Historique · Répéter la dernière · Ajuster la dernière (F9) · Palette (F3) · Préférences |
| **Fenêtre** | Espaces ▸ · chaque panneau · Assistant · Journal · Réinitialiser la disposition |
| **Outils** | Retopologie ▸ · Décimer · Niveaux de détail · Cuire des textures · Générateurs ▸ · Add-ons · Recharger les add-ons |
| **Sélection** | Tout · Rien · Inverser · Boucle · Anneau · Liés · Même matériau · Agrandir / réduire |
| **Objet** (mode Objet) | Transformer ▸ · Appliquer ▸ · Origine ▸ · Parent ▸ · Joindre · Séparer · Convertir ▸ · Ombrage ▸ |
| **Maillage** (mode Édition, remplace Objet) | Extruder · Insérer · Biseauter · Couteau · Boucle · Subdiviser · Souder · Dissoudre · Symétrie · Normales ▸ · UV ▸ |
| **Sculpture** (mode Sculpture) | Brosses ▸ · Masque ▸ · Multirésolution ▸ · Remailler |
| **Aide** | Raccourcis · Documentation · Journal · Crédits et licences (CC BY 4.0 des icônes, livré le 26/09) · À propos |
