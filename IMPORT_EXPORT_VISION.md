# Import / export — ce que Nkentseu doit savoir avaler et recracher

> **Décisions et vision de Rihen, 5 août 2026.** Document suivi par git : il
> doit survivre à un clone neuf. Lié depuis `CLAUDE.md`, complément de
> `CONVENTIONS_FICHIERS.md` (extensions, projet).
>
> Règle du projet qui s'applique ici : **une fonctionnalité naît avec ses
> outils** (`PRINCIPES_CONCEPTION.private.md`). Rien de cette liste ne
> s'affiche dans l'interface avant de fonctionner réellement.

## 1. Les deux gestes d'import, et ce qu'ils font

**Bouton** (« Fichier ▸ Importer… ») **et glisser-déposer** — les deux, jamais
l'un sans l'autre.

Le glisser-déposer se comporte **selon la cible du dépôt** :

| On dépose sur… | Ce qui se passe |
|---|---|
| **le navigateur de projet** | le fichier est **extrait et rangé** : le maillage, les matériaux, le squelette, les textures deviennent des assets distincts, dans le dossier visé — rien n'est ajouté à la scène |
| **la vue 3D** ou **la hiérarchie** | même import (dans le **dossier courant du navigateur**), **puis l'objet est ajouté à la scène** directement |

La différence est le **geste**, pas un réglage : déposer dans une
bibliothèque range, déposer dans une scène place. C'est ce qu'on attend
naturellement des deux endroits.

Un fichier composite (un glTF avec ses matériaux et ses textures) est
**éclaté** en assets Nkentseu à l'import : `.nkmesh`, `.nkmat`, `.nktex`,
`.nkskel`, `.nkanim` — chacun visible et réutilisable seul, comme le fait
Unreal. C'est cela, « extraire les contenus ».

## 2. Formats à l'IMPORT

### Maillages — DÉJÀ ÉCRITS ET CÂBLÉS (`NkMeshSystem::Import`)

glTF / GLB · OBJ · STL · PLY · FBX · DAE · USD / USDA.
Le glTF va jusqu'aux **matériaux PBR complets** (albedo, ORM, normale,
émissif) via `NkGLTFMaterialBridge`. **Rien à écrire — tout à brancher.**

### Images — DÉJÀ ÉCRITS (NKImage, 12 codecs)

PNG · JPEG · BMP · TGA · QOI · HDR · EXR · PPM/PGM · SVG…

### Audio — À BRANCHER (NKAudio décode déjà)

WAV · MP3 · OGG · FLAC · Opus. Un son importé devient un `.nksnd`.

### Vidéo — À BRANCHER (NKMedia lit déjà)

Ce que `NkVideoReader` sait lire (MJPEG, MPEG-1, H.264, VP8/VP9, HEVC selon
l'état de NKMedia — voir sa ROADMAP, les en-têtes la sous-estiment).
Usages : texture animée, référence de tournage, fond de composition.

### SVG — un cas à part : **la 3D depuis du vectoriel**

Le SVG n'est pas qu'une image : ses **contours** sont de la géométrie. À
implémenter : import d'un SVG puis **extrusion en 3D** (épaisseur, biseau),
comme les courbes de Blender. Logo, lettrage, découpe, gabarit technique —
c'est un chemin de modélisation à part entière, pas une décoration.
NKImage décode déjà le SVG (`NkSVGCodec`), et `Noge/IO/NkSVGIO` existe : la
matière est là, l'extrusion est à écrire.

## 3. Formats à l'EXPORT — **rien n'existe encore**

Aucun écrivain de maillage dans le dépôt. À écrire, dans cet ordre :

1. **OBJ** — le plus simple, universel, **vérifiable à l'œil** dans n'importe
   quel autre logiciel. C'est le premier parce qu'il valide toute la chaîne.
2. **glTF / GLB** — celui qui compte vraiment : il emporte **les matériaux**,
   les textures, la hiérarchie, les animations. C'est le format d'échange
   moderne.
3. **STL** — impression 3D (géométrie nue, sans matériau).
4. Le reste selon les besoins réels, jamais « pour la collection ».

## 4. Vision à plus long terme — plan de bâtiment → modèle 3D

**Idée de Rihen (5 août 2026).** Prendre l'**image d'un plan de maison**
(photo, scan, PDF rasterisé), en **extraire le plan** — murs, ouvertures,
cotes, pièces, légendes — et **générer le bâtiment automatiquement** avec les
informations portées par le plan (épaisseurs, hauteurs sous plafond, portes,
fenêtres).

Ce que ça suppose, et pourquoi ce n'est pas pour demain :

- **Vision** : redressement de perspective, binarisation, détection de
  segments (Hough), reconnaissance des symboles normalisés (porte, fenêtre,
  escalier), OCR des cotes et des noms de pièces.
- **Interprétation** : reconstruire une topologie de murs (un trait épais =
  deux faces + une épaisseur), fermer les pièces, déduire les hauteurs.
- **Génération** : extrusion des murs, percement des ouvertures, dalle et
  toiture — la même machinerie que l'extrusion SVG, en plus contraint.

Les briques existent déjà en partie dans le dépôt : NKImage (décodage,
filtres), NKAI (vision, `NKGen` génère déjà des maillages 3D depuis du bruit),
et l'extrusion viendra du chantier SVG. C'est un objectif de **synthèse**,
à attaquer quand SVG→3D et l'éditeur de maillage seront debout.

## 5. Ce que ça débloque tout de suite

Brancher l'import de maillages, c'est **cesser de modéliser sur des cubes**
en attendant que l'éditeur de maillage soit prêt (raison invoquée par Rihen).
C'est le meilleur rapport valeur/effort du moment : sept formats déjà écrits,
un fil à brancher.
