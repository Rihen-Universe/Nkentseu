# UnkenyEditor — le maillage 2D

### Document 06 — R31 : un composant de maillage 2D, sa fenêtre d'édition, une physique par partie

> Rihen, 01/10/2026 : *« un composant de maillage 2D (mesh renderer / mesh editing)
> avec sa fenêtre d'édition : éditer les sommets, découper en parties, et donner une
> physique indépendante à chaque partie si on le souhaite. »*
>
> Et le même jour, validé : R30 (squelette 2D et skinning) vivra dans NKAnima,
> contraint au plan, et **déformera CE maillage par des poids d'os** — d'où la
> règle de ce chantier : le maillage porte DÉJÀ, par sommet, jusqu'à 4 os et leurs
> poids, sans que rien ne les lise encore.

Rédigé le 02/10/2026 sur la branche `unkeny/maillage-2d` (dossier
`Nkentseu-g-r31`), après le code et ses bancs. Tout ce qui est dit « fait » ici est
**exercé** par `UnkenyEditor --selftest` (et `--banc-maillage`) ou vu sur les
captures de `References/Captures/maillage-2d/`.

---

## 1. Où cliquer

| geste | où |
|---|---|
| créer depuis un sprite | clic droit sur l'entité > **Créer un maillage 2D depuis le sprite** ; ou **Détails > Ajouter un composant > Maillage 2D depuis le sprite (contour de l'image)** |
| créer depuis une forme 2D | **Ajouter un composant > Maillage 2D depuis la forme** |
| créer vide | **Ajouter un composant > Maillage 2D vide (sommets à poser)** |
| ouvrir la fenêtre d'édition | carte **Maillage 2D** des Détails > **Éditer le maillage…** ; ou clic droit > **Éditer le maillage 2D…** — elle s'ouvre dans un **onglet de document** (à droite de la scène, la marque verte en triangles) |
| choisir | clic sur un sommet ; **Maj+clic** ajoute / retire ; clic sur une **arête** prend ses deux bouts ; **glisser dans le vide** = cadre de choix ; **Ctrl+A** tout ; **Échap** rien |
| déplacer | glisser un sommet choisi (ses doubles de couture suivent) ; l'**aimant** (bouton aimant de la barre de la vue, partagé avec la scène) colle aux sommets puis aux arêtes ; **Ctrl** l'inverse |
| ajouter | outil **Ajouter (A)** : un clic = un sommet (dedans il coupe son triangle, sur une arête il la coupe, dehors il se relie au bord) ; **Q** revient à la Sélection |
| couper une arête | **double-clic** sur l'arête |
| supprimer | **Suppr** (ou le bouton) : le trou se rebouche dedans, le bord se rogne |
| trianguler | **Trianguler (T)** : Delaunay sur les sommets actuels, la forme gardée |
| densité | **–** / **+** puis **Refaire** : le contour actuel, reéchantillonné, des sommets intérieurs (les parties reviennent à « Corps ») |
| faire une partie | choisir des sommets, **P** (ou **Faire une partie (P)**) : les triangles entièrement choisis deviennent une partie ; sa couture est **dédoublée** |
| la physique d'une partie | panneau de droite > la partie > **Physique : Aucune / Rigide / Molle** ; rigide : **Corps** (statique, cinématique, dynamique), **Collisionneur depuis son contour**, masse, friction, rebond ; molle : masse, raideur, forme, friction, rebond ; **Ordre de dessin** |
| relier deux parties | panneau > **Liens** > **Relier à « … »** ; puis **Genre : Rigide / Élastique / Pivot**, **Rupture** (0 = incassable), raideur et amortissement d'un élastique ; **x** le retire |
| défaire | **Ctrl+Z / Ctrl+Y** dans la fenêtre comme dans la scène (le maillage est un composant de la scène) |
| enregistrer comme asset | **Enregistrer comme asset** (barre de la fenêtre, ou la carte) : `Contenu/Maillages/<nom>.nkmesh2d` |
| réutiliser l'asset | la carte > **Utiliser : Maillages/…** ; ou glisser le `.nkmesh2d` du Contenu dans la vue (clic droit > **Poser au centre de la vue**) |
| jouer | **Jouer** : chaque partie physique vit, le maillage la suit ; la fenêtre d'édition montre le maillage **tel que la physique le tient** (lecture seule) |

Captures sans souris : `UnkenyEditor --captures-maillage=DOSSIER` (la Gelée du Dernier
Feu dessinée au programme : contour, trois parties, Jouer).

---

## 2. Ce qui est fait

### 2.1 Le composant — `Engine/Unkeny/src/Unkeny/Maillage/NkUnkenyMaillage.h`

`NkMaillage2D` : **128 sommets** (position, UV, couleur RGBA, partie ; **4 os +
4 poids 16 bits** pour R30), **240 triangles** (sens trigonométrique, partie par
triangle), **8 parties** (nom, couleur d'édition, **ordre de dessin**, physique,
réglages du corps), **16 liens**, texture (par son **nom** au fichier), teinte,
couche (le tri des sprites et des formes), visibilité, `source` (l'asset d'origine).

- **Données seulement, capacité fixe** : la photo (Jouer / Arrêter, Ctrl+Z), les
  prefabs et le `.nkscene` le portent **champ par champ** (`NkChampsMaillage2D`,
  déclaré par `NkScene::Init` après les scripts). Le prix, mesuré : ~6,5 Ko de plus
  dans la photo de **chaque** entité (`NkPhotoEntite::extra` prend la taille de tous
  les composants décrits) — à peser avant d'augmenter les capacités.
- **Chaque sommet appartient à UNE partie.** Une couture entre deux parties est faite
  de **sommets doubles** (un par partie, à la même place) : c'est ce qui permet à
  deux parties de se séparer. `NkMaillageRefaireCoutures2D` tient cette règle après
  chaque geste (fusion des doubles d'une même partie, dédoublement d'un sommet
  partagé, orphelins retirés).
- **Fabrication** (ce qui manquait au dépôt est écrit là, le reste est repris) :
  - depuis un **sprite** : le contour de l'**alpha** de sa région d'image (suivi de
    fissure sur la grille des pixels, plus grande île 8-connexe, image réduite à
    ~192 px), simplifié (Ramer-Douglas-Peucker), reéchantillonné à la **densité**,
    des **points intérieurs** en quinconce, **Delaunay** (Bowyer-Watson, en double)
    restreint à l'intérieur ; si l'aire couverte s'écarte de plus de 2 % du contour
    (une concavité enjambée), repli sur le découpage en **oreilles** de NKMath
    (`NkEarcutVers`). UV et couleur reprises du sprite ;
  - depuis une **forme 2D** (`NkContourForme2D`, arrondis compris), à sa couleur ;
  - **rectangle** en grille ; **vide**.
- **Édition** : ajouter un sommet, couper une arête, retirer des sommets,
  trianguler (Delaunay, forme gardée, parties reprises), refaire à une densité (UV
  et couleurs **lues dans l'ancien maillage** : la texture ne glisse pas), retournements
  de Lawson à l'intérieur d'une partie.

### 2.2 Le dessin — `Rendu/NkUnkenyRendu.cpp`

Le maillage entre dans **le même tri par couche** que les sprites et les formes. Un
seul appel : `NkGuiDrawList::AddMesh` (ajouté à NKGui, additif) — un maillage indexé,
**un UV et une couleur par sommet**, la texture déformée ; les triangles dans
l'ordre de dessin de leurs parties. Le hors-champ se juge sur les sommets **en
monde** (un éclat parti loin de son entité se dessine où il est).

### 2.3 La physique par partie — `Maillage/NkUnkenyMaillagePhysique.h`

À la manière de l'« asset physique » d'Unreal : le composant **décrit**, les corps
n'existent que dans la scène **qui joue**. Ils naissent au premier `Pas` d'une scène
physique (`NkMaillagesConstruire`) et meurent avec l'entité, à `Restaurer`
(Arrêter), ou quand l'entité s'éteint.

| partie | ce qui naît | le maillage |
|---|---|---|
| **Aucune** | rien | suit l'entité (son transform — donc son corps rigide s'il en a un) |
| **Rigide** | un **corps libre** de la scène (`NkScene::CreerCorpsLibre`, même pont 2D↔3D, même matériau, même masse qu'`AjouterCorps`), posé au centre de la partie ; collisionneur depuis le **contour de la partie** : enveloppe convexe à 8 sommets si dynamique (ce que NKCollision fait tomber), chaîne exacte sinon | chaque sommet garde sa place dans la partie (sa pose) |
| **Molle** | un corps de particules NKPhysics (XPBD, préréglage gelée) : **une particule par sommet**, les arêtes en liens, la flexion entre triangles voisins, l'appariement de forme ; marqué `NK_CORPS_MOU_DE_MAILLAGE` (le rendu des corps mous ne le dessine pas, ses contacts vont à l'entité du maillage) | les sommets **sont** les particules |

**Liens** : rigide↔rigide par les joints de NKPhysics (**soudure**, **pivot**) ou par
deux **ressorts** aux bouts de la couture (**élastique**, forces posées à chaque pas
fixe) ; mou↔rigide par les **attaches** de `NkParticules2D` (raideur 1, ou 0,08
élastique ; rupture en mètres) ; mou↔mou en **un seul** corps de particules (sinon
leurs particules confondues à la couture se repousseraient), reliées par une bande de
liens ; une partie **Aucune** est tenue par le corps de l'entité, ou à défaut par une
**ancre cinématique** qui suit le transform de l'entité. **Rupture** : la force
transmise par le joint (impulsion du dernier sous-pas / sa durée), ou l'allongement
d'un ressort ; un lien cassé est **éteint** (voir § 4).

Les parties physiques d'un même maillage **ne se touchent pas entre elles** par défaut
(un bit de **groupe**, 16 à 31, retiré de leur masque) ; la case « Les parties se
touchent » le lève.

### 2.4 L'asset `.nkmesh2d` — la décision de format

`CONVENTIONS_FICHIERS.md` § 1 : *deux fichiers qu'on peut déposer au même endroit
avec le même effet partagent leur extension ; sinon, ils en changent.* Le `.nkmesh`
est le StaticMesh 3D, consommé par NKRenderer (Noge, NKCraft) ; un maillage 2D
d'Unkeny ne se dépose pas au même endroit (Unkeny rend par NKCanvas, exclusif de
NKRenderer) et ne produit pas la même chose (une texture **plane** déformée, des
**parties** avec chacune sa physique, des liens, un ordre de dessin, demain des poids
d'os contraints au plan). Une « marque 2D » dans le `.nkmesh` ferait dire deux
choses à une extension selon son contenu — ce que le corollaire du § 1 refuse.
D'où **`NkAssetType::Mesh2D` (20) ↔ `.nkmesh2d`**, déclaré au **point de passage
unique** de NKSerialization (`NkAssetExtensionFor` / `NkAssetTypeFromExtension`).
Le contenu est du JSON qui porte son `"format": "unkeny.maillage2d"` (comme
`.nkscene` et `.nkprefab`), écrit par **le même code** qu'un maillage sous « jeu »
d'une scène (`interne::EcrireComposant`) : les deux ne peuvent pas diverger.

### 2.5 L'éditeur — `Applications/UnkenyEditor/src/Editeur/`

| fichier | rôle |
|---|---|
| `NkEditeurMaillage.h/.cpp` | créer (sprite, forme, vide), les **gestes du modèle** (tous retenus : Ctrl+Z), l'asset, les documents, les actions `NK_A_MAILLAGE*` (plage 2450-2499) |
| `NkEditeurPageMaillage.cpp` | la **fenêtre d'édition** (onglet de document `NK_MAILLAGE`) : barre d'outils, vue (texture, parties colorées, arêtes, sommets, aimant, cadre de choix), panneau des parties et des liens, clavier |
| `NkEditeurDetails.cpp` | la carte **Maillage 2D** (pastille Rendu) |
| `NkEditeurChrome.cpp` | « Ajouter un composant » (les trois sources), le clic droit sur un sprite |
| `NkEditeurActions.cpp` | prise au clic sur les triangles, boîte de sélection, échelle cuite, dupliquer, retirer (le sprite masqué réapparaît) |
| `NkEditeurTiroir.cpp`, `NkEditeurContenu.cpp` | le `.nkmesh2d` au Contenu : nature « Maillage 2D », se pose dans la vue |

---

## 3. Les bancs et les captures

- **BANC MAILLAGE 2D** (moteur, `Engine/Unkeny/src/Unkeny/Banc/NkUnkenyBancMaillage.cpp`) :
  **43/43** — depuis un sprite (12 à 128 sommets, aucun triangle retourné, l'aire à
  ±12 % des pixels opaques, l'échancrure vide, UV dans la région), densité, image
  opaque / transparente, étoile ; dessin (texture déformée, déplacer un sommet
  déplace l'image, couleurs de sommet, ordre des parties) ; enregistrer / relire le
  même JSON, l'asset, le prefab, le type Mesh2D ; faire une partie (coutures
  dédoublées), la retirer ; une partie rigide **tombe et se pose**, le maillage la
  **suit** ; soudée elle reste, en pivot elle tourne autour de la couture, une
  rupture faible casse ; une partie **molle** tombe, ses sommets sont ses
  particules ; Restaurer rend le repos sans corps restant ; ajouter / couper /
  retirer / trianguler. **Contre-épreuves** : un triangle inversé (m1n), une couture
  rompue (m6n), sans physique rien ne tombe (m7n) — chaque fois le témoin voit l'ÉCHEC.
- **BANC MAILLAGE ÉDITEUR** (`NkEditeurBancMaillage.cpp`, sur la vraie trame) :
  **15/15** — Ajouter un composant, Ctrl+Z / Ctrl+Y ; clic droit > Créer ; la carte
  des Détails ; clic et glisser d'un sommet, Ctrl+Z ; **contre-épreuve** (n4n) : le
  même déplacement sans retenir, Ctrl+Z ne le rend pas ; Ajouter (A) / Suppr ; cadre
  de choix + P, bouton « Rigide » ; **Jouer** : la partie tombe de 3,4 m, le corps
  reste ; Arrêter : aucun corps ne reste ; enregistrer / rouvrir ; l'asset.
- Joués par `UnkenyEditor --selftest` (et `--banc-maillage`), le banc du moteur aussi
  par `Physic2D --selftest` et `UnkenyPlayer --selftest`.
- **Captures** `References/Captures/maillage-2d/` : 01 la carte dans les Détails,
  02 la fenêtre (sommets, triangles, texture), 03 trois parties colorées (lobe
  droit rigide), 04 la partie molle et son lien élastique (sans texture), 05-06
  **Jouer** : le lobe droit tombe et se pose sur le sol, 07 la fenêtre en jeu
  (lecture seule), 08 le clic droit, 09 « Ajouter un composant ».

---

## 4. Limites connues (dites, pas cachées)

- **L'état des corps des parties n'est pas photographié** : une photo prise EN JEU
  (et donc un enregistrement pendant le jeu) rend le maillage au repos. Jouer /
  Arrêter et Ctrl+Z (en édition seulement) ne le voient pas.
- **Les particules ne touchent pas les polygones** (`NkParticules2D` ne lit que
  cercles, boîtes, capsules et segments) : une partie molle traverse une partie
  rigide polygonale et un sol en chaîne ; elle se pose sur une boîte.
- **Un lien cassé est éteint, pas retiré** : `NkPhysicsWorld::DestroyJoint` retrouve
  un joint par « indice + 1 », et en retirer un décalerait les suivants. Les joints
  éteints restent dans le monde jusqu'à sa destruction (un test par pas chacun).
- **Les groupes de collision** tiennent sur 16 bits : au-delà de 16 maillages
  physiques simultanés, deux maillages partagent un bit et leurs parties ne se
  touchent plus entre elles. Une partie de groupe n'obéit pas à la matrice des
  calques (sa couche n'est que son bit).
- **Le contour d'un sprite** prend la plus grande île et ignore les trous (un anneau
  devient un disque) ; un pixel au seuil d'alpha près.
- **Les doubles d'une couture** reprennent les attributs du premier à chaque geste
  (UV et couleur d'un côté valent pour l'autre).
- La force de rupture d'une soudure est une **estimation** (impulsion du dernier
  sous-pas / sa durée), pas une mesure de contrainte.

---

## 5. Ce que R30 trouvera prêt (le squelette 2D et le skinning)

- **Les poids sont déjà dans le composant et au fichier** : `os[4]` (indices d'os,
  `NK_MAILLAGE2D_SANS_OS` = aucun) et `poids[4]` (16 bits, somme
  `NK_MAILLAGE2D_POIDS_PLEIN`) par sommet, écrits champ par champ (`os`, `poids`).
  Un `.nkscene` ou un `.nkmesh2d` d'aujourd'hui se relira tel quel quand R30 les
  remplira ; un poids nul = un emplacement libre.
- **Le point de branchement est unique** : `NkMaillagePositionsMonde` (la place des
  sommets telle qu'on la dessine). R30 y ajoute le cas « sommet pondéré » : position
  = Σ poids × (matrice de l'os × position de liaison), les os venant du squelette
  NKAnima contraint au plan. Le rendu, la prise au clic, la boîte de sélection et
  la fenêtre d'édition en jeu passent tous par là : rien d'autre à toucher.
- **Les parties sont des régions de skinning toutes trouvées** (« un bras, le
  corps ») : l'auto-pondération de R30 pourra partir d'elles (une partie = un os
  par défaut), et la physique d'une partie restera possible pour le mouvement
  secondaire (écharpe, cheveux en partie molle, R30).
- **La fenêtre d'édition a déjà sa place pour un mode « Poids »** : un outil de plus
  dans la barre (`NkOutilMaillage`), le panneau de droite pour la liste des os ;
  l'aimant, le cadre de choix et Ctrl+Z sont communs.
- **L'ordre de dessin par partie** couvre déjà les « slots » 2D (un bras derrière
  le corps) dont R30 aura besoin.

---

## 6. Ce qui reste (après R31)

- Un **mode Poids** dans la fenêtre (R30) et l'auto-pondération.
- Des **trous** dans le contour d'un sprite (anneau), plusieurs îles.
- Photographier l'état des corps des parties (un enregistrement en jeu fidèle).
- Faire toucher les polygones aux particules (NKPhysics), pour une gelée sur un
  décor en chaîne.
- Retirer vraiment un joint cassé (corriger `DestroyJoint` de NKPhysics : un
  identifiant stable par joint).
- Un aperçu « Simuler » DANS la fenêtre (jouer le seul maillage, sans lancer la scène).
