# NKCraft — le générateur de créatures

### Document 4 — Spécification : squelette paramétrique, topologie par construction, coexistence humain ↔ IA

> **AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen**
> Rédigé le 28/09/2026. Origine : Rodolf, le 28/09, avec dix planches de référence
> (mannequins en blocs, anatomie stylisée, « Puncher ») :
>
> - *« entraîner une IA qui fait de la modélisation : elle commence par un blockout
>   paramétré, partie par partie, en ajoutant les membres si on demande — 4 doigts ou
>   plus, plusieurs pieds —, les animaux comme les humains ou les créations
>   paramétriques ; après, elle ajoute une couche pour lisser, détailler, former les
>   yeux, la tête ; et le maillage doit être correct : des quads bien alignés, faciles
>   à animer pour le jeu vidéo et le film d'animation »* ;
> - *« à partir d'images, une ou plusieurs vues, ou d'une description complète, avec
>   la touche humain ↔ IA, modifiable des deux côtés sans problème »* ;
> - *« on doit pouvoir gérer des créatures de toute sorte, inventées, ni homme ni
>   humain, monstre, dragon… l'artiste sera libre »* ;
> - *« trouver des articles et projets pour que notre retopologie atteigne au minimum
>   95 % »* ;
> - *« le résultat peut être modifié par l'homme, par sculpt ou par édition, puis
>   repris par le système automatique ou l'IA **sans dénaturer ce qui a été retouché**,
>   en l'intégrant dans les modifications — et vice versa »* ;
> - *« R32 doit être résolu »* ;
> - *« tu oublies les animaux marins, les animaux à ailes, les créatures à ailes ou
>   marines et inventées ? »* — ajouté au §5.7.
>
> **Ce document complète** `01-produit-nkcraft.md` (§4.3 retopologie, §4.6 générateurs,
> §4.7 IA) et `Tools/Genia/FORMAT_SCENE.md` (§8, R32). **Il ne les remplace pas** ; sur
> le générateur de créatures, la coexistence humain ↔ IA et R32, il est plus récent.
> Les documents 02 et 03 de NKCraft recevront ses écrans (§15).
> **Compagnon** : le **document 5** (vêtements, cheveux, éléments secondaires), qui s'attache
> aux créatures par les mêmes adresses.

---

## 0. En une phrase

**Le générateur de créatures de NKCraft construit n'importe quel être — humain,
animal de la terre, de l'air ou de l'eau, monstre, dragon, kraken, créature inventée
à six bras ou huit pattes — à partir d'un
squelette et de profils que l'artiste ou l'IA règle ; il en tire directement un
maillage en quads prêt à animer, déjà riggé ; et chaque retouche humaine, en édition
ou en sculpt, est gardée et reportée quand l'automatique ou l'IA reprend la main.**

---

## 1. Ce qui existe au 28/09 — vérifié dans le dépôt

| sujet | état | où |
|---|---|---|
| maillage éditable (demi-arêtes, n-gones), Catmull-Clark, boucles, extrusion, biseau, `BuildSweep`, journal de commandes rejouable, annuler / rétablir | ✅ | `NKRenderer/Mesh/NkEditMesh.*`, banc `NKEditMeshHarness` |
| `.nkscene` (Genia) : parties **nommées** (`partie <nom> forme …`), **modifiable à la main** en texte (« Rodolf peut corriger une valeur sans outil ») | ✅ texte ; ❌ pas d'éditeur dans l'application | `Tools/Genia/FORMAT_SCENE.md:50`, `NkModelerCreation.h` |
| gabarits `personnage` et `creature` | ✅, mais **nombre de membres, de doigts, de pattes écrit en dur**, pas de mains | `Tools/Genia/gabarits/` |
| régénérer depuis `.nkscene` | ⚠️ **écrase** toute retouche manuelle | `ETAT_REPRISE_GENIA3D.md:131-140` |
| R32 (base régénérée + pile d'opérations rejouée, désignation par nom ou critère) | 📝 **conçu, pas codé** ; la sélection par critère n'existe pas | `FORMAT_SCENE.md` §8 |
| retopologie à champ de directions | ⚠️ 26,9 % de sommets de valence 4 sur un personnage généré (98,3 % pour la référence du marché) | `NkMeshRetopo.*` |
| sculpt par pinceaux | ✅ (sur `NkEditMesh`) | `Tools/MeshSculpt` |
| familles paramétriques d'objets (table, porte, maison) avec liaisons | ✅ — le modèle de déclaration à reprendre | `NkMeshFamilles.*` |
| images : PNG, JPEG, WebP sans perte, EXR, BMP, TGA, GIF, QOI, HDR, SVG… | ✅ `NkImage::LoadFromFile` | `NKImage` |
| vidéo → images RGBA | ✅ `media::NkVideoReader` | `NKMedia` |
| pont images → entraînement (NKData) | ❌ NKData ne lit que MNIST et du texte | `Kernel/AI/NKData` |
| visage : unités d'action FACS → poids de blendshapes | ✅ `anim::NkFaceController`, employé par PV3DE et NK3DModeler | `NKAnima/Face/` |
| paramètres de visage procédural (`NkProcFaceParams`), `NkFacialRig` | 📝 déclarations sans implémentation | `Engine/Noge/.../Facial/NkFacialRig.h` |
| squelette, skinning GPU, IK, reciblage | ✅ | `NKAnima` (`NkSkeletonDef`, `NkAnimRetarget`), `NkIKSolver` |
| placement automatique du squelette, calcul des poids, peinture de poids | ❌ (spécifiés pour NkAnima : `07-produit-nkanima.md` §4.1-4.2) | — |
| corpus de vues | 3 927 modèles × 48 vues (4,7 Go) — **provenance et licences à vérifier** avant tout entraînement (§11.3) | `logs_genia3d/corpus_vues` |

---

## 2. Le principe : la topologie par construction

### 2.1 Pourquoi une retopologie « après coup » ne suffit pas

Sur une surface fermée faite de quads, la somme des écarts de valence est fixée par
la topologie :

  **Σ (4 − valence(v)) = 8 − 8g**  (g = nombre d'anses : 0 pour presque toutes les créatures ; 1 pour une créature « percée » d'un trou, que le squelette sait décrire par une boucle, §5.7)

Un maillage fermé sans trou n'a donc besoin, au minimum, que de **8 sommets
irréguliers**. Sur une créature, les sommets irréguliers viennent presque tous des
**bouts** (bout des doigts, des cornes, de la queue) et des **embranchements**
(épaules, hanches, base des doigts). Leur nombre **I** dépend de la forme, pas de la
résolution. Avec **N** sommets :

  **part de valence 4 ≥ 1 − I / N**  → **≥ 95 % dès que N ≥ 20 · I**

Une étape de Catmull-Clark rend 100 % des faces quadrangulaires et multiplie N par
environ 4 **sans créer** de nouveaux sommets irréguliers. Notre 26,9 % ne vient donc
pas d'une limite de la géométrie : il vient d'un champ de directions qui place des
singularités partout.

### 2.2 La conséquence

On ne demande pas à un algorithme de **deviner** la topologie d'un corps : on la
**construit** à partir du squelette.

- chaque **os** devient un **tube d'anneaux** de quads, perpendiculaires à l'os ;
- chaque **articulation** reçoit **2 ou 3 boucles** — ce sont les **bandes rouges** des
  mannequins de référence ;
- chaque **embranchement** reçoit un **nœud de jonction** en quads, quel que soit le
  nombre de branches (une main à 3, 4, 5 ou 7 doigts, un torse à 6 bras) ;
- chaque **bout** reçoit un **capuchon** ;
- les pôles (valence 3 ou 5) sont **au centre** des jonctions et des capuchons, **jamais
  sur une bande de pli**.

**Ce qui en découle** :
1. la **topologie est propre dès le départ** et mesurable (§6) ;
2. **le rig et les poids viennent gratuitement** : le squelette qui construit le corps
   *est* le rig (§9) ;
3. pour une même famille, **la topologie est fixe** : l'IA n'invente pas un maillage,
   elle prédit des **nombres** (§11) ;
4. chaque sommet a une **adresse stable** (§3.2) : c'est ce qui permet de garder les
   retouches humaines (§10).

### 2.3 L'état de l'art qui confirme la voie

| travail | apport | code, licence |
|---|---|---|
| **FEQ** — *Face Extrusion Quad meshes*, Pandey, Bærentzen, Singh, SIGGRAPH 2022 | squelette → maillage **100 % quads** ; nœud de jonction pour **n'importe quel nombre** de branches (Delaunay sphérique des directions, puis « valencification ») ; symétrie ; recalage sur un maillage de référence | **GEL** (`skeleton_to_FEQ.cpp`, `quad_valencify.*`), **licence MIT** (vérifiée) → **portable** |
| **SQM** — Bærentzen, Misztal, Wełnicka, 2012 | le principe squelette → polyèdre de jonction → anneaux | remplacé par FEQ dans GEL |
| **PAM** — *Polar-Annular Mesh*, Bærentzen, Abdrashitov, Singh, SIGGRAPH 2014 | le squelette **vit dans les boucles** du maillage | article |
| **B-Mesh** — Ji, Liu, Wang, 2010 ; **Skin Modifier** de Blender | cadres carrés par nœud, enveloppe convexe aux jonctions | Blender = GPL : **réécrire depuis l'article**, ne pas copier |
| **Scaffolding a skeleton** — Panotopoulou et al., 2018 | la cage de quads la plus grossière sans jonction en T ; choix des tailles d'anneaux | article |
| **Usai et al.**, *Extraction of the quad layout of a triangle mesh guided by its curve skeleton*, ACM TOG 2015 | disposition de quads **guidée par le squelette** pour un maillage sculpté existant, pensée pour l'animation | article |
| **Takayama, Panozzo, Sorkine-Hornung**, *Pattern-Based Quadrangulation for N-Sided Patches*, SGP 2014 | remplir en quads n'importe quel patch à 2–6 côtés (condition : nombre pair de segments sur le bord) | article |
| **QuadriFlow** — Huang et al., SGP 2018 | remaillage **100 % quads**, ~4× moins de singularités qu'Instant Meshes, ~5 s par million de triangles | **BSD-3** (vérifiée) → portable |
| **Instant Meshes** — Jakob et al., 2015 | champs orientation + position (notre retopologie actuelle en est proche) | BSD-3 |
| QuadWild, QuadWild-BiMDF, libQEx | qualité CAO excellente | **GPL-3** : articles seulement |
| NeurCross | champ croisé neuronal | **AGPL-3** : exclu |
| QuadGPT, MeshAnything | génération neuronale de maillages | ~78–80 % de quads (QuadGPT), **non commercial** (MeshAnything) : sous la cible, exclus |

---

## 3. Le modèle en couches

Un personnage n'est **pas** un maillage. C'est une **pile de couches**, de la plus
abstraite à la plus fine ; le maillage affiché en est le **résultat**.

| couche | contenu | qui l'écrit | survit à une régénération ? |
|---|---|---|---|
| **C0 — Squelette** | graphe d'os : noms, longueurs, orientations, répétitions (doigts, pattes), symétries | humain, IA | c'est la source |
| **C1 — Forme** | profils des sections le long de chaque os ; formes des jonctions ; préréglages de style | humain, IA | c'est la source |
| **C2 — Cage** | maillage de quads **généré** depuis C0 + C1, avec **adresses stables** | le générateur (jamais à la main) | régénérée **au bit près** (§10.6) |
| **C3 — Opérations** | la pile R32 : coupes de boucles, extrusions (une corne ajoutée), biseaux, suppressions… **ciblées par nom, critère ou adresse** | humain, IA | **rejouée** par-dessus C2 |
| **C4 — Sculpt** | couches de **déplacement** exprimées **dans le repère local** de la surface | humain (pinceaux), IA (couche à part) | **reportée** par les adresses |
| **C5 — Rig** | squelette d'animation et poids, déduits de C0 et de C2 | le générateur ; ajustable dans NkAnimaEditor | recalculé, **retouches de poids gardées** |
| **C6 — Visage** | tête paramétrique, orbites, bouche, blendshapes FACS | générateur, humain, IA | comme C1–C4 |

📌 **Le maillage final = C2 → C3 rejouée → C4 appliquée**, puis subdivision d'affichage.

### 3.1 Pourquoi des couches

Chaque sorte de retouche a **sa** couche, et chaque couche sait survivre à un
changement des couches du dessous :

- changer une **proportion** (C1) ne touche ni aux opérations (C3) ni au sculpt (C4) ;
- ajouter un **doigt** (C0) ajoute ses anneaux ; le sculpt des autres doigts reste sur
  eux, **par leur nom** ;
- une **corne extrudée à la main** (C3) est rejouée sur la nouvelle tête ;
- un **muscle sculpté** (C4) suit le bras quand l'IA l'allonge.

### 3.2 L'adresse stable — la clé de tout

Chaque sommet de la cage C2 porte une **adresse** qui ne dépend ni de sa position ni
de son indice :

| lieu | adresse | exemple |
|---|---|---|
| le long d'un os | `(os, s, a)` : **s** ∈ [0, 1] le long de l'os, **a** ∈ [0, 1[ autour de l'os, à partir d'une référence d'orientation stable | `(avant_bras_g, 0.42, 0.25)` |
| dans une jonction | `(jonction, patch, u, v)` | `(paume_g, doigt_3, 0.5, 0.2)` |
| dans un capuchon | `(os, bout, r, a)` | `(queue, bout, 0.3, 0.75)` |
| sur le visage | `(tete, zone, u, v)` : zones nommées (orbite_g, bouche, nez…) | `(tete, orbite_g, 0.1, 0.6)` |

- L'adresse est **continue** : si la résolution change (plus d'anneaux, plus de
  segments), une retouche est **rééchantillonnée** à la nouvelle adresse la plus proche,
  par interpolation.
- Une adresse est **lisible** : on peut l'écrire dans le fichier, la lire dans un
  message (« la retouche sur `doigt_5_g` n'a plus de cible »).
- **Symétrie** : l'adresse miroir s'obtient en remplaçant `_g` par `_d` et `a` par
  `1 − a`.

---

## 4. Le format — `.nkscene` version 2

On **étend** `.nkscene` au lieu de créer un format : un document v1 reste lu tel quel
(ses parties deviennent des os à un seul segment). Le document reste **du texte,
modifiable dans n'importe quel éditeur** (`FORMAT_SCENE.md:50`), **et** dans NKCraft
(§15).

### 4.1 Exemple — un dragon ailé à quatre pattes

```nkscene
nkscene 2
creature "Dragon des Monts" style "heroique" symetrie X

squelette
  os bassin        racine            longueur 0.60  direction 0 0 1
  chaine colonne   depuis bassin     segments 4  longueur 1.40  courbure 0 0.15 0
  chaine cou       depuis colonne    segments 5  longueur 1.10  courbure 0 0.40 0
  os tete          depuis cou        longueur 0.50
  chaine queue     depuis bassin     segments 10 longueur 2.80  effilement 0.12  vers -Z
  membre patte_arriere_g depuis bassin
    os cuisse longueur 0.70  os jambe longueur 0.60  os metatarse longueur 0.35
    extremite pied doigts 4  phalanges 3  griffes oui
  membre patte_avant_g depuis colonne.2
    os bras longueur 0.55  os avant_bras longueur 0.50
    extremite main doigts 5  phalanges 3  pouce oppose non  griffes oui
  membre aile_g depuis colonne.3
    os humerus longueur 0.60  os radius longueur 0.90
    extremite aile doigts 4  longueur_doigts 1.80 1.60 1.30 1.00  membrane oui
  appendice corne_g depuis tete  position 0.8 0.3 0.9  longueur 0.35  courbure -0.2 0 0.3  effilement 0.05
  miroir *_g -> *_d

forme
  profil bassin        section superellipse  largeur 0.55 hauteur 0.45  exposant 2.4
  profil colonne       clefs 0:0.60x0.55  0.5:0.70x0.65  1:0.45x0.45
  profil cou           clefs 0:0.40x0.40  1:0.22x0.24
  profil queue         clefs 0:0.35x0.32  1:0.03x0.03
  profil cuisse_*      clefs 0:0.28x0.30  0.6:0.24x0.22  1:0.16x0.16  muscle avant 0.3 0.05
  profil aile_*        membrane epaisseur 0.02  bord_libre courbe 0.25
  tete gabarit "reptile"  museau 0.6  machoire 0.4  orbites 2  yeux 2  narines 2
  resolution anneaux 12  boucles_articulation 3

retouches  fichier "dragon.nkr"      // C3 : la pile R32
sculpt     couche "muscles"  fichier "dragon.muscles.nksc"  auteur humain
sculpt     couche "ecailles" fichier "dragon.ecailles.nksc" auteur ia
```

### 4.2 Le vocabulaire

| mot | sens |
|---|---|
| `os` | un segment rigide nommé |
| `chaine` | N os en suite (colonne, cou, queue, tentacule), avec `courbure` et `effilement` |
| `membre` | une suite d'os nommée, qu'on peut **miroiter** et **répéter** |
| `extremite main / pied / aile / nageoire / pince / sabot` | un **gabarit d'extrémité** paramétré : `doigts n`, `phalanges k` (par doigt si besoin), `pouce oppose`, `griffes`, `membrane` |
| `appendice` | corne, oreille, épine, antenne, crête : un os court partant d'une **adresse** de surface |
| `repeter membre patte_g 4 le_long colonne` | 8 pattes d'araignée, mille-pattes, 6 bras |
| `miroir` | symétrie par règle de noms (`_g` → `_d`) |
| `profil` | sections le long de l'os : superellipse (largeur, hauteur, exposant), **clefs** le long de s, décalages, « muscles » (bosses nommées) |
| `tete gabarit` | `humain`, `cartoon`, `reptile`, `felin`, `canin`, `oiseau`, `insecte`, `libre` (§8) |
| `style` | un préréglage de profils : `moyen`, `heroique`, `trapu`, `gros`, `cartoon`, `puncher` (avant-bras énormes), `elance`… — les silhouettes des planches de Rodolf |
| `resolution` | anneaux par tour, boucles par articulation : **pairs** (§5.4) |
| `corps fuseau / disque / radial / cloche / segmente / libre` | le **plan du corps** (§5.7) : poisson et cétacé, raie, étoile de mer, méduse, insecte et crustacé, forme inventée |
| `nageoire caudale / dorsale / pectorale / pelvienne / anale` | une **nageoire à rayons** : des os fins (rayons) et une membrane tendue entre eux, comme une aile |
| `aile plumes / membrane / insecte` | les trois ailes (§5.7.2) |
| `tentacule souple` | une chaîne **sans articulation marquée** (anneaux réguliers, beaucoup d'os pour le rig) ; `ventouses` en semis |
| `segment rigide` | un anneau de **carapace** (insecte, crustacé, robot organique) : arêtes vives aux joints, pli dans la membrane entre deux segments |
| `semis` | des **ornements répétés** posés sur une zone par adresses : plumes, écailles, épines, ventouses, poils en mèches (§5.7.4) |
| `greffe` | **combiner** des gabarits : buste humain sur queue de poisson (sirène), buste humain sur corps de cheval (centaure), tête d'aigle et ailes sur corps de lion (griffon) |
| `boucle` | une chaîne qui **rejoint** un autre os : anse, corps en anneau (g = 1) |

- **Rien n'est limité en nombre** : ni les bras, ni les doigts, ni les têtes (une
  hydre est un cou répété), ni les ailes, ni les nageoires, ni les tentacules.
- **Rien n'est limité en milieu** : la terre, l'air, l'eau, et ce que l'artiste invente.
- **Les noms** sont les identifiants de tout ce qui suit (retouches, sculpt, rig) : un
  nom en double est refusé, comme aujourd'hui (`NkModelerCreation.h:703`).
- **Déclaration des gabarits** : sur le modèle de `NkMeshFamilles` (registre
  `{nom, constructeur, formulaire}`, valeurs permises, bornes plausibles) — une
  extrémité ou une tête nouvelle s'**ajoute** sans toucher au générateur.

---

## 5. Le générateur

### 5.1 Les tubes

- Le long de chaque os : des **anneaux** de M sommets (M = `resolution anneaux`),
  **perpendiculaires** à l'os, orientés par un **repère à rotation minimale** (pas de
  torsion parasite) — `BuildSweep` fait déjà l'essentiel.
- **Même M le long d'une chaîne** ; M ne change **qu'aux jonctions**.
- **Boucles d'articulation** : 2 ou 3 anneaux resserrés de part et d'autre de chaque
  articulation (coude, genou, phalanges). Elles donnent la **bande de pli**.

### 5.2 Les jonctions

Le cœur du générateur, **porté de FEQ** (GEL, MIT) :

1. directions des branches qui partent du nœud → **triangulation de Delaunay
   sphérique** ;
2. le polyèdre obtenu est rendu **à valence 4** (`quad_valencify`), puis subdivisé une
   fois ;
3. chaque branche est **pontée anneau à anneau** avec sa face du polyèdre ;
4. lissage le long du squelette.

Ça marche pour **n'importe quel nombre** de branches. Les **gabarits d'extrémité**
(main, pied, aile) sont des jonctions **préparées** : paume ou plante comme nœud, un
tube par doigt.

**Repli** : si une jonction dégénère (branches presque parallèles), cadre carré par
nœud et enveloppe convexe fusionnée en quads (principe de B-Mesh et du Skin Modifier,
**réécrit depuis l'article**), puis remplissage des n-gones restants par les motifs de
Takayama 2014.

### 5.3 Les capuchons

Bout de doigt, de corne, de queue : **un pôle** au centre, entouré d'anneaux qui se
referment. Une griffe est un capuchon allongé et effilé.

### 5.4 Les règles de parité

- M **pair** partout ; aux jonctions, les tailles d'anneaux des branches sont choisies
  pour que chaque patch de jonction ait un **bord pair** (condition de remplissage en
  quads).
- Aucune **jonction en T**, aucun triangle dans la cage.

### 5.5 Les parties qui ne sont pas des tubes

| partie | construction |
|---|---|
| **membrane d'aile** | une **grille de quads** tendue entre les doigts de l'aile et le bord libre, soudée aux tubes des doigts ; épaisseur par extrusion |
| **torse massif**, ventre | sections superellipse larges ; « muscles » et « ventre » comme bosses nommées du profil |
| **oreilles, nageoires, crêtes** | profils aplatis (exposant de superellipse élevé), puis `appendice` |
| **yeux, dents, griffes détachables, armure** | **objets séparés** (comme en production), attachés à une adresse : ils suivent la surface |

### 5.6 La subdivision d'affichage

La cage reste légère (quelques milliers de faces) ; l'affichage et l'export appliquent
0 à 3 niveaux de **Catmull-Clark** (existant). Le sculpt (C4) se pose au niveau choisi.

---

### 5.7 Tous les plans de corps — terre, air, eau, et l'inventé

Le squelette n'est pas un « squelette de vertébré » : c'est un **graphe** d'éléments
(os, chaînes, membranes, segments, semis). Chaque grand plan de corps y trouve sa
place ; les gabarits ci-dessous sont des **points de départ modifiables**, pas une
liste fermée.

#### 5.7.1 Les animaux marins

| animal | plan de corps | construction |
|---|---|---|
| **poisson** (tilapia, capitaine, requin) | `corps fuseau` | une chaîne (tête → pédoncule) aux sections **aplaties sur les côtés** (superellipse haute et étroite) ; nageoires à rayons (caudale, dorsale, pectorales, pelviennes, anale) soudées par jonction ; ouïes et bouche en zones nommées de la tête |
| **cétacé** (baleine, dauphin, lamantin) | `corps fuseau` | sections **aplaties de haut en bas** au pédoncule ; nageoire caudale **horizontale** ; pectorales = membre court à doigts fusionnés (`doigts 5 fusion oui`) |
| **raie, manta** | `corps disque` | une **jonction aplatie** qui porte deux « ailes » pectorales en membrane épaisse, et une queue en chaîne fine |
| **pieuvre, calmar, kraken** | `tentacule souple` × n autour d'un manteau | le manteau est une **cloche** (surface de révolution) ; chaque tentacule est une chaîne souple, **ventouses** en semis sur la face intérieure (adresse `a` dans un intervalle) |
| **méduse** | `corps cloche` | cloche de révolution, bord festonné (anneau à n lobes), tentacules fins en chaînes souples ; bras oraux |
| **étoile de mer, oursin** | `corps radial` | une jonction centrale à 5 (ou n) branches aplaties ; épines en semis |
| **crabe, homard, crevette** | `corps segmente` | carapace = segments rigides ; pattes articulées en segments ; **pinces** = gabarit `extremite pince` (deux doigts opposés) |
| **tortue marine** | `corps disque` + carapace | la carapace est un objet rigide séparé (dos + plastron) ; tête, pattes-nageoires et queue sortent par ses ouvertures |
| **anguille, serpent de mer, léviathan** | chaîne longue | 20 à 60 os, sections qui varient ; crêtes et nageoires en membranes le long de la chaîne |

#### 5.7.2 Les ailes

| aile | construction | exemples |
|---|---|---|
| **membrane** | grille de quads tendue entre des **doigts d'aile** et un bord libre (§5.5) | chauve-souris, dragon, ptérosaure, démon |
| **plumes** | l'aile est un **membre** (humérus, avant-bras, main réduite) recouvert de **rémiges** et **couvertures** en semis **orientés** le long des os ; chaque plume est un objet léger (carte en quads, ou tube aplati), rangée par rangée, qui **suit** le pli de l'aile | oiseaux, pégase, griffon, ange |
| **insecte** | des **plaques minces** séparées, attachées au thorax par une articulation ; nervures en relief (sculpt ou semis) ; 1 ou 2 paires | libellule, papillon, abeille, fée |

- Le **repliement** de l'aile (oiseau au repos, dragon qui replie ses ailes) est une
  **pose** du rig, pas un autre modèle.
- Les **plumes de contour** du corps (duvet, plastron) sont un semis sur les zones
  choisies ; pour les jeux, une option les **fusionne** en géométrie simple (cartes) ;
  pour le film, elles restent individuelles.

#### 5.7.3 Les corps segmentés et les exosquelettes

Insectes, araignées, scorpions, crustacés, mille-pattes, **robots organiques** :

- chaque segment est un **tube court à arêtes vives** (boucles de support serrées aux
  bords, qui gardent l'arête nette après subdivision) ;
- entre deux segments, une **membrane souple** (2 ou 3 boucles) qui porte le pli ;
- les pattes articulées sont des chaînes de segments ;
- le rig les traite comme des pièces **rigides** (poids à 1 sur chaque segment, fondu
  seulement dans la membrane).

#### 5.7.4 Les ornements en semis

Plumes, écailles, épines, piquants, ventouses, dents, nageoires dorsales en épines,
poils en mèches : des **objets répétés posés par adresse** sur une zone, avec densité,
taille, orientation (le long de l'os, vers l'arrière, selon la normale) et
variation aléatoire **reproductible** (graine).

- Ils sont une **couche** à part (C4b) : on les régénère sans toucher au corps, et le
  sculpt du corps ne les casse pas (ils suivent leurs adresses).
- Les **cheveux et la fourrure** fine relèvent du système de poils (décision D4 de
  NKCraft : la forme ici, la dynamique dans NkAnima) ; le semis couvre les mèches et
  les ornements rigides. **Vêtements, cheveux, queues, cordes, chaînes et parures :
  document 5** (`05-vetements-cheveux-secondaires-nkcraft.md`).

#### 5.7.5 Les créatures inventées

- **Greffe** : sirène (buste humain + `corps fuseau`), centaure, griffon, pégase, naga,
  hippocampe géant, chimère — les gabarits se combinent **à une jonction**, et la
  topologie reste propre parce que la jonction est construite comme les autres (§5.2).
- **Sans symétrie** : désactiver `symetrie` ; chaque côté a ses propres membres.
- **Symétrie radiale** : `corps radial n` (5 bras, 7 tentacules, une fleur-monstre).
- **Plusieurs têtes, plusieurs queues, des yeux partout** : répétitions et orbites
  posées par adresse (§8).
- **Corps percé** : une `boucle` dans le graphe (une créature-anneau, une anse) ; la
  formule du §2.1 compte alors g = 1 et la cible de 95 % reste valable.
- **Formes vraiment libres** (blob, créature de fumée solidifiée, masse organique) :
  un squelette minimal + beaucoup de sculpt (C4), puis, si la forme change en
  profondeur, le **remailleur** (§7) la ramène dans le modèle en couches.

---

## 6. Les critères de topologie — mesurés à chaque construction

| critère | cible | mesure |
|---|---|---|
| faces quadrangulaires de la cage | **100 %** | comptage |
| sommets de valence 4 (cage subdivisée une fois) | **≥ 95 %** | comptage ; et vérification de Σ(4 − val) = 8 − 8g |
| variétés : arêtes non manifold, trous, auto-intersections | **0** | `NkMeshAnalysis` |
| boucles par articulation | 2 ou 3 (réglable) | par l'adresse `s` |
| pôles | aucun à moins de 2 anneaux d'une bande de pli | par l'adresse |
| symétrie | exacte (écart ≤ 1e-6) | miroir des adresses |
| déterminisme | même document → **même maillage au bit près** | empreinte |
| déformation | chaque articulation pliée à 90° en skinning linéaire : perte de volume ≤ 15 %, aucune face retournée | banc de pose |

**Le catalogue d'essai** (banc `NKCreatureHarness`, sur le modèle de
`NKEditMeshHarness`) : humain moyen · humain à 4 doigts · personnage « Puncher » ·
enfant cartoon · quadrupède · **dragon ailé** · créature à **6 bras** · araignée à
**8 pattes** · serpent · centaure · hydre à 3 têtes ·
**marins** : poisson à nageoires, requin, dauphin, raie manta, pieuvre, méduse, crabe,
étoile de mer, tortue marine ·
**ailés** : oiseau (aigle), chauve-souris, libellule, papillon, pégase, griffon ·
**inventés** : sirène, kraken, créature radiale à 7 bras, créature-anneau (g = 1),
créature asymétrique. Chacun passe **tous** les critères, sur les trois plateformes ;
les semis (plumes, écailles, ventouses) sont mesurés à part.

---

## 7. Le remailleur pour les formes libres

La construction couvre ce que le générateur crée. Il reste deux cas : un **sculpt qui
change la forme en profondeur** (dynamique de topologie, fusion) et un **maillage
importé**.

1. **Remailler guidé par le squelette** (Usai 2015) : extraire un squelette du maillage
   (sphères de Thiery 2013 ou squelettisation), construire la cage FEQ correspondante,
   la **recaler** sur la forme (`non_rigid_registration` de GEL, MIT), subdiviser,
   projeter. On retrouve des **boucles d'artiste** et des adresses : le maillage
   **rentre** dans le modèle en couches.
2. **Remaillage global** pour ce qui n'a pas de squelette (rocher, objet) :
   **réimplémenter QuadriFlow** (BSD-3) — notre retopologie actuelle y gagne l'étape de
   flot qui supprime les singularités et la réparation des retournements.
3. **Guides de l'artiste** : traits de direction et peinture de densité (principe de
   ZRemesher et Quad Remesher), pris en compte par les deux méthodes.

Cible : les mêmes critères du §6 (≥ 95 % de valence 4).

---

## 8. Le visage

- **La tête est une jonction spéciale** au bout du cou : un **gabarit de tête** (§4.2)
  avec ses zones nommées et les **boucles d'animation** du visage — anneaux autour des
  yeux, autour de la bouche, sillon nasogénien. Le gabarit humain s'inspire de
  l'écoulement d'arêtes du maillage **MakeHuman hm08 (CC0)**.
- **Paramétrique** : les champs de `NkProcFaceParams` (mâchoire, yeux, nez, lèvres,
  oreilles) — **déclarés** dans Noge, **implémentés** ici. Nombre d'yeux libre
  (cyclope, quatre yeux) : une orbite est une **adresse** sur la tête.
- **Les yeux** sont des objets séparés (globe, cornée), placés dans les orbites.
- **Le visage sort riggé** : le générateur produit les **blendshapes FACS** à partir
  des zones (paupières, lèvres, joues, mâchoire), et `anim::NkFaceController`
  (NKAnima, déjà employé par PV3DE) les pilote. NKCraft **n'anime pas** le visage : il
  le livre prêt à l'être.

---

## 9. Le rig et le skinning — après la modélisation

- **Le squelette C0 devient `anim::NkSkeletonDef`** : mêmes noms, mêmes articulations.
  Rien à placer à la main.
- **Les poids par construction** : chaque anneau appartient à un os ; dans les boucles
  d'articulation, le poids passe progressivement d'un os à l'autre ; dans une jonction,
  il se répartit selon les patchs.
- **Après un sculpt ou un remaillage** : poids recalculés par **diffusion de chaleur**
  (Baran & Popović 2007, *Pinocchio*, principe du *Bone Heat* — **réécrit depuis
  l'article**) ; plus tard les **poids biharmoniques bornés** (Jacobson 2011).
- **Les retouches de poids** faites dans NkAnimaEditor (peinture, 07-produit-nkanima
  §4.2) sont **gardées** comme une couche, par adresse, comme le sculpt.
- **Vérification** : le banc de pose du §6 ; aperçu en direct dans NKCraft avec le
  skinning GPU existant et l'IK (`NkIKSolver`).
- **Par plan de corps** :
  - **tentacules, queues, cous longs** : chaînes de nombreux os, prêtes pour l'IK à
    spline de NkAnima ;
  - **ailes à membrane** : un os par doigt d'aile, la membrane pondérée entre ses
    doigts ; **ailes à plumes** : les plumes suivent leurs os et s'ouvrent en éventail
    par une commande de repli ;
  - **nageoires** : un os par rayon principal (ou une chaîne pour la caudale) ;
  - **segments rigides** : poids à 1, fondu seulement dans les membranes ;
  - **ornements en semis** : attachés à l'os le plus proche de leur adresse.
- **Frontière** : NKCraft **produit** le rig et les poids ; **NkAnimaEditor** les
  **ajuste** et **anime** (décision D-C9).

---

## 10. Humain ↔ automatique ↔ IA — sans jamais dénaturer une retouche

C'est la demande centrale de Rodolf, et ce qui **résout R32**.

### 10.1 Les deux règles

1. **Rien de ce qu'un humain a touché ne se perd.** Une retouche est gardée, reportée,
   ou — si sa cible a disparu — **mise de côté et montrée**, jamais effacée en silence.
2. **Rien de ce que fait l'IA n'est irréversible.** Chaque action de l'IA est un lot
   annulable d'un geste, dans le **même journal** que l'humain.

### 10.2 Un seul journal, trois auteurs

Chaque entrée du journal porte son **auteur** : `humain`, `auto` (le générateur, un
recalcul), `ia`. Le journal existe (`NkEditMesh`, lots d'annulation de
`NkModelerCreation`) ; on lui ajoute l'auteur et la **couche** (C0 à C6).

### 10.3 Où va chaque retouche humaine

| ce que fait l'humain | couche | comment ça survit |
|---|---|---|
| bouge un curseur (longueur de bras, nombre de doigts, style) | C0 / C1 | c'est la source : la régénération **l'applique** |
| coupe une boucle, extrude une corne, biseaute, supprime des faces | **C3** | opération R32 ciblée **par nom, critère ou adresse** (§10.4), rejouée après chaque régénération |
| sculpte (pinceau), déplace des sommets à la main | **C4** | déplacement stocké **dans le repère local** `(normale, tangente de l'os, bitangente)` **à l'adresse** ; quand la forme change, le détail **suit** (option : absolu ou proportionnel à la taille de la section) |
| sélection libre puis opération | **C3** | la sélection est **convertie en adresses** au moment du geste : elle survit (c'est la seule manière de désigner de `FORMAT_SCENE.md` §8 qui ne survivait pas) |
| peint des poids dans NkAnimaEditor | C5 | couche de poids par adresse |

### 10.4 Désigner sans indice — la sélection par critère

Écrite en R32 (`FORMAT_SCENE.md` §8), **à coder** :

| désignation | exemple | sens |
|---|---|---|
| par nom | `partie:avant_bras_g` | tout l'os |
| par adresse | `adresse:(avant_bras_g, 0.4..0.6, 0..1)` | une bande de l'os |
| par critère | `faces:normale>+Y@partie:tete` | les faces du dessus de la tête |
| par zone | `zone:orbite_g` | une zone nommée du visage |
| par chemin | `boucle:(cuisse_g, 0.5)` | la boucle au milieu de la cuisse |

Une cible **introuvable** donne un **refus nommé** (« la retouche 14 vise `doigt_5_g`,
qui n'existe plus ») ; l'opération devient **orpheline**, **les autres continuent**
(critère R32.7).

### 10.5 L'IA ou l'automatique reprend — ce qui se passe

| action de l'IA ou du générateur | effet sur les retouches humaines |
|---|---|
| change des paramètres (C0, C1) | C3 rejouée, C4 reportée par adresse : **les retouches suivent la nouvelle forme** |
| ajoute un membre ou un doigt | le nouveau membre est vierge ; les autres gardent leurs retouches |
| **retire** un membre ou un doigt retouché | l'IA **propose** le changement (aperçu) avec la **liste des retouches touchées** ; l'humain accepte (les retouches deviennent orphelines, **gardées**) ou refuse |
| ajoute du détail (écailles, muscles) | dans **sa propre couche** `auteur ia`, jamais dans une couche humaine |
| régénère la topologie (résolution) | C3 et C4 **rééchantillonnées** par adresse |

- **Verrou par partie** : l'humain peut **verrouiller** une partie (« ne touche pas à la
  tête ») ; l'IA la lit et ne la modifie pas.
- **L'IA lit les retouches** : elles font partie de ce qu'on lui donne (le document,
  les couches, leurs auteurs). Une demande « rends-le plus musclé » **ajoute** à la
  couche de l'IA, elle n'efface pas les muscles sculptés par l'humain.

### 10.6 Intégrer, détacher

- **Intégrer** (« absorber ») : l'humain a sculpté un bras plus épais ; il peut demander
  d'**intégrer** : le système **ajuste les paramètres** C1 pour s'approcher de la forme
  sculptée (moindres carrés sur les profils), et **seul le reste** du détail demeure
  dans C4. Les prochaines régénérations partent alors de cette forme.
- **Détacher** : l'inverse — figer l'effet des paramètres dans une couche de sculpt
  pour les retoucher librement.
- **Déterminisme** : une pile vide redonne **la base au bit près** ; même document →
  même maillage (R32.7).

### 10.7 Ce qui se vérifie

- Le jalon C de NKCraft (01 §7) : **trois tours** prompt → retouche → prompt sur le même
  personnage **sans qu'une retouche disparaisse**.
- Tests dédiés : allonger un bras sculpté (le sculpt suit) ; ajouter un 6ᵉ doigt (les 5
  autres gardent leurs retouches) ; retirer un doigt retouché (orpheline montrée,
  rien d'effacé) ; changer la résolution (retouches rééchantillonnées, écart borné) ;
  pile vide = base au bit.

---

## 11. L'IA — le plan d'entraînement

### 11.1 Ce que l'IA fait, dans l'ordre

| rôle | entrée | sortie | modèle | quand |
|---|---|---|---|---|
| **A — description → paramètres** | une phrase ou une fiche complète | un `.nkscene` v2 (C0, C1, C6) | l'IA de langage déjà branchée (`NkModelerCreation` : planifier → placer → vérifier → corriger), avec la grammaire v2 | dès le format v2 |
| **B — images → paramètres** | 1 à N vues (face, profil, dos, 3/4) | C0, C1, C6 | un **encodeur d'images convolutif** (Conv2D existe) par vue, mise en commun des vues, tête de régression ; 5 à 20 M de paramètres | après le générateur |
| **C — détail** | le personnage + une image ou une consigne | une couche C4 `auteur ia` (déplacements sur les adresses, cartes par partie) | réseau convolutif sur des **cartes de déplacement** par partie (la topologie étant fixe, c'est une image à prédire) | après B |
| **D — opérations** | le personnage + une demande | des commandes C3 | l'IA de commandes existante (26 verbes, `NkModelerIA`) | existe ; à brancher sur les adresses |

📌 **Rôle B avec l'existant** : `ajuster_depuis_image.py` ajuste déjà un gabarit sur des
silhouettes, sans apprentissage. Il sert de **référence** et de **repli**.

### 11.2 Les données

**Le fonds principal est synthétique, et il nous appartient** :

1. tirer des paramètres au hasard dans des bornes plausibles (familles, styles,
   nombres de membres) ;
2. construire la créature (générateur), ajouter un détail procédural ;
3. la **rendre** sous plusieurs vues avec notre moteur (toon et réaliste, éclairages
   variés à partir des HDRI **CC0** de Poly Haven) ;
4. on obtient des couples **(images → paramètres)** et **(images → déplacements)**
   **exacts et libres de droits**, en nombre illimité.

C'est la méthode de BEDLAM et d'Infinigen (entraîner sur du synthétique marche) —
**leurs données** ne sont pas utilisables (non commerciales), **leur méthode** oui.

**Sources extérieures autorisées pour l'IA commerciale** (licence lue à la source) :

| source | contenu | licence | usage |
|---|---|---|---|
| **MakeHuman / MPFB** | humains paramétriques, maillage en quads | CC0 (ressources) | modèle de topologie humaine, formes d'humains |
| **Blender Studio** (personnages des films) | 50+ personnages riggés, topologie de production | CC-BY 4.0 (vérifier chaque film : les plus anciens en CC-BY-ND) | **la meilleure source de topologie d'artiste** ; crédits |
| **Smithsonian Open Access 3D** | ~2 400 numérisations (fossiles, spécimens, dont des **animaux marins** et des **oiseaux**) | CC0 | anatomie animale réelle : proportions de poissons, cétacés, oiseaux, insectes |
| **Objaverse** (sous-ensemble **CC0 / CC-BY**, sans étiquette « NoAI ») | objets et personnages variés | par objet | après filtrage ; écarter NC, SA et NoAI |
| **Kenney 3D**, **OpenGameArt** (CC0 / CC-BY) | personnages simples | CC0 / CC-BY | complément |
| **VRoid** : modèles de base et AvatarSample 1–4 | avatars anime riggés | CC0 | complément |
| **Quaternius** | personnages, animaux, monstres riggés | licence propre, **sans clause IA** | **demander par écrit** avant de l'utiliser |
| **3DBiCar / RaBit** | 1 500 bipèdes cartoon sur une topologie unique | non affichée | **demander une licence commerciale** aux auteurs (la plus proche de notre approche) |
| **CGTrader** (articles **payants** sans « No AI ») | au choix | la licence §21A.2 permet l'entraînement | achat ciblé possible |

**Exclus pour l'IA commerciale** : SMPL, SMPL-X, SMAL, STAR, AMASS (non commerciaux ;
licence commerciale payante chez Meshcapade), Mixamo (Adobe §17 interdit
l'entraînement), ShapeNet, PartNet, ModelNet, 3D-FUTURE, CO3D, RenderPeople,
TurboSquid, téléchargements gratuits de CGTrader, VRoid Hub (robots interdits),
Meshy et Tripo (clause de non-concurrence), sorties de tout générateur tiers.

**Nos propres données** :

- **sculpts commandés** : des artistes déforment **nos** gabarits (même topologie, même
  adresses) — ce sont les exemples parfaits pour le rôle C ;
- **contrat** : cession (ou licence exclusive) couvrant **explicitement** l'usage comme
  données d'entraînement, de validation et de test de modèles, y compris génératifs, et
  l'exploitation commerciale des modèles et de leurs sorties ; garantie que l'œuvre est
  originale et faite sans IA ni ressources tierces ; fiche de provenance par modèle ;
- **photogrammétrie** : sculptures, masques, personnes, avec autorisations couvrant
  l'IA.

⚠️ **Le corpus existant** (3 927 modèles × 48 vues) : **provenance à établir** modèle par
modèle avant tout entraînement ; ce qui ne passe pas les règles ci-dessus en sort.
⚠️ **Les planches de référence** (Gumroad, tenten, artistes nommés) : **inspiration**
pour les styles, **jamais** données d'entraînement.

### 11.3 Ce qu'il faut ajouter à Kernel/AI

| manque | pourquoi |
|---|---|
| **pont NKImage → NKData** : chargeur d'images en tenseurs (redimensionner, normaliser, lots, augmentations) | NKImage décode déjà PNG, JPEG, WebP, EXR ; NKData ne les lit pas |
| rendu hors écran en lot → fichiers ou tenseurs | produire les millions de vues synthétiques |
| concaténation sur l'axe des caractéristiques, rassemblement (*gather*) différentiable | mettre en commun plusieurs vues |
| GroupNorm (ou BatchNorm) | encodeurs d'images stables |
| **correction du bogue GPU** (§12) | condition préalable à tout entraînement sérieux |
| (plus tard) petit Vision Transformer | si le convolutif plafonne |

Tailles : un encodeur de 5 à 20 M de paramètres tient sur la carte actuelle de 8 Go ;
le rôle C est plus gourmand (§13).

### 11.4 Mesures

- **B** : erreur sur les paramètres ; et surtout **écart de silhouette** entre la
  créature produite et les images données (IoU par vue) ; comptage exact des doigts,
  membres, yeux.
- **C** : écart de surface (distance de Chamfer) avec les sculpts de validation ;
  **aucun** critère du §6 ne régresse (la topologie n'est jamais touchée par l'IA).

---

## 12. Le bogue GPU — diagnostic et remède

### 12.1 Ce qui est mesuré (`Kernel/AI/ROADMAP.md:1025-1043`)

Même lot effectif de 6 144 jetons, sur la RTX 3070 Laptop, en Vulkan :

| lot | tenseur le plus gros (logits) | groupes de travail (1D, 64 fils) | résultat |
|---|---|---|---|
| B=6, accumulation 4 | ~100 Mo | ~393 000 | ✅ apprend |
| B=12, accumulation 2 | ~201 Mo | ~786 000 | ✅ apprend |
| **B=24**, accumulation 1 | **~403 Mo** (6 144 × 16 385 flottants) | **~1,57 M** | ❌ **n'apprend rien**, la perte reste à ln(16385) = 9,70, et le pas paraît 3,6× plus rapide |

**« À partir de quelle taille »** : entre **201 Mo** et **403 Mo** pour un seul tenseur
(entre 786 000 et 1,57 M groupes dans un seul lancement de calcul).

### 12.2 Les causes probables, dans l'ordre

1. **La carte est « perdue » et personne ne le voit.** Dans `NkVulkanDevice.cpp`, le
   résultat de `vkQueueSubmit` n'est **pas vérifié** sur le chemin de calcul (ligne
   ~2300), ni celui de `vkQueueWaitIdle` (ligne ~1001). Sous **Windows**, un calcul qui
   dure plus de **2 secondes** déclenche la réinitialisation du pilote (**TDR**) : le
   périphérique est perdu (`VK_ERROR_DEVICE_LOST`), les lancements suivants ne font
   **plus rien** et **reviennent tout de suite**. C'est exactement le symptôme : **plus
   rapide, et rien n'est calculé**. Les plus gros lancements (logits, matmul) sont ceux
   qui franchissent les 2 s en premier.
2. **Le nombre de groupes** : tous les noyaux lancent `(count + 63) / 64` groupes sur
   l'axe X, **sans plafond** (`NkTensorGpu.cpp:1319`, `:1902`…), et NKRHI ne connaît
   pas la limite `maxComputeWorkGroupCount` (absente de `NkDeviceCaps`). Les pilotes
   NVIDIA acceptent en général plus de 2 milliards de groupes en X — mais pas tous les
   pilotes (65 535 est le minimum garanti) : c'est un bogue **latent** pour d'autres
   cartes, même si ce n'est probablement pas celui-ci.
3. **Une allocation qui échoue** en silence quand la mémoire se remplit (logits +
   gradients + états d'Adam) — l'hypothèse `maxStorageBufferRange` écrite dans le code
   est peu probable (NVIDIA annonce ~4 Go).

### 12.3 Le remède

1. **Vérifier tous les résultats Vulkan** (soumission, attente, allocation,
   `vkMapMemory`) et **remonter** `DEVICE_LOST` comme une erreur nommée qui **arrête**
   l'entraînement — c'est le « ⬜ remonter une erreur » déjà écrit dans la feuille de
   route.
2. **Ajouter `maxComputeWorkGroupCount` à `NkDeviceCaps`** et **découper** les
   lancements : grille 2D (`gl_GlobalInvocationID.x + y · largeur`) ou boucle de
   lancements par tranches.
3. **Ne plus matérialiser les logits entiers** : entropie croisée **par tranches** (ou
   fusionnée avec la dernière couche) — 403 Mo deviennent quelques dizaines de Mo, et
   chaque lancement reste court.
4. **Découper les gros matmuls** en plusieurs soumissions de moins de ~0,5 s chacune.
5. **Pour le développement seulement**, sous Windows : augmenter `TdrDelay` dans le
   registre (`HKLM\System\CurrentControlSet\Control\GraphicsDrivers`) pour **confirmer**
   le diagnostic — **pas** comme solution.
6. **Un banc** qui rejoue B=24 et vérifie que la perte descend, et qu'une erreur
   **nommée** apparaît si on force un lancement trop long.

---

## 13. Le matériel

### 13.1 Ce que dit le contexte

- Le moteur calcule en **Vulkan / DX12 / OpenGL**, pas en CUDA : **NVIDIA et AMD sont
  tous deux utilisables**. Les cœurs matriciels sont atteignables en Vulkan par
  l'extension `VK_KHR_cooperative_matrix` (NVIDIA depuis Turing ; AMD RDNA3 et plus
  récent) — **pas encore employée** par NkSL : c'est un gain à venir sur toute carte
  récente.
- **Septembre 2026** : les prix sont **40 à 90 % au-dessus** des prix de lancement
  (pénurie de mémoire due aux centres de données).
- **Au Cameroun** : TVA 19,25 % + droits du tarif CEEAC-CEMAC → compter **+30 à 45 %**
  plus le fret ; garantie internationale rarement honorée pour l'occasion ; **onduleur
  en ligne à double conversion** (1,5 à 3 kVA), stabilisateur, climatisation.

### 13.2 La GP100 : non

Architecture Pascal (2016), **16 Go** seulement, **pas de cœurs matriciels**, ~10× plus
lente qu'une carte récente en calcul matriciel, et **fin du support pilote** (la
branche 580 est la dernière pour Pascal). Son seul atout (le calcul en double précision)
ne sert pas à l'IA. **Même verdict pour les V100** d'occasion (refroidissement de
serveur, fin de support).

### 13.3 Trois budgets

| budget | proposition | pourquoi |
|---|---|---|
| **~1 500–2 500 €** | **AMD Radeon AI PRO R9700 32 Go** (~1 800–2 000 $ neuve) | 4× la mémoire actuelle, neuve et garantie, 300 W, turbine (facile à refroidir), matrices RDNA4 ; **à valider sous Windows** (le moteur n'a été validé que sur NVIDIA) |
| | *ou* **RTX 3090 24 Go d'occasion** (~1 400 $) | continuité NVIDIA, sans garantie |
| **~4 000–6 000 €** | **Radeon PRO W7900 48 Go** (~3 500–3 900 $) **ou RTX A6000 48 Go d'occasion** (~3 200–4 400 $) | **48 Go** : l'encodeur d'images, le rôle C et le réglage fin des grands modèles ; la A6000 garde la continuité NVIDIA |
| **~8 000–10 000 €+** | **RTX PRO 5000 Blackwell 48 Go** (~8 800 $), ou 72 Go si le prix suit | neuve, garantie, 300 W ; la PRO 6000 96 Go (14 000–16 000 $) dépasse ce budget |

**Recommandation** : **d'abord** corriger le bogue GPU et construire le générateur
(§14, C0–C3) — ils ne demandent **aucun** nouveau matériel. **Ensuite**, pour
l'entraînement : une carte **48 Go** (W7900 ou A6000 d'occasion avec garantie
vendeur), et la **location** ponctuelle (A100 80 Go ~1,4 $/h, H100 ~1,8–2,7 $/h) pour
les grosses campagnes — en vérifiant d'abord `vulkaninfo` sur une instance à 0,30 $/h :
les images par défaut des loueurs n'activent pas Vulkan (il faut
`NVIDIA_DRIVER_CAPABILITIES=all` et les bibliothèques graphiques du pilote).

---

## 14. Phasage

| palier | contenu | critère de fin |
|---|---|---|
| **C0 — les fondations** | bogue GPU corrigé (§12.3) ; **R32 codé** : adresses, sélection par nom / critère / adresse, pile C3 rejouée, orphelines, déterminisme | B=24 apprend ; les tests R32.7 passent ; une corne extrudée à la main survit à trois régénérations |
| **C1 — tubes et capuchons** | `.nkscene` v2 (squelette, profils), tubes, boucles d'articulation, capuchons, miroir | un serpent et un bras à 3 articulations passent le §6 |
| **C2 — jonctions** | portage de FEQ ; gabarits d'extrémité (main à n doigts, pied, aile, pince) ; repli B-Mesh | les terrestres du catalogue passent le §6 (dont dragon, 6 bras, 8 pattes) |
| **C2b — tous les plans de corps** | fuseau, disque, radial, cloche, segmenté ; nageoires à rayons ; ailes à plumes et d'insecte ; tentacules souples ; semis ; greffe ; boucle | **le catalogue d'essai complet** passe le §6, marins, ailés et inventés compris |
| **C3 — formes et sculpt** | profils, styles, subdivision ; **couches de sculpt en repère local** ; intégrer / détacher ; verrous | les tests du §10.7 passent |
| **C4 — rig et visage** | `NkSkeletonDef` depuis C0, poids par construction et par diffusion de chaleur, export vers NkAnimaEditor ; gabarits de tête, orbites, blendshapes FACS | chaque créature du catalogue se pose et s'anime dans NkAnimaEditor sans retouche de poids |
| **C5 — l'IA de description et d'images** | grammaire v2 dans `NkModelerCreation` ; pont NKImage → NKData ; rendu synthétique ; encodeur d'images | « un dragon à 4 pattes, 2 ailes, 3 cornes » donne le bon compte de tout ; une planche de face et de profil donne une silhouette à IoU ≥ 0,85 |
| **C6 — le remailleur** | squelette extrait + cage FEQ recalée (Usai) ; QuadriFlow réimplémenté ; guides | un sculpt en dynamique de topologie revient dans le modèle en couches avec ≥ 95 % de valence 4 |
| **C7 — l'IA de détail** | rôle C sur cartes de déplacement ; sculpts commandés | détails plausibles sans qu'aucun critère du §6 régresse |

📌 C0 à C4 sont **du code géométrique** que Rihen maîtrise ; ils donnent déjà un
générateur utilisable **à la main et par la parole**, sans nouvel entraînement.

---

## 15. Les écrans — à porter aux documents 02 et 03 de NKCraft

- **Panneau Squelette** : l'arbre des os (renommer, répéter, miroir, ajouter un membre
  depuis un gabarit : bras, patte, aile, tentacule, queue) ; dans la vue, les os en
  **sphères reliées** qu'on tire (esprit ZSpheres).
- **Panneau Forme** : profils par os (courbe des sections), styles, muscles nommés.
- **Panneau Couches** : C3 (opérations), C4 (couches de sculpt, auteur humain / IA,
  opacité, visibilité), verrous par partie, **orphelines** en évidence avec « Revoir »
  et « Réattacher ».
- **Indicateurs de topologie** en direct : % quads, % valence 4, pôles hors bandes de
  pli — en vert quand les critères du §6 sont tenus.
- **Assistant** (existant, 03 §5.13) : phrase ou images → aperçu **hachuré** → accepter
  (un lot annulable).
- **Éditeur texte** du `.nkscene` v2, synchronisé avec les panneaux (le texte reste la
  vérité, `FORMAT_SCENE.md:50`).

---

## 16. Sources

**Topologie et squelettes**
- FEQ — https://www.dgp.toronto.edu/projects/feq/ · code : https://github.com/janba/GEL (`src/GEL/HMesh/skeleton_to_FEQ.cpp`, `quad_valencify.*`, MIT)
- SQM — https://orbit.dtu.dk/en/publications/converting-skeletal-structures-to-quad-dominant-meshes/
- PAM — https://dl.acm.org/doi/10.1145/2601097.2601226
- B-Mesh — https://onlinelibrary.wiley.com/doi/10.1111/j.1467-8659.2010.01805.x
- Usai et al. 2015 — https://dl.acm.org/doi/10.1145/2809785
- Scaffolding a skeleton — https://link.springer.com/chapter/10.1007/978-3-319-77066-6_2
- Pattern-Based Quadrangulation — https://cims.nyu.edu/gcl/papers/pattern-based-quadrangulation.pdf
- QuadriFlow — https://github.com/hjwdzh/QuadriFlow (BSD-3)
- Instant Meshes — https://github.com/wjakob/instant-meshes (BSD-3)
- QuadWild — https://github.com/nicopietroni/quadwild (GPL-3, article seulement) · BiMDF — https://github.com/cgg-bern/quadwild-bimdf
- QuadGPT — https://arxiv.org/abs/2509.21420
- Sphere-Meshes — https://perso.telecom-paristech.fr/boubek/papers/SphereMeshes/

**Rig et skinning**
- Pinocchio — https://github.com/pmolodo/Pinocchio (LGPL : principe seulement)
- UniRig — https://arxiv.org/abs/2504.12451 (MIT)
- MakeHuman (licence CC0 des ressources) — https://static.makehumancommunity.org/about/license.html

**Données**
- Objaverse — https://huggingface.co/datasets/allenai/objaverse · Sketchfab (clause NoAI) — https://sketchfab.com/terms
- Blender Studio — https://studio.blender.org/terms-and-conditions/
- Smithsonian Open Access — https://www.si.edu/openaccess/faq
- Quaternius — https://quaternius.com/license.html
- SMPL (non commercial) — https://smpl.is.tue.mpg.de/modellicense.html
- Adobe (Mixamo, §17) — https://www.adobe.com/legal/terms.html
- CGTrader — https://www.cgtrader.com/pages/terms-and-conditions
- 3DBiCar / RaBit — https://gaplab.cuhk.edu.cn/projects/RaBit/dataset.html
- Infinigen — https://infinigen.org/ · BEDLAM (licence) — https://bedlam.is.tue.mpg.de/license.html

**Matériel**
- Prix 2026 — https://tech-insider.org/gpu-prices-2026/ · https://gpupoet.com/gpu/learn/price/september-2026/amd-radeon-ai-pro-r9700 · https://gpudojo.com/radeon-pro-w7900 · https://gpudojo.com/a6000 · https://www.newegg.com/nvidia-pro-5000-blackwell-48g-rtx-pro-5000-48gb-video-cards/p/1FT-0004-00974
- Fin du support Pascal — https://www.phoronix.com/news/NVIDIA-580-Linux-Driver-Last-HW
- Matrices en Vulkan sur AMD — https://www.phoronix.com/news/RADV-VK_KHR_cooperative_matrix
- Vulkan dans les conteneurs — https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/docker-specialized.html
- Tarif CEEAC-CEMAC 2026 — https://www.agenceecofin.com/actualites-services/1401-134813-cameroun-entree-en-vigueur-du-tarif-exterieur-commun-ceeac-cemac-le-1er-janvier-2026

---

## 17. Décisions

| # | question | proposition | état |
|---|---|---|---|
| D6 | la voie | **topologie par construction** depuis un squelette ; l'IA prédit des paramètres, jamais un maillage brut | 🔴 proposé (Rodolf : « je suis tout oui ») |
| D7 | le format | `.nkscene` **version 2** (squelette, forme, retouches, sculpt), compatible v1, texte modifiable | 🔴 proposé |
| D8 | R32 | **priorité C0** : adresses stables, sélection par nom / critère / adresse, orphelines gardées | ✅ Rodolf, 28/09 : « ça doit être résolu » |
| D9 | la frontière du rig | NKCraft **produit** le rig, les poids et les blendshapes du visage ; NkAnimaEditor les **ajuste** et anime | 🔴 proposé (Rodolf, 28/09 : « pas pour l'animation mais pour le rig après modélisation ») |
| D10 | le code tiers | **porter** FEQ / GEL (MIT) et QuadriFlow (BSD-3) en les réécrivant sans STL, avec leurs notices ; GPL, AGPL et non commercial : **articles seulement** | 🔴 proposé |
| D11 | les données d'IA | synthétique maison d'abord ; sources extérieures seulement CC0 / CC-BY sans « NoAI » ; sculpts commandés avec clause IA ; corpus existant audité | 🔴 proposé |
| D12 | le bogue GPU | le remède du §12.3 avant toute nouvelle campagne d'entraînement | 🔴 proposé |
| D13 | le matériel | pas de GP100 ; une carte **48 Go** (W7900 ou A6000 d'occasion garantie) après C4, location ponctuelle pour les grosses campagnes | 🔴 à décider par Rodolf (budget) |
| D14 | Quaternius, 3DBiCar | demander par écrit l'autorisation d'entraînement commercial | 🔴 à faire |
| D15 | les plans de corps | terre, air, eau et inventé **au même rang** : fuseau, disque, radial, cloche, segmenté, ailes (membrane, plumes, insecte), nageoires, tentacules, semis, greffe (§5.7) | 🔴 proposé (Rodolf, 28/09 : « les animaux marins, les animaux à ailes, les créatures à ailes ou marines et inventées ») |
