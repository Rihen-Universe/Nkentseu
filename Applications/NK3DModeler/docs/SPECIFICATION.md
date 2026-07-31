# NK3DModeler — spécification produit et cahier des charges

> **Statut : SPÉCIFICATION.** Le dossier ne contient pas encore de code.
> Rédigé le **2026-07-31**. Auteur du besoin : Rihen.
>
> **Ce document tranche.** Il n'a d'utilité que s'il ferme des questions : ce qui
> est dans le produit, ce qui n'y est pas, et à quoi l'on reconnaît qu'une étape
> est finie. Un document qui paraphraserait Blender serait pire qu'aucun document
> — il donnerait l'illusion d'une décision en laissant tout ouvert.

---

## 1. Ce qu'est NK3DModeler

Une **application de modélisation 3D** autonome, bâtie sur le socle
`Engine/NKEditorKit`, et partageant son composant viewport avec Nogee (éditeur de
jeu), NkAnima (animation) et PV3DE.

**Le problème qu'elle résout, et c'est le seul qui justifie de l'écrire :** tout
le travail de modélisation vit aujourd'hui dans `Applications/Sandbox/src/Demo/Demo3D.cpp`
— dix mille lignes pilotées au clavier, dont les retours n'existent que dans un
fichier de log. 17 modificateurs, 36 paramètres, une pile réordonnable, un mode
édition complet : **rien de tout cela n'est atteignable pour un utilisateur**.
La preuve la plus nette est qu'il a fallu inventer des variables d'environnement
(`NK_MOD_ADD`, `NK_LIGHT_PICK_AT`, `NK_EDIT_MODE`) pour vérifier ces fonctions
soi-même.

Le moteur de modélisation existe et il est protégé (harnais `NKEditMeshHarness`,
111 cas). **Ce qui manque, c'est la surface par laquelle on l'utilise.**

---

## 2. À qui il s'adresse

| utilisateur | ce qu'il en attend | conséquence sur le produit |
|---|---|---|
| **Rihen** (premier et principal) | modéliser les assets de ses propres projets sans quitter son écosystème | les raccourcis doivent être ceux de Blender : réapprendre des gestes acquis serait un recul |
| **L'IA (NKGen)** | appeler les mêmes opérations qu'un humain, pas une variante dégradée | toute opération exposée à l'interface doit exister comme **commande donnée**, rejouable et sérialisable |
| **Le lecteur / l'étudiant** | comprendre comment un modeleur est construit | le code reste lisible et commenté ; aucune dépendance tierce |

**Conséquence directe** : l'interface n'est pas une couche cosmétique posée
au-dessus. Elle consomme le **même** chemin de commandes que le rejeu et que
l'IA — sinon trois comportements divergeront en silence.

---

## 3. Ce que « parité Blender » veut dire, et ce que ça ne veut pas dire

**Cela veut dire** :
- les **mêmes raccourcis** (G/R/S, TAB, E, I, K, Ctrl+R, Alt+clic, B/C, `.` et `,`) ;
- le **même vocabulaire** (« pivot », « orientation », « modificateur »,
  « appliquer », « aimantation ») ;
- le **même modèle mental** : mode objet ↔ mode édition, pile non destructive,
  élément actif distinct de la sélection.

**Cela ne veut pas dire** :
- le même **nombre** de fonctions. Blender a trente ans et des centaines de
  contributeurs ;
- les mêmes **algorithmes** internes ;
- reproduire ses défauts par mimétisme.

**Règle d'arbitrage, à appliquer chaque fois que la question se pose :** quand
notre comportement diffère de Blender, ce doit être une **décision écrite et
motivée**, pas un accident. Le cas déjà tranché fait jurisprudence — j'avais
refusé de déplacer une lumière directionnelle au motif que ça n'a pas d'effet sur
l'image ; c'était une erreur, parce que dans Blender une lampe **est** un objet
qu'on range. Correction : le geste est autorisé, l'absence d'effet est **dite**.

---

## 4. Périmètre

### 4.1 Dans le produit (v1)

| domaine | contenu |
|---|---|
| **Modes** | objet ↔ édition (sommet / arête / face, combinables) |
| **Navigation** | orbite / panoramique / zoom, vues numpad, caméra éditeur |
| **Sélection** | clic, multi, zone (rectangle / lasso / cercle), boucles, tout/rien, **élément actif distinct** |
| **Transformation** | déplacer / tourner / redimensionner, verrou d'axe, pivot (5 modes), orientation (global/local/normal), **aimantation** |
| **Édition de maillage** | extruder, insérer, chanfreiner, subdiviser, loop cut, couteau, bisect, souder, dissoudre, spin, to sphere, shrink/fatten, edge split, proportional editing, symétrie |
| **Modificateurs** | les **17 types** livrés, pile réordonnable, activer / dupliquer / retirer / **appliquer**, paramètres générés depuis la table publiée |
| **Ombrage** | flat / smooth, par angle |
| **Affichage** | modes viewport (fil de fer / solide / matériau / rendu + normales / occlusion), matcaps, grille, gizmos |
| **Fichier** | nouveau / ouvrir / enregistrer une scène, importer les 7 formats de maillage déjà lus, journal de commandes rejouable |
| **Annulation** | historique complet, y compris sur les opérations de pile |

### 4.2 Hors produit (v1) — et où ça vit

| ce qui n'y est pas | pourquoi | où |
|---|---|---|
| Édition UV, dépliage | dépend du dépliage automatique, encore à écrire | v2 (`NKGen`, roadmap IA) |
| Nœuds de matériaux | substrat NKGraph déjà décidé, consommateur à part | Nogee |
| Animation, rigging | module dédié | NkAnima |
| Rendu final, compositing | politique de rendu | Nogee / PV3DE |
| Simulation (eau, fumée) | module `NKSimulation`, encore en spécification | NKSimulation |
| Modificateurs à dépendance externe | groupes de sommets, textures, autre objet, simulation, poils — les systèmes n'existent pas | quand leur dépendance existera |

**Cette colonne « pourquoi » est le cœur du document.** Sans elle, chaque absence
ressemble à un oubli et sera re-proposée à chaque session.

### 4.3 Le SCULPT est dans le produit *(décision de Rihen, 31/07)*

> Il figurait d'abord hors périmètre. **C'était une erreur** : le sculpt fait
> partie de ce qu'on attend d'un modeleur, et surtout il impose une contrainte
> d'architecture qu'il vaut bien mieux poser **avant** d'écrire l'application.

**Ce qui existe déjà et le sert** — il ne s'agit pas de partir de zéro :

| brique existante | ce qu'elle apporte au sculpt |
|---|---|
| **proportional editing** (6 courbes d'atténuation) | c'est **exactement** la mécanique de retombée d'une brosse. Une brosse est un déplacement pondéré par une courbe autour d'un point — le calcul existe et il est testé. |
| **symétrie 1-3 axes** | sculpter symétriquement, sans code neuf |
| **Catmull-Clark** | la densité de maillage que le sculpt exige |
| **cycles radial et disque** (BMesh étape 2) | le voisinage d'un sommet en O(k), nécessaire au lissage et à la dilatation |
| **harnais, 116 cas** | un filet déjà en place pour une opération qui déforme massivement |

**⚠️ LE VRAI OBSTACLE, et il est architectural.** Aujourd'hui, toute opération
d'édition **reconstruit et resynchronise le maillage entier** (`Demo3D_SyncFromHE`).
C'est sans conséquence sur un cube de 24 sommets ; c'est **rédhibitoire** pour du
sculpt, où chaque mouvement de souris touche quelques milliers de sommets sur un
maillage qui peut en compter des centaines de milliers, à 60 images par seconde.

> **Conséquence pour la v1, à respecter même avant d'écrire la première brosse :**
> la chaîne doit permettre une **mise à jour PARTIELLE** — modifier un sous-ensemble
> de sommets et n'envoyer au GPU que ce sous-ensemble. Ce n'est pas du sculpt, c'est
> une propriété de la chaîne de rendu. **L'ajouter après coup coûterait une refonte
> du chemin d'édition ; l'y prévoir maintenant ne coûte presque rien.**

**Ce qui manque vraiment**, par ordre de dépendance :

| # | brique | pourquoi elle est nécessaire |
|---|---|---|
| **S1** | **mise à jour partielle** du maillage GPU | sans elle, tout le reste est inutilisable en pratique |
| **S2** | **requête spatiale** (grille ou arbre) : « quels sommets dans ce rayon ? » | un balayage linéaire par mouvement de souris s'effondre dès la centaine de milliers de sommets |
| **S3** | **trait** (stroke) : entrée continue, espacement, pression | une brosse n'est pas un clic ; c'est ce qui donne un geste régulier plutôt qu'une série de bosses |
| **S4** | **brosses** : tirer, attraper, lisser, gonfler, aplatir, pincer | la partie visible — et la plus rapide à écrire une fois S1-S3 en place |
| **S5** | **densité adaptative** : dynamic topology **ou** multirésolution | ajouter du détail là où on sculpte. **À trancher :** dyntopo est plus simple et plus libre, la multirésolution préserve une cage éditable et permet de revenir en arrière. Les deux ne se remplacent pas. |

**Position honnête** : S1 est une contrainte à respecter **dès la v1** ; S2 à S5
constituent un chantier à part entière, à ouvrir quand l'interface existe — parce
qu'une brosse sans interface pour la régler ne se teste pas.

### 4.4 Verrou partagé identifié

Les **groupes de sommets** bloquent simultanément : Mask par groupe, les trois
modificateurs Vertex Weight, et le skinning côté animation. C'est le prérequis au
meilleur rapport valeur/coût du périmètre v2 — à traiter comme une brique du
noyau, pas comme une fonction du modeleur.

---

## 5. Contraintes non négociables

Elles viennent du projet, pas du produit, et aucune n'est discutable ici :

1. **Zéro STL.** `NKMemory`, `NKMath`, `NKContainers`. Ce qui manque s'ajoute au
   module concerné.
2. **Allocation uniquement via NKMemory** ; tout `Create` a son `Destroy`.
3. **From scratch** : aucun code tiers, même reformaté. Sont légitimes : un
   oracle en boîte noire, un texte de spécification, des tables extraites par
   script depuis un standard.
4. **Cinq backends** (Vulkan, DX11, DX12, OpenGL, logiciel) : aucune fonction ne
   peut dépendre d'un seul.
5. **Debug ET Release** doivent compiler et se comporter pareil.
6. **Toute évolution du maillage passe par le harnais** : cas ajoutés **en fin**,
   les lignes précédentes gardant leur numéro.
7. **Aucun raccourci en dur** : ils passent tous par la table de raccourcis de
   NKEditorKit. C'est ce qui rend possible à la fois leur affichage dans
   l'interface et leur reconfiguration.

---

## 6. Critères d'acceptation

Une étape est finie quand ces phrases sont **vraies et vérifiables**, pas quand
le code compile.

### Jalon A — l'outil devient utilisable *(cible immédiate)*
- [ ] L'application démarre, ouvre une scène, affiche un viewport navigable.
- [ ] Le **panneau de modificateurs** liste les 17 types, permet d'en ajouter un,
      de le déplacer dans la pile, de le désactiver, de le dupliquer, de le
      retirer, de l'appliquer — **à la souris, sans toucher au clavier**.
- [ ] Les paramètres affichés sont **générés depuis `NkModifierParams`** : ajouter
      un 18ᵉ modificateur ne demande **aucune ligne d'interface**.
- [ ] Un **en-tête** montre en permanence : mode, aimantation (état + pas + grille),
      pivot, orientation, objet actif. Aucune de ces informations ne doit plus
      exiger de lire un fichier de log.
- [ ] La **table de raccourcis** est affichable, et modifier une entrée change
      effectivement le comportement.

### Jalon B — parité de geste
- [ ] Un utilisateur de Blender modélise une forme simple **sans consulter d'aide**.
- [ ] Les 40 raccourcis existants sont dans la table, aucun en dur.
- [ ] Écarts de comportement avec Blender : **liste écrite**, chacun motivé.

### Jalon C — l'IA passe par la même porte
- [ ] Toute opération de l'interface produit une **commande** rejouable.
- [ ] Rejouer le journal d'une session reproduit le maillage **au bit près**
      (même critère que `NK_EDIT_IDENTITY`).

---

## 7. Ce qui pourrait faire échouer ce projet

Écrit d'avance, pour être surveillé :

1. **Recommencer le viewport au lieu de l'extraire.** Demo3D contient dix mille
   lignes éprouvées (pick, cage, opérations modales, journal, rejeu). Les
   réécrire « proprement » perdrait des mois et des correctifs subtils. Le cap est
   l'**extraction**, prouvée par déplacement — la méthode a déjà servi et est
   documentée (`66a489ba` : 0 ligne supprimée sur la comparaison ligne à ligne).
2. **Une interface qui double la logique.** Si un bouton fait autre chose que la
   commande correspondante, l'IA et le rejeu divergeront de l'humain, en silence.
3. **Ajouter des fonctions avant que les précédentes soient atteignables.** C'est
   l'erreur déjà commise : 17 modificateurs qu'on ne peut pas cliquer.
4. **Des raccourcis en dur.** Ils rendent la table décorative et l'affichage faux.

---

## 8. Ordre de travail décidé

Il découle du cap déjà acté dans `Engine/Noge/HANDOFF.md` :

1. **Table de raccourcis** dans NKEditorKit (configurable, aucun raccourci en dur).
2. **Extraction du câblage viewport** de Demo3D vers NKEditorKit, par déplacement
   prouvé, à comportement identique.
3. **Application NK3DModeler** : fenêtre, disposition, panneaux.
4. **Panneau modificateurs** piloté par la table de paramètres.
5. **En-tête et barre d'état**.
6. Le reste, par ordre d'utilité mesurée à l'usage.

Chaque étape doit laisser l'outil **au moins aussi utilisable** qu'avant. Aucune
étape n'a le droit de casser Demo3D tant que l'extraction n'est pas terminée :
c'est aujourd'hui le seul endroit où la modélisation fonctionne.
