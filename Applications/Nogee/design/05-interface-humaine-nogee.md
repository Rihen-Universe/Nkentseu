# Nogee — l'interface, pour un humain

### Document 5 — Spécification d'interface côté humain (produit / UX)

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen** — 26/09/2026.
> Ce document dit **quoi**, **où** et **pourquoi**. Le **06** le traduit pour
> l'agent qui écrit les `.nkgui`. Le **04** dit ce que le moteur et l'éditeur
> savent faire. **Si 05 et 06 se contredisent, 05 gagne.**

---

## 1. Le style

**Unreal Engine 5, net** — le même thème, les mêmes jetons et les mêmes
composants que NkAnima (doc 08 §1) et NKScena (doc 02 §1). Nogee est celui des
quatre éditeurs qui **ressemble le plus** à UE5 : c'est un éditeur de niveau, et la
disposition d'UE5 est la bonne pour ce métier.

📌 **Un utilisateur d'UE5 doit trouver les choses à leur place** : Scène (World
Outliner) en haut à droite, Détails en dessous, tiroir de contenu en bas
(Ctrl+Espace), boutons de jeu au centre de la barre d'outils, clic droit maintenu
+ ZQSD/WASD pour voler dans la vue. **Et un utilisateur de NkAnima ou de NKScena
ne doit rien réapprendre** : mêmes panneaux, mêmes raccourcis.

### 1.1 Les couleurs d'information propres à Nogee

En plus des couleurs communes (orange « écrit », rouge « enregistrement »,
axes, violet hachuré « proposition », ambre « à regarder », rouge vif
« erreur ») :

| jeton | sens | couleur | pourquoi |
|---|---|---|---|
| `@jeu.en_cours` | **le jeu tourne** dans l'éditeur | vert | liseré de 2 px autour de la vue : ce qu'on modifie **pendant** le jeu est perdu à l'arrêt, sauf si on le conserve — il faut le savoir |
| `@jeu.pause` | jeu en pause | ambre | idem |
| `@jeu.simulation` | simulation sans joueur | cyan | idem |
| `@compile.ok` · `@compile.a_faire` · `@compile.erreur` | état d'un Blueprint ou du C++ | vert · ambre · rouge | un Blueprint modifié et non compilé ne fait pas ce qu'on voit |
| `@asset.modifie` | asset modifié non enregistré | astérisque + point blanc | convention universelle |
| `@plateforme.prete` · `.incomplete` · `.absente` | état d'une cible de construction | vert · ambre · gris | on sait avant de construire ce qui manque |
| `@pin.execution` · `.booleen` · `.entier` · `.reel` · `.texte` · `.vecteur` · `.objet` · `.structure` | **type** d'une broche de graphe | blanc · rouge · turquoise · vert · magenta · jaune · bleu · bleu foncé | la couleur dit ce qui peut se brancher sur quoi — convention des Blueprints |
| `@noeud.evenement` · `.fonction` · `.pur` · `.flux` | catégorie d'un nœud (en-tête) | rouge · bleu · vert · gris | on lit un graphe par sa forme |

📌 **Jamais la couleur seule** : l'état de jeu est aussi écrit dans la barre
d'état (« EN JEU », « PAUSE », « SIMULATION ») ; les broches ont aussi leur **forme**
(triangle pour l'exécution, rond pour les données, losange pour les tableaux).

---

## 2. Le lanceur

- **À gauche** : Récents · Nouveau projet · Ouvrir · Apprendre.
- **Récents** : cartes (vignette, nom, moteur, dernière ouverture, plateformes).
- **Nouveau projet** : grille des **modèles** (Vide, Troisième personne, Première
  personne, Vue du dessus, Plateforme 3D, Course, Jeu de plateau, Réalité
  virtuelle), chacun avec sa vignette et une phrase ; puis, à droite : nom,
  dossier, **plateformes visées** (cases), **qualité** (Maximale / Mobile),
  **contenu de départ** (oui / non), et **Créer**.

---

## 3. L'éditeur de niveau

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ Fichier Édition Fenêtre Outils Construire Sélection Acteur Aide   Niveau_01* │
├──────────────────────────────────────────────────────────────────────────────┤
│ 💾 │ Mode : Sélection ▾ │ + Ajouter ▾ │ Blueprints ▾ │ Cinématiques ▾ │        │
│            ▶ Jouer ▾   ⏸   ⏹   ⏏       │ Plateformes ▾ │ Réglages ▾           │
├─────────────────────────────────────────────────────────┬────────────────────┤
│  ┌ Perspective · Éclairé · Afficher ▾ ┐   ⊞ 10 ∠ 15 ⤢ 0,25 │  Scène            │
│                                                         │  (acteurs, dossiers)│
│                    Vue 3D                               ├────────────────────┤
│                                                         │  Détails           │
│                                                         │  (composants +     │
│                                                         │   catégories)      │
├─────────────────────────────────────────────────────────┴────────────────────┤
│ Tiroir de contenu · Journal · > commande      ● Compilé   ⎇ main   Windows ✓ │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 3.1 La barre d'outils

De gauche à droite, comme UE5 :

1. **Enregistrer** (tout ce qui est modifié).
2. **Mode** : Sélection · Monde · Peinture · Volumes (§4).
3. **+ Ajouter** : menu rapide des acteurs courants (formes, lumières, caméras,
   volumes, déclencheurs, point d'apparition, son, effet) — et **la palette
   complète** en bas du menu.
4. **Blueprints ▾** : nouvelle classe Blueprint, ouvrir le Blueprint du niveau,
   classes récentes.
5. **Cinématiques ▾** : ajouter une cinématique au niveau (déclencheur),
   **ouvrir dans NKScena**.
6. **Au centre — le jeu** :
   - **▶ Jouer** (grand bouton vert) avec sa flèche **▾** : Dans la vue ·
     Nouvelle fenêtre · Sur l'appareil ▸ · **Simuler** · Nombre de joueurs ·
     Mode réseau (Autonome, Serveur, Client) ;
   - **⏸ Pause**, **⏹ Arrêter**, **⏏ Éjecter** (F8), **⏭ Image suivante**
     (en pause) — visibles **seulement pendant le jeu**.
7. **Plateformes ▾** : la liste des cibles avec leur pastille d'état ;
   **Empaqueter**, **Lancer sur**…, **Préparer** (ce qui manque).
8. **Réglages ▾** : Réglages du projet, Réglages du monde, Préférences.

### 3.2 La Vue 3D

La Vue commune (NkAnima, NKScena), plus :

- **Barre de vue** : caméra (Perspective, Dessus, Face, Côté…), **mode**
  (Éclairé, Sans éclairage, Filaire, **Collisions**, **Navigation**, Complexité
  de la lumière, Complexité des shaders), **Afficher ▾**, et à droite les
  **aimantations** (grille, angle, échelle — valeurs cliquables) et la **vitesse
  de caméra**.
- **Pendant le jeu** : le liseré de la Vue passe au vert (ou ambre, ou cyan), la
  souris est **capturée par le jeu** ; Maj+F1 la rend à l'éditeur.
- **Statistiques** (Afficher ▸ Statistiques) : ips, ms par image, appels de
  dessin, triangles, mémoire.
- **Glisser un asset** du tiroir de contenu dans la Vue le place sous la souris,
  posé sur la surface.

### 3.3 Scène (World Outliner)

- Arbre des acteurs du niveau, **dossiers** créés par l'utilisateur,
  sous-niveaux. Colonnes : œil, cadenas, nom, **type**, et pendant le jeu une
  colonne « créé en jeu ».
- Recherche, filtres par type, **tri**.
- Clic droit : renommer, dupliquer, supprimer, déplacer vers un dossier,
  **sélectionner tous les acteurs de cette classe**, convertir en Blueprint,
  **Conserver les valeurs de simulation** (pendant le jeu).

### 3.4 Détails

Le panneau Détails commun (catégories, étiquette/valeur, retour au défaut), plus
ce qui fait la manière UE5 :

- En tête, **l'arbre des composants** de l'acteur (racine, enfants), avec
  **+ Ajouter un composant** (recherche) et **Modifier le Blueprint** ▾.
- En dessous, les catégories du composant sélectionné : Transformation, Maillage,
  Matériaux, Physique, Collision, Éclairage, Rendu, Audio, Navigation,
  **Gameplay** (les propriétés exposées par le C++ ou le Blueprint), Tags.
- Les champs de **référence d'asset** (un maillage, un son, un matériau) montrent
  la vignette, le nom, et trois boutons : **parcourir**, **utiliser la sélection
  du tiroir**, **effacer**. On y glisse un asset.

### 3.5 Le tiroir de contenu (Content Browser)

Monte du bas (Ctrl+Espace) ; peut être **amarré** (« Amarrer dans la disposition »).

- **Arborescence** du projet à gauche, **chemin** en haut, **filtres** par type
  (Maillages, Matériaux, Textures, Blueprints, Niveaux, Sons, Animations,
  Interfaces, Tables, Effets).
- **Vignettes** (taille réglable) ou **liste** ; nom, type, **point de
  modification**, vignette animée au survol pour les animations et les effets.
- **+ Ajouter** (nouvel asset par type), **Importer**, **Enregistrer tout**.
- Double clic → **l'éditeur de l'asset** dans un nouvel onglet (§5).
- Clic droit : renommer, dupliquer, supprimer (avec **les références**
  listées avant confirmation), **Visionneuse de références**, **Carte des
  tailles**, **Ouvrir dans** NKCraft / NkAnima / NKScena / NKUIDesign /
  NKCode selon le type.

### 3.6 Journal et console

Le Journal commun, avec **filtres** (catégorie, sévérité : message, avertissement,
erreur), recherche, effacer, **copier**. Une ligne d'erreur de Blueprint ou de C++
est **cliquable** : elle ouvre l'endroit fautif.

### 3.7 La barre d'état

Tiroir de contenu · Journal · champ **> commande** · … · **état de compilation**
(point vert / ambre / rouge + texte) · branche git (lecture) · **plateforme
courante** et son état · pendant le jeu : **EN JEU** / **PAUSE** / **SIMULATION**.

---

## 4. Les modes de l'éditeur de niveau

| mode | ce que ça change |
|---|---|
| **Sélection** | le mode normal : placer, sélectionner, transformer |
| **Monde** | panneau des outils de monde : **Terrain** (sculpter, peindre les matières, gérer les morceaux), **Végétation** (pinceau de dispersion), **Eau** (rivière, lac, océan), **Ciel** — **la même brique et les mêmes panneaux que l'espace Décor de NKScena** |
| **Peinture** | couleur de sommets et masques sur les maillages placés |
| **Volumes** | outils pour dessiner des volumes : navigation, post-traitement, audio, déclencheur, zone de mort, streaming |

Le mode choisi remplace **le haut du panneau de gauche** par ses outils (UE5 met
les modes dans un panneau ; ici le panneau **Outils du mode** apparaît à gauche
quand le mode n'est pas Sélection).

---

## 5. Les éditeurs d'assets

Chacun s'ouvre dans **un onglet de document** à côté de l'onglet du niveau (comme
UE5). Un onglet porte l'**astérisque** tant que l'asset n'est pas enregistré.
**Fermer l'onglet ne supprime rien.**

### 5.1 L'éditeur de Blueprint

```
┌────────────────────────────────────────────────────────────────────────┐
│ ⚙ Compiler ●  💾  Parcourir  │ Déboguer : (aucune instance) ▾ │ Classe │
├──────────────┬─────────────────────────────────────────┬───────────────┤
│ MON BLUEPRINT│  [Graphe d'événements] [Construction] [+]│  Détails      │
│ Graphes      │                                         │  (nœud, var,  │
│  ▸ Événements│      ┌──────────┐     ┌──────────────┐  │   fonction    │
│ Fonctions  + │      │▶ Début   │────▶│ Afficher le  │  │   choisie)    │
│ Variables  + │      │  du jeu  │     │ texte        │  │               │
│ Répartiteurs+│      └──────────┘     └──────────────┘  │               │
│              │                                         │               │
├──────────────┴─────────────────────────────────────────┴───────────────┤
│ Résultats de compilation · Recherche dans le Blueprint                  │
└────────────────────────────────────────────────────────────────────────┘
```

- **Composants** (onglet Vue) : l'arbre des composants de la classe et un
  aperçu 3D.
- **Graphes** : Graphe d'événements, Script de construction, fonctions ; le
  graphe au centre, la palette par **clic droit** (recherche contextuelle : ne
  montre que ce qui peut se brancher sur la broche tirée).
- **Mon Blueprint** : graphes, fonctions, variables (type, instance éditable,
  exposée), **répartiteurs d'événements**.
- **Compiler** : le bouton porte l'état (vert / ambre / rouge). **Résultats de
  compilation** en bas, cliquables.
- **Déboguer** : choisir une instance **pendant le jeu** ; points d'arrêt (F9 sur
  un nœud), le chemin d'exécution s'illumine, la valeur d'une broche au survol.
- **Nœud → C++** : voir le C++ équivalent (lecture seule).

### 5.2 L'éditeur de matériau

Même disposition : **aperçu** (sphère, cube, plan, maillage choisi) à gauche,
**graphe** au centre, **Détails** à droite, **palette** de nœuds à droite en
onglet (constantes, textures, maths, coordonnées, paramètres), **statistiques**
(instructions, échantillonneurs — le budget d'échantillonneurs est une limite
réelle mesurée par la maison) en bas. **Appliquer** recompile par NkSL.

### 5.3 L'éditeur d'arbre de comportement

Graphe **de haut en bas** (racine en haut) : sélecteurs, séquences, décorateurs,
tâches ; **tableau noir** (les variables de l'IA) à droite ; pendant le jeu, le
chemin actif s'illumine sur l'instance déboguée.

### 5.4 Les autres

- **Aperçu de maillage** : collisions (générer, simplifier), LOD, sockets,
  matériaux par section.
- **Animation (lecture)** : lecteur de clip et de graphe d'animation, avec
  **Ouvrir dans NkAnima** en évidence.
- **Son composé** : graphe simple (aléatoire, couches, modulateur, atténuation).
- **Interface** : aperçu du `.nkgui` et **Ouvrir dans NKUIDesign**.
- **Table de données** : un tableau éditable, typé par une structure.
- **Physique** : profils de collision (canaux × réponses : ignorer, chevaucher,
  bloquer), matières physiques (frottement, rebond, son d'impact).

---

## 6. Réglages et construction

### 6.1 Réglages du projet

Une fenêtre à **catégories à gauche** (comme UE5) et Détails à droite :
Description · Cartes et modes (niveau de départ, mode de jeu par défaut) ·
**Entrées** (actions et axes, avec **capture de touche**) · Physique ·
**Collision** (canaux, profils) · Rendu · Audio · Réseau · **Plateformes** (une
page par cible) · Langues.

### 6.2 Plateformes et empaquetage

Un panneau (Construire ▸ Plateformes) :

- une ligne par cible : **pastille d'état**, nom, ce qui manque (« SDK Android
  introuvable — ANDROID_SDK_ROOT »), **Préparer** ;
- pour la cible choisie : profil (Développement / Expédition), **Cuire**,
  **Empaqueter**, **Lancer sur** (appareil détecté) ;
- en bas, **la sortie de Jenga** en direct, avec les erreurs cliquables.

---

## 7. Gestes et raccourcis

Tous ceux de NkAnima et NKScena (navigation, sélection, W E R, F, annuler,
Ctrl+Espace, Ctrl+K), plus :

| touche | effet |
|---|---|
| Ctrl+A | tout sélectionner (**comme UE5** ; A dans NkAnima) |
| Échap | hors jeu : tout désélectionner ; en jeu : arrêter |
| Ctrl+Alt+S | enregistrer sous |
| Alt+P | jouer dans la vue |
| Alt+S | simuler |
| F8 | éjecter / reprendre le joueur |
| Maj+F1 | rendre la souris à l'éditeur pendant le jeu |
| Pause | pause du jeu |
| Fin | poser l'acteur sélectionné sur le sol |
| Alt + glisser (gizmo) | dupliquer en déplaçant |
| Ctrl+B | trouver l'asset de l'acteur dans le tiroir de contenu |
| Ctrl+E | éditer l'asset de l'acteur |
| G | vue de jeu (masque les aides de l'éditeur) |
| F7 | compiler (Blueprint ou C++ à chaud) |
| F9 | point d'arrêt (dans un graphe) |
| clic droit dans un graphe | palette contextuelle des nœuds |
| Alt+clic sur un lien | le couper |

---

## 8. Toujours vrai

- **Le liseré de la Vue dit si le jeu tourne.** Ce qu'on change en jeu est
  perdu à l'arrêt, sauf « Conserver les valeurs de simulation ».
- **Un Blueprint non compilé porte l'ambre**, partout où il apparaît.
- **Un asset non enregistré porte l'astérisque**, dans son onglet et dans le
  tiroir.
- **Supprimer un asset montre d'abord ce qui le référence.**
- **Une proposition de l'IA est violette et hachurée** ; rien n'est appliqué sans
  « Appliquer ».
- **Une action indisponible est grisée avec sa raison.**
- **Fermer un onglet ne supprime rien.**

---

## 9. Les menus

| menu | contenu |
|---|---|
| **Fichier** | Nouveau niveau · Ouvrir un niveau · Niveaux récents ▸ · Enregistrer le niveau · Enregistrer sous · Enregistrer tout · Importer dans le niveau · Exporter la sélection · Nouveau projet · Ouvrir un projet · Récents ▸ · Quitter |
| **Édition** | Annuler · Refaire · Historique · Couper · Copier · Coller · Dupliquer · Supprimer · Renommer · Réglages de l'éditeur · Réglages du projet |
| **Fenêtre** | Scène · Détails · Tiroir de contenu · Journal · Outils du mode · Palette · Statistiques · Profileur · Visionneuse de références · Carte des tailles · Assistant IA · Dispositions ▸ · Réinitialiser |
| **Outils** | Nouvelle classe C++ · Ouvrir dans NKCode · Recharger le C++ · Construire la navigation · Construire l'éclairage · Rechercher dans les Blueprints · Audit des assets |
| **Construire** | Compiler · Cuire ▸ · Plateformes · Empaqueter ▸ · Lancer sur ▸ |
| **Sélection** | Tout · Rien · Inverser · Même classe · Même matériau · Enfants · Groupes ▸ |
| **Acteur** | Poser au sol · Aligner ▸ · Grouper · Dégrouper · Attacher à · Détacher · Convertir en Blueprint · Remplacer par ▸ · Transformations ▸ (réinitialiser, miroir) |
| **Aide** | Raccourcis · Documentation · Journal · Crédits et licences · À propos |
