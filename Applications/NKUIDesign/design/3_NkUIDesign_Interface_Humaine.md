# NkUIDesign — Interface humaine
### Document 3 — Spécification lisible par un humain (fenêtre par fenêtre)

> Complète `1_NkUIDesign_Specification_Application.md` (positionnement, modules) et
> `2_NkUIDesign_Langage_Description_NodeBlueprint.md` (langage `.nkgui`). Là où ce
> document diverge du document 1 sur l'agencement des fenêtres, **ce document fait
> foi** — il traduit une demande explicite de disposition. Le thème visuel réutilise
> le système GitHub Light / GitHub Dark Pro déjà établi pour le reste de la suite
> Nkentseu (Aetherion Engine / Animate & FX), pour une cohérence d'écosystème.

---

## 0. Sommaire

1. Rappel de positionnement
2. Système de thème
3. Launcher
4. Anatomie de la fenêtre principale
5. Barre de menu
6. Barre d'onglets de projets
7. Barre d'outils flottante — une par mode de canvas
8. Canvas infini — mode Design
8bis. Édition vectorielle au sommet (Pen tool, ancres, booléens)
8ter. Panneau Effets (ombres, flou, dégradés, contours, fusion)
8quater. Cibles, responsive, ancrage, marges et alignement
9. Canvas — mode Behavior (Node Graph / Code)
9bis. Canvas — mode Animation (widgets animés par événement)
10. Mode Split — combiner deux canvas au choix
11. Panneau Hiérarchie (Structure)
12. Panneau Inspecteur + vue "Objets de la scène"
13. Système de pastilles rétractables (Dock Rail)
14. Palette de composants
14bis. Bibliothèque de composants — import, instances, overrides
14ter. Donner un rôle à un composant
15. Gestionnaire de callbacks / contrôleurs
16. Chat IA
17. Génération IA — points d'entrée sur le canvas
18. Simulation du système — la fenêtre d'essai
19. Export / Validation
20. Préférences
21. Glossaire des composants

---

## 1. Rappel de positionnement

NkUIDesign est un éditeur de design d'interface pour NKGui, à trois vues d'un
même document (Design / Structure / Behavior — doc 1 §1). Ce document ne
redéfinit pas le modèle de données ni le langage `.nkgui` : il spécifie
uniquement **où et comment** ces concepts apparaissent à l'écran.

**Extension actée pour cette version** : le design doit pouvoir se faire
« la totale » — édition vectorielle libre au sommet près (comme un outil de
dessin vectoriel complet, pas seulement des rectangles), effets visuels
(ombres, flou, dégradés), import et modification libre de composants
existants, et un **système d'animation de widgets déclenché par les
événements** (distinct du Behavior logique, mais qui s'y articule). Ce
dernier point vise en priorité le jeu vidéo et l'application (menus, HUD,
transitions d'état d'un bouton…), le web restant un cas d'usage futur plutôt
qu'une contrainte de conception actuelle — les choix ci-dessous privilégient
donc un modèle proche de l'animation UI de moteur de jeu (façon Unity
Animator / Rive) plutôt qu'un modèle strictement CSS.

---

## 2. Système de thème

Identique à `01-specification-humaine.md` §2 (Aetherion Engine) : palette
GitHub Light et GitHub Dark Pro, mêmes tokens de couleur, même règle « pas de
noir/blanc pur », mêmes principes de densité. Une différence assumée :

- Le **canvas de design** garde un fond neutre clair légèrement texturé
  (petit motif de points de grille, pas de damier de transparence par
  défaut — NkUIDesign dessine des interfaces, pas des images à canal alpha en
  priorité), aussi bien en thème Light qu'en thème Dark, pour rester lisible
  quel que soit le thème choisi pour l'app elle-même.
- Le **canvas Behavior (Node Graph)** reprend le même traitement que le
  Material/Blueprint Editor d'Aetherion (doc 1 §12-13) : fond quadrillé
  sombre, câbles colorés par type — cohérence intentionnelle avec le reste de
  l'écosystème puisqu'il s'agit du même paradigme visuel (nœuds Blueprint).

---

## 3. Launcher

Réutilise la structure du Launcher Aetherion (doc 1 §3 : sidebar gauche +
grille de projets), avec son propre contenu :

- Sidebar : `Bibliothèque`, `Marketplace de composants`, `Apprendre`,
  `Préférences`
- Bibliothèque : grille de cartes projets `.nkgui`, miniature = rendu du
  premier écran du projet
- Bouton `+ Nouveau projet` ouvre un choix à 3 branches (pas un stepper
  complet comme Aetherion, plus direct car NkUIDesign a moins de paramètres
  cibles) :
  - **Vierge** : canvas infini vide, une page "Page 1" créée par défaut
  - **Gabarit** : galerie de gabarits (Formulaire de connexion, Dashboard,
    Palette d'outils, HUD de jeu…)
  - **Via IA** : champ de description libre → génère un ou plusieurs écrans
    complets (doc 1 §6.1), ouvre directement l'éditeur avec un aperçu à
    valider avant écriture définitive dans le document

---

## 4. Anatomie de la fenêtre principale

⚠️ **Deux bandes horizontales seulement en haut.** La barre d'outils n'est plus
une bande : c'est un **panneau flottant vertical** posé entre la Hiérarchie et le
canvas (§7).

```
┌────┬────────────────────────────────────────────────────────────┐
│ ▓▓ │ Fichier Édition Affichage …     Nom du design     [—][□][x] │ ← Barre de menu   28px
│ ▓▓ ├────────────────────────────────────────────────────────────┤
│ ▓▓ │ [Projet A ×] [Projet B ×] [+]                               │ ← Barre d'onglets 28px
├────┴───────┬──┬──────────────────────────────────────┬───────────┤
│            │▐▌│   [ Design │ Behavior │ Animation │ Split ]      │ ← bascule de mode, flottante
│ Hiérarchie │▐▌│                                      │ Inspecteur│
│            │▐▌│           CANVAS INFINI              │           │
│            │▐▌│                                      │           │
│            │▐▌│                       [100% ▾][⊞][🧲]│           │ ← cluster canvas, flottant
├────────────┴──┴──────────────────────────────────────┴───────────┤
│  Rail de pastilles (bas) — Console / Validation, ancré discret    │
└──────────────────────────────────────────────────────────────────┘
              ▲
              └── Barre d'outils FLOTTANTE, verticale, propre au mode actif
```

**Le logo (`▓▓`) occupe les deux bandes** : il commence sur la barre de menu et se
termine sur la barre d'onglets. **56 × 56 px, strictement carré**, collé au coin
supérieur gauche. Les deux bandes sont ramenées à **28px chacune** (contre 32 et
34 auparavant) — la hauteur totale de l'en-tête passe de 66 à 56px, et le logo
devient l'ancre visuelle de la fenêtre.

Rails de pastilles supplémentaires en bordure gauche et droite de la zone
canvas (fines bandes de 28px, en dehors de la Hiérarchie/Inspecteur qui sont
des panneaux fixes distincts) — détaillés en §13.

---

## 5. Barre de menu

Hauteur **28px**. ⚠️ **Le logo n'est plus dedans** : il occupe un bloc carré de
**56 × 56 px** à gauche, qui **chevauche cette barre et celle des onglets**. Les
deux bandes commencent donc à `x = 56`, jamais à `x = 0`.

Trois zones, dans cet ordre exact de gauche à droite :

- **Zone gauche** : le menu principal horizontal en texte simple, **collé au bord
  droit du bloc logo** : `Fichier · Édition · Affichage · Objet · Comportement ·
  IA · Fenêtre · Aide`. Pas d'espace mort entre le logo et le menu — ils forment
  un seul groupe visuel.
- **Zone centrale** : nom du design/projet actif (ex. `Dashboard_Admin.nkgui`),
  centré par rapport à la fenêtre entière (pas par rapport à l'espace
  restant), petit indicateur `•` si modifications non sauvegardées,
  double-clic pour renommer inline.
- **Zone droite** : trois boutons standard de fenêtre — Réduire, Agrandir/
  Restaurer, Fermer.

La barre de menu est **draggable** sur toute sa zone vide (comportement OS
standard) sauf sur les zones interactives (menu, nom, boutons). **Le bloc logo
est draggable lui aussi.**

---

## 5bis. Les menus, entrée par entrée

> §5 déclarait la barre et ses huit entrées, **sans jamais dire ce qu'elles
> contiennent**. Voici le contenu.

### ⚠️ Une collision à trancher : le mot « Fenêtre » désigne deux choses

Depuis §8quater.1ter, l'utilisateur **dessine la fenêtre de son application**. Or la
barre porte déjà un menu **Fenêtre** qui parle des fenêtres de l'**éditeur**.
Laisser les deux sous le même mot garantit qu'on cherchera la décoration Client
dans le menu qui gère les dispositions de panneaux.

**Proposition : une neuvième entrée, `Cible`**, qui rassemble ce qui relève de
l'application visée — classe de cible, appareil, orientation, zone sûre, décoration
de fenêtre, curseur, points de rupture, rapport de transposition. `Fenêtre` reste
à l'éditeur.

✅ **Adopté par Rodolf le 2026-08-20**, après validation de la planche du menu
déroulant. **La barre porte neuf entrées.**

⚠️ **Conséquence à ne pas perdre de vue** : les planches validées qui montrent la
barre à huit entrées sont désormais **périmées** — §22.5 à §22.7 du document Banani
et le `plan_fenetre_principale.svg`. Elles restent utilisables pour tout le reste
de leur contenu ; **seule la barre est à reprendre**, au moment de la composition.

### 5bis.1 Règles communes à tous les menus

- **Un raccourci n'existe qu'à un seul endroit.** Le même geste dans deux menus,
  et le jour où l'un change, l'autre ment.
- Une entrée qui ouvre une boîte de dialogue se termine par **`…`** ; une entrée
  qui ouvre un sous-menu porte **`▸`**. La distinction se lit avant le clic.
- Une entrée qui bascule un état porte une **coche**, jamais un libellé qui
  s'inverse. ⚠️ « Afficher la grille » qui devient « Masquer la grille » oblige à
  déduire l'état courant du libellé proposé — on se trompe une fois sur deux.
- **Les entrées de premier niveau ne disparaîtnt jamais** : elles se grisent. Leur
  position est apprise par la main, et un menu dont la forme change casse ce que
  l'utilisateur a mémorisé. Seuls des **blocs entiers** sans objet se retirent.
- ⚠️ **La coche se place toujours du même côté : à GAUCHE**, dans une colonne
  réservée que les lignes non cochées laissent vide — et les libellés restent
  alignés entre eux. Dans un sous-menu comme dans un menu principal, qu'il s'agisse
  d'une bascule ou d'un choix exclusif.

  *Le rendu du 2026-08-20 22h08 la mettait à gauche dans le menu et à droite dans
  le sous-menu.* **C'est le même signe pour le même sens ; deux positions obligent
  l'œil à chercher au lieu de balayer une colonne.**

### 5bis.2 `Fichier`

| entrée | raccourci |
|---|---|
| Nouveau projet… | `Ctrl+N` |
| Ouvrir… | `Ctrl+O` |
| Ouvrir récent `▸` | |
| Fermer le projet | `Ctrl+W` |
| *—* | |
| Enregistrer | `Ctrl+S` |
| Enregistrer sous… | `Ctrl+Maj+S` |
| Enregistrer tout | `Ctrl+Alt+S` |
| Revenir à la version enregistrée | |
| *—* | |
| Importer `▸` — Composant… · Document… · Ressources… | |
| Exporter `▸` — Document `.nkgui` · Ressources · Code | `Ctrl+E` |
| Valider le document | `Ctrl+Maj+V` |
| *—* | |
| Préférences… | `Ctrl+,` |
| Quitter | `Alt+F4` |

⚠️ **« Revenir à la version enregistrée » demande confirmation en nommant ce qui
sera perdu** — nombre d'éléments modifiés, heure du dernier enregistrement. Une
entrée de menu qui jette du travail sans le décrire est un piège à un clic de
« Enregistrer ».

### 5bis.3 `Édition`

| entrée | raccourci |
|---|---|
| Annuler | `Ctrl+Z` |
| Rétablir | `Ctrl+Y` |
| *—* | |
| Couper · Copier · Coller | `Ctrl+X` · `Ctrl+C` · `Ctrl+V` |
| Coller à la même place | `Ctrl+Maj+V` |
| Coller le style seul | `Ctrl+Alt+V` |
| Dupliquer | `Ctrl+D` |
| Supprimer | `Suppr` |
| *—* | |
| Tout sélectionner | `Ctrl+A` |
| Sélectionner tous les éléments du même rôle | |
| Désélectionner | `Échap` |
| *—* | |
| Rechercher… | `Ctrl+F` |
| Remplacer une propriété… | `Ctrl+H` |
| Renommer | `F2` |

### 5bis.4 `Affichage`

| entrée | raccourci |
|---|---|
| Zoom avant · arrière | `Ctrl++` · `Ctrl+-` |
| Zoom 100 % | `Ctrl+0` |
| Ajuster à la sélection | `Maj+2` |
| Ajuster à la page | `Maj+1` |
| *—* | |
| ☑ Grille | `Ctrl+'` |
| ☑ Magnétisme | `Ctrl+;` |
| ☐ Règles | |
| ☑ Repères intelligents | |
| *—* | |
| ☑ Marges et remplissage | |
| ☐ Régions de fenêtre | |
| ☐ Éléments désactivés par héritage | |
| *—* | |
| Mode `▸` — Design `Ctrl+1` · Behavior `Ctrl+2` · Animation `Ctrl+3` · Split `Ctrl+4` | |
| Thème `▸` — Sombre · Clair · Système | |
| Panneaux `▸` — Hiérarchie · Inspecteur · Console · Palette · Bibliothèque · Chat IA | |
| Plein écran | `F11` |

⚠️ **Deux collisions ont été trouvées en écrivant ces tables, et tranchées par
Rodolf le 2026-08-20.** Les modes étaient sur `F1..F4`, où `F1` heurtait la
documentation (§5bis.10) et `F2` le renommage (§5bis.3). **Les modes passent sur
`Ctrl+1..4`**, et l'ajustement de vue libère `Ctrl+1/2` pour `Maj+1/2`.

*Le renommage par `F2` et l'aide par `F1` sont des réflexes que l'utilisateur
apporte avec lui ; les modes n'ont pas d'antériorité à défendre.* **C'est le
nouveau venu qui cède, jamais l'habitude.**

⚠️ **Et l'exercice a une leçon qui dépasse ces deux touches** : ces collisions
n'existaient nulle part tant que les raccourcis n'étaient pas écrits **dans une
seule table**. Éparpillés section par section, ils se contredisaient sans que rien
ne le montre — et on ne l'aurait découvert qu'en appuyant sur la touche.

### 5bis.5 `Objet`

| entrée | raccourci |
|---|---|
| Attribuer un rôle… | |
| Retirer le rôle | |
| *—* | |
| Grouper | `Ctrl+G` |
| Dégrouper | `Ctrl+Maj+G` |
| Convertir en composant | `Ctrl+K` |
| Détacher l'instance | |
| Promouvoir en composant partagé | |
| *—* | |
| Aligner `▸` · Répartir `▸` | |
| Ordre `▸` — Premier plan · Avancer · Reculer · Arrière-plan | |
| *—* | |
| ☐ Verrouiller | `Ctrl+L` |
| ☐ Masquer dans l'éditeur | `Ctrl+Maj+H` |
| Disponibilité `▸` — Actif · Désactivé · Lecture seule · Occupé | |

⚠️ **« Masquer dans l'éditeur » porte ces trois mots, et pas seulement
« Masquer ».** C'est la distinction de §11.1 : masquer pour travailler n'est pas
rendre invisible à l'utilisateur final. Le libellé court les confondrait au moment
où l'on choisit.

### 5bis.6 `Cible`

| entrée | |
|---|---|
| Classe `▸` — Bureau · Mobile · Web | |
| Appareil… | |
| Orientation `▸` — Portrait · Paysage | |
| *—* | |
| ☑ Afficher la zone sûre | |
| Décoration `▸` — Native · Client | |
| Curseur… | |
| *—* | |
| Points de rupture… | |
| Aperçu multi-cibles | |
| **Rapport de transposition…** | |

### 5bis.7 `Comportement`

| entrée | raccourci |
|---|---|
| Ouvrir le graphe | |
| Vue Code | `Ctrl+²` |
| *—* | |
| Ajouter un événement… | |
| Lier à un callback… · Délier | |
| Gestionnaire de callbacks… | |
| *—* | |
| Simuler | `F5` |
| Geler la simulation | `F6` |
| Recharger la simulation | `Maj+F5` |
| Système simulé… | |
| **Rapport de couverture…** | |

### 5bis.8 `IA`

| entrée | |
|---|---|
| Générer un composant… · un comportement… · une animation… | |
| Proposer un rôle pour la sélection | |
| *—* | |
| ☑ Chercher dans la bibliothèque avant de générer | |
| *—* | |
| Ouvrir le chat IA | |
| Réglages du modèle… | |

⚠️ **La coche « chercher avant de générer » est exposée ici, et cochée par
défaut** (§17.1). La rendre visible dit à l'utilisateur que l'outil réutilise plutôt
qu'il ne duplique — une garantie qu'on ne peut pas donner par un comportement
silencieux.

### 5bis.9 `Fenêtre` — celles de l'éditeur

| entrée | raccourci |
|---|---|
| Nouvelle fenêtre | |
| Détacher l'onglet dans une fenêtre | |
| *—* | |
| Disposition `▸` — Par défaut · Design · Comportement · Enregistrer la disposition… · Réinitialiser | |
| *—* | |
| Onglet suivant · précédent | `Ctrl+Tab` · `Ctrl+Maj+Tab` |
| *—* | |
| *(liste des fenêtres ouvertes, la courante cochée)* | |

### 5bis.10 `Aide`

| entrée | raccourci |
|---|---|
| Documentation | `F1` |
| Raccourcis clavier… | |
| Glossaire des composants | |
| *—* | |
| **Gestionnaire de greffons…** | |
| *—* | |
| Console… | |
| Informations système — copier | |
| *—* | |
| Rechercher les mises à jour | |
| À propos | |

⚠️ **`F1` reste à la documentation.** Le mode Design, qui le partageait, est passé
sur `Ctrl+1` (§5bis.4).

### 5bis.11 Les menus contextuels

Le clic droit ne montre **pas** un sous-ensemble d'un menu unique : il montre ce
qui s'applique **à ce qu'on a sous le curseur**.

| contexte | entrées |
|---|---|
| **canvas vide** | Coller · Coller à la même place · *—* · Ajouter un cadre… · Générer avec l'IA… · *—* · Ajuster à la page · Grille · Magnétisme |
| **élément** | Devenir… · Retirer le rôle · *—* · Couper · Copier · Dupliquer · Supprimer · *—* · Grouper · Convertir en composant · *—* · Ordre `▸` · Aligner `▸` · *—* · Verrouiller · Masquer dans l'éditeur · Disponibilité `▸` · *—* · Définir des événements… |
| **page / cadre** | Renommer · Dupliquer la page · Supprimer la page · *—* · Cible `▸` · Orientation `▸` · *—* · Centrer sur cette page · Rapport de transposition… |
| **instance de composant** | Éditer le maître · Réinitialiser au maître · Détacher l'instance · *—* · Sélectionner toutes les instances · *—* · Promouvoir en composant partagé |
| **composant (section basse)** | Renommer · Éditer · Dupliquer · Supprimer · *—* · Promouvoir en composant partagé · *—* · Sélectionner ses instances |
| **ligne d'événement** | Lier… · Délier · Ouvrir dans le graphe · *—* · Retirer du rôle · Rétablir |
| **nœud du graphe** | Couper · Copier · Supprimer · *—* · Désactiver ce nœud · Ajouter un commentaire · *—* · Aller à la définition |
| **greffon** | Activer · Désactiver · *—* · Voir les permissions… · Voir les contributions… · *—* · Désinstaller… |

⚠️ **Dans un menu contextuel, un bloc entier sans objet se retire ; une entrée
isolée se grise.** C'est le même principe qu'en §12.2, appliqué avec une nuance :
un menu contextuel apparaît sous le curseur et se lit une seule fois, donc une
longue liste de gris coûte un balayage inutile — mais garder la position des
entrées de tête préserve le geste appris. **On retire des blocs, on ne déplace pas
les premières lignes.**

⚠️ **Le clic droit sur une sélection multiple n'affiche que ce qui vaut pour
TOUS.** Proposer « Éditer le maître » quand un seul des cinq éléments est une
instance ferait porter l'action à un objet que l'utilisateur n'a pas désigné.

---

## 6. Barre d'onglets de projets

Hauteur **28px**, immédiatement sous la barre de menu, commençant à `x = 56`
(après le bloc logo). **Chaque onglet
représente un projet `.nkgui` ouvert** (pas une page à l'intérieur du
projet — voir §8 pour les pages, qui vivent toutes sur le canvas infini d'un
même onglet).

- Onglet : icône miniature très réduite du projet (optionnelle, sinon icône
  générique), nom du projet, point `•` si non sauvegardé, bouton `×` fermer
  au survol
- Onglet actif : fond légèrement différent (`--bg-canvas` vs `--bg-subtle`
  pour les inactifs), petit liseré accent en bas de l'onglet actif
- Bouton `+` en fin de bande : ouvre le Launcher en modal rapide (choix
  Vierge/Gabarit/IA) sans quitter la fenêtre courante
- Drag d'un onglet pour réordonner ; drag hors de la bande = détache le
  projet dans une **fenêtre NkUIDesign indépendante** (comportement type
  navigateur web / VS Code)
- Molette de la souris au-dessus de la bande = scroll horizontal si trop
  d'onglets pour la largeur disponible, chevron `»` de dépassement avec menu
  déroulant listant les onglets masqués

---

## 7. Barre d'outils flottante — une par mode de canvas

⚠️ **Ce n'est plus une bande horizontale.** C'est un **panneau flottant vertical**,
posé **entre la Hiérarchie et le canvas** — donc à droite de la Hiérarchie, à
gauche du canvas. Largeur 48px, hauteur ajustée à son contenu, centré
verticalement, ombre portée, coins arrondis. Il **flotte au-dessus du canvas** :
le canvas passe dessous, la barre ne lui vole pas de place.

**Chaque mode de canvas a sa propre barre d'outils, au même endroit.** Changer de
mode remplace le contenu du panneau, jamais sa position — l'œil n'a pas à
chercher.

⚠️ **La bascule de mode n'est PAS dans cette barre**, et c'est délibéré : elle
choisit *quelle* barre s'affiche, elle ne peut donc pas vivre dedans. Elle devient
un **segmented control flottant, centré en haut du canvas** :
`[ Design │ Behavior │ Animation │ Split ]`, raccourcis `1`/`2`/`3`/`4`. En mode
`Split`, le menu de combinaison et d'orientation apparaît juste à côté (§10).

De même, le **cluster canvas** — zoom éditable `100% ▾`, grille, magnétisme —
flotte **en bas à droite** du canvas. Ce sont des réglages de vue, pas des outils.

### 7.1 Outils groupés en familles — le principe

Un bouton ne porte pas un outil mais une **famille**. Le bouton montre le dernier
outil utilisé de sa famille et porte un petit chevron `⌄` en bas à droite.

- **clic** → active l'outil affiché ;
- **clic maintenu**, ou clic sur le chevron → **déplie la famille en éventail
  horizontal**, vers le canvas ;
- choisir un outil le rend actif **et** le fait devenir la face visible du bouton ;
- **la touche du raccourci fait tourner la famille** — `R` puis `R` passe du
  rectangle au rectangle arrondi, etc.

C'est le comportement de Lunacy, et il tient dans 48px de large parce que la
famille se déplie **par-dessus** le canvas.

### 7.2 Barre d'outils — mode **Design**

| famille | outils |
|---|---|
| **Sélection** `V` | Sélection · Sélection directe (points) · Main/Pan `Espace` |
| **Cadre** `F` | Cadre/Artboard · Section · Groupe |
| **Formes fixes** `R` | **Rectangle · Rectangle arrondi · Cercle/Ellipse · Triangle · N-gone · Étoile · Ligne · Flèche** |
| **Vectoriel** `P` | **Plume · Crayon libre · Courbe · Ciseaux · Booléens (union, soustraction, intersection, exclusion)** |
| **Texte** `T` | Texte simple · Texte de zone · Texte sur tracé |
| **Média** | Image · Icône · Masque |
| **Mesure** | Règle · Cotation · Pipette |

⚠️ Le **N-gone** demande son nombre de côtés : une petite saisie apparaît sous le
bouton au moment du tracé, et le nombre reste modifiable ensuite dans
l'Inspecteur — une forme fixe reste **paramétrique**, jamais convertie en tracé
tant que l'utilisateur ne le demande pas explicitement.

### 7.3 Barre d'outils — mode **Behavior**

| famille | outils |
|---|---|
| **Sélection** `V` | Sélection · Main/Pan |
| **Nœud** `N` | Nœud d'événement · Nœud d'action · Nœud de condition · Nœud de calcul |
| **Liaison** `L` | Liaison · Coupe-liaison · Reroutage |
| **Commentaire** | Note · Cadre de groupe |

### 7.4 Barre d'outils — mode **Animation**

| famille | outils |
|---|---|
| **Sélection** `V` | Sélection · Main/Pan |
| **Clé** `K` | Poser une clé · Supprimer · Copier la pose |
| **Courbe** `C` | Linéaire · Accéléré · Ralenti · Personnalisé (éditeur de courbe) |
| **Déclencheur** | Au chargement · Au survol · Au clic · Sur événement nommé |

### 7.5 En mode **Split**

**Deux barres flottantes**, une par sous-canvas, chacune contre le bord gauche de
son propre volet. Elles ne fusionnent jamais : chaque canvas garde ses outils.

---

## 8. Canvas infini — mode Design

- **Toutes les pages du projet coexistent sur le même canvas infini**,
  chacune représentée par un **cadre (artboard/frame)** rectangulaire nommé
  (étiquette au-dessus du cadre, ex. `Page — Connexion`, `Page — Dashboard`,
  `Composant — CarteProduit`), positionnable librement par glisser-déposer
  du cadre lui-même. Pas de notion d'onglet "par page" : on navigue entre
  pages en zoomant/scrollant sur le canvas, ou via le panneau Hiérarchie
  (§11) qui liste les pages comme des racines et permet un double-clic pour
  **cadrer la vue** dessus (`Maj+2` ou "Zoom to fit" façon Figma).
- Zoom/pan fluide (molette = zoom au curseur, espace+drag ou clic molette =
  pan), règles horizontales/verticales optionnelles (toggle), guides
  draggables depuis les règles.
- Grille et snapping configurables (pas, couleur d'accroche, snap aux bords/
  centres des autres éléments avec lignes de repère roses/violettes
  temporaires façon Figma au moment du drag).
- Sélection : clic simple, rectangle de sélection, double-clic pour entrer
  dans un groupe/cadre et sélectionner un enfant.
- Un élément sélectionné affiche ses poignées de redimensionnement, son
  contour accent, et — spécificité NkUIDesign — un **badge de rôle** discret
  en coin supérieur gauche de la sélection si l'élément est déjà promu en
  widget (ex. petit tag `Button`), absent si c'est encore une forme
  décorative non promue.
- Menu contextuel clic-droit : Grouper, Créer un cadre depuis la sélection,
  **Promouvoir en widget…** (ouvre un sous-menu = catalogue de rôles, cf.
  doc 1 §4.5 / §14), **Rétrograder en forme**, Convertir en composant
  réutilisable, Copier le style, Définir des événements… (raccourci direct
  vers le mode Behavior filtré sur cet élément).

---

## 8bis. Édition vectorielle au sommet (Pen tool, ancres, booléens)

Réponse au besoin « designer au sommet près » : NkUIDesign n'est pas limité à
des rectangles/ellipses paramétriques, il inclut un véritable outil de dessin
vectoriel, activable via l'outil `Tracé (P)` de la toolbar (§7) ou en
double-cliquant une forme existante pour entrer en **mode édition de points**.

- **Points d'ancrage** : petits carrés blancs à contour accent sur le tracé,
  déplaçables individuellement. Chaque ancre a un type de coin : `Sommet dur`
  (angle droit, pas de poignée), `Miroir` (poignées symétriques, tangente
  continue), `Asymétrique` (poignées de longueur indépendante, même
  direction), `Libre` (poignées totalement indépendantes) — cycle au clic
  droit sur l'ancre, ou boutons dédiés dans l'Inspecteur (onglet Design,
  section "Point" affichée seulement en mode édition de points).
- **Poignées de courbe de Bézier** : segments fins sortant de chaque ancre,
  extrémité = petit rond creux draggable ; afficher la courbe en temps réel
  pendant le drag.
- **Rayon de coin par sommet** : un sommet dur peut recevoir un arrondi
  numérique individuel (pas seulement un rayon global de rectangle) —
  champ dans l'Inspecteur quand une seule ancre est sélectionnée.
- **Outils complémentaires de la barre d'outils Tracé** : Ajouter un point sur
  segment, Supprimer un point, Couper un tracé, Fermer/ouvrir un tracé.
- **Opérations booléennes** (menu contextuel sur une sélection de ≥2 formes,
  aussi accessible en boutons dans l'Inspecteur) : `Union`, `Soustraction`,
  `Intersection`, `Exclusion` — produisent un tracé composé unique, toujours
  redécomposable (`Détacher les composants`) sans perte, cohérent avec le
  principe non-destructif du doc 1 §3.
- **Import vectoriel** : `Fichier > Importer > SVG…` convertit les tracés SVG
  en formes `ShapeNode` de type tracé natif (pas une image bitmap encapsulée)
  — immédiatement éditable au sommet comme un tracé dessiné à la main.

---

## 8ter. Panneau Effets (ombres, flou, dégradés, contours, fusion)

Section dédiée de l'onglet **Design** de l'Inspecteur (doc 3 §12.2), pour
tout élément sélectionné (forme ou widget) :

- **Remplissage** : couleur unie, dégradé (linéaire/radial/angulaire/
  diamant — éditeur de dégradé avec curseurs de couleur draggables sur une
  rampe), image (avec mode d'ajustement Cover/Contain/Tile/Stretch)
- **Contour** : couleur/dégradé, épaisseur, position (intérieur/centre/
  extérieur), style de tirets (motif éditable, pas seulement plein/pointillé)
- **Ombres** : `+ Ajouter une ombre`, chaque ombre = ligne avec décalage X/Y,
  flou, étendue, couleur, toggle `Ombre portée` / `Ombre interne`, liste
  réordonnable par glisser (empilement comme des calques d'effet)
- **Flou** : `Flou de calque` (flou l'élément lui-même) ou `Flou d'arrière-
  plan` (effet verre dépoli sur ce qui est derrière), slider d'intensité
- **Mode de fusion** (blend mode) : dropdown standard (Normal, Multiplier,
  Écran, Superposition, etc.), affecte le rendu par rapport aux calques en
  dessous
- **Opacité globale** de l'élément, distincte de l'opacité de chaque
  remplissage individuel
- Tous ces effets sont des propriétés normales du modèle de document (pas un
  mode à part) : ils s'exportent dans `.nkgui` comme des `shape_prop`/
  `prop_decl` standards (doc 2 §2-4), donc rejouables tels quels par le
  runtime NKGui — aucune fonctionnalité de l'éditeur qui ne soit pas
  restituable en production.

---

Bascule accessible depuis le segmented control de la toolbar (§7) ou depuis
le menu contextuel d'un élément (§8).

- **Portée du graphe affiché**, sélectionnable en haut du canvas Behavior
  via un dropdown contextuel `Portée : [ Composant sélectionné ▾ ]` avec
  options `Ce composant`, `Cette page (tous les événements)`, `Vue globale
  (tous les contrôleurs du projet)` — répond directement au besoin
  « définir les événements d'un composant ou de toute une page ».
- Sous-barre d'outils spécifique Behavior : toggle `Code ⇄ Node Graph`
  (doc 1 §4.6), recherche de nœud, minimap, mode debug (surbrillance du
  chemin exécuté pendant un test — voir §18).
- Le graphe lui-même reprend le style `NodeGraphCanvas` déjà établi pour
  Aetherion (câbles Bézier, pins colorés par type de donnée), avec les
  familles de nœuds propres au langage `.nkgui` (doc 2 §6.2) : Événement
  (rose), Flux (blanc épais), Donnée (fin coloré par type), Action (bleu),
  Commentaire (gris pointillé, cosmétique).
- Vue Code : éditeur texte avec coloration `NkGuiSyntax` (doc 1 §4.6),
  panneau d'erreurs ancré en bas de cette vue uniquement (distinct de la
  Console générale du rail de pastilles, §13), autocomplétion des callbacks
  déclarés.
- Un élément du canvas Design correspondant au nœud/callback survolé dans le
  Behavior s'illumine brièvement dans l'autre vue si elle est visible
  (utile surtout en mode Split, §10).

---

## 8quater. Cibles, responsive, ancrage, marges et alignement

> Le mode Behavior, annoncé au sommaire et longtemps absent du corps, est
> désormais écrit en **§9**.

### 8quater.1 Un cadre porte une cible

Tout cadre (artboard) porte une **cible** choisie à sa création et modifiable
ensuite dans l'Inspecteur :

| cible | exemples de gabarits |
|---|---|
| **Bureau** | 1920×1080 · 1440×900 · 1366×768 · libre |
| **Mobile** | 390×844 · 360×800 · 412×915 · libre |
| **Tablette** | 820×1180 · 768×1024 · libre |
| **Libre** | dimensions saisies à la main |

⚠️ **La cible n'est pas décorative** : elle fixe la **zone sûre** (§ safe area), la
densité de pixels de référence, et les gabarits de composants proposés par la
palette. Un bouton mobile n'a pas la même taille de cible tactile qu'un bouton de
bureau, et l'outil doit le savoir.

**Un même projet contient des cadres de cibles différentes** côte à côte sur le
canvas infini — c'est ainsi qu'on conçoit une application bureau **et** sa version
mobile sans changer de fichier.

### 8quater.1bis Trois classes de cible — bureau, mobile, web

> **Demande de Rodolf, 2026-08-20.**

Une cible ne se résume pas à une largeur et une hauteur. **Trois classes, et
chacune impose des contraintes que les deux autres n'ont pas.**

| classe | ce qu'elle impose |
|---|---|
| **Bureau** | fenêtre redimensionnable librement, pas de zone imposée, pointeur précis |
| **Mobile** | **zone sûre**, orientation, cibles tactiles plus grandes, clavier qui recouvre |
| **Web** | chrome du navigateur, hauteur visible **variable pendant l'usage** |

#### La zone sûre n'est pas une marge

Encoche, indicateur d'accueil, coins arrondis, barre d'état : le matériel réserve
des bandes où l'on peut **dessiner** mais pas **placer ce qui doit être lu ou
touché**.

⚠️ **Ne surtout pas la représenter par une valeur de remplissage.** Un remplissage
est un nombre que l'auteur écrit ; la zone sûre est **imposée par l'appareil et
change avec lui** — et avec l'orientation. Figée en `padding: 44`, elle cesse de
suivre l'appareil le jour où l'on change de modèle, et **plus rien ne le signale**.

Le cadre expose donc la zone sûre comme un **retrait de premier ordre**, dessiné en
hachures sur le canvas, non éditable à la main.

#### ⚠️ Ancrer au cadre ou ancrer à la zone sûre sont deux intentions

Une image de fond doit aller **bord à bord**, sous l'encoche. Un bouton, **jamais**.

Chaque ancre (§8quater.3) porte donc une référence : **bord du cadre** ou **bord de
la zone sûre**. Sans ce choix, on obtient soit des fonds qui laissent des bandes
blanches, soit des boutons sous l'encoche — et l'un des deux défauts est invisible
sur l'appareil du concepteur.

**Par défaut** : les éléments de décor s'ancrent au cadre, tout ce qui porte un
rôle interactif s'ancre à la zone sûre. Le défaut se change, mais il est du bon côté.

#### L'orientation n'est pas un point de rupture de largeur

Tourner un téléphone échange largeur et hauteur, **et déplace la zone sûre de façon
asymétrique** : l'encoche passe sur le côté, l'indicateur d'accueil reste en bas.
Deux appareils de même largeur en paysage n'ont donc pas la même zone utile.

⚠️ **Une règle de rupture fondée sur la largeur ne capture pas l'orientation.**
L'orientation est un **axe à part** dans `NkBreakpoint`, pas une largeur déguisée.

La barre du canvas porte une bascule **Portrait / Paysage** qui applique les deux
changements ensemble — dimensions et zone sûre — parce que les séparer permettrait
de composer un état qui n'existe sur aucun appareil.

#### Voir une interface de bureau sur un mobile

C'est l'usage que Rodolf demande explicitement : prendre un cadre conçu pour le
bureau et le regarder à la taille d'un téléphone.

L'outil le permet en changeant la **cible** d'un cadre sans toucher à son contenu.
Il affiche alors un **rapport de transposition** :

- les éléments qui **sortent** du cadre ;
- ceux qui passent **sous la zone sûre** ;
- ceux dont la **cible tactile** devient trop petite (§14ter, `a11y.minTargetPx`) ;
- les points de rupture qui **ne se déclenchent pas** à cette taille.

⚠️ **Ce rapport ne corrige rien.** Il nomme. Une transposition automatique
produirait une mise en page que personne n'a dessinée et que tout le monde
croirait validée.

#### Web : la hauteur visible change pendant l'usage

Sur un navigateur mobile, la barre d'adresse apparaît et disparaît au défilement :
**la hauteur utile change en cours d'utilisation**, sans rotation ni
redimensionnement.

⚠️ **Une cible Web porte donc deux hauteurs — barre visible et barre masquée — et
la simulation doit pouvoir basculer entre les deux.** Un pied de page ancré en bas
qui n'a été vérifié qu'à une seule des deux se retrouvera un jour coupé, sur
l'appareil de quelqu'un d'autre.

### 8quater.1ter Concevoir la fenêtre elle-même, et le curseur — Bureau

> **Demande de Rodolf, 2026-08-20 : « l'utilisateur doit être libre. »**

Il l'est. **Mais l'outil doit nommer ce que chaque liberté lui transfère comme
responsabilité** — sinon la liberté se paie en défauts qu'on ne voit pas sur sa
propre machine.

#### Deux régimes de décoration

| régime | qui dessine la fenêtre | qui gère déplacement, redimensionnement, ancrage |
|---|---|---|
| **Native** | le système | le système |
| **Client** | **l'application** | **l'application** |

Le régime **Client** est celui qui rend la fenêtre dessinable : barre de titre,
bordures, coins, ombre, boutons — tout devient du contenu qu'on compose.

⚠️ **Passer en décoration Client, ce n'est pas gagner une surface à dessiner : c'est
reprendre une liste de charges que le système assurait.** L'outil l'affiche au
moment du basculement, et la validation vérifie ce qu'elle peut vérifier :

| charge reprise | ce que l'outil contrôle |
|---|---|
| **zone de saisie** (où l'on attrape pour déplacer) | **erreur** si aucune n'est déclarée |
| **huit bords de redimensionnement** | avertissement si absents |
| affordance de fermeture | avertissement si absente |
| double-clic sur le titre = agrandir | rappelé, non vérifiable |
| ancrage système (`Win`+flèches, bords) | dépend de la zone de saisie déclarée |
| menu système (`Alt`+Espace), `Alt`+F4 | rappelés |
| état agrandi : ni coins arrondis ni ombre | avertissement si conservés |
| thèmes système, contraste élevé | rappelés |
| changement de densité entre deux écrans | rappelé |

⚠️ **Une fenêtre en décoration Client sans zone de saisie déclarée ne peut PAS Être
DÉPLACÉE — et rien sur le canvas ne le montre.** Elle se dessine parfaitement, elle
s'exporte, et l'utilisateur final se retrouve avec une fenêtre clouée à l'écran.
C'est le défaut emblématique de ce régime, et la seule chose qui le prévienne est
un refus à l'export.

⚠️ **Et l'état agrandi n'est pas l'état normal avec d'autres dimensions.** Une
fenêtre agrandie qui garde ses coins arrondis laisse quatre triangles du bureau
visibles aux angles de l'écran ; si elle garde son ombre, elle en projette une sur
rien. Ces deux défauts n'apparaissent jamais sur le canvas, seulement en usage.

#### Ce que le canvas montre

En cible **Bureau**, le cadre porte une **bande de décoration** au-dessus de la zone
de contenu — l'équivalent exact de la zone sûre du mobile (§8quater.1bis), et
traitée de la même façon : hachurée en régime **Native** (on ne peut rien y poser,
le système la dessine), **éditable** en régime **Client**.

Les **zones de saisie** et les **bords de redimensionnement** se dessinent en
surimpression colorée, comme le remplissage et les marges (§8quater.5) : visibles
au survol de l'élément sélectionné, modifiables en tirant.

#### Le curseur

Deux choses, très inégales, qu'il ne faut pas ranger dans le même champ.

**1. Choisir un curseur système** par élément ou par région : flèche, main, texte,
redimensionnement (huit directions), interdit, attente, croix. C'est le cas
courant, il ne coûte rien, et **c'est le défaut** : un élément à rôle interactif
reçoit le curseur de son rôle sans qu'on ait à le dire.

**2. Dessiner son propre curseur** — une image. Là, trois obligations :

- ⚠️ **le point chaud doit être déclaré** : quel pixel de l'image *est* le curseur.
  Sans lui, l'utilisateur clique à côté de ce qu'il vise, de quelques pixels,
  **partout et tout le temps** — une gêne permanente dont personne n'identifie la
  cause ;
- ⚠️ **une doublure système est obligatoire**, pas optionnelle. Un curseur
  personnalisé qui échoue à se charger laisse l'utilisateur **sans aucun curseur** ;
  il ne peut alors plus ni viser, ni comprendre pourquoi, ni le signaler ;
- ⚠️ **la taille doit suivre la densité de l'écran ET le réglage système de taille
  de curseur.** Ce réglage existe pour les personnes qui voient mal ; une image de
  taille fixe l'ignore, et l'ignore **précisément pour celles qui en ont besoin**.

⚠️ **Le système reprend la main par endroits** — pendant un glisser natif, au-dessus
de la décoration en régime Native, sur certains dialogues. Un curseur personnalisé
n'est donc **jamais garanti partout** : le dire, plutôt que de laisser découvrir le
clignotement.

#### ⚠️ Fenêtre et curseur n'existent qu'en cible Bureau

Sur **Mobile** il n'y a pas de fenêtre et pas de curseur. Sur **Web**, la fenêtre
appartient au navigateur ; seul le curseur survit, et parmi les curseurs système
seulement.

Changer la cible d'un cadre qui porte une décoration Client **ne doit pas la
supprimer en silence** : elle est conservée, marquée inactive, et le rapport de
transposition (§8quater.1bis) la nomme. *Un travail effacé par un changement de
cible se redécouvre au moment de revenir en arrière, quand il n'est plus là.*

### 8quater.2 Taille — le vocabulaire est déjà celui de l'application

Chaque élément porte une largeur et une hauteur exprimées dans le **même
vocabulaire que la déclaration**, celui que l'aperçu écrit déjà :

| mode | signification |
|---|---|
| `fixed <n>` | taille fixe en pixels logiques |
| `content` | la taille du contenu |
| `fraction <n>` | une fraction de l'espace du parent |
| `weight <n>` | un poids dans la répartition entre frères |
| `expand` | prend tout l'espace restant |

⚠️ **Ne pas inventer un second vocabulaire pour l'interface.** L'aperçu écrit
`fixed 260` → `fixed 340` quand on tire un bord ; l'Inspecteur doit montrer et
éditer **exactement ces mots**. Deux vocabulaires pour une même chose, c'est deux
sources de vérité.

**Bornes minimale et maximale.** Chaque dimension porte, en plus de son mode, une
borne basse et une borne haute **optionnelles** :

- elles s'appliquent **après** le mode — `expand` prend tout l'espace restant,
  *puis* la borne le ramène dans l'intervalle ;
- elles ont un sens pour tous les modes sauf `fixed`, où elles seraient muettes —
  l'interface les grise alors dans le popover plutôt que de les cacher, pour que
  la question ne change pas de forme selon le mode ;
- **une borne absente s'écrit `—`, jamais `0` et jamais un champ vide.** Un zéro
  est une borne à zéro ; un champ vide ne dit pas si la valeur est absente ou
  perdue. Le tiret dit « pas de borne » et ne se confond avec rien.

⚠️ **Une borne qui contredit le mode doit se dire.** `fraction 1` avec un maximum
de 100px sur un parent de 1440px, c'est un élément qui n'atteindra jamais sa
fraction. L'outil le signale — il ne corrige pas à la place du concepteur.

⚠️ **Une borne minimale supérieure à la borne maximale est une erreur, pas un
arbitrage.** L'outil refuse la saisie plutôt que de choisir laquelle gagne : deux
règles contradictoires acceptées en silence produisent une mise en page dont
personne ne sait dire d'où elle vient.

**Il n'y a pas de troisième nombre.** (Décision Rodolf, 2026-08-20.)

| mode | le nombre qu'il porte |
|---|---|
| `fixed 44` | **44 est la valeur** |
| `fraction 0.5` | 0.5 est la valeur ; la taille se calcule depuis le parent |
| `weight 2` | 2 est la valeur ; la taille se calcule entre frères |
| `content` | aucun — le contenu décide |
| `expand` | aucun — l'espace restant décide |

Chaque mode porte déjà son paramètre, ou n'en porte aucun. **Aucune « taille par
défaut » ne manque nulle part** ; en ajouter une redonnerait à `fixed` deux champs
pour une seule valeur, c'est-à-dire la seconde source de vérité qu'on vient
d'enlever.

⚠️ **Et le défaut n'est pas le minimum.** Si un `expand` ramené à sa borne basse
s'affichait comme un `fixed min`, « l'espace manque » et « je l'ai voulu ainsi »
deviendraient indistinguables. Deux causes différentes ne doivent pas produire le
même affichage.

⚠️ **Mais basculer de mode ne doit rien détruire.** L'outil **mémorise le dernier
nombre de chaque mode** : quitter `fixed 44` pour `expand` puis revenir rend 44.
C'est une mémoire d'édition — elle ne s'affiche pas, elle ne se sérialise pas.
*Un champ visible en ferait une donnée que l'utilisateur doit entretenir.*

**Comment la ligne de taille se présente.** Deux lignes, pas quatre :

- état normal : `Largeur : expand ▾` et `Hauteur : fixed 44 ▾`, **rien d'autre** ;
- **si et seulement si une borne est posée**, la ligne porte à sa droite un petit
  marqueur en lecture seule — `120–320`, `120–…`, `…–320` — gris, non éditable ;
- **un clic sur la ligne ouvre un popover** contenant, ensemble : la liste des
  modes, puis `min`, puis `max`. Ce sont les trois réponses à une même question.

**Le popover porte en plus une ligne `Rapport`** — une aide à la saisie, pas un
mécanisme de plus. On y écrit `largeur = 4 × espacement` ; l'outil résout

```
W + 2g = P   et   W = k·g     ⟹     g = P/(k+2),  W = kP/(k+2)
```

et **écrit les deux ancres proportionnelles** correspondantes (ici 16,67 %). Rien
d'autre n'est enregistré : le fichier ne contient que des ancres, exactement comme
si elles avaient été tapées à la main.

⚠️ **C'est délibérément une aide à la saisie et rien de plus.** Conserver le
rapport comme une propriété vivante ouvrirait un système de contraintes où toute
propriété peut en citer une autre — donc des cycles, donc un solveur, donc un autre
produit. *Le modèle ne gagne aucun champ ; seule l'interface gagne une phrase.*

⚠️ **Cacher les champs, jamais cacher le fait.** Le popover fait gagner la place
que la ligne de bornes permanente coûtait sur *tous* les éléments alors que
presque aucun n'a de borne. Mais une borne invisible est une borne oubliée : le
jour où un `expand` refuse de remplir, personne ne doit avoir à cliquer pour
découvrir un `max 320` posé trois semaines plus tôt. **Le marqueur est la
contrepartie non négociable du popover** — l'un sans l'autre échange du bruit
permanent contre des bugs invisibles.

### 8quater.3 Ancrage au parent

Indépendamment de la taille, chaque élément porte quatre **ancres** — gauche,
droite, haut, bas — **relatives à son parent**, jamais au cadre :

- **ancre fixe** : la distance à ce bord du parent reste constante ;
- **ancre proportionnelle** : la distance reste un pourcentage de la dimension du
  parent ;
- **deux ancres opposées actives** = l'élément **s'étire** avec le parent ;
- **aucune des deux** = l'élément reste **centré** sur cet axe.

L'Inspecteur affiche un petit **carré d'ancrage** — quatre traits autour d'un
rectangle central, cliquables — qui rend la règle lisible d'un coup d'œil. C'est
la convention que tout designer d'interface reconnaît.

### 8quater.4 Responsive

Le responsive **découle** des ancres et des modes de taille : il n'y a pas de
mécanisme séparé, et c'est délibéré — un second système de règles finirait par
contredire le premier.

Ce que l'outil ajoute par-dessus :

- **Points de rupture** optionnels par cadre — au-delà/en-deçà d'une largeur, un
  élément peut prendre une autre valeur d'ancrage, de taille ou de visibilité. La
  valeur de base reste visible, la valeur de rupture s'affiche à côté avec un
  liseré ;
- **poignée de redimensionnement du cadre** : tirer le bord d'un cadre applique
  les règles en direct — **on voit le responsive au lieu de l'imaginer** ;
- **aperçu multi-cibles** : afficher côte à côte le même cadre à trois largeurs.

⚠️ **Une règle de rupture qui n'est jamais atteinte doit se dire.** Un point de
rupture à 2000px sur un cadre mobile est du travail mort ; l'outil le signale
plutôt que de le laisser dormir.

### 8quater.5 Marge, remplissage et espacement

Trois notions distinctes, souvent confondues, et qu'il faut nommer séparément
parce qu'elles ne répondent pas à la même question.

| notion | question à laquelle elle répond | portée |
|---|---|---|
| **remplissage** (*padding*) | quelle distance entre **mon bord et mon contenu** ? | vers l'intérieur |
| **marge** (*margin*) | quelle distance entre **mon bord et ce qui m'entoure** ? | vers l'extérieur |
| **espacement** (*gap*) | quelle distance **entre mes enfants** ? | entre frères |

Chacune se règle **par côté** (haut, droite, bas, gauche) ou d'un seul nombre pour
les quatre, avec un cadenas de liaison dans l'Inspecteur. L'espacement se règle
séparément en horizontal et en vertical.

**Chaque valeur porte une unité** (décision Rodolf, 2026-08-20) :

| unité | sens |
|---|---|
| `px` | une distance absolue |
| `% du parent` | une fraction de la dimension correspondante du parent |
| `% de ma taille` | une fraction de **ma propre** dimension |

La troisième existe pour un cas que rien d'autre ne couvre : un bouton en `content`,
dont la largeur vient du texte, et dont on veut que l'espacement à droite vaille
toujours la moitié de cette largeur. **L'ancrage ne sait pas le dire** — les ancres
se mesurent depuis le parent, jamais depuis l'élément lui-même.

⚠️ **`% de ma taille` n'est autorisé que si la taille ne dépend pas de l'espace
restant.** Donc permis sur `fixed`, `content` et `fraction` ; **interdit sur
`expand` et `weight`**. Sur ceux-là, la marge alimenterait le calcul de l'espace
restant, qui déterminerait la taille, qui déterminerait la marge. La saisie est
refusée et la raison est dite ; **l'outil n'arbitre pas le cycle**, parce qu'un
cycle brisé en silence produit une disposition dont plus personne ne sait
expliquer l'origine.

⚠️ **La règle qui lève toute ambiguïté avec l'ancrage** (§8quater.3), et il faut
l'énoncer une fois pour toutes :

> **Le remplissage du parent définit sa zone de contenu. La marge de l'enfant
> définit sa boîte de marge. Les ancres se mesurent entre les deux.**

Autrement dit : une ancre gauche de 12px sur un enfant qui porte 8px de marge, dans
un parent qui porte 16px de remplissage, place le bord visible de l'enfant à
16 + 12 + 8 = 36px du bord du parent. **Chaque nombre a une seule signification et
ils s'additionnent** — aucun ne remplace l'autre, aucun n'est ignoré.

⚠️ **Et l'espacement ne se simule pas avec des marges.** Deux enfants séparés par
`gap 8` restent séparés de 8 quel que soit leur nombre ; les mêmes séparés par des
marges de 4 chacun donnent 8 entre eux **mais aussi 4 contre les bords**, ce qui
n'est presque jamais voulu. L'outil doit proposer l'espacement en premier.

**Affichage sur le canvas** : au survol d'un élément sélectionné, le remplissage
s'affiche en aplat translucide vers l'intérieur, la marge en hachures vers
l'extérieur, l'espacement par des barres entre enfants — chacun avec sa valeur
lisible et **modifiable en tirant directement**, jamais seulement dans un champ.

---

### 8quater.6 Alignement

**Sur l'élément** (comment il se place dans son parent) : gauche · centre ·
droite pour l'horizontal, haut · milieu · bas pour le vertical, plus
**étirer**.

**Sur le contenu** (comment ses enfants se placent en lui) : les mêmes, plus la
**distribution** — espacement égal, espace entre, espace autour.

**Sur le texte**, en plus : alignement horizontal (gauche, centre, droite,
justifié), alignement vertical dans sa boîte, et **alignement sur la ligne de
base** — deux textes de tailles différentes posés côte à côte doivent pouvoir
partager leur ligne de base, sinon la composition semble bancale sans qu'on sache
dire pourquoi.

**Sur une sélection multiple** : aligner et distribuer les éléments entre eux,
avec le choix de la référence — la sélection, le premier élément, ou le parent.

---

## 9. Canvas — mode Behavior (Node Graph / Code)

Le mode qui répond à *« que se passe-t-il quand on clique ? »*. Même document que
le mode Design — **un seul fichier, deux vues**. Un nœud qui référence un widget
supprimé se signale immédiatement ; il ne devient pas orphelin en silence.

### 9.1 Le canvas

Fond sombre quadrillé, façon Blueprint (§2), pan et zoom identiques au mode Design
pour ne pas réapprendre les gestes. **La barre d'outils flottante (§7.3) est au même
endroit** que celle du mode Design — seul son contenu change.

**Sélecteur de portée** en haut à gauche du canvas, flottant : `Portée : Ce
composant ▾` · `Cette page` · `Ce projet`. Il filtre ce que le graphe montre —
sans lui, un projet un peu gros devient illisible.

### 9.2 Les nœuds

| famille | rôle | apparence |
|---|---|---|
| **Événement** | point d'entrée — `cliqué`, `survol entré`, `au chargement`, ou un événement créé à la main (§14ter) | en-tête rose, une seule sortie de flux |
| **Action** | appelle un callback, change une propriété, joue une animation | en-tête bleue |
| **Condition** | branche selon un test | en-tête ambrée, deux sorties `vrai`/`faux` |
| **Calcul** | lit une valeur, compose, transforme | en-tête grise, pas de flux — **données seulement** |

⚠️ **Les événements disponibles viennent des rôles.** Attribuer le rôle `bouton`
à une forme (§14ter) fait apparaître `pressé`, `relâché`, `cliqué`, `survol
entré`, `survol sorti` comme nœuds d'événement posables. **Un élément sans rôle
n'offre que les événements que l'utilisateur a créés lui-même.** C'est le lien
direct entre le dessin et le comportement.

### 9.3 Les câbles

- **flux d'exécution** : trait blanc épais, dit *dans quel ordre* les choses
  arrivent ;
- **données** : trait fin **coloré par type** — un coup d'œil suffit à voir qu'un
  texte entre là où un nombre est attendu ;
- **événement** : rose, du widget vers son nœud d'entrée.

Un câble refusé ne se branche pas : l'extrémité incompatible s'assombrit pendant
le glisser, et une bulle dit **pourquoi** — jamais un simple refus muet.

### 9.4 La vue Code

Bascule `⟨⟩` en haut à droite du canvas : le même comportement, **écrit dans le
langage `.nkgui`** (document 2). Ce n'est pas un export : c'est **la même chose,
lue autrement**.

⚠️ **Les deux sens fonctionnent.** Éditer le texte met le graphe à jour ; déplacer
un nœud met le texte à jour. Une vue en lecture seule ferait du graphe la seule
vérité, et priverait d'un moyen de réparer ce que le graphe ne sait pas exprimer.

### 9.5 Débogage pendant la simulation

Quand la fenêtre de simulation (§18) tourne et que le canvas Behavior est visible,
**le chemin réellement exécuté s'illumine en direct** — les câbles de flux
parcourus pulsent, les nœuds atteints s'éclairent, les branches non prises restent
ternes.

> **C'est ce qui transforme le graphe en instrument de diagnostic.** On ne se
> demande plus pourquoi rien ne se passe : on voit où le flux s'arrête.

Un compteur discret sur chaque nœud indique **combien de fois il a été atteint**
depuis le début de la simulation. Un nœud à zéro, alors qu'on vient de cliquer,
est le défaut lui-même.

---

## 9bis. Canvas — mode Animation (widgets animés par événement)

Réponse au besoin « ajouter des animations sur un widget en fonction des
événements reçus ». C'est un **troisième canvas**, distinct du Behavior
logique : le Behavior décide *quoi déclencher* (callback, variable...),
l'Animation décide *comment ça bouge à l'écran*. Les deux se déclenchent par
les mêmes événements (catalogue doc 2 §9) mais restent deux programmes
séparés dans le document (voir doc 5 §3bis pour le modèle).

### Deux façons de construire une animation, réutilisant volontairement les
### éditeurs déjà définis pour Aetherion Animate & FX (mêmes composants,
### cohérence totale de l'écosystème) :

- **State Machine de widget** (vue par défaut) : reprend exactement le style
  de l'AnimGraph/State Machine d'Aetherion (voir `04-specification-humaine-
  animation-vfx.md` §7) — des états ("Idle", "Hover", "Pressed",
  "Disabled") reliés par des transitions déclenchées par un **événement du
  widget** plutôt que par une condition de gameplay. Un état "Entry" fixe le
  point de départ. Double-clic sur une transition ouvre son détail dans
  l'Inspecteur : événement déclencheur (dropdown, catalogue doc 2 §9),
  durée, courbe d'interpolation.

  ⚠️ **L'état `Disabled` de cette machine n'est PAS une notion propre à
  l'animation.** Il est **piloté par la disponibilité de l'élément**
  (§14quater), héritage d'un ancêtre compris. Le laisser se définir
  indépendamment donnerait un widget **qui paraît actif en animation tout en
  étant désactivé en réalité** — ou l'inverse. On dessine ici **l'apparence**
  de cet état et les transitions qui y mènent ; **jamais la condition qui le
  déclenche.**

  De même, `Hover`, `Pressed` et `Focus` viennent du contrat de rôle
  (§14ter) et n'existent que si le rôle les porte. **Un élément sans rôle n'a
  pas d'état `Pressed` à animer**, et la machine ne doit pas lui en proposer.
- **Dope Sheet / Curve Editor** (bascule via un bouton toggle, exactement
  comme pour Aetherion, voir `04-specification-humaine-animation-vfx.md`
  §6) : pour éditer *l'intérieur* d'un état ou d'une transition — quelles
  propriétés du widget varient dans le temps (Position, Échelle, Rotation,
  Opacité, Couleur de fond, Rayon de coin, intensité de Flou/Ombre — tout ce
  qui est exposé en §8ter est animable) et selon quelle courbe d'accélération
  (Curve Editor, mêmes poignées de tangente Bézier que dans Aetherion).

### Barre d'outils spécifique
`Portée : [ Ce widget ▾ ]` (widget sélectionné / groupe / page entière —
même principe que `BehaviorScopeSelector`, doc 5 §6.7), bouton `+ Ajouter un
état`, toggle `State Machine / Dope Sheet`, bouton `▶ Prévisualiser
l'animation` (joue la transition dans le canvas Design en incrustation, sans
quitter le mode Animation).

### Bibliothèque de courbes d'accélération
Preset rapide dans le Curve Editor : `Linéaire`, `Ease In`, `Ease Out`,
`Ease In-Out`, `Bounce`, `Élastique`, plus la possibilité de dessiner une
courbe entièrement custom — cohérent avec les attentes d'animation UI de jeu
vidéo (menus, feedback de bouton) plus que le jeu d'easings CSS standard,
tout en restant un sur-ensemble compatible si un export web est envisagé
plus tard.

### Lien avec l'Inspecteur
L'onglet **Behavior** de l'Inspecteur (doc 3 §12.2) affiche le statut lié/
non lié des *callbacks* par événement ; un nouvel indicateur discret (petite
icône "vague"/pulse) apparaît à côté de chaque événement qui a **en plus**
une animation définie, cliquable pour basculer directement en mode
Animation filtré sur cette transition.

---

## 9ter. Trois familles d'animation — la machine à états n'en couvre qu'une

> **Précision de Rodolf, 2026-08-20** : « quand je parlais d'animation je parlais
> des trucs comme le bouton qui gonfle quand il n'y a pas de souris, ou le bouton
> qui donne des effets de miroir ou de lumière — les effets dépendent de
> l'utilisateur et peuvent être de tout type. »

⚠️ **§9bis ne décrivait qu'un tiers du besoin.** Une machine à états anime le
**passage** d'un état à un autre, déclenché par un événement. Un bouton qui respire
au repos n'est déclenché par rien, et un reflet qui suit le curseur n'a ni début
ni fin. **Ce ne sont pas des transitions mal réglées : ce sont d'autres objets.**

| famille | ce qui la déclenche | ce qui la décrit |
|---|---|---|
| **A. Transition** | un **événement** | durée + courbe (§9bis) |
| **B. Ambiance** | **rien** — elle tourne | boucle : durée, répétition, sens, délai |
| **C. Continu** | une **entrée** qui varie | une **liaison** source -> propriété |

### 9ter.1 Famille B — les animations d'ambiance

Elles tournent tant qu'un état dure : gonflement au repos, halo qui pulse,
balayage de lumière périodique, léger flottement.

Réglages : **durée**, **répétition** (infinie ou N fois), **sens** (aller, ou
aller-retour), **délai avant départ**, et **décalage aléatoire**.

⚠️ **Une ambiance appartient à un ÉTAT, jamais à l'élément en général.** Le
gonflement vit dans `Idle` et doit **s'arrêter** dans `Pressed`. Attachée à
l'élément, on obtient un bouton qui respire pendant qu'on appuie dessus — et
personne ne saura dire d'où vient ce tremblement.

⚠️ **Le décalage aléatoire n'est pas une coquetterie.** Vingt cartes qui pulsent
exactement en phase forment une vague qui capte le regard bien plus fort que
l'effet voulu sur une seule. Un décalage réparti casse la synchronisation — et
l'outil doit le proposer dès qu'un élément animé est instancié plusieurs fois.

### 9ter.2 Famille C — les effets continus pilotés par une entrée

Inclinaison qui suit le curseur, projecteur qui suit le curseur, parallaxe,
reflet qui se déplace, ondulation partant du point de clic.

⚠️ **Ce ne sont pas des scénarios sur une ligne de temps, ce sont des fonctions.**
On ne pose pas des images-clés : on déclare une **source** et une **projection**.

| source | exemple d'usage |
|---|---|
| position du pointeur, relative à l'élément | inclinaison, projecteur, reflet |
| distance du pointeur au centre | intensité d'un halo |
| temps qui passe | balayage de lumière, déplacement d'un dégradé |
| défilement | parallaxe |
| valeur d'une propriété | une barre dont la couleur suit la valeur |

La projection est un intervalle vers un intervalle : *« le pointeur de -1 à +1 sur
l'axe X donne une rotation de -6° à +6° »*, avec une courbe et un **lissage**
(l'élément rattrape la valeur cible au lieu de la suivre au pixel, sinon le
mouvement est nerveux).

⚠️ **Chaque effet continu DOIT déclarer sa valeur de repos** — celle qu'il prend
quand sa source est absente. Sur un écran tactile il n'y a pas de pointeur ; en
navigation au clavier non plus. Sans valeur de repos déclarée, l'élément **reste
figé dans la dernière position qu'il avait** — une carte inclinée de travers pour
toujours, sur l'appareil de quelqu'un d'autre, sans que rien ne l'explique.

### 9ter.3 Ce qu'on anime : tout, y compris les effets

Les propriétés animables ne se limitent pas à position, échelle, rotation et
opacité. **Tous les paramètres de la pile d'effets (§8ter) le sont aussi** : rayon
de flou, intensité et couleur d'un halo, angle et position d'un dégradé,
décalage et diffusion d'une ombre, épaisseur d'un contour, mode de fusion.

C'est de là que viennent les effets que Rodolf nomme :

| effet | ce qui est animé | famille |
|---|---|---|
| gonflement au repos | échelle | B |
| halo qui pulse | intensité du halo | B |
| balayage de lumière | position d'un dégradé masqué | B ou C (temps) |
| reflet / miroir | position et opacité d'un calque miroir | C (pointeur) |
| inclinaison 3D | rotation X et Y | C (pointeur) |
| projecteur | centre d'un dégradé radial | C (pointeur) |
| ondulation au clic | rayon et opacité d'un cercle | A (événement) |
| parallaxe | décalage de position | C (défilement) |

> **Il n'y a donc pas de catalogue fermé d'effets à fournir.** Il y a des
> propriétés animables, trois façons de les faire varier, et l'utilisateur compose.
> *Un catalogue fermé aurait borné son imagination à la nôtre.*

Des **préréglages** sont fournis — respiration, balayage, reflet, inclinaison,
ondulation — mais comme **points de départ modifiables**, jamais comme la liste de
ce qui est possible.

### 9ter.4 ⚠️ Ce que les familles B et C coûtent, et qu'il faut dire

**Le réglage système « réduire les animations » doit être respecté.** Il existe pour
les personnes sujettes aux troubles vestibulaires, chez qui un mouvement continu
provoque des nausées réelles. Quand il est actif : **les ambiances s'arrêtent**, les
effets continus prennent leur valeur de repos, et **les transitions se raccourcissent
au lieu de disparaître** — les supprimer entièrement ferait perdre le retour visuel
qui dit qu'un clic a été pris en compte.

Chaque animation porte donc un comportement déclaré dans ce cas : `arrêter`,
`raccourcir`, ou `conserver` — ce dernier réservé à ce qui porte de l'information
(une barre de progression n'est pas une décoration).

**Une ambiance ne s'arrête jamais toute seule.** Sur une liste de deux cents
éléments qui respirent, ce sont deux cents animations qui tournent en permanence :
la batterie se vide, le ventilateur se met en route, et l'interface n'atteint
**jamais** un état de repos.

⚠️ **Cet état de repos n'est pas un confort, c'est une condition technique** : sans
lui, une capture d'écran automatique n'est jamais deux fois la même, un test visuel
ne peut pas conclure, et un lecteur d'écran annonce des changements en boucle.
L'outil **compte les ambiances actives par page** et signale au-delà d'un seuil.

**Un effet continu lié au pointeur s'évalue à chaque mouvement de souris.** Sur
beaucoup d'éléments simultanément, c'est du travail à chaque image. Le lissage
aide, il ne supprime pas le coût.

---

## 9quater. Y a-t-il une route pour animer côté client ? — relevé du 2026-08-21

> **Question de Rodolf** : *« est-ce qu'on a une route pour animer des UI
> directement côté client ? ça peut en plus permettre de jolis effets. »*

§9bis et §9ter décrivent trois familles d'animation avec leurs invariants. **Rien
n'avait vérifié que le moteur sache les jouer.** Vérification faite, voici l'état
réel — et il est plus favorable que je ne le craignais.

### 9quater.1 Ce qui existe déjà, et qui est générique

`Kernel/Runtime/NKAnimation` porte une machinerie **qui n'est pas liée au
maillage** :

| type | ce qu'il donne |
|---|---|
| `NkAnimationClip`, `NkAnimationTrack`, `NkKeyframe` | des pistes de valeurs dans le temps |
| `NkInterpMode`, `NkMotionCurve` | l'interpolation et les courbes |
| `NkAnimationPlayer` | la lecture |
| `NkAnimStateMachine`, `NkAnimationState`, `NkCondKind` | **une machine à états** |
| `NkBlendTree` | le mélange |

⚠️ **La machine à états de §9bis n'est donc pas à inventer : elle existe.** Les
familles **A** (transitions) et **B** (ambiances) sont des pistes jouées en boucle
ou une fois — exactement ce que `NkAnimationPlayer` fait déjà.

### 9quater.2 Ce qui manque — trois raccordements, pas un moteur

**1. Rien ne relie une piste à une propriété de widget.** `NkAnimationTrack` sait
faire varier une valeur ; personne ne lui dit que cette valeur est l'échelle du
bouton `Bouton_Connexion`.

**2. 🔴 Le format `.nkgui` n'a pas de section d'animation.** Quatre sections —
`geometry`, `widgets`, `behavior`, `controller` — et **aucune ne peut stocker une
transition, une ambiance ou un effet continu**. Tout §9ter décrit donc quelque
chose qui **n'a nulle part où s'écrire**. *(Troisième manque de la même famille
après `Text` et l'apparence — et le plus lourd des trois : le texte est un rôle,
l'apparence une section, l'animation un sous-système.)*

**3. ⚠️ NKGui est en mode immédiat, et c'est LE point technique.** En mode immédiat,
un widget est redessiné de zéro à chaque image et **ne conserve aucun état entre
deux images**. Or une animation n'est que cela : une valeur courante, une
progression, un instant de départ.

> **La route existe pourtant, et elle est déjà posée** : NKGui identifie chaque
> widget par un `NkGuiId` stable (haché FNV-1a, doc 2 §4), justement pour retenir
> son état d'interaction d'une image à l'autre. **L'état d'animation se range au
> même endroit, sous la même clé.**

C'est ce qui rend l'affaire raisonnable : on n'ajoute pas un système d'identité,
on se branche sur celui qui existe.

⚠️ **Et la conséquence à ne pas manquer : un widget qui disparaît une image doit
perdre son état d'animation — ou le retrouver s'il revient.** En mode immédiat, un
widget non appelé n'existe pas ; sans règle d'expiration, la table d'états grossit
sans fin, et un bouton qui réapparaît reprend une animation vieille de dix minutes.

### 9quater.3 La famille C ne se joue pas, elle se calcule

Les familles A et B sont des **lignes de temps** : `NkAnimationPlayer` sait faire.

**La famille C n'en est pas une.** Une inclinaison qui suit le curseur n'a ni
début, ni fin, ni durée — c'est une **fonction d'une entrée**, réévaluée à chaque
image. La faire passer pour un clip obligerait à fabriquer une ligne de temps
factice qu'on repositionnerait sans cesse.

**Elle demande donc un mécanisme distinct** : lire la source, projeter, lisser vers
la cible. C'est peu de code, mais ce n'est pas le même code.

### 9quater.4 Ce que ça coûte, honnêtement

| à faire | ampleur |
|---|---|
| section `animation` dans le format | petite — grammaire + sérialisation |
| table d'états d'animation par `NkGuiId`, avec expiration | moyenne |
| raccord piste -> propriété de widget | moyenne |
| évaluateur d'effets continus (famille C) | petite |
| respect du réglage « mouvement réduit » (§9ter.4) | petite, **à faire dès le début** |

⚠️ **Le dernier point se fait au début ou jamais.** Ajouter le respect du mouvement
réduit après coup oblige à repasser sur chaque animation déjà écrite — et on en
oublie toujours.

**Et Rodolf a raison sur le fond** : les familles B et C sont ce qui fait qu'une
interface paraît vivante plutôt que dessinée. Un bouton qui respire, un reflet qui
suit la souris — c'est peu de calcul et beaucoup d'effet.

---

### ⚠️ 9quater.5 CORRECTION, une heure plus tard : j'avais regardé NKGui sans regarder NKUI

Rodolf : *« je pensais que NKGui avait déjà son mode retenu. »* La question m'a fait
ouvrir un module que je n'avais pas ouvert. **Deux affirmations de §9quater sont
fausses.**

#### 1. La table d'états d'animation par identifiant N'EST PAS à construire

`Kernel/Runtime/NKUI/NkUIAnimation.h` — écrit par Rodolf lui-même — porte
**`NkUIAnimator`**, un bassin de `NkUITween` **indexés par identifiant** :

```
float32 v = animator.Play("btn_hover", 0.f, 1.f, 0.15f, NkEase::NK_OUT_QUAD);
float32 v = animator.Get("btn_hover");
animator.Stop("btn_hover");
```

Et le guide de NKUI le dit en toutes lettres : *« l'état persistant est conservé
dans des stores (ids, focus, **animation**, scroll, window states) »*.

**C'est exactement le mécanisme que j'ai décrit comme manquant.** Il existe, il
tourne, et il règle le problème du mode immédiat de la façon que j'avais proposée —
sauf que quelqu'un l'avait fait avant.

#### 2. La famille B a déjà son nom dans le code

Le même fichier annonce des **effets prêts à l'emploi** :

| effet NKUI | famille de §9ter |
|---|---|
| **`Pulse`** — *« battement, attirer l'attention »* | **B — ambiance** |
| `Shake` — oscillation rapide | A, sur événement |
| `Bounce` — rebond à l'atterrissage | A |
| `FadeIn` / `FadeOut` | A |
| `SlideIn` | A |

⚠️ **« Un bouton qui respire », que §9ter.1 présente comme à concevoir, s'appelle
`Pulse` et existe depuis un moment.** J'ai décrit un pays en ignorant qu'il était
déjà cartographié.

#### 3. Sur le mode retenu : la mémoire de Rodolf est juste — comme intention

`NKGui.h` annonce *« deux paradigmes — immédiat ET retenu »*, puis, quatre lignes
plus bas : *« État : Phase 1 (squelette). Le cœur, les widgets, fenêtres, docking
**et le mode retenu** arrivent aux phases 2→6. »*

**Le mode retenu est donc prévu, pas construit.** Ce qui tourne aujourd'hui, c'est
**NKUI**, en mode immédiat avec ses magasins d'état.

⚠️ **Et une contradiction que je ne peux PAS trancher depuis les en-têtes** :
`NKGui.h` se dit « squelette de phase 1 », mais `NkGuiWidgets.h` déclare un jeu de
widgets complet — quatre-vingts fonctions, conteneurs, tableaux, docking. **L'un
des deux fichiers est périmé**, et lire les en-têtes ne dit pas lequel.

**C'est une question pour Rodolf, et elle porte loin** : le document 2 fait
correspondre les rôles `.nkgui` à l'API **NKGui**. Si NKGui est un squelette et NKUI
le moteur qui tourne, **l'éditeur vise une cible qui n'existe pas encore** — ou bien
le commentaire de phase est simplement vieux.

#### 4. Ce qui reste vrai de §9quater

- **le format `.nkgui` n'a toujours aucune section d'animation** — ce manque-là est
  réel et il est le seul qui bloque réellement ;
- **la famille C n'est toujours pas un tween** : `NkUITween` interpole *de start à
  end sur une durée*. Un reflet qui suit le curseur n'a ni durée ni fin ;
- **le respect du « mouvement réduit »** n'apparaît nulle part dans NKUI, et il se
  fait au début ou jamais.

> **Leçon, la même que celle du drapeau et du journal** : j'ai cherché dans le module
> dont je connaissais le nom, conclu à l'absence, et écrit cette absence comme un
> fait. **Une recherche qui ne trouve pas ne prouve rien tant qu'on n'a pas
> énuméré où l'on a cherché.**

#### 5. ⚠️ Ce que le mode retenu change — et pourquoi cet éditeur le réclame

**Précision de Rodolf, 2026-08-21** : *« NKUI n'a pas le mode retenu ; on devait
concevoir le mode retenu dans NKGui, en plus de son mode immédiat. »*

Cela déplace la question, et dans un sens qui compte pour NkUIDesign.

> **Un `.nkgui` décrit un ARBRE PERSISTANT. C'est un objet de mode retenu par
> nature.** Une interface en mode immédiat est un *programme* qu'on réexécute à
> chaque image ; un `.nkgui` est une *structure*, pas un programme.

Charger un document dans un moteur immédiat oblige donc à écrire un **interprète**
qui parcourt l'arbre à chaque image et appelle `Begin`/`End` — c'est faisable, NKUI
le ferait, mais **chaque état par widget doit alors vivre dans un magasin à côté**,
indexé par identifiant : animation, défilement, focus, repli.

En **mode retenu**, la correspondance est directe : un nœud du document devient un
objet, et son état vit sur lui.

**Deux des trois manques trouvés cette nuit en découlent** — et je le dis avec
mesure, parce que le troisième n'en découle pas :

| manque | lié au mode immédiat ? |
|---|---|
| pas de section d'animation | **oui** — l'animation demande un état qui dure |
| pas d'apparence par widget | **oui** — en immédiat, le style s'empile globalement, il ne s'attache pas |
| pas de rôle `Text` | **non** — le moteur a `Text()`, c'est la table §8 qui l'a oublié |

⚠️ **Le mode retenu n'est donc pas une case de plus sur la feuille de route : c'est
la cible naturelle de cet éditeur.** Le construire règle deux manques d'un coup au
lieu de les rustiner séparément dans un moteur qui n'est pas fait pour eux.

*Ce qui ne dit pas quand le faire — seulement que le retarder coûtera deux
contournements qu'il faudra ensuite défaire.*

---

## 10. Mode Split — combiner deux canvas au choix

- Diviseur draggable entre les deux moitiés (poignée fine, curseur
  redimensionnement au survol), ratio par défaut 50/50, min 25%/75%.
- Le menu à côté du bouton `Split` (§7) permet de choisir **laquelle des
  trois paires** afficher : `Design + Behavior`, `Design + Animation`, ou
  `Behavior + Animation` — la combinaison la plus utilisée au quotidien est
  `Design + Animation` (régler une transition en voyant le widget bouger en
  direct à côté), mais les trois restent disponibles.
- Chaque moitié a sa **propre** sous-barre d'outils réduite, mais partagent
  la même sélection courante : sélectionner un élément dans une moitié
  centre/filtre automatiquement l'autre moitié sur cet élément (son graphe
  Behavior, sa State Machine d'Animation, ou sa position sur le canvas
  Design selon le cas).
- Cas où l'élément sélectionné n'a aucun programme défini côté Behavior ou
  Animation : la moitié correspondante affiche un état vide avec un bouton
  centré `+ Définir un événement…` ou `+ Créer une animation…` selon le cas.
- Le mode Split n'est pas un troisième document ni une troisième vue de
  données indépendante — c'est un pur choix d'affichage de la même vérité
  documentaire (cohérent avec le principe « un seul modèle de vérité »,
  doc 1 §3), désormais étendu à trois programmes (Design, Behavior,
  Animation) plutôt que deux.

---

## 11. Panneau Hiérarchie (Structure)

Panneau **fixe** à gauche (pas dans le rail de pastilles rétractables — il
reste toujours visible, mais peut être réduit à une bande fine par un
bouton de collapse dédié dans son en-tête, distinct du mécanisme de pastille
du §13, car c'est un panneau structurant permanent de l'outil, pas un
panneau secondaire).

- Racines = **pages/cadres du projet** (chacune avec icône "page", petit
  badge du nombre d'enfants), puis, sous chaque page, son arbre de calques/
  widgets — exactement la structure déjà décrite en doc 1 §4.3.
- Icône de rôle par ligne : icône générique "forme" tant que non promu,
  icône spécifique du widget une fois promu (bouton, slider, panel…) —
  cohérent avec le catalogue doc 2 §8.
- Double-clic sur une racine "page" = la vue canvas se recentre/zoome sur
  cette page (cf. §8).
- Actions en clic-droit : Renommer, Dupliquer, **`Devenir…`** /
  **`Retirer le rôle`**, Définir des événements (raccourci vers Behavior filtré),
  Supprimer.
- Recherche/filtre en haut, avec un toggle "Afficher seulement les éléments
  à rôle" pour masquer les calques purement décoratifs quand on veut
  auditer la structure fonctionnelle.

⚠️ **Vocabulaire aligné sur §14ter** (corrigé le 2026-08-20) : cette section disait
« Promouvoir / Rétrograder », là où le reste du document dit « attribuer / retirer un
rôle ». **Deux mots pour un même geste, c'est deux gestes pour l'utilisateur** — il
cherchera « promouvoir » dans l'Inspecteur et ne le trouvera pas.

### 11.1 Ce que chaque ligne porte

De gauche à droite : chevron de pliage · icône de rôle · nom · **badge
d'avertissement** si besoin · **œil de visibilité** · **cadenas de verrouillage**.
Les deux dernières n'apparaissent qu'au survol, sauf quand elles sont actives —
un élément masqué ou verrouillé le montre en permanence.

⚠️ **La visibilité de l'éditeur et la visibilité à l'exécution sont DEUX choses, et
elles ne doivent jamais partager une icône.** L'œil de la Hiérarchie masque pendant
qu'on travaille ; la propriété `visible` du widget décide de ce que voit
l'utilisateur final. Les confondre livre un jour une application où un panneau
entier manque parce que quelqu'un l'avait caché pour dessiner derrière.

L'œil fermé se **sérialise comme état d'éditeur**, pas comme propriété du document
exporté. Le verrou empêche la sélection au canvas mais **jamais** la sélection dans
la Hiérarchie — sinon un élément verrouillé par erreur devient inatteignable.

### 11.2 Le badge d'avertissement remonte

Une ligne porte un petit badge ambré si l'élément a un problème de validation :
événement lié à un callback non déclaré (§12.2), point de rupture jamais atteint
(§8quater.4), borne contradictoire (§8quater.2), sous-élément attendu absent
(§14ter).

⚠️ **Un parent replié affiche le badge de ce qui se trouve sous lui**, avec le
compte. Sans cette remontée, **plier une branche fait disparaître ses erreurs** —
et on plie justement les branches qu'on croit terminées.

Le badge du parent se distingue de celui de l'enfant : celui du parent est creux,
celui de l'élément fautif est plein. *Sinon on cherche l'erreur sur la ligne qui ne
fait que la signaler.*

### 11.3 Déplacer, reparenter

Glisser une ligne la déplace ; la déposer **sur** une autre la reparente, **entre**
deux la réordonne. Un liseré horizontal indique le réordonnancement, un
sur-lignage du parent indique le reparentage — **deux retours visuels différents,
parce que ce sont deux opérations différentes**.

⚠️ **Une cible qui ne peut pas accueillir d'enfant refuse VISIBLEMENT** — curseur
d'interdiction, aucun liseré — plutôt que d'accepter puis de replacer l'élément
ailleurs. Un dépôt silencieusement réinterprété se remarque trois manipulations
plus tard, quand on ne peut plus le relier au geste.

### 11.4 Instances de composants

Une ligne qui est l'instance d'un composant de bibliothèque (§14bis) porte un
losange devant son nom, **plein** si elle est conforme à sa source, **creux** si
elle porte des surcharges locales. Déplier une instance montre ses enfants en
lecture seule, sauf ceux qui sont surchargés.

### 11.6 Le panneau se coupe en deux : la page en haut, les composants du projet en bas

> **Proposition de Rodolf, 2026-08-20 — retenue.**

Le panneau Hiérarchie porte **deux sections empilées**, séparées par une poignée
qu'on peut tirer :

- **en haut, l'arbre de la page courante** — ce que cette page *contient* ;
- **en bas, les composants du projet** — ce que ce projet *possède*.

**Pourquoi c'est la bonne ligne** : les deux sont *le contenu du projet*. Les pages
et les composants du projet sont de la même matière — ils naissent ici, ils se
renomment ici, ils se perdent ici. La Bibliothèque devient alors **ce que le projet
utilise sans le posséder** : Partagé, Importé, Système. Une seule signification par
panneau, au lieu d'un panneau fourre-tout.

⚠️ **Le coût réel, à ne pas balayer : la hauteur.** Un arbre profond a besoin de
place, et couper le panneau en deux la divise. D'où : **poignée déplaçable**,
**section basse repliée par défaut** (elle n'affiche alors qu'un bandeau
`Composants du projet (7)`), et **mémorisation de la hauteur choisie**. Sur un
portable, une section qu'on ne peut pas replier coûte plus qu'elle ne rapporte.

⚠️ **Le risque à surveiller : trois endroits pour « poser quelque chose ».** La
Palette (§14, rôles natifs), le bas de la Hiérarchie (composants du projet) et la
Bibliothèque (le reste). Une même intention éclatée en trois surfaces, c'est trois
endroits où chercher avant de trouver.

**La règle qui lève le risque : un verbe par panneau, et un seul.**

| panneau | verbe |
|---|---|
| **Palette** | **poser** — et elle cherche dans *tout*, y compris les composants |
| **Bas de la Hiérarchie** | **gérer** — renommer, compter les instances, éditer le maître, promouvoir en Partagé |
| **Bibliothèque** | **acquérir** — importer, mettre à jour, retirer |

*Trois panneaux, trois verbes : la question « où dois-je aller ? » a une réponse
qui ne dépend pas de l'objet, seulement de ce qu'on veut en faire.*

Chaque ligne de la section basse porte : miniature, nom, **compte d'instances**, et
un point coloré si le composant est utilisé dans une **autre** page que la courante
— parce que c'est l'information qui manque au moment où l'on s'apprête à le modifier.

### 11.5 Sélection multiple

`Ctrl`+clic ajoute, `Maj`+clic étend une plage. La sélection est **la même** que
celle du canvas et que celle de l'Inspecteur — sélectionner trois lignes ici fait
apparaître `Multiple` dans les champs qui diffèrent (§12.2). *Trois panneaux, une
seule sélection : deux notions de « ce qui est sélectionné » finiraient par
diverger, et personne ne saurait laquelle fait foi.*

---

## 12. Panneau Inspecteur + vue "Objets de la scène"

Panneau **fixe** à droite, symétrique de la Hiérarchie (même mécanisme de
collapse simple, hors rail de pastilles).

### 12.1 Rien n'est sélectionné → vue "Objets de la scène"
Plutôt qu'un panneau vide, affiche par défaut une **grille de vignettes des
pages/cadres du projet** (miniature rendue de chaque page, nom en dessous,
clic = sélectionne/centre cette page sur le canvas) — répond directement au
besoin exprimé. C'est la même information que les racines de la Hiérarchie,
présentée différemment (visuel plutôt que texte), pour un choix rapide à la
souris. Un bouton `+ Nouvelle page` en bas de cette grille.

### 12.2 Sélection active → Inspecteur classique

⚠️ **Cette section a été réécrite après §8quater et §14ter.** L'ancienne version
décrivait trois onglets sans dire ce qu'ils contenaient exactement ; elle datait
d'avant le vocabulaire de taille, l'ancrage, l'espacement et le rôle. Ce qui suit
énumère les sections **dans l'ordre où elles apparaissent**, parce qu'un ordre
laissé au hasard se met à varier d'un écran à l'autre.

**En-tête du panneau**, toujours visible, jamais replié :
nom de l'élément (éditable au double-clic) · icône du rôle s'il en porte un ·
**la ligne de rôle, qui est le contrôle d'attribution lui-même** (§14ter).

Puis **trois onglets** : `Design` · `Widget` · `Behavior`.

#### Onglet **Design** — les sections, dans cet ordre

| # | Section | Contenu | Repliée par défaut |
|---|---|---|---|
| 1 | **Cible** | cible du cadre, en lecture seule si l'élément n'est pas le cadre lui-même (§8quater.1) | non |
| 2 | **Disposition** | Position X/Y · Largeur et Hauteur dans le vocabulaire `fixed/content/fraction/weight/expand`, **une seule ligne chacune** · marqueur `120–320` à droite de la ligne **seulement si une borne est posée** · clic sur la ligne = popover mode + `min` + `max` (§8quater.2) | non |
| 3 | **Ancrage** | le carré d'ancrage — quatre traits autour d'un rectangle central, cliquables (§8quater.3) | non |
| 4 | **Alignement** | deux rangées d'icônes, horizontale et verticale, `étirer` compris (§8quater.6) | non |
| 5 | **Espacement** | Remplissage (4 champs + cadenas de liaison) · Marge (4 champs) · Espacement enfants (H, V) — §8quater.5 | **oui** |
| 6 | **Apparence** | fond, bordure, rayon de coin, opacité | non |
| 7 | **Typographie** | police, graisse, taille, interlignage, alignement du texte — **présente seulement si l'élément porte du texte** | non |
| 8 | **Effets** | pile d'effets (§8ter) | **oui** |
| 9 | **Points de rupture** | liste, avec le badge ambré des règles jamais atteintes (§8quater.4) | **oui** |
| 10 | **Layout parent** | n'apparaît **que** si l'élément est dans un conteneur live (VBox/HBox/Grid/Flow/Stack, doc 2 §8) : gap, colonnes, tailles, exposés comme des `PropertyRow` ordinaires | non |

⚠️ **Une section qui ne s'applique pas ne s'affiche pas — elle ne s'affiche pas
vide.** Une section vide fait croire à une propriété manquante ; une section
absente dit que la question ne se pose pas. En revanche un **champ** d'une section
affichée qui ne s'applique pas se **grise** au lieu de disparaître, pour que la
ligne garde sa place et que l'œil n'ait pas à la rechercher.

⚠️ **L'état replié/déplié de chaque section se retient par type d'élément, pas
globalement.** Celui qui déplie Typographie sur un texte ne veut pas la voir
dépliée sur un panneau.

#### Onglet **Widget** — les propriétés du rôle

Propriétés spécifiques au rôle promu (bind, min/max de valeur, flags — doc 2
§4/§8). Champ **flags combinables** rendu comme une liste de cases à cocher
plutôt qu'un champ texte brut (ex. `NkGuiWindowFlags` : `Resizable`, `Closable`,
`NoTitleBar`…), cohérent avec la grammaire `flags := Identifier ('|' Identifier)*`
du doc 2 §3.

En tête de cet onglet : les **états du rôle** (repos, survol, pressé, désactivé,
focus) sous forme d'une rangée de pastilles ; cliquer une pastille bascule le
canvas sur l'édition de cet état. Une pastille **jamais éditée** se distingue de
celle qui l'a été — c'est ainsi que l'outil tient sa promesse « aucun état oublié »
(§14ter).

⚠️ **Si l'élément ne porte pas de rôle, cet onglet affiche une seule phrase qui
renvoie à la ligne de rôle de l'en-tête** — il ne duplique pas le contrôle. Le
rôle reconfigure les *trois* onglets ; son contrôle ne peut pas vivre dans l'un
d'eux (§14ter).

#### Onglet **Behavior** — les événements

Liste des événements disponibles pour ce rôle (catalogue doc 2 §9), chaque ligne
avec un statut visuel :

| pastille | sens |
|---|---|
| grise | non lié |
| bleue | lié à un Node Graph / du Code |
| orange | lié mais callback non déclaré (`E-CALLBACK-UNDECLARED`, doc 2 §12) |

Clic sur une ligne = bascule le canvas en mode Behavior **filtré sur cet événement
précis**.

Sous les événements du rôle, une seconde liste — **les événements ajoutés à la
main** (§14ter) — et un bouton `+ Nouvel événement`. Les deux listes sont
séparées et étiquetées : on doit voir d'un coup d'œil ce qui vient du contrat et
ce qui vient du concepteur, **parce qu'un rôle retiré emporte la première et laisse
la seconde**.

Un événement du rôle que l'utilisateur a **retiré** reste affiché, barré, avec un
bouton pour le rétablir. *Retirer n'est pas supprimer : sans trace, on ne
distingue pas un événement écarté d'un événement jamais vu.*

Si l'élément sélectionné est un **groupe ou une page entière** plutôt qu'un widget
unique, l'onglet Behavior affiche les événements de portée « page » (`OnPageOpen`,
`OnPageClose`…) plutôt qu'une liste vide.

#### Sélection multiple

⚠️ **Ce cas ne peut pas rester non spécifié — c'est celui où un éditeur ment le
plus facilement.** Quand plusieurs éléments sont sélectionnés :

- une propriété **identique** partout s'affiche normalement ;
- une propriété qui **diffère** s'affiche `Multiple` en italique grisé — jamais la
  valeur du premier élément, qui ferait croire à une uniformité fausse ;
- **éditer** ce champ applique la nouvelle valeur à toute la sélection ;
- une section ne s'affiche que si **tous** les éléments sélectionnés la portent.

---

## 13. Système de pastilles rétractables (Dock Rail)

Réponse directe à la demande : éviter que Palette, Callback Manager, Chat
IA, Console, Preview ne saturent l'écran en permanence, tout en restant
accessibles en un clic.

### 13.1 Principe
Deux rails fins (28px) verticaux, ancrés aux bords extérieurs de la zone
canvas (donc **en dehors** de la Hiérarchie et de l'Inspecteur, qui sont des
panneaux fixes distincts — voir §11-12), plus un rail horizontal fin en bas
de fenêtre. Chaque rail contient des **pastilles** (icônes rondes/carrées
28x28, une par panneau secondaire) empilées verticalement (rails latéraux)
ou horizontalement (rail bas).

| Rail | Pastilles par défaut |
|---|---|
| Gauche (sous la Hiérarchie, rail additionnel) | Palette de composants, Bibliothèque de composants |
| Droite (sous l'Inspecteur, rail additionnel) | Gestionnaire de callbacks, Chat IA |
| Bas | Console/Validation, Preview/Test |

### 13.2 Trois états par pastille
1. **Repliée** (défaut) : seule l'icône est visible dans le rail, tooltip au
   survol donnant le nom.
2. **Dépliée en overlay** : un clic simple fait glisser un panneau **par-
   dessus** le canvas (overlay semi-transparent en dessous, comme un tiroir),
   depuis le bord correspondant, largeur/hauteur par défaut ~320px. Ne
   redimensionne pas le canvas. Un clic ailleurs sur le canvas, ou un
   deuxième clic sur la pastille, la referme. Petite icône "épingle" en
   en-tête du panneau pour la faire passer en mode **ancré** (état 3).
3. **Ancrée** : le panneau devient un vrai panneau dockable poussant le
   canvas (redimensionne réellement la zone de travail), intégré au système
   de docking classique décrit en doc 1 §5 (glisser pour redocker ailleurs,
   empiler en onglets avec Hiérarchie/Inspecteur si l'utilisateur le
   souhaite). Une icône "détacher" symétrique permet de revenir en overlay
   ou de passer directement en **fenêtre flottante indépendante** (état 4).
4. **Fenêtre flottante** : identique au comportement de détachement décrit
   pour Aetherine (doc 1 §5) — fenêtre indépendante, toujours au-dessus,
   redockable en la glissant vers un rail ou vers un bord du canvas.

### 13.3 Règle de non-encombrement
Au maximum **une pastille par rail peut être dépliée en overlay
simultanément** par défaut (dérouler une deuxième pastille du même rail
referme la première) — sauf si l'utilisateur a explicitement ancré ou
détaché un panneau, auquel cas il n'est plus soumis à cette règle (il occupe
sa propre place stable). Ce compromis garde l'écran lisible sans empêcher un
usage avancé multi-panneaux pour qui le souhaite.

---

## 14. Palette de composants

Contenu de la pastille "Palette" (§13), catalogue exact des rôles disponibles
(doc 1 §4.5, doc 2 §8), généré automatiquement — jamais tenu à la main.

- Grille d'icônes+labels par catégorie repliable (`Conteneurs`, `Entrées`,
  `Affichage`, `Navigation`, `Feedback` — regroupement logique des rôles du
  tableau doc 2 §8)
- Glisser un rôle directement sur le canvas = insère le widget avec ses
  valeurs par défaut **et** promeut automatiquement (pas de forme
  intermédiaire à promouvoir manuellement dans ce cas précis)
- Recherche par nom de rôle ou de fonction moteur associée (ex. taper
  `SliderFloat` ou `slider` trouve le même résultat)

---

## 14bis. Bibliothèque de composants — import, instances, overrides

Réponse au besoin « charger des composants existants, les modifier comme on
veut, sans restriction ». Distinct de la Palette (§14, qui liste les **rôles
natifs du moteur**) : la Bibliothèque de composants liste les **assemblages
réutilisables propres au projet ou importés** (un bouton stylisé complet,
une carte, un menu entier).

### Créer un composant
Clic-droit sur une sélection (Hiérarchie ou Canvas) → `Convertir en
composant` (déjà mentionné doc 3 §8) : la sélection devient un **composant
maître**, stocké dans la Bibliothèque, remplacé sur le canvas par une
**instance** qui le référence.

### Importer un composant existant
- Depuis un fichier `.nkgui` externe (`Fichier > Importer > Composant…`)
- Depuis le Marketplace du Launcher (doc 3 §3, sidebar `Marketplace de
  composants`)
- Depuis un autre projet ouvert (glisser un composant d'un onglet de projet
  vers un autre, cf. §6)

### Modifier une instance — liberté totale sans casser la source
- Par défaut, une instance suit son maître (toute modification du maître se
  répercute sur toutes les instances).
- **Toute propriété modifiée directement sur une instance** (couleur,
  texte, taille, sous-élément ajouté/supprimé…) devient un **override
  local** : petite pastille violette dans la marge de l'Inspecteur à côté de
  la propriété concernée (même traitement visuel que les propriétés
  modifiées par rapport à un parent dans Aetherion, `01-specification-
  humaine.md` §8), avec un clic droit `Réinitialiser au maître` pour annuler
  l'override ponctuel.
- **Bouton `Détacher l'instance`** (Inspecteur, en-tête, ou clic-droit) :
  rompt définitivement le lien au maître, l'élément redevient un
  assemblage de formes/widgets 100% libre, éditable au sommet près (§8bis),
  avec effets (§8ter) et animations (§9bis) propres — c'est la réponse
  directe à « la designer de manière libre et complète » quand l'utilisateur
  ne veut plus aucune contrainte d'héritage.
- Éditer le maître directement : double-clic sur une instance dans le
  Canvas avec `Alt` maintenu (ou bouton dédié dans l'Inspecteur) ouvre le
  maître dans un onglet de canvas isolé (pas un nouvel onglet de projet,
  un mode d'édition contextuel avec bandeau `Édition du composant maître —
  Retour` en haut du canvas).

### Panneau Bibliothèque (pastille du rail, §13)
Grille de miniatures des composants maîtres du projet (+ ceux importés),
recherche, tri par date/nom/utilisation, badge du nombre d'instances actives
par composant, glisser directement sur le canvas pour créer une nouvelle
instance.

### 14bis.1 Trois provenances, trois droits

> **Demande de Rodolf, 2026-08-20** : réutiliser ce qu'on a déjà fait, utiliser ce
> que d'autres ont fait, et utiliser ce que le système fournit.

Ce sont trois choses différentes, et **la bibliothèque doit les séparer visiblement**
parce qu'elles n'accordent pas les mêmes droits :

| provenance | portée | éditer la source | qui décide de la mise à jour |
|---|---|---|---|
| **Projet** | ce projet seul | oui, directement | sans objet |
| **Partagé** | toutes **mes** applications | oui, directement | **moi** |
| **Importé** | vient d'autrui | non : **bifurcation explicite** | l'amont, mais j'applique |
| **Système** | fourni par le moteur | **jamais** | la version du moteur |

⚠️ **« Partagé » a été ajouté le 2026-08-20 après une proposition de Rodolf**, qui
voulait « promouvoir un composant de projet en composant système pour ne pas le
perdre d'une application à l'autre ». Le besoin est juste ; **le mot était faux**, et
la confusion aurait coûté cher.

Un composant **Système** suit **la version du moteur** : on ne le modifie pas, et il
change quand NKGui change. Y déposer ses propres composants produirait l'une de ces
deux catastrophes, sans qu'on puisse choisir laquelle :

- soit une mise à jour du moteur **écrase du travail personnel** ;
- soit les composants du moteur **accumulent des modifications locales** et cessent
  d'être les mêmes d'une machine à l'autre.

**Partagé** donne exactement ce que Rodolf demande — vivre hors du projet, survivre
d'une application à l'autre — **avec le versionnage entre ses mains**. C'est la même
portée, sans le couplage au moteur.

> *Deux choses qui se mettent à jour selon deux calendriers différents ne peuvent
> pas partager un tiroir.*

`Promouvoir en composant partagé` figure au clic droit d'un composant de Projet.
L'opération est **réversible** et le composant garde son historique.

Le panneau Bibliothèque affiche **Partagé, Importé et Système** ; les composants de
**Projet** vivent désormais dans la Hiérarchie (§11.6).

⚠️ **Modifier un composant importé ne doit JAMAIS le bifurquer en silence.** Deux
chemins légitimes, et l'outil demande **une fois** lequel on prend :

- **surcharge** — l'instance change, la source reste liée et **continue de recevoir
  les mises à jour** ;
- **bifurcation** — une copie devient un composant de Projet, nommée, et **ne reçoit
  plus rien**.

Sans ce choix, on croit surcharger et on a bifurqué : la mise à jour amont n'arrive
jamais, et **rien ne le signale** — le composant continue de fonctionner, simplement
il ne bouge plus. *Un lien rompu sans bruit ne se découvre qu'au moment où l'on
comptait dessus.*

### 14bis.2 Mise à jour d'une source externe

Un composant importé porte **la version de sa source**. Quand l'amont change, la
bibliothèque affiche une pastille « mise à jour disponible » sur sa miniature.

⚠️ **Une mise à jour ne s'applique jamais toute seule.** On la demande, et l'outil
montre **ce qui change avant d'appliquer** : propriétés modifiées, sous-éléments
ajoutés ou retirés, surcharges locales qui entrent en conflit.

Une mise à jour automatique modifierait ce qui est à l'écran sans que personne
l'ait demandé — et on le découvrirait au pire moment, en croyant à une erreur de
sa propre main. **Le contrôle de version d'un projet ne doit pas dépendre de
l'humeur d'un serveur.**

⚠️ **Une surcharge locale en conflit avec la mise à jour se conserve et se nomme**,
elle ne se résout pas d'office. L'outil présente les deux valeurs et laisse
choisir ; il n'a aucun moyen de savoir laquelle porte l'intention.

### 14bis.3 Un composant importé apporte son contrat de rôle

Depuis §14ter, un composant peut porter un **rôle de projet**. L'import doit donc
amener **le composant et son contrat** — événements, propriétés, états.

⚠️ **Un composant importé sans son contrat donne un objet dont les événements ne
pointent nulle part.** Il se dessine, il s'instancie, et il ne se branche pas —
l'échec le plus déroutant, parce que tout a l'air correct.

⚠️ **Et si le contrat importé dérive d'un rôle natif que ce moteur-ci ne porte pas
(version différente), l'import est REFUSÉ et le rôle manquant est nommé.** Importer
à moitié produirait un composant partiellement inerte — exactement le défaut que
§14ter.2 interdit, arrivé par une autre porte.

### 14bis.4 Les composants du système suivent le moteur, pas le projet

Un composant Système change quand le moteur change. **Chaque instance retient donc
la version du moteur avec laquelle elle a été posée**, et la validation signale
l'écart plutôt que de le laisser deviner.

⚠️ **Sans cette trace, une mise à jour du moteur peut changer une interface sans
que rien dans le projet n'ait bougé** — et on cherchera la cause dans le projet,
qui est le seul endroit où elle ne se trouve pas.

---

## 14ter. Donner un rôle à un composant

⚠️ **Le moment décisif de tout l'outil.** On dessine une forme ; on lui **attribue
un rôle** ; elle devient un composant qui *se comporte*.

**Où** (décision Rodolf, 2026-08-20) : **la ligne de rôle de l'en-tête de
l'Inspecteur EST le contrôle**, au même endroit dans tous les cas :

| état | ce que lit la ligne | au clic |
|---|---|---|
| aucun rôle | `Aucun rôle — Attribuer…` en bleu | la liste des rôles |
| rôle posé | `Rôle : bouton ▾` | la liste, plus `Changer de rôle` et `Retirer le rôle` |
| sélection multiple hétérogène | `Rôle : Multiple` | attribuer s'applique à toute la sélection |

⚠️ **Pourquoi l'en-tête et pas l'onglet Widget.** Le rôle reconfigure les **trois**
onglets à la fois — Widget reçoit ses propriétés, Behavior ses événements, Design
ses états et ses contraintes d'accessibilité. Un contrôle qui redéfinit les trois
ne peut pas vivre à l'intérieur de l'un d'eux : on regarderait les événements d'un
rôle sans pouvoir atteindre le rôle qui les produit.

Le clic droit → `Devenir…` sur le canvas et dans la Hiérarchie reste, comme
raccourci vers la même commande.

La liste complète des rôles natifs est en §14ter.3 — **elle est dérivée du
catalogue réel de NKGui, pas inventée.**

**Ce que l'attribution apporte immédiatement** — sans rien écrire :

- **les états** du rôle : repos, survol, pressé, désactivé, focus — chacun
  éditable visuellement, et l'outil garantit qu'aucun n'est oublié ;
- **les événements** du rôle : un bouton reçoit `pressé`, `relâché`, `cliqué`,
  `survol entré`, `survol sorti` — déjà nommés, déjà branchables ;
- **les propriétés** du rôle : libellé, icône, activé/désactivé, valeur ;
- **les sous-éléments attendus** : un bouton attend un libellé et
  optionnellement une icône ; l'outil les propose et signale ce qui manque ;
- **les contraintes d'accessibilité** : taille de cible minimale selon la cible
  du cadre (§8quater.1), contraste du texte sur le fond.

⚠️ **Et l'utilisateur n'est pas enfermé dans le rôle.** Il peut :

- **ajouter ses propres événements**, nommés librement, à côté de ceux du rôle ;
- **retirer un événement du rôle** dont il n'a pas l'usage ;
- **redéfinir l'apparence d'un état** sans perdre les autres ;
- **composer** : un élément peut porter un rôle **et** contenir des
  sous-éléments qui portent les leurs.

> **Le rôle donne un point de départ complet, jamais une cage.**

⚠️ **Un rôle retiré ne détruit pas le travail** : les événements créés à la main
survivent, les états dessinés restent des groupes ordinaires. On perd le contrat,
on ne perd pas le dessin.

### 14ter.1 Changer de rôle

Retirer était spécifié ; **changer ne l'était pas**, et c'est le cas dangereux —
`bouton` → `interrupteur` laisse des événements que le nouveau rôle ne connaît pas
et des états qu'il n'a pas.

⚠️ **Un événement de l'ancien rôle qui était lié ne disparaît pas : il devient un
événement ajouté à la main.** Sa liaison pointe vers un callback qui existe dans le
code ; l'effacer casserait le programme sans que le programme ait bougé, et
l'erreur apparaîtrait à la compilation, **loin du geste qui l'a causée**.

Le changement ouvre une confirmation qui **énumère, un par un** : ce qui devient
un événement ajouté, ce qui reste en groupe ordinaire, ce qui sera perdu.

⚠️ **Jamais un « êtes-vous sûr ? » générique.** Une confirmation qui n'énumère pas
déplace la responsabilité sans donner de quoi décider — et on apprend à la cliquer
sans la lire, ce qui la rend pire qu'absente.

### 14ter.2 Définir ses propres rôles

Oui — **mais deux choses très différentes se cachent derrière cette demande**, et
l'outil ne doit surtout pas les confondre.

| | **Rôles natifs** | **Rôles de projet** |
|---|---|---|
| d'où ils viennent | catalogue fermé, adossé aux widgets NKGui | définis par l'utilisateur, dans le projet |
| ce qui les fait vivre | du code dans le moteur | de la **composition** |
| extensibles depuis l'éditeur | **non** | oui |

**Comment on définit un rôle de projet** : on prend un groupe déjà dessiné, on
déclare ses états, ses événements, ses propriétés et ses sous-éléments attendus,
et on l'enregistre comme rôle réutilisable ailleurs dans le projet. C'est le
prolongement naturel des composants de bibliothèque (§14bis) : même matière, mais
on déclare un **contrat** en plus d'une apparence.

⚠️ **Un rôle de projet doit déclarer ce dont il dérive** — soit rien (pure
composition : il n'a d'interaction que celle de ses enfants), soit un rôle natif
qu'il étend (« carte cliquable » dérive de `bouton`, et reçoit donc réellement le
comportement de clic du moteur).

⚠️ **Sans cette déclaration, on fabrique des rôles qui ont l'air interactifs dans
l'éditeur et sont inertes à l'exécution.** C'est le pire défaut possible pour cet
outil : l'éditeur promet un comportement que rien n'implémente, et l'écart ne se
découvre qu'une fois l'application lancée.

⚠️ **Et un rôle de projet ne peut pas créer une primitive.** Un potentiomètre
circulaire, une roue chromatique — leur comportement est du code, il n'apparaît
pas parce qu'on l'a nommé. L'outil doit dire cette limite au moment de la
création, pas la laisser découvrir. *Nommer une chose ne la fait pas exister.*

Les deux familles s'affichent **dans deux groupes séparés** de la liste des rôles,
les natifs d'abord, et un rôle de projet porte visiblement le nom de ce dont il
dérive.

### 14ter.3 Catalogue des rôles natifs

⚠️ **Cette liste est relevée dans `Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h`,
le 2026-08-20. Elle n'est pas une proposition : c'est ce que le moteur sait
instancier aujourd'hui.** Un rôle natif qui ne figure pas dans ce fichier serait un
`id` que le runtime ne sait pas construire.

### ⚠️ Correction du 2026-08-21 : j'avais spécifié sans lire le document 2

Ce catalogue a été dérivé des en-têtes du moteur **sans jamais ouvrir
`2_NkUIDesign_Langage_Description_NodeBlueprint.md`**, qui définit le format
`.nkgui` et porte déjà, en §8, sa propre table de correspondance. Trois écarts en
résultent, et il faut les connaître avant d'écrire quoi que ce soit qui produise
des fichiers.

**1. Les noms français de ce tableau ne sont PAS les identifiants du fichier.**

| ici, dans l'interface | dans un `.nkgui` (doc 2 §8) |
|---|---|
| bouton | `Button` |
| champ de saisie | `InputText` |
| case à cocher | `Checkbox` |
| liste déroulante | `Combo` |
| boîte verticale | `VBox` |
| en-tête repliable | `CollapsingHeader` |

⚠️ **Ce sont deux registres distincts : le français est l'étiquette qu'on montre,
le `NkPascalCase` est ce qui s'écrit dans le fichier.** Sans cette table écrite
noir sur blanc, quelqu'un finira par écrire `bouton` dans un `.nkgui` — et le
compilateur le refusera sans que personne comprenne pourquoi le mot affiché par
l'outil n'est pas celui que le format accepte.

**2. Le doc 2 disait déjà que l'infobulle n'est pas un nœud.** §14quinquies.1
présente cette correction comme une découverte de la nuit du 20 août. Elle était
écrite depuis le début : *« `Tooltip` (modificateur, pas un nœud) »*. **La
conclusion était bonne, la découverte ne l'était pas** — il suffisait de lire.

**3. `bouton à répétition` n'est pas un rôle à part.** Doc 2 le porte comme deux
propriétés de `Button` : `repeatDelay` et `repeatRate`. Ce tableau l'a compté comme
un rôle natif distinct, parce que `RepeatButton` existe dans les en-têtes du
moteur. **Les deux sont vrais à leur niveau** — fonction moteur d'un côté, rôle du
format de l'autre — et c'est le format qui fait foi pour le catalogue de rôles.

### 🔴 Et un manque qui bloque : le format ne sait pas dire « texte »

**La table du doc 2 §8 ne contient ni `Text`, ni `Label`, ni `Spacer`.** Le moteur
les porte (`Text`, `TextWrapped`, `TextAt`, `Spacer` dans `NkGuiWidgets.h`), mais
**le langage `.nkgui` n'offre aucun rôle pour les écrire.**

⚠️ **Un libellé est l'élément le plus fréquent de toute interface.** Sans rôle
`Text`, un document ne peut pas porter un titre, une étiquette de champ, un
paragraphe — et tout convertisseur bute sur `<p>`, `<h1>`, `<span>`, qui sont
partout.

Trois issues possibles, à trancher par Rodolf :

1. **c'est une omission du doc 2 §8** — le plus probable — et il faut y ajouter
   `Text` avec ses propriétés (`text`, `wrap`, `align`) ;
2. le texte vit dans la section `geometry` — mais `geometry` est décrite comme
   « formes visuelles brutes », ce qui en ferait un calque et non un widget, donc
   sans rôle, sans état, sans événement ;
3. le texte n'est qu'une propriété `label` d'un autre rôle — ce qui interdirait le
   texte libre.

**Tant que ce n'est pas tranché, aucun convertisseur ne peut produire un document
complet.**

### ✅ Tranché par Rodolf, 2026-08-21 : le format accueille `Text` et `Spacer`

Issue **1** retenue. Mais les trois mots de la question n'ont pas le même statut, et
les traiter pareil créerait un doublon.

#### `Text` — un vrai rôle, et il lui faut plus que `text`

| propriété | pourquoi |
|---|---|
| `text` | la chaîne |
| `wrap` | passe à la ligne ou non |
| `align` | `start` · `center` · `end` · `justify` |
| **`maxLines`** | nombre maximal de lignes |
| **`overflow`** | `ellipsis` · `clip` · `visible` |
| `for` | *optionnel* : identifiant du contrôle que ce texte étiquette |

⚠️ **`maxLines` et `overflow` ne sont pas du confort.** Le texte qui ne rentre pas
est **le défaut de disposition le plus fréquent qui existe** — un libellé traduit en
allemand, un nom d'utilisateur long, et la mise en page casse. Si le format ne sait
pas dire « tronque avec des points de suspension », **tout document produit portera
ce défaut sans pouvoir l'exprimer**, et l'outil n'aura aucun moyen de le signaler.

#### `Label` — **non**, ce serait un doublon

`Button` porte déjà `label`. `MenuItem`, `CollapsingHeader`, `TreeNode`,
`Selectable` aussi. Ajouter un rôle `Label` donnerait **deux façons d'écrire le
texte d'un bouton**, et deux façons d'écrire la même chose finissent toujours par
diverger.

> **Le texte est une PROPRIÉTÉ quand il appartient au contrat du contrôle** — un
> bouton sans libellé n'est pas un bouton. **Il est un NŒUD quand il tient debout
> seul** : un paragraphe, un titre de section, une légende.

⚠️ **Le cas qui tranche, et qui justifie `for`** : l'étiquette d'un champ de
saisie. En propriété du champ, on ne peut plus la placer où l'on veut — au-dessus,
à côté, alignée à droite — et les concepteurs en ont besoin. En nœud indépendant,
on perd le lien sémantique dont un lecteur d'écran a besoin pour annoncer « champ
Adresse e-mail » au lieu de « champ de saisie ». **`Text` avec `for` garde les
deux** : la liberté de placement et le lien déclaré.

#### `Spacer` — **oui, mais pour une raison qui n'est pas technique**

Techniquement il est **redondant** : un conteneur vide en `expand` fait exactement
le même travail.

⚠️ **On le garde quand même, parce qu'il déclare une INTENTION.** `Spacer` dit
« ce vide est voulu » ; un `Panel {}` vide dit « quelqu'un a oublié de le remplir ».
**Aucun validateur ne peut distinguer les deux** sans que l'auteur le dise. Un
rôle dédié coûte une ligne dans la table et évite un avertissement inutile sur
chaque espace délibéré.

---

### 🔴 Ce que la question de Rodolf a fait sortir de plus gros : l'apparence n'a nulle part où aller

En vérifiant où mettre les propriétés de `Text`, j'ai relevé ceci dans le
document 2 : **aucun rôle de la table §8 ne porte la moindre propriété
d'apparence.** Ni couleur, ni police, ni remplissage, ni rayon de coin, ni ombre.
Sur aucun rôle. Et §4 précise que **le compilateur rejette toute propriété absente
du schéma du rôle**.

La seule trace d'un mécanisme de style est un `include "Theme.nkgui"` dans un
exemple — **dont la grammaire n'est définie nulle part.**

**Ce n'est peut-être pas un défaut.** Séparer la structure de l'apparence est une
vertu, et c'est cohérent avec NKGui, qui est une interface en mode immédiat dont
l'apparence vient d'une pile de style, pas de chaque widget.

⚠️ **Mais alors ce document-ci est en contradiction avec le format.** §12.2 donne
à l'Inspecteur une section **Apparence** (fond, bordure, rayon, opacité) et une
section **Typographie**, par élément. §8ter donne une pile d'effets par élément.
**Si l'apparence ne peut pas s'écrire par widget, tout ce que l'Inspecteur laisse
régler n'a nulle part où aller** — et l'éditeur exporterait moins que ce qu'il
laisse dessiner.

**Trois issues, et il faut en choisir une :**

| | |
|---|---|
| **a. Le widget porte des surcharges** | un bloc de style optionnel par nœud ; le thème donne les défauts |
| **b. L'Inspecteur édite le THÈME** | l'apparence se règle par style nommé, pas par élément |
| **c. L'apparence va dans `geometry`** | les widgets restent purs — mais la couleur d'un bouton vivrait dans une autre section que le bouton |

**Ma recommandation : (a), avec un garde-fou.** (c) est à écarter : deux sections à
tenir synchronisées pour un même objet, c'est la garantie qu'elles divergeront.
(b) est le plus propre en théorie et le plus frustrant en pratique — on ne peut
plus rendre *ce* bouton-ci rouge sans créer un style.

⚠️ **Le garde-fou de (a) : la validation compte les surcharges.** Un document où
presque chaque élément surcharge son apparence a **abandonné son thème sans le
dire** — changer le thème ne changera plus rien, et c'est exactement ce qui rend un
système de design sans valeur. L'outil doit le dire avant que ce soit irréversible.

**C'est une décision plus lourde que celle du texte, et elle appartient à Rodolf.**

**Actions**

| rôle | API NKGui |
|---|---|
| bouton | `Button` / `ButtonEx` |
| bouton à répétition | `RepeatButton` |
| bouton image | `ImageButton` |
| bouton de couleur | `ColorButton` |
| élément de menu | `MenuItem` |

**Saisie**

| rôle | API NKGui |
|---|---|
| champ de saisie | `InputText` / `InputTextEx` |
| champ multiligne | `InputTextMultiline` |
| champ entier | `InputInt` |
| champ décimal | `InputFloat` |
| curseur | `SliderFloat` |
| glisseur | `DragFloat` / `DragInt` |
| sélecteur de couleur | `ColorEdit4` / `ColorPicker4` |

**Sélection**

| rôle | API NKGui |
|---|---|
| case à cocher | `Checkbox` |
| case à trois états | `CheckboxTristate` / `CheckBox3` |
| liste déroulante | `BeginCombo` |
| liste | `BeginListBox` |
| élément sélectionnable | `Selectable` / `SelectableEditable` |
| élément de sélection | `SelectItem` / `SelectItemEditable` |
| nœud d'arbre | `TreeNode` / `TreeNodeEditable` |

**Navigation**

| rôle | API NKGui |
|---|---|
| barre d'onglets | `TabBar` / `TabBarEx` / `TabBarEditable` |
| barre de menu | `BeginMenuBar` |
| menu | `BeginMenu` |
| menu contextuel | `BeginPopupMenu` |
| en-tête repliable | `CollapsingHeader` |
| espace d'ancrage | `DockSpace` / `DockSpaceOverViewport` |

**Conteneurs**

| rôle | API NKGui |
|---|---|
| fenêtre | `Begin` / `EndWindow` |
| panneau | `BeginPanel` |
| groupe | `BeginGroup` |
| enfant | `BeginChild` |
| boîte verticale | `BeginVBox` |
| boîte horizontale | `BeginHBox` |
| grille | `BeginGrid` |
| pile | `BeginStack` |
| flux | `BeginFlow` |
| tableau | `BeginTable` |
| séparateur ajustable | `Splitter` |

**Affichage** — aucun état, aucun événement

| rôle | API NKGui |
|---|---|
| texte | `Text` / `TextWrapped` / `TextAt` |
| image | `Image` |
| barre de progression | `ProgressBar` |
| courbe | `PlotLines` |
| histogramme | `PlotHistogram` |
| séparateur | `Separator` |
| espace | `Spacer` |

⚠️ **Les rôles d'Affichage portent un contrat à états et événements VIDES.** C'est
légitime — `NkRoleContract` l'autorise — mais l'interface ne doit pas leur
présenter les mêmes affordances qu'à un bouton : l'onglet Behavior d'un `texte`
n'affiche pas une liste vide, il dit que ce rôle n'a pas d'événements.

#### Deux rôles promis par la spécification que le moteur ne porte pas

⚠️ **`interrupteur` et `bouton radio` n'existent pas dans NKGui.** Vérifié le
2026-08-20 : aucun `Switch`, aucun `Toggle`, aucun `RadioButton` dans les en-têtes.
L'inventaire complet des manques est en §14ter.4.
L'`interrupteur` figurait pourtant dans la liste d'exemples de ce document depuis
le début.

C'est exactement le défaut contre lequel §14ter.2 vient d'écrire une règle : **un
rôle annoncé que rien n'implémente.** Deux issues, au choix de Rodolf, mais il faut
en choisir une :

⚠️ **Et les deux cas ne se valent pas.**

**`interrupteur` — rôle de projet dérivant de `case à cocher`.** Même état
booléen, mêmes événements, seule l'apparence change. **Zéro ligne de moteur**, et
rien ne manque à l'exécution.

**`bouton radio` — la même astuce ne marche PAS.** Un bouton radio n'est pas une
case à cocher habillée autrement : il porte l'**exclusivité dans un groupe**, et
aucune case à cocher ne décoche ses voisines. C'est un comportement, donc du code.
Il faut soit un `RadioButton` dans NKGui, soit un conteneur natif « groupe
exclusif ». *La dérivation copie une apparence, elle n'invente pas une règle.*

⚠️ **Tant que le choix n'est pas fait, ils ne doivent pas apparaître dans le groupe
« RôLES NATIFS » de l'interface.** Une planche qui les y montre — comme celle du
2026-08-20 11h55 — documente une promesse qui n'a pas de code derrière.

#### Le glisser-déposer n'est pas un rôle

`BeginDragSource` et `BeginDropTarget` existent dans NKGui, mais ils ne décrivent
pas *ce qu'un élément est* — ils décrivent *ce qu'il accepte*. Un bouton, une
ligne de liste et un nœud d'arbre peuvent tous être source ou cible.

⚠️ **Donc ce sont des capacités cochables sur n'importe quel rôle, pas des entrées
du catalogue.** En faire des rôles obligerait à créer « bouton déplaçable »,
« ligne déplaçable », « nœud déplaçable »… et le catalogue doublerait à chaque
capacité transversale ajoutée.

#### Conséquence sur le menu de la ligne de rôle

Quarante entrées ne tiennent pas dans une liste plate.

- **Les natifs s'affichent groupés par famille** — Actions, Saisie, Sélection,
  Navigation, Conteneurs, Affichage — comme les outils de §7.1. Même principe,
  même vocabulaire, pas un second système de rangement ;
- **le champ de recherche devient indispensable**, plus décoratif : il est
  pré-focalisé à l'ouverture, et filtre sur les deux groupes à la fois ;
- **les rôles récemment utilisés remontent** dans un petit groupe en tête.

⚠️ **Le rôle courant se marque PARTOUT où il apparaît** — même fond, même coche,
dans « Récents » *et* dans sa famille. Le rendu du 2026-08-20 12h28 le montrait
coché en tête et non coché dans Actions : le même objet, deux états. C'est ainsi
qu'on apprend à un utilisateur à ne pas faire confiance à la coche.

⚠️ **Et on ne règle pas cela en retirant le rôle courant du groupe « Récents »** :
le groupe changerait de contenu à chaque sélection, donc de hauteur, donc les
familles en dessous se déplaceraient. On perdrait le repère de position qu'un
groupe « récents » existe justement pour donner.

⚠️ **Les familles de §14ter.3 et les familles d'outils de §7.1 doivent porter les
mêmes noms.** Deux taxonomies voisines mais différentes pour les mêmes objets, et
personne ne saura plus dans laquelle chercher.

### 14ter.4 Inventaire des manques — ce qu'une trousse complète exige

⚠️ **Relevé le 2026-08-20 dans les en-têtes de NKGui.** Trois statuts, et **le
statut est la seule information qui compte** — il dit ce que ça coûte.

| statut | sens | coût |
|---|---|---|
| ✅ **natif** | le moteur le porte | rien |
| 🔵 **dérivable** | rôle de projet, par composition ou habillage d'un natif | **zéro ligne de moteur** |
| 🔴 **code** | comportement qu'aucune composition ne produit | du travail dans NKGui |

#### Actions

| rôle | statut | note |
|---|---|---|
| bouton, à répétition, image, couleur, élément de menu | ✅ | |
| bouton à bascule | 🔵 | `Checkbox` habillé en bouton — même booléen |
| bouton fractionné | 🔵 | `Button` + `BeginPopupMenu` |
| bouton d'icône seul | 🔵 | `Button` sans libellé ; §1quater invariant 3 prévoit déjà l'avertissement |
| bouton flottant d'action | 🔵 | pure apparence |
| lien hypertexte | 🔵 | `Button` habillé en texte, curseur main |
| **groupe de boutons segmentés** | 🔴 | **exclusivité** |

#### Booléens et sélection

| rôle | statut | note |
|---|---|---|
| case à cocher, à trois états, liste, liste déroulante, sélectionnable, nœud d'arbre | ✅ | |
| sélection multiple | ✅ | `NkGuiSelectFlags::MultiSelect` existe déjà |
| **interrupteur** | 🔵 | dérive de `case à cocher` |
| liste déroulante éditable | 🔵 | `InputText` + `BeginCombo` |
| jetons / étiquettes | 🔵 | composition |
| arbre (le conteneur) | 🔵 | composition de `TreeNode` |
| **bouton radio** | 🔴 | **exclusivité** |
| **groupe exclusif** | 🔴 | **la primitive manquante** |

#### Saisie

| rôle | statut | note |
|---|---|---|
| champ, multiligne, entier, décimal, curseur, glisseur, couleur | ✅ | |
| champ de recherche | 🔵 | `InputText` + icône + bouton d'effacement |
| champ à incréments | 🔵 | `InputInt` + deux `RepeatButton` |
| champ de mot de passe | 🔴 | aucun masquage dans `NkGuiInputFlags` — **à vérifier avant de trancher** |
| **curseur entier** | 🔴 | `SliderFloat` seul existe ; pas de `SliderInt` |
| **curseur à deux poignées** | 🔴 | une plage n'est pas deux curseurs côte à côte |
| **sélecteur de date, calendrier** | 🔴 | |
| **sélecteur d'heure** | 🔴 | |
| **sélecteur de fichier** | 🔴 | dialogue système — relève de NKSystem, pas de NKGui |
| **potentiomètre circulaire** | 🔴 | |
| **éditeur de texte riche** | 🔴 | |
| **éditeur de code** | 🔴 | coloration, pliage, numéros de ligne |

#### Navigation

| rôle | statut | note |
|---|---|---|
| onglets, barre de menu, menu, menu contextuel, en-tête repliable, ancrage | ✅ | |
| **accordéon non exclusif** | 🔵 | plusieurs `CollapsingHeader` empilés |
| **accordéon exclusif** | 🔴 | **exclusivité** — un seul volet ouvert à la fois |
| fil d'Ariane | 🔵 | composition |
| pagination | 🔵 | composition |
| assistant par étapes | 🔵 | composition |
| barre d'outils, barre d'état | 🔵 | composition |

#### Conteneurs

| rôle | statut | note |
|---|---|---|
| fenêtre, panneau, groupe, enfant, VBox, HBox, grille, pile, flux, tableau, splitter | ✅ | |
| zone défilante | ✅ | `NkGuiScrollState` |
| carte | 🔵 | composition |
| tiroir | 🔵 | composition + animation |
| **liste virtualisée** | 🔴 | des milliers de lignes sans les construire toutes |

#### Affichage et retour

| rôle | statut | note |
|---|---|---|
| texte, image, progression, courbe, histogramme, séparateur, espace, infobulle | ✅ | |
| badge, avatar, bandeau d'alerte, squelette de chargement | 🔵 | composition |
| notification éphémère | 🔵 | composition + minuterie |
| indicateur d'activité | 🔵 | dessin via la `DrawList` |
| **vidéo** | 🔴 | |
| **visionneuse 3D** | 🔴 | intégration NKRenderer |

#### ⚠️ Le résultat qui compte : une seule primitive débloque quatre rôles

**bouton radio · groupe de boutons segmentés · accordéon exclusif · groupe
exclusif** butent tous sur la même chose : **une seule sélection vivante parmi
plusieurs éléments frères.** Ce n'est pas quatre chantiers, c'en est **un**.

C'est le meilleur rapport de tout ce tableau : une primitive `groupe exclusif`
dans NKGui, et quatre rôles deviennent dérivables sans code supplémentaire.

⚠️ **Et c'est aussi la frontière exacte entre 🔵 et 🔴.** Tout ce qui est
dérivable l'est parce qu'aucun élément n'a besoin de savoir ce que font ses frères.
Dès qu'un élément doit en éteindre un autre, la composition ne suffit plus — *elle
copie une apparence, elle n'invente pas une règle*.

#### ⚠️ La « fenêtre modale » n'est pas un rôle

Ce document la listait comme un rôle. NKGui porte la modalité comme un **état de
contexte** (`modalDepth`, `appModal`, couche 100), pas comme un widget. C'est donc
un **drapeau de `fenêtre`**, exactement comme le glisser-déposer est une capacité
et non un rôle. Une fenêtre modale et une fenêtre ordinaire ont les mêmes états,
les mêmes événements et les mêmes propriétés ; seul leur rapport au reste change.

#### Ce que l'interface doit montrer de tout ça

⚠️ **Un rôle 🔴 non implémenté ne doit pas apparaître dans le menu**, même grisé,
même « bientôt ». Un catalogue qui montre ce qu'il n'a pas fait perdre du temps à
chaque ouverture, et le jour où l'entrée devient réelle personne ne le remarque.

En revanche, **un rôle de projet doit dire de quoi il dérive** (§14ter.2), et c'est
précisément ce qui rend ce tableau lisible pour l'utilisateur sans qu'on le lui
montre : un « interrupteur — dérive de case à cocher » dit tout seul qu'il n'a que
le comportement d'une case à cocher.

### 14ter.5 Composant et rôle ne sont pas des alternatives

⚠️ **Question de Rodolf, 2026-08-20 — et le document n'y répondait pas.** §14bis
décrit les composants, §14ter décrit les rôles, et **aucun des deux ne dit ce qu'ils
sont l'un pour l'autre.** On peut donc croire qu'il faut choisir. Il ne faut pas.

> **Un rôle est un contrat. Un composant est un morceau de document réutilisable.**
> Deux axes, pas deux options.

| | sans rôle | avec rôle |
|---|---|---|
| **pas un composant** | une forme dessinée | un bouton dessiné sur une page |
| **un composant** | une carte purement visuelle | **un explorateur de contenu** |

Les quatre cases existent et servent. La dernière est celle des gros objets
d'interface.

**Un explorateur de contenu** (*content browser*) est **les deux à la fois** : un
composant — réutilisable, instanciable, aux entrailles surchargeables — **portant
un rôle de projet** dont la dérivation est `composition`. Il déclare ses événements
(`sélection changée`, `élément activé`, `dossier ouvert`), ses propriétés (`racine`,
`filtre`, `mode d'affichage`) et ses états. Aucun widget natif unique derrière lui :
son interactivité vient entièrement de ses enfants — arbre, éléments
sélectionnables, champ de recherche.

⚠️ **Mais il restera lent tant que la liste virtualisée (§14ter.4, 🔴) n'existe
pas.** Un explorateur bâti sur des éléments réels construit dix mille lignes pour en
montrer trente. **« Dérivable » ne veut pas dire « utilisable à l'échelle »** — et
c'est une distinction que le tableau des statuts ne portait pas.

### 14ter.6 Le dialogue de fichier — deux choses sous un seul nom

⚠️ **« Dialogue de fichier » désigne deux objets sans rapport, et les confondre
promet du contrôle là où il n'y en a aucun.**

**1. Le dialogue natif du système.** Ce n'est **pas un widget** : c'est un **appel**
qui rend un chemin. Il ne vit pas dans l'arbre du document, ne porte ni états ni
apparence, ne se dessine pas sur le canvas, et relève de **NKSystem, pas de
NKGui**. Il n'a donc **aucun rôle** et n'apparaît pas dans le catalogue.

Ce qui apparaît dans le catalogue, c'est le **bouton** qui le déclenche et le
**champ** qui montre le chemin obtenu. *L'éditeur ne doit jamais laisser croire
qu'on peut styliser une fenêtre que le système dessine.*

**2. Le sélecteur de fichier dessiné dans l'application.** Celui-là est **un
composant portant un rôle de projet dérivant de `fenêtre`** (avec le drapeau de
modalité, §14ter.4), et il contient généralement un explorateur de contenu. Il se
dessine, se style, se surcharge — tout ce que le premier interdit.

⚠️ **Les deux ne se substituent pas l'un à l'autre.** Le natif connaît les
permissions, les lecteurs réseau et les raccourcis du système ; celui qu'on dessine
connaît le projet. **Un outil sérieux offre les deux et nomme lequel est lequel** —
parce que le jour où une ouverture de fichier échoue, la première question est de
savoir qui a dessiné la fenêtre.

---

## 14quater. États d'indisponibilité — désactivé, lecture seule, occupé

> **Question de Rodolf, 2026-08-20 : « des zones et des widgets peuvent être
> inactifs, donc gris — est-ce pris en compte ? »**
>
> À moitié. `désactivé` figurait dans la liste des états d'un rôle (§14ter), mais
> **rien ne disait qui en décide, ni ce qui arrive quand c'est toute une zone.**

### 14quater.1 Trois indisponibilités, souvent confondues

| état | ce qu'il dit à l'utilisateur | focus | copie du contenu |
|---|---|---|---|
| **Désactivé** | « pas disponible dans ce contexte » | **non** | non |
| **Lecture seule** | « disponible, mais pas modifiable » | **oui** | **oui** |
| **Occupé** | « temporairement indisponible, ça revient » | non | non |

⚠️ **Désactivé et lecture seule ne sont pas la même chose, et les confondre a un
coût immédiat : on ne peut plus copier le contenu d'un champ.** Un numéro de
référence qu'on ne peut ni modifier ni sélectionner oblige à le recopier à la main.

⚠️ **Occupé n'est pas désactivé non plus.** « Revenez plus tard » et « ce n'est pas
pour vous ici » appellent deux comportements différents de la part de l'utilisateur.
Les peindre du même gris lui fait attendre ce qui ne viendra pas, ou abandonner ce
qui allait revenir.

### 14quater.2 D'où vient l'indisponibilité

Le gris est une **conséquence**, jamais une propriété qu'on peint. Trois sources :

1. **Constante d'auteur** — cet élément est désactivé dans ce design ;
2. **Liée à une condition** — désactivé tant que le formulaire est invalide ;
3. **Héritée d'un ancêtre** — le panneau entier est désactivé.

⚠️ **Un enfant ne peut pas se réactiver sous un ancêtre désactivé.** Si c'était
permis, « ce panneau est désactivé » cesserait de vouloir dire quoi que ce soit : il
faudrait inspecter chaque descendant pour savoir ce qui reste cliquable. **La règle
descend, elle ne se négocie pas.**

### 14quater.3 Désactivé doit BLOQUER, pas seulement repeindre

⚠️ **Un élément désactivé sort du test de pointage et de l'ordre de tabulation.**
Un gris qui laisse passer le clic donne un contrôle **qui a l'air mort et qui agit
vivant** — et le défaut se manifeste loin de sa cause, dans le callback, où rien
n'indique que le bouton n'aurait pas dû être cliquable.

⚠️ **Et si l'élément portait le focus au moment où il devient désactivé, le focus
doit se déplacer.** Sinon la personne qui navigue au clavier reste posée sur un
élément qui ne répond à rien, sans rien à l'écran pour lui dire où elle est. Elle
n'a alors plus aucun moyen d'avancer.

### 14quater.4 Le contraste du gris se vérifie aussi

⚠️ **Un texte désactivé doit rester LISIBLE.** Il faut pouvoir lire ce qu'est un
contrôle pour comprendre pourquoi il n'est pas disponible — un bouton gris
illisible n'informe de rien, il occupe de la place.

La vérification de contraste (§14ter, `a11y.minContrast`) s'applique donc à l'état
`désactivé` **comme aux autres**. C'est l'état qu'on oublie de tester, parce qu'on
le regarde rarement et qu'il « a le droit » d'être pâle.

### 14quater.5 Dire POURQUOI

Un contrôle désactivé sans explication est une impasse : on voit qu'on ne peut
pas, on ne voit pas ce qu'il faudrait faire. Chaque élément peut donc porter une
**raison d'indisponibilité**, affichée au survol.

⚠️ **Mais la raison ne peut pas être portée par l'élément désactivé lui-même** —
puisqu'il ne reçoit plus les événements, il ne reçoit pas non plus le survol.
L'outil l'attache à une **région enveloppante** qui, elle, reste active. Sans cette
précaution l'infobulle est écrite, enregistrée, exportée — et **ne s'affiche
jamais**.

### 14quater.6 Ce que le canvas et la simulation doivent montrer

**Dans l'éditeur** : les pastilles d'état de l'onglet Widget (§12.2) permettent de
dessiner l'apparence de `désactivé` sans désactiver l'élément sur le canvas.

⚠️ **Mais le canvas doit signaler les éléments désactivés PAR HÉRITAGE**, d'une
marque discrète. Sans elle, on passe une heure à soigner un bouton qui ne sera
jamais cliquable, parce que son panneau est désactivé six niveaux plus haut.

**Dans la simulation** (§18bis.2) : quand un point de connexion figure dans la
colonne « jamais atteints » **et** que son élément était désactivé pendant toute la
session, le journal le dit — `jamais atteint : ancêtre désactivé`.

> **C'est la seule cause de non-atteinte que l'outil connaisse avec certitude.**
> Toutes les autres demandent un jugement humain ; celle-là, il la nomme.

---

## 14quinquies. Infobulles

> **Question de Rodolf, 2026-08-20.** La réponse honnête était « à moitié », et de
> la pire façon : **trois choses différentes s'appelaient « infobulle »** dans ce
> document, sans que rien ne les distingue.

| | de quoi il s'agit | dans le document exporté ? |
|---|---|---|
| **infobulle de l'éditeur** | le nom d'une pastille, d'une icône d'outil, d'une valeur tronquée | **non** |
| **infobulle conçue** | celle que l'utilisateur attache à un élément de SON application | **oui** |
| **raison d'indisponibilité** | §14quater.5 — pourquoi ce contrôle est grisé | oui, cas particulier |

### ⚠️ 14quinquies.1 Une infobulle n'est pas un élément qu'on pose

§14ter.3 listait `infobulle` parmi les **rôles natifs**, adossée à `SetTooltip`.
**C'est une erreur de classement, et je la corrige** : une infobulle n'a pas de
place à elle dans l'arbre.

- **frère** d'un élément, elle n'a rien à quoi s'accrocher ;
- **enfant** de cet élément, elle entre dans son calcul de disposition et le
  déforme.

Elle est donc une **propriété d'un élément**, comme le glisser-déposer et la
modalité (§14ter.4) — **troisième fois que ce même classement est corrigé**. Une
capacité transversale n'entre jamais au catalogue des rôles : sinon il faudrait
« bouton avec infobulle », « champ avec infobulle », et le catalogue doublerait.

Son **contenu**, en revanche, peut être riche : un sous-arbre d'éléments, pas
seulement du texte. Il vit à part, hors du flux de disposition du parent.

### 14quinquies.2 La règle qui prime toutes les autres

> ⚠️ **Une infobulle ne porte JAMAIS une information dont on a besoin pour agir.**

Sur un écran tactile il n'y a pas de survol. En navigation au clavier, elle
n'apparaît que si on la déclenche au focus. Chez quelqu'un qui utilise un lecteur
d'écran, elle peut ne jamais être annoncée.

Une infobulle **complète**, elle ne **remplace** pas. Le jour où une information
n'existe que là, elle n'existe pas pour une partie des utilisateurs — et rien dans
l'interface ne le montre à celui qui l'a conçue, puisque **lui** la voit.

### 14quinquies.3 Délais — et la règle du groupe

Trois réglages : **délai d'apparition**, **délai de disparition**, et **délai de
groupe**.

⚠️ **Le délai de groupe est celui qu'on oublie et qui décide de tout.** Une fois
qu'une infobulle est apparue, passer sur un élément voisin doit afficher la
sienne **immédiatement**, sans repayer le délai. Sans cette règle, parcourir une
barre d'outils de dix icônes coûte dix attentes — et l'utilisateur renonce à
chercher.

Le groupe se referme après un court silence : on repaie alors le délai complet.

### 14quinquies.4 Position

Un côté **préféré** (haut, bas, gauche, droite) et un **basculement** automatique
quand l'infobulle sortirait de l'écran ou du cadre simulé.

⚠️ **Une infobulle coupée par un bord est pire qu'absente** : elle occupe la
place, capte l'attention, et ne dit pas ce qu'elle avait à dire. Le basculement
n'est pas une finition, c'est la condition pour que la fonction serve.

⚠️ **Et elle ne doit jamais recouvrir ce qu'elle décrit.** Une infobulle posée sur
son propre élément empêche de vérifier de quoi elle parle.

⚠️ **Elle est transparente aux clics.** Une infobulle qui avale un clic rend
inaccessible ce qui se trouve dessous — au moment précis où l'utilisateur, ayant
lu, veut agir.

### 14quinquies.5 Clavier et fermeture

- elle apparaît **aussi au focus clavier**, pas seulement au survol — sinon celui
  qui navigue au clavier ne la voit jamais ;
- **`Échap` la ferme** sans déplacer le focus ;
- elle disparaît au clic, à la sortie du pointeur, et à la perte du focus.

### 14quinquies.6 Contenu — ce qui ne sert à rien

⚠️ **Une infobulle qui répète le libellé visible n'ajoute rien.** « Enregistrer »
sur un bouton portant déjà le mot « Enregistrer » coûte un délai et une occultation
pour zéro information. La validation le signale (`W-TOOLTIP-REDUNDANT`).

Ce qu'elle doit porter : le **raccourci clavier**, la **valeur complète** quand
l'affichage est tronqué, ou une **précision** que le libellé ne peut pas contenir.

Sur une icône seule, en revanche, elle est **obligatoire** : c'est le seul endroit
où le nom existe. La validation le signale aussi, dans l'autre sens.

### 14quinquies.7 Dans l'éditeur et dans la simulation

L'infobulle conçue est **dessinable** : elle a son apparence, ses états, ses
animations d'apparition (famille A, §9ter) — qui respectent « mouvement réduit ».

⚠️ **Et elle doit être épinglable sur le canvas pendant qu'on la dessine.** Une
infobulle qui disparaît dès que le pointeur bouge est impossible à mettre au point :
on la quitte pour aller régler sa couleur, et elle s'évanouit.

---

## 15. Gestionnaire de callbacks / contrôleurs

Contenu de la pastille "Callbacks" (§13) — vue centralisée (doc 1 §4.7) :

- Tableau : Nom du callback, Contrôleur parent, Signature (types d'arguments
  formatés comme dans le langage, ex. `(axis: Enum[X,Y,Z], value: Float) →
  Void`), colonne "Utilisé par" (liste cliquable des widgets/événements qui
  l'appellent, ouvre/sélectionne dans le canvas), colonne Statut (`Lié en
  test` / `Jamais lié` — cf. `W-CALLBACK-UNBOUND`, doc 2 §12)
- Bouton `+ Nouveau contrôleur` / `+ Nouveau callback` (ouvre un petit
  formulaire : nom, liste de paramètres typés, type de retour — génère la
  déclaration `controller`/`callback` du doc 2 §10)
- Filtre "Callbacks orphelins" (déclarés mais jamais appelés) et "Appels
  invalides" (callback appelé mais jamais déclaré, `E-CALLBACK-UNDECLARED`)
  en surbrillance rouge

---

## 16. Chat IA

Contenu de la pastille "Chat IA" (§13) — **repositionné en pastille rétractable
plutôt qu'en bande fixe en bas**, pour ne pas manger de hauteur canvas en
permanence alors qu'il n'est pas utilisé à chaque instant. Comportement :

- Pastille dans le rail droit, icône distincte (étincelle/étoile), badge
  numérique si une génération en cours ou une réponse en attente
- Dépliée : panneau de chat vertical classique (historique de messages,
  champ de saisie en bas, boutons rapides `Générer un écran`, `Modifier la
  sélection`, `Générer un comportement` qui pré-remplissent le contexte —
  cf. doc 1 §6.1)
- Chaque réponse de génération affiche une **carte d'aperçu inline dans le
  chat** (miniature de ce qui serait ajouté/modifié) avec deux boutons
  directement dans la carte : `Appliquer` / `Rejeter` — jamais d'écriture
  automatique dans le document (doc 1 §6.2)
- Peut être **détachée en fenêtre flottante** (état 4 du §13.2) pour un
  usage prolongé côte à côte avec le canvas sur un deuxième écran par
  exemple — c'est la réponse concrète à « elle peut même être flottante »
- **Entrée contextuelle additionnelle**, indépendante de la pastille : un
  petit bouton `✨` apparaît au survol d'une sélection sur le canvas
  (Design ou Behavior), ouvrant un **popover léger ancré près de la
  sélection** avec juste un champ de prompt — pour les demandes rapides
  sans ouvrir le panneau complet (ex. « rends ce panneau plus dense »,
  doc 1 tableau §6.1)

---

## 16bis. Ce que l'IA fait, et dans quelles limites

> **Demande de Rodolf, 2026-08-20** : « je veux que l'IA nous aide non seulement à
> concevoir mais aussi à designer, créer des animations et tout. »
>
> §16 décrivait **où** le chat vit et **comment** on applique une proposition. Il ne
> disait pas **ce qu'elle peut produire**, ni **ce qu'elle voit**, ni **où va ce
> qu'elle voit**.

### 16bis.1 Les six axes

| axe | ce qu'elle produit |
|---|---|
| **structure** | une hiérarchie d'éléments, groupée, nommée |
| **rôles** | une **proposition** de rôle par élément (§17.2) |
| **apparence** | couleurs, typographie, espacement, effets — dans la pile de §8ter |
| **disposition** | modes de taille, ancrages, bornes, points de rupture |
| **animation** | les **trois familles** de §9ter : transitions, ambiances, effets continus |
| **comportement** | un squelette de graphe et ses points de connexion |

⚠️ **Elle produit dans le vocabulaire de l'outil, jamais dans le sien.** Une
animation générée est un `NkAmbientAnim` ou un `NkDrivenEffect` — pas une
description libre qu'il faudrait ensuite traduire. Le jour où l'IA invente un
quatrième mécanisme d'animation, on a **deux systèmes** : celui qu'on a spécifié et
celui qu'elle écrit. Et c'est le sien qu'il faudra maintenir, parce qu'il sera dans
les documents.

Cela vaut pour tous les axes : **ce qu'elle écrit doit être exactement ce qu'un
humain aurait pu écrire à la main dans l'éditeur.** C'est la seule garantie qu'on
puisse reprendre, corriger et comprendre son travail.

### 16bis.2 ⚠️ Le modèle est local ou distant, et ça doit se voir en permanence

Le panneau affiche **en clair** quel modèle répond, et **où il tourne** :

| | ce que cela implique |
|---|---|
| **local** | rien ne quitte la machine |
| **distant** | **le contexte envoyé sort du poste** |

C'est la même question que les permissions d'un greffon (§20bis.5), et elle mérite
le même traitement : **dite avant, pas découverte après.**

⚠️ **Un basculement automatique de local vers distant — parce que le modèle local
ne répond pas, parce qu'il est trop lent — doit être REFUSÉ par défaut**, et
demandé explicitement. Un repli silencieux enverrait le projet dehors au moment
précis où personne ne regarde. *C'est le même défaut que le repli silencieux de
backend (§20ter.2), avec des conséquences d'une autre nature.*

### 16bis.3 Ce qu'elle voit — et l'utilisateur le sait

La portée du contexte est un **choix explicite**, affiché dans le panneau :

`la sélection` · `la page courante` · `le projet entier` · `+ la bibliothèque`

⚠️ **« Le projet entier » avec un modèle distant, c'est le projet entier qui
part.** L'outil affiche la portée à côté du champ de saisie, en permanence, et non
dans un réglage qu'on ouvre une fois puis qu'on oublie.

### 16bis.4 Une proposition est un ENSEMBLE, pas une suite de retouches

Une réponse produit une **proposition unique**, même quand elle touche quarante
propriétés sur six éléments. Elle s'affiche comme un **relevé de changements** :
ce qui est ajouté, ce qui est modifié (ancienne valeur -> nouvelle), ce qui est
supprimé.

- **`Appliquer`** l'applique en **une seule opération annulable** ;
- **`Rejeter`** ne laisse rien ;
- **on peut décocher des lignes** du relevé avant d'appliquer.

⚠️ **Une opération annulable, pas quarante.** Si l'application se décompose en
quarante petites modifications, revenir en arrière demande quarante annulations —
et on s'arrête au milieu, dans un état que personne n'a voulu.

⚠️ **Et le relevé se lit AVANT d'appliquer.** Une miniature d'aperçu montre à quoi
ça ressemble ; elle ne montre pas **ce qui a changé**. Deux mises en page peuvent
se ressembler et différer sur douze propriétés.

### 16bis.5 Elle dit pourquoi

Chaque proposition porte une **justification courte**, propriété par propriété
quand c'est utile : *« `expand` plutôt que `fixed` pour que le bouton suive la
largeur du formulaire »*.

⚠️ **Sans justification, on ne peut ni corriger ni apprendre — seulement accepter
ou refuser en bloc.** Et l'utilisateur qui accepte sans comprendre hérite d'un
document qu'il ne saura pas modifier.

### 16bis.6 Ce qu'elle ne fait pas d'elle-même

- **elle n'attribue pas un rôle** (§17.2) : elle le propose ;
- **elle ne corrige pas un rapport de transposition** (§8quater.1bis) : elle peut
  proposer une correction, ligne par ligne, dans un relevé ;
- **elle n'installe pas de greffon** ;
- **elle n'écrit jamais dans le document sans passage par un relevé**, y compris
  depuis le popover contextuel `✨`.

⚠️ **Le popover rapide est là pour aller vite, pas pour sauter la revue.** C'est
le raccourci qu'on serait tenté d'exempter, et c'est celui qu'on utilise le plus.

### 16bis.7 Choisir le modèle depuis l'interface

> **Demande de Rodolf, 2026-08-20.**

La bande de modèle (§16bis.2) porte un **sélecteur**. Elle liste :

- les modèles **locaux** détectés sur la machine ;
- les modèles **distants** configurés ;
- une entrée `Ajouter un modèle…`.

Chaque entrée affiche son nom, son emplacement (**local** / **distant**), et sa
**fenêtre de contexte**.

⚠️ **Passer d'un modèle local à un modèle distant redemande confirmation, en
nommant ce qui sortira de la machine** — pas un simple changement de valeur dans
une liste. C'est le même geste que d'accorder une permission à un greffon
(§20bis.5), et il mérite le même arrêt.

⚠️ **Et la portée du contexte doit être confrontée à la fenêtre du modèle.** Choisir
« le projet entier » avec un modèle dont la fenêtre ne le contient pas produit une
**troncature silencieuse** : le modèle répond, la réponse a l'air normale, et elle
est fondée sur une fraction du projet que personne n'a choisie.

L'interface l'annonce **avant** l'envoi : *« le projet fait 180 k jetons, ce modèle
en accepte 32 k — 82 % ne sera pas envoyé »*, avec le choix de réduire la portée ou
de changer de modèle. **Une troncature qui ne se dit pas est un mensonge par
omission sur la base du raisonnement.**

### 16bis.8 Mettre au point l'IA sur un élément

> **Demande de Rodolf, 2026-08-20** : « sélectionner un widget ou un bloc pour
> demander de se concentrer dessus. »

Au-delà de la portée (§16bis.3), on peut **épingler** un ou plusieurs éléments
comme **point de mire** : clic droit → `Mettre au point l'IA sur ceci`, ou bouton
dédié dans le panneau.

**Épingler n'est pas sélectionner** :

| | sélection | point de mire |
|---|---|---|
| change quand on clique ailleurs | **oui** | **non** |
| sert à éditer | oui | non |
| dit à l'IA où regarder | par défaut | **explicitement** |

⚠️ **C'est exactement pour ça que les deux doivent être distincts.** Sans épingle,
demander « et maintenant compare avec la carte d'à côté » oblige à garder le
premier élément sélectionné — donc à ne pas pouvoir aller voir le second.

**Le point de mire est visible sur le canvas** : les éléments épinglés portent un
liseré distinct et une petite épingle, et le panneau affiche `Point de mire :
Carte_Produit, Bouton_Valider (2)` au-dessus du champ de saisie.

⚠️ **Une épingle invisible est pire qu'aucune épingle.** On oublie ce sur quoi
l'IA regarde, on lui pose une question sur autre chose, et on ne comprend pas
pourquoi elle répond à côté. L'épingle se retire d'un clic sur son marqueur, et se
vide entièrement par `Effacer le point de mire`.

**Ce que le point de mire change pour l'IA** : ces éléments sont fournis **en
entier** — avec leurs propriétés, leurs états, leurs animations, leurs événements
— tandis que le reste de la portée n'est fourni qu'en **structure**. C'est ce qui
rend la mise au point utile plutôt que décorative : à fenêtre de contexte égale,
on dépense les jetons là où la question porte.

---

## 17. Génération IA — points d'entrée sur le canvas

Récapitulatif de tous les points d'entrée IA, tous alimentant le même Chat
IA / popover contextuel décrit en §16, jamais un système parallèle :

| Point d'entrée | Déclenchement |
|---|---|
| Launcher | Carte "Via IA" (§3) |
| Canvas Design vide/cadre | Popover contextuel (clic-droit → "Générer avec l'IA…") |
| Sélection Design | Bouton `✨` flottant au survol (§16) |
| Canvas Behavior, élément sélectionné | Bouton `✨` identique, propose un squelette de Node Graph (doc 1 tableau §6.1, dernière ligne) |

Chaque génération applique le badge "Généré par IA" décrit en doc 1 §6.2,
visible comme une petite icône étincelle dans l'en-tête de l'Inspecteur
(§12.2) tant que l'élément n'a pas été modifié manuellement.

### 17.1 L'IA cherche avant de générer

⚠️ **Avant de produire un composant, l'IA fouille la bibliothèque** (§14bis) — les
trois provenances — et **propose la réutilisation en premier**, la génération
ensuite. Elle dit toujours laquelle des deux elle a choisie, et pourquoi.

Générer un composant qui existe déjà crée **deux sources pour une même chose** — le
défaut contre lequel ce document se bat partout ailleurs. Et il est pire ici
qu'ailleurs : les deux versions divergeront lentement, chacune corrigée séparément,
**sans que rien ne signale qu'elles auraient dû rester identiques**.

### 17.2 L'IA peut proposer un rôle, elle ne l'attribue pas

Sur une forme dessinée, l'IA peut reconnaître une intention et **proposer** un rôle
(§14ter). Elle ne l'applique pas d'elle-même.

⚠️ **Attribuer un rôle reconfigure les trois onglets de l'Inspecteur et engage un
contrat.** Ce n'est pas une suggestion de style qu'on annule d'un `Ctrl+Z` sans y
penser : c'est le geste qui fait passer un dessin au rang de composant qui se
comporte. Il appartient à la personne.

### 17.3 Ce qu'un composant généré porte

Il entre en provenance **Projet** — il est à nous — et garde le badge « Généré par
IA » jusqu'à la première modification manuelle.

⚠️ **Le badge disparaît à la première retouche, et c'est voulu** : il ne dit pas
« ceci vient d'une IA » pour l'éternité, il dit **« personne n'a encore relu ceci »**.
Un badge qui survivrait à la relecture cesserait d'informer, et on apprendrait à ne
plus le voir.

---

## 18. Simulation du système — la fenêtre d'essai

⚠️ **Ce n'est pas un aperçu, c'est une simulation.** L'aperçu montre ; la
simulation **se comporte**. C'est la différence entre voir un bouton et pouvoir
appuyer dessus et constater ce qui arrive.

- Bouton `▶ Simuler` **en haut à droite du canvas**, dans le cluster flottant
  (§7), distinct du reste — icône verte, cohérence d'écosystème avec le Play
  d'Aetherine. Raccourci `F5`.
- ⚠️ **Ouvre une véritable fenêtre séparée**, pas un panneau — parce qu'on simule
  une **application**, et qu'une application a sa propre fenêtre, sa propre
  barre de titre, ses propres bords redimensionnables. La simuler dans un
  panneau enseignerait de mauvaises proportions.
- Cette fenêtre affiche le design **rendu par NKGui réel** — le même moteur que
  l'application finale. Ce qu'on voit là est ce qu'on livrera.
- **Sa taille de départ est celle de la cible du cadre simulé** (§8quater.1) : un cadre
  mobile ouvre une fenêtre à la taille d'un téléphone. On peut la
  redimensionner à la souris, **et le responsive s'applique en direct** — c'est
  le moyen le plus rapide de vérifier §8quater.4.
- **Les interactions sont réelles** : clic, survol, saisie, glisser. Elles
  déclenchent les callbacks avec des implémentations factices journalisées dans
  une **console de simulation** dédiée (distincte de la pastille
  Console/Validation du §13, accessible depuis le même rail bas).
- **Barre de simulation** en haut de cette fenêtre : cible courante, taille
  actuelle, `⟲ Recharger`, `⏸ Geler` (fige l'état pour inspecter), et
  `⧉ Comparer` (affiche le design à côté du rendu, pour voir l'écart).
- Les interactions déclenchent réellement les callbacks avec des
  implémentations factices journalisées dans une **console de test** dédiée
  (distincte de la pastille "Console/Validation" générale du §13, mais
  accessible depuis le même rail bas)
- En mode debug du Node Graph (§9), le chemin exécuté pendant le test
  s'illumine en direct dans le canvas Behavior si celui-ci est visible

---

## 18bis. Le système simulé — NkUIDesign comme conteneur

> **Décision Rodolf, 2026-08-20.** Énoncé à partir du cas du dialogue de fichier
> (§14ter.6), mais il vaut pour tout le reste du système.

Un service du système — ouvrir un fichier, enregistrer, lire l'heure, joindre le
réseau — **n'est pas dessiné par NkUIDesign**. Il est **appelé**, et il **rend une
donnée** : un chemin, un contenu, un échec. Ce que NkUIDesign possède, ce n'est pas
le service : c'est **le point de connexion où la donnée arrive**.

D'où le rôle de l'outil : **un conteneur dans lequel on branche l'interface, et où
l'on peut simuler les parties du système qui ne sont pas encore là.** On dessine,
on câble les événements, on simule, un journal dit si les branchements sont
atteints — puis on branche l'application réelle sur **les mêmes points**.

### ⚠️ L'invariant qui porte tout le reste

> **Le point de connexion est le MÊME objet à la conception, en simulation et à
> l'exécution.**

Si la simulation passait par un mécanisme et l'application finale par un autre,
**la simulation cesserait de prouver quoi que ce soit sur l'application** : elle
deviendrait une démonstration qui rassure. Or c'est précisément quand elle rassure
à tort qu'elle coûte le plus cher — on cesse de vérifier ce qu'on croit vérifié.

Cet invariant est **la raison d'être** du mode simulation. Tout le reste de cette
section en découle.

### 18bis.1 Le catalogue des doublures

Chaque service simulable est une **doublure** dont on règle la valeur de retour :

| service | ce que la doublure rend |
|---|---|
| dialogue d'ouverture | un chemin, ou l'annulation |
| dialogue d'enregistrement | un chemin, ou l'annulation |
| système de fichiers | un contenu, ou une erreur de lecture |
| horloge | une date fixe, ou une horloge accélérée |
| réseau | une réponse, une lenteur, une coupure |
| presse-papiers | un contenu |
| locale et clavier | une langue, une disposition |

⚠️ **Chaque doublure doit pouvoir rendre l'échec, pas seulement le succès** —
annulation, permission refusée, réseau coupé, disque plein. **Le chemin d'annulation
est celui que personne ne câble**, et c'est celui qui casse l'application le jour où
l'utilisateur ferme la fenêtre au lieu de choisir un fichier. Une simulation qui ne
rend que des succès entraîne à n'écrire que le cas heureux.

⚠️ **Une doublure doit se voir comme telle dans le journal.** Sans marque
distinctive, une session simulée finit par se lire comme une session réelle — et
une capture de journal circule bien plus loin que le contexte qui l'a produite.

### 18bis.2 Le journal dit surtout ce qui n'a PAS été atteint

Le journal connaît **d'avance** la liste complète des points de connexion déclarés
dans le document. À la fin d'une session il rend deux colonnes : **atteints** et
**jamais atteints**, avec le nombre de passages.

⚠️ **C'est la colonne « jamais atteints » qui a de la valeur.** Un journal qui
n'énumère que ce qui s'est produit ne peut pas montrer ce qui manque : l'événement
qui ne se déclenche jamais **n'y laisse aucune ligne**, et son absence ressemble à
un journal propre. *C'est exactement le défaut qu'on vient chercher en simulant.*

Un point jamais atteint n'est pas une erreur en soi — il peut appartenir à un
chemin qu'on n'a pas essayé. Le journal le **nomme**, il ne le condamne pas.

### 18bis.3 Ce que la simulation ne prouve pas

À écrire avant de simuler, pas après :

- elle prouve que **le branchement existe et qu'il est atteignable** ;
- elle **ne prouve pas** que le callback fait la bonne chose — la doublure ne
  contient pas la logique métier ;
- elle **ne prouve pas** les temps de réponse : la doublure répond instantanément
  là où un disque ou un réseau prendra du temps ;
- elle **ne prouve pas** que le service réel se comporte comme sa doublure — c'est
  une hypothèse qu'on pose, pas un résultat qu'on obtient.

⚠️ **« La simulation passe » ne signifie donc jamais « l'application marche ».**
Le dire ici évite d'avoir à le découvrir plus tard, quand quelqu'un aura pris la
première phrase pour la seconde.

### 18bis.4 Le passage à l'application réelle

L'export (§19) écrit **les points de connexion, pas les doublures**. L'application
les implémente ; **les noms sont le contrat**.

⚠️ **Un point de connexion exporté que l'application n'implémente pas doit être une
erreur de compilation, jamais un silence à l'exécution.** Sinon l'écart entre ce
qu'on a dessiné et ce qui tourne se découvre par un bouton qui ne fait rien, et
c'est le symptôme le plus coûteux à diagnostiquer : rien ne s'est produit, donc
rien n'est à lire.

C'est le pendant exact de `E-CALLBACK-UNDECLARED` (§12.2), dans l'autre sens : là,
un branchement désignait un callback absent ; ici, un point déclaré n'est branché
par personne.

---

## 19. Export / Validation

Modal (réutilise `<Modal>` standard) accessible depuis le menu principal
(`Fichier > Exporter`) :

- Résumé avant export : nombre de pages, de widgets, de callbacks déclarés/
  liés/orphelins
- **Rapport de validation** : liste des erreurs/avertissements du doc 2 §12
  (`E-PARSE`, `E-TYPE`, `E-CALLBACK-UNDECLARED` en rouge bloquant ;
  `W-CALLBACK-UNBOUND`, `W-ID-DUPLICATE` en jaune ; `W-ORPHAN-GEOMETRY` en
  gris info), chaque ligne cliquable = sélectionne l'élément fautif sur le
  canvas
- Choix du découpage en fichiers `include` (doc 2 §7) : arbre éditable où
  glisser des pages/composants dans des regroupements de fichiers avant
  export
- Bouton final `Exporter` désactivé tant qu'une erreur bloquante existe

---

## 20. Préférences

Reprend la structure standard (doc 1 §16), catégorie additionnelle
"Intelligence Artificielle" : fournisseur IA, clé API, modèle, quota/usage
affiché en jauge, et catégorie "Canvas" : grille/snap par défaut, position
par défaut des pastilles au premier lancement, comportement du rail
(règle « une seule pastille ouverte », §13.3, désactivable ici pour les
utilisateurs avancés).

---

## 20bis. Greffons — le système d'extension

> **Nom retenu par Rodolf, 2026-08-20 : un « greffon ».** Le mot est déjà le terme
> français pour *plugin*, donc personne n'a à l'apprendre — et la métaphore porte
> l'ingénierie : **une greffe doit être compatible avec l'hôte, elle peut être
> rejetée, et une fois prise elle fait partie de l'organisme.**

⚠️ **Ce système appartient à l'écosystème, pas à NkUIDesign.** Rodolf le veut dans
**toutes** les applications. Cette section est écrite ici parce que NkUIDesign en
est le premier consommateur ; **elle doit migrer vers le document partagé de
l'écosystème** dès qu'une deuxième application l'utilise. La recopier ailleurs
créerait deux contrats qui divergeraient.

### 20bis.1 Ce qu'un greffon peut ajouter

Tout ce que la spécification a fermé jusqu'ici :

| axe | aujourd'hui fermé par | ce qu'un greffon ajoute |
|---|---|---|
| **composants** | bibliothèque du projet (§14bis) | des composants livrés avec leur contrat |
| **rôles** | catalogue natif (§14ter.3) | des rôles dérivant d'un natif |
| **effets** | pile d'effets (§8ter) | de nouveaux effets et leurs paramètres |
| **animations** | trois familles (§9ter) | de nouvelles **sources** d'effet continu, de nouvelles courbes |
| **propriétés** | par rôle | des propriétés supplémentaires sur un rôle existant |
| **événements** | catalogue de rôle (§12.2) | de nouveaux événements |
| **outils** | barre flottante (§7) | un outil de canvas |
| **import / export** | §19 | un format de plus |
| **doublures** | système simulé (§18bis.1) | un service simulable de plus |

⚠️ **Un greffon ne crée jamais un rôle natif.** Un rôle natif est adossé à un
widget du moteur (§14ter.2) ; un greffon qui en déclarerait un produirait un `id`
que le runtime ne sait pas instancier. Il déclare des **rôles de projet**, avec
leur dérivation obligatoire.

### 20bis.2 La greffe se rejette — c'est le cœur du mécanisme

Chaque greffon déclare **la version d'hôte qu'il exige**. À l'ouverture, l'hôte
vérifie **avant** de charger quoi que ce soit.

⚠️ **Un greffon incompatible est REJETÉ, nommé, et l'application continue.** Ni
chargement « pour voir », ni plantage. Un système d'extension qui charge d'abord et
découvre ensuite fait tomber l'application au démarrage — **et l'utilisateur ne
peut même plus ouvrir l'outil pour retirer le greffon fautif**. La seule sortie
devient la ligne de commande, que la plupart des gens n'ont pas.

Le rejet est **visible et réversible** : le greffon apparaît dans la liste, barré,
avec la raison écrite — *« exige l'hôte 1.2, celui-ci est en 1.0 »*.

### 20bis.3 Chaque contribution porte le nom de son greffon

Tout ce qu'un greffon ajoute est **préfixé par son identifiant** : `lumen.carte`,
`lumen.reflet`, `lumen.OnCarteRetournee`.

⚠️ **Sans préfixe, deux greffons qui ajoutent tous deux « carte » entrent en
collision — et le gagnant dépend de l'ordre de chargement**, c'est-à-dire de rien
de lisible. Le document, lui, ne référence qu'un nom : il changerait de sens d'une
machine à l'autre.

⚠️ **Et retirer un greffon doit retirer EXACTEMENT ses contributions**, ni plus ni
moins. C'est le préfixe qui le rend possible.

### 20bis.4 Un document enregistre les greffons dont il dépend

Un `.nkgui` qui utilise `lumen.carte` écrit cette dépendance, avec sa version.

⚠️ **Ouvrir un document dont un greffon manque ne doit RIEN supprimer.** L'outil
nomme le greffon absent, affiche les éléments concernés comme **inconnus mais
préservés**, et refuse d'exporter tant que la dépendance n'est pas résolue.

Charger, ignorer les éléments inconnus puis enregistrer **détruit du travail en
silence** — et la perte n'apparaît qu'à la prochaine ouverture sur la machine qui,
elle, avait le greffon.

### 20bis.5 Un greffon déclare ce à quoi il touche

Un greffon exécute du code. Il déclare **avant installation** ce dont il a besoin :
lecture du document, écriture du document, système de fichiers, réseau, presse-papiers,
processus externes.

⚠️ **L'énoncé de ce que l'action coûte ne doit JAMAIS être le plus petit texte de
la boîte.** « Un greffon exécute du code dans l'éditeur » est la phrase qui porte
tout le risque ; la reléguer en bas, en gris, plus petite que le reste, c'est le
motif exact des bandeaux de consentement où la conséquence est en petits
caractères. **Elle se place au-dessus des boutons, à la taille des libellés, dans
la couleur du texte courant.**

*Cette règle vaut pour toute boîte où l'on accorde quelque chose : la phrase qui
dit ce qu'on accorde a le même poids typographique que celle qui le demande.*

⚠️ **La liste est montrée à l'installation, et un greffon ne peut pas l'élargir en
silence à la mise à jour** — une extension qui gagne l'accès réseau entre deux
versions sans le redemander est exactement le schéma par lequel des écosystèmes
entiers se sont fait prendre. Un élargissement redemande l'accord.

### 20bis.6 Un greffon qui tombe ne doit pas emporter l'application

Un greffon défaillant est **désactivé et signalé**, l'application continue. Son
temps d'exécution est mesuré et visible dans la console (§20ter).

⚠️ **Sans cette mesure, un greffon lent devient « l'application est lente »**, et
personne ne remonte de l'un à l'autre — on réécrit du code d'éditeur pendant que la
cause est une extension installée trois semaines plus tôt.

### 20bis.7 Quand un greffon change ses contributions

Renommer ou supprimer une propriété casse les documents qui l'utilisent. Le greffon
fournit donc une **table de migration** — ancien nom vers nouveau nom.

⚠️ **En l'absence de migration, l'ouverture échoue avec le nom exact de ce qui a
disparu**, et non par un élément qui ne s'affiche plus. Un nom manquant se cherche ;
un élément absent ne se remarque même pas.

---

## 20ter. Console — et le backend graphique en cours

> **Décision Rodolf, 2026-08-20 : « on doit avoir un seul backend pour les deux. »**

§13 posait une pastille « Console/Validation » et §18 une console de simulation,
**sans jamais dire ce qu'elles affichent**. Voici ce qu'elles affichent.

### 20ter.1 Quatre flux, quatre onglets

| onglet | contenu |
|---|---|
| **Validation** | les codes d'erreur et d'avertissement du document, cliquables : le clic sélectionne l'élément fautif |
| **Simulation** | le journal de §18bis.2, avec les doublures marquées et la couverture des points de connexion |
| **Greffons** | ce qui est chargé, ce qui est rejeté et pourquoi, le temps d'exécution de chacun |
| **Système** | version de l'hôte, backend graphique, mémoire, images par seconde |

### 20ter.2 Un seul backend pour l'éditeur et la simulation

§18 promet que la simulation rend « par NKGui réel, le même moteur que
l'application finale ». **Cette promesse n'a de sens que si l'éditeur et la
simulation partagent le même backend.** C'est désormais une règle : un seul
backend, choisi une fois, pour les deux.

⚠️ **Mais cela ne supprime qu'un écart sur deux, et il faut le dire.** Un seul
backend garantit que **ce que montre l'éditeur est ce que montre la simulation**.
Il ne garantit **pas** que la simulation montre ce que verra l'utilisateur final :
si l'application est livrée sur DirectX 12 et que tout le travail se fait sur
Vulkan, l'écart demeure — il a seulement changé de place.

La console affiche donc **le backend en cours** et, quand la cible déclare une
autre plateforme, **le dit** : *« rendu Vulkan · la cible Windows livre en
DirectX 12 — non vérifié ici »*. Nommer l'écart ne le referme pas ; le taire le
rend invisible jusqu'à la livraison.

⚠️ **Et un repli automatique de backend doit être BRUYANT.** Si le backend demandé
échoue à s'initialiser et que l'outil retombe sur un autre en silence, la garantie
« un seul backend » devient fausse **sans que rien ne l'annonce** — et c'est
précisément le jour où les couleurs changent que personne ne saura pourquoi.
*(L'historique du magenta et l'état de NkSL — cinq backends propres sur six —
rendent ce cas concret, pas théorique.)*

### 20ter.3 Ce que l'onglet Système montre en permanence

Backend et version de pilote · adaptateur et mémoire vidéo · densité de l'écran
courant · images par seconde de l'éditeur · nombre d'ambiances actives (§9ter.4) ·
version de l'hôte et des greffons chargés.

⚠️ **Ces lignes doivent être copiables en un geste.** C'est ce qu'on demande à
quelqu'un qui signale un défaut, et une capture d'écran illisible de ces
informations coûte un aller-retour à chaque rapport.

---

## 21. Glossaire des composants

En plus du glossaire générique déjà défini (doc 1 §19, réutilisé tel quel) :

| Composant | Description |
|---|---|
| `ProjectTabStrip` | Bande d'onglets où chaque onglet = un projet `.nkgui` ouvert |
| `CanvasModeSwitch` | Segmented control Design/Behavior/Split de la toolbar |
| `InfiniteDesignCanvas` | Canvas avec toutes les pages/cadres du projet |
| `SceneObjectsGrid` | Vue par défaut de l'Inspecteur quand rien n'est sélectionné |
| `DockRailPill` | Pastille rétractable à 4 états (repliée/overlay/ancrée/flottante) |
| `RoleBadge` | Petit tag affiché sur une sélection déjà promue en widget |
| `BehaviorScopeSelector` | Dropdown "Composant / Page / Global" du canvas Behavior |
| `AIContextButton` | Bouton `✨` flottant au survol d'une sélection |
| `AIPreviewCard` | Carte d'aperçu Appliquer/Rejeter dans le chat IA |
| `CallbackStatusPill` | Pastille de statut lié/non lié/invalide dans l'Inspecteur Behavior |
| `VectorPathEditor` | Outil d'édition de tracé au sommet (ancres, poignées Bézier) |
| `EffectsStackPanel` | Section Effets de l'Inspecteur (ombres, flou, dégradés, fusion) |
| `ComponentLibraryPanel` | Grille des composants maîtres, import et instanciation |
| `InstanceOverrideBadge` | Pastille violette marquant une propriété d'instance modifiée localement |
| `WidgetAnimationCanvas` | Éditeur d'animation de widget (State Machine + Dope Sheet/Curve, réutilise Aetherion Animate) |
| `AnimationEventLink` | Icône reliant un événement de l'Inspecteur Behavior à une transition d'animation définie |

---

**Fin du document 3.** Voir `4_NkUIDesign_Brief_Banani.md` pour les prompts de
génération visuelle et `5_NkUIDesign_Specification_Claude.md` pour la
spécification technique d'implémentation.
