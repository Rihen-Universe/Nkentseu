# Software — grille de sol « vue d'en dessous » + géométrie debug qui masque la scène

- **Catégorie** : Backends-Rendu
- **Sévérité** : majeur (cosmétique bloquant pour la lisibilité)
- **Date** : 2026-07-05
- **Statut** : résolu (grille + debug) ; convention profondeur software : partiellement (voir §Règle)

## Symptômes

Backend **Software** (rasterizer CPU) via NKRenderer (`renderdemo --demo=2 -bsw`) :

1. **Grille de sol infinie mal orientée** : elle apparaît comme si la **caméra était SOUS la grille** (rayons « vers le haut » qui touchent le plan), alors que la caméra est au-dessus et regarde vers le bas. Sensation de grille « inversée sur l'axe Y ».
2. **Un grand plan (magenta, puis rouge/bleu) masque la scène** depuis le centre : les **axes/gizmo de debug** rendus comme d'énormes triangles pleins.

## Contexte

- Backend Software uniquement (GL/VK/DX corrects).
- Découle de la reconstruction de rayon par pixel de la grille (`InfiniteGrid` fixed-function software) et de la topologie **LINE** non gérée par le rasterizer software.
- Convention de profondeur : **NKRenderer force NDC z ∈ [0,1]** (cf. `opengl-ombres-manquantes-glclipcontrol-clipz01.md` : `glClipControl(GL_ZERO_TO_ONE)`), alors que le cœur rasterizer software suppose historiquement `[-1,1]`.

## Cause racine

1. **Grille inversée en Y.** Dans le fragment de grille, l'`ndcy` du pixel était calculé `ndcy = 1 - uv.y*2`. Or le rasterizer software mappe **NDC y = +1 → HAUT de l'écran**, et pour le triangle plein-écran généré (`uv (0,0)(2,0)(0,2)`, `pos = uv*2-1`) `uv.y = 1` correspond au **haut**. Donc en haut de l'écran on obtenait `ndcy = -1` (au lieu de `+1`) → rayon reconstruit **inversé verticalement** → intersection plan du mauvais côté → « caméra sous la grille ».
2. **Plan qui masque = géométrie debug.** Les shaders `DebugLine` / `DebugLineOverlay` (axes/gizmo) sont au format **LIGNE** (pos vec3 @0 + couleur vec4 @12, stride 28). Le rasterizer software ne gère pas la topologie `LINE` → il **remplit les lignes en gros triangles pleins**. En plus, le fixed-function 3D lisait la couleur comme normale et appliquait un **tint magenta** → grandes bandes magenta (puis rouge/bleu) occultant tout.

## Solution

`Kernel/Runtime/NKRHI/src/NKRHI/Software/NkSoftwareDevice.cpp` :

1. **Grille** — corriger l'`ndcy` :
   ```cpp
   // AVANT : float ndcy = 1.f - f.uv.y*2.f;   // inversé
   float ndcx = f.uv.x*2.f - 1.f, ndcy = f.uv.y*2.f - 1.f;   // NDC y=+1 en haut
   ```
2. **Debug** — handler dédié qui NE dessine PAS la géométrie `DebugLine*` en software (sommet clippé `w<0`), en attendant une vraie rasterisation de lignes. Les axes fins rouge/bleu restent fournis par le fragment de la grille (`axXCol`/`axZCol` quand `x≈0` / `z≈0`).

Diagnostic décisif : `SwCurStride()==28` (format des lignes debug) + `[SKY-DIAG]` du fragment géométrie en haut de l'écran (`tint magenta`, `normal (0,0,1)` = couleur lue comme normale).

## Vérification

`renderdemo --demo=2 -bsw` (env `NK_FIX_CAM=1 NK_CAM_DIST=12`) : la grille s'étend correctement vers l'horizon vue **d'en haut**, axes rouge/bleu fins, plus aucun plan occultant. Composition fidèle à la référence OpenGL.

## Règle générale

Le cœur rasterizer software (`NkSWRasterCore.h`) mappe encore la profondeur en supposant NDC z ∈ [-1,1] (`sz = z*invW*0.5+0.5`, near-clip `z+w>=0`). NKRenderer fournit du **[0,1]**. L'ordre de profondeur est préservé (occlusion correcte) car la grille utilise **la même** formule que `Project` (les deux « faux » de façon cohérente). À terme : aligner le rasterizer software sur `[0,1]` (`sz = z*invW`, near-clip `z>=0`) pour la précision — **attention** : chemin partagé avec `NkRHIDemoFull` (vérifier sa convention avant de changer).

## Liens

- Correctif : `Kernel/Runtime/NKRHI/src/NKRHI/Software/NkSoftwareDevice.cpp` (fragment `InfiniteGrid`, handler `DebugLine`).
- Convention profondeur : `opengl-ombres-manquantes-glclipcontrol-clipz01.md` (même dossier).
- Publication + article : `D:/Rihen/Rodolf/Publications/08_2026-07-05_software-rasterizer/`.
