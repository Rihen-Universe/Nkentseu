# UnkenyEditor — le squelette 2D

### Document 07 — R30 : rigging, skinning et IK 2D, mouvement secondaire

> Rihen, 01/10/2026 : *« rigging et skinning 2D : poser des os sur un personnage,
> déformer l'image (maillage pondéré), IK 2D, animer les os sur la frise, mouvement
> secondaire par les corps mous (écharpe, cheveux) »*.
>
> Plan validé le 02/10 : **le squelette 2D vit dans NKAnima**, comme un squelette 3D
> **contraint au plan** (os dans XY, rotation autour de Z), avec un « mode 2D » qui
> verrouille ces contraintes — **pas de second moteur d'animation**. S'y ajoutent ce
> qui est propre à la 2D : l'**ordre de dessin animable** et les **emplacements**
> façon Spine. Unkeny a un composant **Squelette 2D** (optionnel : un sprite reste un
> sprite) qui **déforme le maillage 2D de R31** (document 06).

Rédigé le 02/10/2026 sur la branche `anima/squelette-2d` (dossier `Nkentseu-g-r30`),
après le code et ses bancs. Tout ce qui est dit « fait » est **exercé** par
`NKAnima_Tests`, `UnkenyEditor --selftest` (ou `--banc-squelette`),
`Physic2D --selftest`, `UnkenyPlayer --selftest`, ou vu sur les captures de
`References/Captures/squelette-2d/`.

---

## 1. Où cliquer

| geste | où |
|---|---|
| poser un squelette | **Détails > Ajouter un composant > Animation > Squelette 2D : Humanoïde (de face) / Quadrupède / Oiseau / Créature libre / Humanoïde de profil**, ou **Squelette 2D vide** ; clic droit sur une entité à maillage > **Poser un squelette 2D (humanoïde)**. Sans maillage, il est d'abord créé depuis le sprite (ou la forme) ; le modèle est mis à la boîte du maillage ; les poids sont posés par la **chaleur**. La fenêtre s'ouvre sur l'outil Os. |
| ouvrir la fenêtre | carte **Squelette 2D** > **Éditer le squelette…** ; clic droit > **Éditer le squelette 2D…** — c'est la **fenêtre du maillage** (document 06), quatre outils de plus |
| **Os (B)** | glisser dans le vide = un os, **enfant de l'os choisi** ; **Maj + glisser depuis le bout de l'os choisi** = un os **chaîné** ; la **tête** d'un os le déplace, sa **queue** le tourne et l'allonge (le **repos**) ; panneau : **Nom**, **Angle**, **Longueur**, **X / Y** (repère du parent), **Symétrie G/D**, **Supprimer l'os** (Suppr) ; **Remplacer par un modèle : Face, Profil, Quadrup., Oiseau, Créature** |
| **Poids (W)** | clic sur une **tête** = l'os à peindre (ou la liste) ; **glisser = peindre** (Maj, ou la case **Retirer**, enlève) ; **Rayon**, **Force** ; **Auto : chaleur**, **Auto : distance**, **Par parties** (une partie au nom d'un os = cet os) ; **Normaliser** ; la vue montre la **chaleur** du poids (bleu 0 → vert → rouge 1) |
| **Pose (R)** | tirer la **queue** d'un os le tourne (ses enfants suivent, la peau aussi) ; la tête d'une **racine** la déplace ; **Revenir au repos** |
| **IK (I)** | tirer la **tête d'une main** : le bras et l'avant-bras la suivent ; tirer la **queue** d'un os : lui et son parent ; **Coude : Garder / Gauche ↺ / Droite ↻** (Garder = le sens qu'il avait au début du geste) |
| emplacements | outil Os, panneau **Emplacements** > **Nouvel emplacement : « partie » sur l'os choisi** (la partie est celle choisie dans l'outil Sélection), **Attache de plus : « partie »** ; une attache non montrée est **cachée**, la montrée prend l'**ordre** de l'emplacement |
| chaîne molle | outil Os, choisir le premier os (l'écharpe) > **Chaîne molle depuis « os » (n os)** ; la croix **x** la retire ; en **Jouer**, la chaîne pend, se balance et traîne derrière le corps |
| animer les os | **Fenêtre > Animation (frise)** > **+ Piste** > **Squelette 2D** : **Tous les os (clé de pose)** ou **Os : BrasG (angle, x, y)** ; une clé = la place **locale** de l'os ; ◆, double-clic, glisser une clé, interpolations (palier, linéaire, entrée, sortie, douce, rebond…) et **Courbes** comme toute piste ; l'**Aperçu** pose le squelette au curseur sans laisser de trace |
| animer un emplacement | **+ Piste** > **Squelette 2D** > **Main : image** / **Main : ordre** (paliers) |
| jouer | **Jouer en jeu** (page Animation) pose le clip sur l'entité ; ou un état de l'**Animateur** qui désigne le clip — fondus, arbres, **couches masquées par os** (le masque « Torse » tient l'os et ses descendants) |
| enregistrer | carte > **Enregistrer le squelette (.nkskel)** : `Contenu/Squelettes/<nom>.nkskel` ; le clip : **Enregistrer** de la page Animation (`.nkanim`) |
| les os dans la scène | l'entité choisie montre ses os dans la vue (en jeu, si **Os visibles en jeu**) |
| NkAnimaEditor | ouvrir un **`.nkskel`** (lanceur > Ouvrir, ou en argument) : le `.nkanim` du même nom (sinon le premier du dossier) vient avec ; l'aperçu montre le squelette, le **tiroir Frise** ses pistes d'os |

Captures sans souris : `UnkenyEditor --captures-squelette=DOSSIER` (Bodofia dessinée au
programme), puis `NkAnimaEditor --captures-squelette2d=DOSSIER DOSSIER/fichiers/Bodofia.nkskel`.

---

## 2. Le premier contrôle : ce que NKAnima met dans un jeu 2D livré

Fait **avant** d'écrire une ligne (02/10, `nm -C` sur `UnkenyPlayer.exe` Debug) :
NKAnima ne dépend que de Foundation (`NKAnima.jenga` : NKPlatform, NKCore, NKMemory,
NKMath, NKContainers, NKLogger, NKFileSystem ; aucun `#include` de rendu dans ses
sources). Le joueur lie **1594** symboles `anim::`, tous de `Clip/NkAnimation.cpp`
(clips, machine à états) et `Blend/NkAnimMix.cpp` (mélange) ; **0** de `Physics/`
(NkPoseMass, NkBalance, NkPoseBalancer, NkAutoPose, NkContactDetector,
NkClipBalancePass), `Retarget/`, `Face/`, `Motion/`, `Edit/`, `NkClipRegistry` ; **0**
de NKRenderer, NKRHI, Noge (les `renderer::NkRenderer2D…` vus sont ceux de NKCanvas).
L'éditeur de liens ne prend que les objets référencés d'une bibliothèque statique :
**rien à isoler**. Depuis R30 s'y ajoute `Skeleton/NkSkeleton2D.cpp` (le squelette
lui-même, voulu).

---

## 3. Ce qui est fait

### 3.1 NKAnima — `Kernel/Runtime/NKAnima/src/NKAnima/Skeleton/NkSkeleton2D.h`

- **`NkBone2D`** (x, y, angle autour de Z, échelle x/y) ↔ `NkMat4f` / `NkBoneTRS`.
  **`NkLockToPlane`** (le mode 2D : ce que les outils appliquent à tout ce qu'ils
  écrivent), **`NkIsPlanar`**.
- **`NkSkeleton2D`** = **`NkSkeletonDef`** (la structure unique du moteur, repos MONDE,
  FK `NkForwardKinematics`) + **longueurs** + **emplacements** (`NkSlot2DDef` : os
  porteur, attaches nommées, attache et ordre au repos). `AddBone`,
  `AddBoneHeadTail`, `SetBindLocal2D` (les descendants suivent), `PoseToWorld`,
  `Head`/`Tail`, **`Validate`** (topologie, longueurs, os dans le plan),
  **`PrepareClip`** (un clip local pour ce squelette : parents, noms, ordre,
  inverses de repos).
- **`.nkskel`** : sections `BONE` (tout lecteur) + `PL2D` (la marque 2D) ;
  `NkLoadSkeletonDef` pour un outil 3D. Décision écrite dans
  `CONVENTIONS_FICHIERS.md` § 2.
- **Modèles de départ** (`NkMakeSkeleton2D`, mis à une boîte) : humanoïde **de face**
  (16 os, G/D en miroir), **quadrupède** et **oiseau** de profil, **créature libre**
  (une racine : aucune forme imposée), humanoïde **de profil** (le jeu de plateforme).
  `NkMirrorBoneName` (BrasG ↔ BrasD).
- **IK à deux os** : `NkSolveTwoBoneIK2D` (loi des cosinus, coude à gauche ou à
  droite, tendu hors de portée — et le dit), `NkApplyTwoBoneIK2D` sur une pose,
  `NkApplyTwoBoneIK2DWorld` sur une pose monde déjà calculée (Unkeny), et
  `NkTwoBoneBendIsPositive2D` (garder le sens d'un coude pendant qu'on tire).
- **Emplacements animés** : pistes de propriétés à paliers
  `Emplacement[<nom>].image` / `.ordre` (`NkSlot2DProperty`, `NkEvaluateSlots2D`,
  `NkDrawOrder2D`) — le format du clip ne change pas.
- Trois corrections du module, faites pour la 2D, valables pour tous :
  - une **clé d'os suit sa courbe** (entrée, sortie, douce, rebond…) dans
    `NkAnimMix` ; linéaire et « cubique » inchangées ;
  - un **os sans clé d'un clip local reste à son repos** (il valait l'identité : il
    s'effondrait sur son parent), dans `NkSampleClip` **et** dans le lecteur
    (`NkAnimationPlayer`, arbres 1D/2D) ;
  - le `.nkanim` relu **rend ses noms d'os** (depuis les noms de pistes ; le fichier
    n'écrivait pas `jointNames`).

### 3.2 Unkeny — `Engine/Unkeny/src/Unkeny/Squelette/NkUnkenySquelette.h`

`NkSquelette2D` : le **miroir à capacité fixe** d'un `anim::NkSkeleton2D` — **32 os**
(nom, parent toujours avant l'os, **repos** et **pose** locaux, longueur),
**8 emplacements** (4 attaches = des parties du maillage), **4 chaînes molles**,
**4 IK en jeu**, `source` (le `.nkskel`). Photographié, sauvé et préfabriqué champ
par champ (`NkChampsSquelette2D`, déclaré par `NkScene::Init` après le maillage).

| quoi | comment |
|---|---|
| **la peau** | `NkMaillageDeformer2D` : p' = Σ poids × (monde_pose × inverse(monde_repos)) × p, la FK étant celle de NKAnima. **Un seul point de branchement** : `NkMaillagePositionsMonde` reçoit maintenant l'**entité** et passe par elle — rendu, prise au clic (`NkDistanceMaillage2D`), boîte de sélection, fenêtre en jeu ; rien d'autre n'a été touché. Un sommet sans poids suit l'entité comme avant ; une partie **physique** (rigide, molle) reste tenue par la physique. |
| **les emplacements** | `NkEmplacementsParties2D` : le **dessin** cache les attaches non montrées et donne l'ordre de l'emplacement à la montrée — **rien n'est écrit dans le maillage** (un aperçu ne laisse pas de trace). |
| **les poids** | 4 os + 4 poids 16 bits par sommet (R31), somme pleine exacte ; pinceau, normaliser, **auto par distance** (1/d²), **auto par chaleur** (Baran et Popović 2007 : l'os le plus proche chauffe, la chaleur diffuse le long des arêtes, (−L + H) w = H p par Gauss-Seidel), **par parties** ; les **doubles d'une couture reçoivent les mêmes poids** (sinon la couture s'ouvre dès qu'un os plie). |
| **jouer** | un clip d'os joué par le **clip de propriétés** (`NkAppliquerClipProprietes` prend le chemin de la pose de NKAnima) ou par l'**Animateur** (`NkMelangerAnimateurs` : fondus à leur courbe, arbres, couches masquées par os) ; `NkSqueletteAppliquerPose` écrit les os **par nom** et les emplacements ; un os que le clip ne clé pas garde sa pose. |
| **la chaîne molle** | en jeu, un corps de particules **XPBD** (`NkParticules2D`, matière corde) : un maillon par os, la tête **tenue** par l'os parent (cinématique), liens de structure (et de flexion si demandé), **traînée de l'air** ; après les animations, chaque os pointe vers le maillon suivant. Elle naît au premier pas (`NkSquelettesAvantPasFixe`), meurt avec l'entité, à Restaurer ou Arrêter. |
| **l'IK en jeu** | `NkContrainteIK2D` : un os « poignée » tire une chaîne de deux os, après les animations (`NkSquelettesApresAnimation`). |
| **les propriétés** | `Emplacement[x].image` / `.ordre` sont listées, lues et écrites (frise, scripts, IA). |

### 3.3 L'éditeur — `Applications/UnkenyEditor/src/Editeur/`

| fichier | rôle |
|---|---|
| `NkEditeurSquelette.h/.cpp` | créer (modèle, maillage s'il manque, poids), les **gestes du modèle** (tous retenus : Ctrl+Z), l'asset `.nkskel`, les actions `NK_A_SQUELETTE*` (plage **2550-2599**), les os dans la vue de la scène |
| `NkEditeurPageMaillage.cpp` | les outils **Os, Poids, Pose, IK** (vue, panneau, clavier B / W / R / I) ; la peau posée en Pose/IK et en jeu ; la liste des os **en arbre** |
| `NkEditeurPagesAnim.cpp`, `NkEditeurPageAnimation.cpp` | les **pistes d'os** de la frise (`Squelette/Hanches/…/BrasG`, propriété `Os`, trois canaux : angle en degrés, x, y) ↔ les pistes d'os du clip ; « + Piste » ; l'aperçu garde et rend la pose |
| `NkEditeurPageAnimateur.cpp` | les **os** proposés comme masques de couche |
| `NkEditeurDetails.cpp`, `NkEditeurChrome.cpp`, `NkEditeurActions.cpp` | la carte **Squelette 2D** (pastille Animation), « Ajouter un composant », le clic droit, dupliquer, retirer, l'échelle cuite |
| `NkEditeurBancSquelette.cpp` | le banc éditeur et les captures (Bodofia) |

### 3.4 NkAnimaEditor — `Applications/NkAnimaEditor/src/NkAnimaEditor/`

Son « espace 2D » est **spécifié** (document 04 § 3, § 12-13 : vue 2D, arbre du rig
2D, slots, mode maillage pondéré, atlas, pelure d'oignon) ; **branché a minima** pour
prouver que les fichiers sont les mêmes : `AnimInit` ouvre un `.nkskel` (et son
`.nkanim`) ou un `.nkanim` (et le `.nkskel` voisin), reprend les pistes **par nom
d'os** ; l'aperçu montre le squelette à la pose courante, le **tiroir Frise** ses
pistes d'os et ses poses-clés ; le Contenu et le lanceur connaissent `.nkskel` et
`.nkanim` ; `--captures-squelette2d=DOSSIER`.

---

## 4. Les bancs et les captures

- **NKAnima_Tests** (`tests/test_squelette2d.cpp`), **s1-s6** : modèles et mode 2D
  (contre-épreuve : un os penché hors du plan est refusé), pose (aller-retour,
  chaîne à la main), **IK à deux os qui atteint sa cible** (coude des deux côtés,
  tendu hors de portée ; contre-épreuve : une longueur fausse rate), **mélange de
  deux clips d'os** (0° et 90° → 45°, masque par os, courbe d'une clé, contrôleur à
  deux états, os sans clé au repos), `.nkskel` (aller-retour, lecteur 3D, fichier
  tronqué refusé, noms d'os du `.nkanim`), emplacements. Contre-épreuves à la main
  notées en tête du fichier : `base + sg * alpha` → `base - sg * alpha` (s3 rouge),
  `m10 = -s * sy` → `s * sy` (s2 et s3 rouges).
- **BANC SQUELETTE 2D** (moteur, `Engine/Unkeny/src/Unkeny/Banc/NkUnkenyBancSquelette.cpp`,
  joué par `UnkenyEditor`, `Physic2D` et `UnkenyPlayer --selftest`) : **21/21** —
  **un sommet pondéré 50/50 entre deux os suit la moyenne** (calculée à la main ;
  q1n : à 100/0 le témoin voit ÉCHEC), un sommet sans poids suit l'entité, la prise
  au clic suit la peau ; `.nkskel` et scène JSON (q2n : tronqué refusé) ; **un clip
  d'os joué** (45° à mi-clip) et par l'**Animateur** (q3n : clip inconnu, rien ne
  bouge) ; emplacement fermé par une piste ; **la chaîne molle pend, traîne, reste
  accrochée** (q5n : sans particules, rien ne pend) ; IK en jeu ; auto-poids ;
  Arrêter.
- **BANC SQUELETTE ÉDITEUR** (`NkEditeurBancSquelette.cpp`, sur la vraie trame) :
  **22/22** — Ajouter un composant + Ctrl+Z / Ctrl+Y ; le menu et ses cinq modèles,
  la carte ; **Maj + glisser** chaîne un os, Ctrl+Z ; **IK à la souris** (la main
  arrive sur sa cible, le coude garde son sens, **Ctrl+Z** rend la pose ; e4n : la
  même pose écrite sans retenir, le témoin de Ctrl+Z voit ÉCHEC) ; pinceau + Ctrl+Z ;
  **frise** (une piste par os, une clé changée change le clip, l'aperçu sans trace) ;
  **Jouer** (la marche plie la cuisse, la peau bouge ; Arrêter rend le repos) ;
  **chaîne molle** en Jouer (pend, traîne ; Arrêter la rend) ; `.nkskel` relu par
  NKAnima.
- **Captures** `References/Captures/squelette-2d/` : 01 Bodofia (silhouette
  encapuchonnée de profil, capuche dans l'ombre, **aucun trait de visage**, écharpe
  braise, braise dans la main) et ses os, carte Squelette 2D ; 02 la fenêtre, outil
  Os (liste en arbre) ; 03 la **chaleur** des poids de CuisseD ; 04 une **pose IK**
  (la main lève la braise, la cible se voit) ; 05 la **frise** et ses pistes d'os ;
  06-07 la **marche** jouée, deux instants ; 08 l'**écharpe molle** qui traîne quand
  Bodofia file ; 09 **NkAnimaEditor** ouvre le même `.nkskel` et la même marche ;
  `fichiers/` : `Bodofia.nkskel`, `Marche.nkanim`.

---

## 5. Limites connues (dites, pas cachées)

- **Échelles d'os positives** : un miroir se fait sur l'entité (son transform), pas
  sur un os (la décomposition TRS ne le rendrait pas). L'IK suppose des parents à
  échelle uniforme.
- **Noms d'os : 15 caractères** dans le composant ; un clip nomme ses os par leur
  nom : un nom plus long ne se retrouve pas.
- **La frise montre angle, x, y** : l'échelle d'une clé est reprise de l'ancien clip
  (pas éditable là) ; les **tangentes** posées à la main ne valent que pour les
  pistes de propriétés (« Courbe » est linéaire sur un os, les autres courbes —
  entrée, sortie, douce… — sont suivies).
- **Le chaînage** : Maj + glisser depuis le bout du choisi ; un glisser simple sur
  la queue la **tourne**.
- **La chaîne molle** ne fait que **tourner** ses os (leurs têtes restent à leur
  place) ; elle ne touche pas le décor par défaut (`decor`) ; son état n'est **pas
  photographié** (une photo prise en jeu la rend au repos) ; la flexion est la
  raideur du corps / 20 (NKPhysics n'a qu'une raideur par corps).
- **Les poids par chaleur** n'ont pas de test de visibilité (un os voit à travers
  un trou de la silhouette) ; le Laplacien est pondéré par 1/longueur², pas par les
  cotangentes. Les **modèles** se mettent à la boîte du maillage : une écharpe qui
  dépasse élargit la boîte — on reprend alors les os à la main (comme pour Bodofia).
- **Une attache cachée reste prenable au clic.**
- **Une partie physique** (rigide, molle) naît à la place de repos du maillage, pas
  à sa place posée.
- **Les IK en jeu** (`NkContrainteIK2D`) n'ont pas encore de panneau dans l'éditeur
  (données et moteur seulement).
- **NkAnimaEditor** montre le squelette en articulations, sans la peau, et ses outils
  de pose ne sont pas encore verrouillés au plan.
- **Le prix** : ~2,4 Ko de plus dans la photo de chaque entité (le maillage en prend
  ~6,5) — à peser avant d'augmenter les capacités.

---

## 6. Ce qui reste (après R30)

- NkAnimaEditor : l'**espace 2D** du document 04 § 12-13 en entier (vue 2D avec la
  peau, arbre du rig 2D, slots, pondération, **atlas de sprites**, **pelure
  d'oignon**), ses outils verrouillés au plan.
- La pelure d'oignon et l'atlas dans UnkenyEditor aussi.
- Un panneau pour les **IK en jeu** et les **contraintes** (poignées animées,
  pieds plantés).
- Photographier l'état des chaînes molles ; leur contact avec le décor réglé par
  chaîne ; la flexion vraie (une raideur par lien dans NKPhysics).
- La visibilité dans les poids par chaleur ; un « lisser les poids ».
- Renommer `NkAssetType::SkeletalMesh` (à trancher par Rihen, `CONVENTIONS_FICHIERS.md`).
