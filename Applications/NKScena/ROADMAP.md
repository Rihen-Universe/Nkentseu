# NKScena — feuille de route

AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
Créé le 2026-09-13.

> ## ⚠️ Ce dossier ne contient AUCUN code, et c'est délibéré
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
| sérialisation de séquence | voir `Engine/Noge/ROADMAP.md` |

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

**Le pont à écrire (seul travail restant)** : la frise ↔ `NkSequence` de Noge, sur le
modèle de `NkFriseDepuisClip` / `NkClipDepuisFrise` d'UnkenyEditor
(`Applications/UnkenyEditor/src/Editeur/NkEditeurPagesAnim.cpp`) : une piste
`Animation` de Noge = une piste de clips de la frise ; `clipHandle` ↔ nom par
`anim::NkClipRegistry`.

---

*Mettre ce fichier à jour dès qu'une des briques du §3 change d'état. Une feuille
de route qui décrit un état révolu est pire que pas de feuille de route : elle se
lit avec confiance.*
