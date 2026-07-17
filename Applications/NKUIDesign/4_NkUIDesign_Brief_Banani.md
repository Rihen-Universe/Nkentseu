# Brief de génération de maquettes — NkUIDesign (pour Banani)

But de ce document : fournir à **Banani** (génération de prototypes UI
multi-écrans à partir de texte) une description en langage clair, écran par
écran, pour produire des maquettes visuelles de l'application NkUIDesign.
Contenu volontairement descriptif (pas de grammaire technique) — à coller tel
quel dans le prompt, écran par écran ou en un seul bloc.

---

## Présentation du produit (contexte à donner en premier)

> NkUIDesign est un outil professionnel desktop de design d'interfaces
> logicielles, dans l'esprit de Figma croisé avec un éditeur de blueprints
> visuels (façon Unreal Engine). Il permet de dessiner librement une
> interface, de transformer des formes en composants d'UI fonctionnels
> (boutons, sliders, tableaux, fenêtres...), puis de définir leur
> comportement soit par du code, soit par un graphe de nœuds visuel. Public :
> développeurs et designers d'outils techniques (éditeurs, dashboards,
> panneaux d'inspection, HUD). Ambiance visuelle : **thème sombre
> professionnel**, dense en information mais aéré, proche des IDE modernes
> (VS Code, Unreal Editor, Blender) — pas un style « grand public » coloré.

---

## Charte visuelle (à respecter strictement sur tous les écrans)

Palette exacte (issue du thème par défaut du moteur sous-jacent — à réutiliser
telle quelle pour que les maquettes correspondent au rendu réel) :

| Rôle | Couleur | Hex |
|---|---|---|
| Fond principal | gris-bleu très sombre | `#1A1D24` |
| Fond de panneau | gris-bleu sombre | `#262A34` |
| En-tête de panneau | gris-bleu | `#2E333F` |
| Bouton (repos) | gris-bleu moyen | `#3A4050` |
| Bouton (survol) | bleu-gris clair | `#4E5C78` |
| Bouton (actif/pressé) | bleu vif | `#6096E6` |
| Bordure | gris moyen | `#565E70` |
| Texte principal | blanc cassé | `#E6E8F0` |
| Texte désactivé | gris moyen | `#787C86` |
| Sélection (fond) | bleu | `#406EC8` |
| Accent (liens, icônes actives, focus) | bleu clair | `#60A5FA` |
| Fond de piste (slider) | gris-bleu très sombre | `#1E222B` |
| Fond barre d'onglets | quasi-noir | `#16191F` |
| Onglet inactif | gris-bleu | `#282D38` |
| Onglet survolé | bleu-gris clair | `#3A4050` |
| Onglet actif | gris-bleu clair | `#343A48` |

Typographie : police sans-serif géométrique/neutre (type Inter, IBM Plex
Sans, ou équivalent), tailles compactes (13-14px texte courant, 11-12px
labels secondaires), coins arrondis discrets (~5px), padding interne serré
mais lisible. Éviter tout gradient ou ombre marquée — style **plat, dense,
outil pro**, pas « app mobile ».

---

## Liste des écrans à générer (dans cet ordre)

1. Launcher
2. Dialog « Nouveau projet »
3. Dialog « Nouveau projet via IA »
4. Éditeur principal — vue Design (Canvas + Structure + Inspecteur)
5. Éditeur principal — vue Structure avec menu « Promouvoir en… » ouvert
6. Éditeur principal — Inspecteur, onglet Behavior (liste d'événements)
7. Panneau Behavior — sous-onglet Code
8. Panneau Behavior — sous-onglet Node Graph
9. Panneau IA (génération contextuelle, aperçu avant validation)
10. Fenêtre Gestionnaire de callbacks
11. Fenêtre Aperçu / Test interactif (avec console de callbacks)
12. Dialog Export (avec rapport de validation)

---

## Prompts détaillés par écran

### Écran 1 — Launcher
> Écran d'accueil desktop, fond très sombre (#1A1D24). En haut, un logo
> minimaliste texte « NkUIDesign » avec un badge de version, à côté une barre
> de recherche discrète pour les projets récents. À gauche, une colonne
> verticale de boutons empilés : un bouton principal bleu « Nouveau projet »,
> un bouton secondaire avec icône étincelle « Nouveau via IA », un bouton
> « Ouvrir un projet », un bouton « Importer un fichier .nkgui », puis un
> lien discret « Documentation » en bas de colonne. À droite, une grande
> grille de cartes/vignettes représentant des projets récents : chaque carte
> a un aperçu miniature sombre de l'interface, un titre, une date. Style
> sobre, dense, aucune couleur criarde à part le bleu d'accent #60A5FA sur
> les éléments actifs.

### Écran 2 — Dialog « Nouveau projet »
> Fenêtre modale centrée sur fond assombri, panneau sombre (#262A34) avec
> coins arrondis. Titre « Nouveau projet ». Champ texte « Nom du projet »,
> champ « Emplacement » avec bouton parcourir, une rangée de vignettes de
> gabarits (Vierge, Panneau d'outils, Fenêtre de dialogue, HUD de jeu,
> Dashboard) sélectionnables, un champ de taille de canvas avec presets, un
> sélecteur de thème (Sombre coché par défaut, Clair, Personnalisé). En bas à
> droite, deux boutons : « Annuler » (gris) et « Créer » (bleu plein,
> proéminent).

### Écran 3 — Dialog « Nouveau projet via IA »
> Fenêtre modale, plus grande que la précédente. Grand champ de texte
> multilignes avec placeholder « Décris l'interface que tu veux créer... »,
> une zone de dépôt de fichier pointillée en dessous pour glisser une image
> de référence. Trois menus déroulants compacts en ligne : nombre d'écrans,
> plateforme cible, style visuel. Bouton bleu proéminent « Générer un
> aperçu » en bas.

### Écran 4 — Éditeur principal, vue Design
> Application desktop complète en plein écran, fond très sombre (#1A1D24).
> Tout en haut, une fine barre de menus horizontale (Fichier, Édition,
> Affichage, Widget, Behavior, IA, Fenêtre, Aide) sur fond quasi-noir. En
> dessous, une barre d'outils avec des icônes d'outils de dessin (sélection,
> cadre, rectangle, ellipse, texte, image, tracé), un sélecteur à trois
> onglets « Design / Structure / Behavior », des contrôles d'alignement, un
> pourcentage de zoom, et à droite un bouton bleu avec icône étincelle
> « Générer avec l'IA ». Sous la barre d'outils, disposition en trois
> colonnes : à gauche un panneau étroit avec un arbre hiérarchique de calques
> (icônes de rôle colorées, indentation), au centre un grand canvas gris très
> sombre avec une règle graduée en haut et à gauche, contenant un cadre
> blanc/clair représentant un écran en cours de conception (formes,
> rectangles arrondis, un bouton bleu, un champ de saisie, un slider) ; à
> droite un panneau Inspecteur avec trois onglets (Design/Widget/Behavior) et
> des champs de propriétés (position, taille, couleur). Barre de statut fine
> tout en bas.

### Écran 5 — Vue Structure, menu « Promouvoir en… » ouvert
> Même disposition générale que l'écran 4, mais le panneau de gauche est
> élargi, focus sur l'arbre hiérarchique. Un élément de la liste est
> sélectionné et un menu contextuel flottant est ouvert à côté, intitulé
> « Promouvoir en… », listant des icônes + noms : Button, Checkbox,
> SliderFloat, InputText, ColorEdit4, Table, TreeNode, TabBar, Window, Panel,
> DockSpace, Menu — organisés en petites catégories avec séparateurs.

### Écran 6 — Inspecteur, onglet Behavior
> Zoom sur le panneau de droite (Inspecteur) en pleine hauteur. Onglet
> « Behavior » actif (surligné en bleu). Liste verticale de lignes, chacune
> avec : une petite pastille colorée de statut à gauche (grise, bleue,
> violette ou orange), le nom de l'événement (« Click », « Changed »,
» « HoverEnter »...), et à droite un bouton « + » discret. Une ligne est
> survolée et affiche un petit menu contextuel avec trois choix : « Callback
> direct », « Éditeur Code », « Éditeur Node Graph ».

### Écran 7 — Panneau Behavior, sous-onglet Code
> Panneau ancré en bas de l'écran principal, prenant environ un tiers de la
> hauteur. Éditeur de code sombre avec coloration syntaxique : mots-clés en
> bleu clair, chaînes de caractères en orange doux, noms de types en
> turquoise, commentaires en vert éteint, sur fond presque noir. Numéros de
> ligne à gauche. Une barre latérale étroite à droite liste des variables et
> des callbacks disponibles en autocomplétion. En bas du panneau, une petite
> zone d'erreurs/avertissements.

### Écran 8 — Panneau Behavior, sous-onglet Node Graph
> Même zone que l'écran 7, mais affichant un canvas infini de type éditeur
> de blueprint : grille de points discrets sur fond très sombre, plusieurs
> boîtes rectangulaires arrondies (nœuds) avec un en-tête coloré différent
> selon la catégorie (vert pour Événement, bleu pour Flux, orange pour
> Action, gris pour Donnée), des petits cercles colorés sur les bords
> gauche/droite des nœuds (pins de données), des triangles blancs en haut/bas
> (pins d'exécution), reliés par des câbles courbes lumineux. Une palette de
> nœuds rétractable à gauche, une minimap en bas à droite.

### Écran 9 — Panneau IA (génération contextuelle)
> Panneau flottant ou ancré à droite, plus petit, avec un champ de texte en
> haut « Décris ce que tu veux générer ou modifier », un bouton bleu
> « Générer », et en dessous une grille de vignettes d'historique de
> génération avec, sur la vignette actuellement sélectionnée, une
> superposition semi-transparente bleutée sur le canvas principal montrant
> l'aperçu proposé avant validation, avec deux boutons flottants « Accepter »
> et « Rejeter ».

### Écran 10 — Gestionnaire de callbacks
> Fenêtre secondaire indépendante, tableau à colonnes : Nom, Type, Signature,
> Utilisations, Statut (badge « lié » en vert discret ou « non lié » en gris/
> orange). Barre de recherche en haut, boutons « Nouveau contrôleur » et
> « Nouveau callback » en haut à droite.

### Écran 11 — Aperçu / Test interactif
> Fenêtre secondaire simulant un rendu final d'application (mêmes couleurs
> de thème que le design édité), avec une barre d'outils en haut proposant
> des presets de taille d'écran et un bouton « Rejouer ». Une console
> rétractable en bas ou sur le côté affiche en direct une liste de callbacks
> déclenchés avec leurs arguments, façon console de développeur.

### Écran 12 — Dialog Export
> Fenêtre modale avec un champ de chemin de fichier, une case à cocher
> « Découper en plusieurs fichiers » révélant un mini-aperçu d'arborescence
> de fichiers, et une liste de validation en dessous avec des lignes marquées
> d'icônes rouge (erreur bloquante) ou orange (avertissement). Boutons
> « Annuler » et « Exporter » (désactivé si des erreurs rouges sont
> présentes) en bas.

---

## Prompt global (si génération en un seul lot multi-écrans)

> Génère un prototype multi-écrans d'une application desktop professionnelle
> de design d'interfaces appelée NkUIDesign (mélange Figma + éditeur de
> blueprints visuels façon Unreal Engine), thème sombre strict avec la
> palette suivante : fond #1A1D24, panneaux #262A34, bordures #565E70, texte
> #E6E8F0, accent bleu #60A5FA, boutons #3A4050 (survol #4E5C78, actif
> #6096E6). Style dense et professionnel façon IDE (VS Code / Unreal
> Editor), coins arrondis discrets, pas de style « grand public ». Écrans à
> produire : 1) Launcher avec projets récents, 2) dialog Nouveau projet,
> 3) dialog Nouveau projet via IA, 4) éditeur principal trois colonnes
> (arbre de calques à gauche, canvas de design au centre, inspecteur à
> droite, barre d'outils avec bascule Design/Structure/Behavior), 5) panneau
> de code avec coloration syntaxique, 6) éditeur de graphe de nœuds visuel
> façon blueprint avec câbles colorés, 7) fenêtre de test interactif avec
> console de logs.

---

*Voir aussi : Document 1 (spécification fonctionnelle), Document 2 (langage),
Document 3 (interface humaine, référence exhaustive derrière ces maquettes).*
