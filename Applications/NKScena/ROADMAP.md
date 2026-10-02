# NKScena — feuille de route

AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
Créé le 2026-09-13. **Née le 2026-10-02** (branche `nkscena/naissance`).

---

## 🎬 0. ELLE EST NÉE — 2026-10-02

Sa condition de naissance (§3 : une séquence se sauve et se relit) était remplie
depuis le 13/09 ; Rihen a demandé le 01/10 qu'elle ait **exactement l'apparence
d'UnkenyEditor**, comme Nogee et NkAnimaEditor, la vue 3D au centre. La
définition du §1 tient mot pour mot : elle n'édite ni maillage ni squelette ni
la scène ; **sa seule surface propre est la frise**, tout le reste est emprunté.

### 0.1 Ce qui marche (et où c'est)

| quoi | comment | où |
|---|---|---|
| la face de la famille | barre de titre peinte, **logo provisoire** (un clap, rouge « antenne ») au coin, menus Fichier · Édition · Piste · Rendu · Fenêtre · Aide, barre d'outils (Ouvrir scène, Enregistrer séquence, outils, + Piste, Clé, lecture/pause/stop/image, timecode, vue libre / caméra du plan, Rendre), Outliner (**la scène + les pistes**), Détails en **cartes** (entité, piste, clé, plan, séquence/rendu), tiroir : **Frise** en premier onglet, puis Contenu, Journal, Terminal | `src/NKScena/NKScenaInterface*.cpp` — dérivée de `NKEditorKit/Famille/NkFamilleEditeur` |
| le lanceur | le **lanceur partagé** de la famille (`NkProjectLauncherModel`), touche NKScena : glyphe caméra, accent rouge, séquences récentes, modèles (Exemple, Séquence vide, Sur une scène…) ; au lancement nu et par Fichier > Accueil | `src/NKScena/NKScenaLanceur.cpp` |
| la vue 3D au centre | **la vue de Nogee, empruntée** : `NogeeVue3D.cpp` et `NogeeModele.cpp` sont **compilés tels quels** par `NKScena.jenga` (pas copiés) — NkApplication + NkEngineLayer, rendu hors écran affiché en image, gizmo, caméra souris, liseré de sélection ; les **caméras de la scène** et leur pyramide (celle du plan en rouge), le cadre « PLAN » quand la vue regarde par la caméra du plan | `NKScenaInterfaceVue.cpp`, `NKScenaApp.cpp` |
| la frise | **LA frise partagée** (`NKEditorKit/Components/NkTimeline*`, façon Sequencer d'UE5) : pistes en arbre, muet/solo/verrou, règle, plage, marqueurs, clés, courbes et tangentes, **piste de clips « Plans caméra »** | `NKScenaInterfacePanneaux.cpp` (Frise) |
| le pont frise ↔ séquence | une entité = trois pistes de frise (Position, Rotation, Échelle) = une `NkTrack` Transform **nommée** ; la piste de clips « Plans caméra » = `NkCameraTrack` ; marqueurs, plage ; ce que la frise ne montre pas passe intact (`reste`). **Déterministe** : relire puis réécrire donne les mêmes octets | `NKScenaPont.{h,cpp}` |
| le modèle | pistes, clés (valeur **vivante**, rotations **déroulées**), plans, lecture, caméra du plan (priorité empruntée puis rendue), `.nkseq` qui **pointe vers sa scène** `.nkscene3d`, liaison **par le nom**, exemple de démonstration | `NKScenaModele.{h,cpp}` |
| « Rendre » | dans l'application : image par image dans la vue (taille de la sortie, caméra du plan), relue, **PNG numérotés** (`NkImageSequenceWriter`), **progression au Journal** ; sans fenêtre : `NkScenaRendreSansFenetre` (DirectX 11 sans surface, `NkRenderer` hors écran, `NkTransformSystem` + `NkRenderSystem` de Noge) | `NKScenaApp.cpp`, `NKScenaRendu.{h,cpp}` |
| le banc | `NKScena --selftest` : **34 verts, 0 rouge**, contre-épreuves comprises (mutation d'une clé, octet abîmé, cible renommée, image retournée, temps figé) | `NKScenaBanc.cpp` |

Ce que Noge a reçu pour elle (`Engine/Noge/src/Noge/Sequencer/NkSequencer.*` ;
`NkSequenceCheck` toujours **61 verts**) :

- les canaux **`localRotation.x/.y/.z`** (degrés : tangage, lacet, roulis),
  composés **lacet · tangage · roulis** — la convention de la caméra d'édition de
  Nogee, exposée par `NkSequenceRotationFromDegrees` /
  `NkSequenceDegreesFromRotation`. ⚠️ Pas `NkQuatf(NkEulerAngle)` de NKMath :
  Z·Y·X, singulière au **lacet de 90°**, celui qu'une caméra franchit le plus
  (mesuré au premier banc : |q·q'| = 0,85 au lieu de 1,00) ;
- le format **`.nkseq` v2** : la **scène visée** (`NkSequence::scene`) et les
  **noms** des cibles (`entityName`, `cameraName`) ; la v1 reste lue ; une cible
  nommée s'écrit avec un identifiant `Invalid` (sinon deux sauvegardes de la même
  séquence différaient d'une session à l'autre — mesuré : 14 octets) ;
- `NkSequence::BindByName(world)` : relie pistes et plans par `NkName`, rend le
  nombre de cibles introuvables.

### 0.2 La lancer (depuis la RACINE du dépôt : les nuanceurs y sont lus)

```
jenga build --target NKScena
./Build/Bin/Debug-Windows/NKScena/NKScena.exe              # le lanceur
./Build/Bin/Debug-Windows/NKScena/NKScena.exe --exemple    # la séquence de démonstration
./Build/Bin/Debug-Windows/NKScena/NKScena.exe --sequence=Build/NKScena/exemple.nkseq
./Build/Bin/Debug-Windows/NKScena/NKScena.exe --selftest   # le banc, sans fenêtre
./Build/Bin/Debug-Windows/NKScena/NKScena.exe --rendre-sans-fenetre=F.nkseq [--sortie=DOSSIER]
```

Gestes : Espace joue, ← → image par image, **I pose une clé** (la piste active,
ou toute l'entité choisie), Suppr, Ctrl+Z / Ctrl+Y (la frise), **0 regarde par la
caméra du plan**, Ctrl+S enregistre, Ctrl+R rend. Un objet déplacé à la main
revient à sa piste au prochain changement d'instant tant qu'une clé ne l'a pas
retenu (le comportement du Sequencer d'UE5).

Captures (hors écran) : `References/Captures/nkscena/`.

### 0.3 Ce qui suit (P1 du document 01, dans l'ordre)

1. **La vue 3D et le modèle de scène dans une brique partagée** : NKScena compile
   aujourd'hui deux fichiers de Nogee ; la place de `NogeeVue3D` / `NogeeModele`
   est dans Noge (un « éditeur de scène » commun à Nogee, NKScena, PV3DE).
2. **Pistes `Animation` / NLA** dans la frise (les clips d'un personnage :
   `NkClipRegistry`, le mélange de NKAnima) — le §6 ci-dessous ; le pont les fait
   déjà passer intactes (`reste`), il ne les MONTRE pas encore.
3. **Fondus entre plans** (`NkCutType::Blend`) : écrits et relus, mais la vue
   coupe franc (le moteur choisit UNE caméra ; le fondu d'image reste à écrire).
4. **Sortie vidéo** : `NkVideoConverter::ImageSequenceToVideo` / `NkMp4H264Writer`
   derrière les PNG (critère P1 : un plan de 10 s en MP4).
5. **Caméra physique** (focale, mise au point, profondeur de champ) dans la carte
   Caméra ; **projet Film → Séquence → Plan** (`.nknoge`, le projet partagé de la
   famille 3D, CONVENTIONS_FICHIERS §6).
6. L'**auto-clé** d'UE5 ; le **son** (`NkTrackType::Audio`, §5.3).

**Vu dans les images rendues, pas réglé ici** : près du sol, le bas de l'image
devient sombre à partir de la deuxième image (l'ombre du soleil au-delà de sa
carte d'ombres ?). C'est NKRenderer, pas la séquence (la même scène, la même
caméra, le même renderer que la vue) : à mesurer de son côté.


---

> ## ⚠️ (HISTORIQUE, état du 2026-09-13 — révolu le 2026-10-02, voir §0) Ce dossier ne contient AUCUN code, et c'est délibéré
>
> Il n'y a ici ni `main.cpp`, ni `.jenga`, ni `src/`. Une application qui existe
> sous forme de coquille coûte plus qu'elle ne rapporte : elle apparaît dans les
> listes, elle se compile, elle se met à jour — et elle ne fait rien. Le dépôt en
> a déjà payé le prix (`Applications/NkAnima/`, un dossier sans une ligne de code
> ni `.jenga`, retiré depuis).
>
> **Ce fichier existe pour combler un trou documenté, pas pour ouvrir un chantier.**
> `cartographie_markdown/divergences.md` §1.3 mesurait le 2026-08 que `NKScena`
> apparaissait dans **2 fichiers sur 747**, dans les deux cas en passant, et
> concluait : « *ce n'est pas une contradiction : c'est un trou, et c'est
> probablement plus grave, parce qu'un trou ne se détecte pas à la lecture* ».

---

## 1. Ce que NKScena est — une phrase

> **NKScena est l'application qui pose le TEMPS sur une scène que d'autres ont
> construite : elle n'édite ni maillage ni squelette, elle ouvre une scène Noge,
> y place des pistes, des clés et des plans caméra sur une timeline, et rend la
> séquence en fichiers — elle est au temps ce que NKCraft est à la forme et
> NkAnimaEditor au mouvement d'un personnage.**

Formulée le 2026-09-13, au vu de ce que Noge et NkAnimaEditor font **déjà**, et
validée par le coordinateur le même jour.

## 2. Ce que ça exclut — la partie utile de la définition

| NKScena ne fait PAS | qui le fait déjà |
|---|---|
| éditer une géométrie, des UV, une topologie | **NKCraft** |
| poser un squelette, corriger une pose, juger un équilibre | **NkAnimaEditor** |
| écrire un cinquième viewport de zéro | **Nogee** — son viewport affiche enfin sa scène, et `NKRenderer/Core/NkGizmo.h` (2 009 l.) s'emprunte |
| inventer un format de scène | la sérialisation de Noge |

**Sa seule surface propre est la timeline.** Tout le reste est emprunté. Une
application dont la surface propre est étroite est une application qui peut
exister ; c'est cette étroitesse qui rend la définition utile.

## 3. Pourquoi elle n'existe pas encore — et sa condition de naissance

Elle applique le principe de conception de Rihen
(`PRINCIPES_CONCEPTION.private.md`) : **une fonctionnalité ne naît que quand ses
outils existent**. L'outil de NKScena, c'est le séquenceur.

État de cet outil au 2026-09-13 :

| brique | état |
|---|---|
| `Engine/Noge/src/Noge/Sequencer/NkSequencer.h` | 416 l., spécification — **16 méthodes inline, 16 sans corps** |
| `Engine/Noge/src/Noge/Sequencer/NkSequencer.cpp` | **451 l., écrit le 2026-09-13** — les 16 corps manquants |
| `Applications/NkSequenceCheck` | le banc : clés → pose ECS → PNG numérotés, **sans fenêtre ni GPU** |
| `NKMedia/Video/NkImageSequenceWriter` | 205 l. — écrit `frame_0001.png`, **aucune dépendance GPU** |
| `NKRenderer/Tools/Offscreen/NkOffscreenTarget` | 351 l. — `ReadbackPixels` + `Capture(path)`, **tourne déjà** dans NKCraft |
| pistes NLA | **livrées le 2026-10-01** : `NkNLATrack::Evaluate(t, monde, registre)` (Noge) sur la pile de poses de NKAnima (`Blend/NkAnimMix.h`) ; la piste `Animation` fond ses clips qui se chevauchent |
| sérialisation de séquence | voir `Engine/Noge/ROADMAP.md` — **livrée** (f7 de NkSequenceCheck) ; **format v2 le 2026-10-02** (scène visée, noms des cibles) |

> **Condition de naissance de l'application** : quand une séquence se sauve et se
> relit (`NkSequence::SaveToFile`/`LoadFromFile`, qui rendent `false` aujourd'hui).
> Sans elle, NKScena serait un éditeur dont le travail disparaît à la fermeture —
> et c'est ce que NkAnimaEditor a été jusqu'au 2026-09-13, ce qui a coûté un lot
> entier pour le corriger. Ne pas refaire ce chemin-là.

## 4. Ce qu'elle empruntera, nommément

- le viewport et le gizmo : Nogee + `NKRenderer/Core/NkGizmo.h` ;
- l'interface : `NKEditorKit` (thème GitHub Dark Pro / Light Pro, comme toutes
  les applications de la maison) ;
- la lecture d'un clip : `NKAnima::NkAnimationPlayer::Evaluate(clip, t, out)` —
  **absolue en `t`**, et non `AnimUpdate(dt)` de NkAnimaEditor, qui est relative
  et vit dans un état global d'application ;
- la sortie : `NkImageSequenceWriter` pour la suite d'images, `NkOffscreenTarget`
  pour le rendu, `NkVideoConverter::ImageSequenceToVideo` pour le film final.

## 5. Ce qui reste à trancher — et qui ne se tranche pas ici

1. **Rodolf** : NKScena est-elle une application distincte, ou un **espace** de
   Nogee ? La définition ci-dessus tient dans les deux cas ; le coût de
   maintenance, non.
   *(2026-10-02 : née application distincte, comme le proposait le document 01
   (R1), sur la demande de Rihen du 01/10 ; elle partage avec Nogee sa vue et son
   modèle de scène — compilés, pas copiés — ce qui garde la question ouverte à
   faible coût.)*
2. ~~La timeline est-elle partagée avec NkAnimaEditor ?~~ **Tranché le 2026-10-01** :
   oui, LA frise de `NKEditorKit` (`Components/NkTimelineModel.h`, façon Sequencer
   d'UE5), déjà dans UnkenyEditor et NkAnimaEditor — voir §6.
3. Le son : `NkTrackType::Audio` est déclaré et n'agit pas. Un film sans son se
   monte quand même, mais pas longtemps.

## 6. Le MÉLANGE à brancher (2026-10-01, demande de Rihen)

> « On peut intégrer le MÉLANGE de plusieurs animations, que ce soit dans
> NkAnimaEditor, Noge, Unkeny, NKScena ou PV3DE. » NKScena n'a pas de code : voici
> ce qu'elle BRANCHERA, rien de plus. Tout existe et a ses témoins.

| Besoin de NKScena | Ce qui existe | Où |
|---|---|---|
| la frise de montage | la frise partagée : arbre de pistes (muet / solo / verrou), règle secondes + images, **plage de lecture**, **marqueurs**, clés en formes d'interpolation, courbes + **tangentes**, boîte d'échelle, copier / coller, **pistes de clips** (NLA : clips posés, rognés, fondus, chevauchés) | `Engine/NKEditorKit/src/NKEditorKit/Components/NkTimelineModel.h`, `NkTimelineDraw.cpp` (témoins : NKEditorKitTest famille 31, t1–t17) |
| un clip de séquence (des clips posés sur des pistes) | `NkAnimationClip::clipTracks` (`NkClipStripTrack`, `NkClipStrip`), section `CLPS` du `.nkanim` | `Kernel/Runtime/NKAnima/src/NKAnima/Clip/NkAnimation.h` |
| jouer la séquence à `t` (absolu) | `anim::NkSampleClip(clip, t, pose, &lookup)` → `NkAnimPose` (os TRS + propriétés à couverture) ; `NkEvaluateStrips` pour des pistes seules | `Kernel/Runtime/NKAnima/src/NKAnima/Blend/NkAnimMix.h` |
| appliquer à un personnage Noge | `NkNLATrack::Evaluate(t, monde, registre)` (transform + `NkSkeleton`) | `Engine/Noge/src/Noge/Sequencer/NkSequencer.cpp` |
| un personnage qui *réagit* pendant le plan | `NkAnimController` (base + couches masquées + arbres 1D/2D + courbes de fondu) et `NkAdvanceController` (état fixe `NkAnimControllerRuntime`, un par acteur) | `NkAnimMix.h` ; graphe d'états partagé `Components/NkStateGraphModel.h` |

**Le pont à écrire (seul travail restant)** — ✅ **écrit le 2026-10-02**
(`src/NKScena/NKScenaPont.{h,cpp}` : pistes Transform et plans caméra ; les pistes
`Animation`/NLA passent intactes et restent à MONTRER, §0.3) : la frise ↔ `NkSequence` de Noge, sur le
modèle de `NkFriseDepuisClip` / `NkClipDepuisFrise` d'UnkenyEditor
(`Applications/UnkenyEditor/src/Editeur/NkEditeurPagesAnim.cpp`) : une piste
`Animation` de Noge = une piste de clips de la frise ; `clipHandle` ↔ nom par
`anim::NkClipRegistry`.

---

*Mettre ce fichier à jour dès qu'une des briques du §3 change d'état. Une feuille
de route qui décrit un état révolu est pire que pas de feuille de route : elle se
lit avec confiance.*
