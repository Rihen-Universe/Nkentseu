# Étape 5 — Vos propres meshes et la sélection d'objets

> **Cible jenga** : `Tuto05Meshes` · **Source** : [main.cpp](main.cpp) (~470 lignes)
> **Nouveaux systèmes** : `NkMeshDesc::Simple` · `NkEditMesh` (demi-arête) · lancer de rayon
> **Prérequis** : [Étape 4](../04-Camera/README.md) — caméra interactive + `ConsumeTap`

Dernière étape de la série : on quitte les primitives toutes faites
(`GetCube`, `GetSphere`) pour créer sa **propre géométrie**, de deux façons
très différentes — puis on ajoute une interaction réelle : cliquer/toucher un
objet pour le sélectionner, avec un rayon 3D qui touche vraiment le maillage
(pas juste sa boîte englobante).

## Ce que vous saurez faire à la fin

- construire un mesh GPU à partir d'un tableau de sommets écrit dans le code ;
- comprendre la différence entre un mesh « figé » et un mesh **éditable**
  (structure demi-arête) qu'on peut extruder/subdiviser en direct ;
- convertir un clic écran en rayon 3D ;
- tester ce rayon contre une boîte englobante (rapide) PUIS contre chaque
  triangle (exact) — et savoir pourquoi les deux sont nécessaires.

---

## 1. Un mesh, dans sa forme la plus nue

Un mesh GPU n'est jamais qu'un tableau de sommets + un tableau d'indices.
`CreatePyramidMesh` construit une pyramide à 4 faces triangle par triangle :

```cpp
auto pushTri = [&](NkVec3f a, NkVec3f b, NkVec3f c) {
    NkVec3f n = (b - a).Cross(c - a);          // normale = produit vectoriel des 2 arêtes
    if (n.Len() > 1e-6f) n = n * (1.f / n.Len());

    NkVertex3D vert{};
    vert.normal = n; vert.color = 0xFFFFFFFFu;
    vert.pos = a; vert.uv = {0.f, 0.f}; idx.PushBack((uint32)v.Size()); v.PushBack(vert);
    vert.pos = b; vert.uv = {1.f, 0.f}; idx.PushBack((uint32)v.Size()); v.PushBack(vert);
    vert.pos = c; vert.uv = {0.5f, 1.f}; idx.PushBack((uint32)v.Size()); v.PushBack(vert);
};
for (int32 i = 0; i < 4; i++)
    pushTri(base[i], base[(i + 1) % 4], apex);   // 4 faces latérales
pushTri(base[0], base[3], base[2]);              // base, 2 triangles
pushTri(base[0], base[2], base[1]);

NkMeshDesc desc = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(),
                                     v.Data(), (uint32)v.Size(), idx.Data(), (uint32)idx.Size());
return meshes->Create(desc);
```

Deux détails de modélisation qui comptent :

- **Un sommet par face, pas par position** : chaque coin de la pyramide
  apparaît 4 fois dans `v` (une fois par face latérale) au lieu d'une seule,
  parce que **chaque copie porte une normale différente** (celle de sa face).
  C'est ce qui donne des arêtes nettes plutôt qu'un ombrage lissé — même
  technique qu'un cube.
- **La normale se calcule**, elle ne se devine pas :
  `(b-a).Cross(c-a)` normalisé. Inverser `b` et `c` inverse la face (elle
  deviendrait invisible, backface-culling oblige).

> 💡 Le `NkVector<NkVertex3D>` et `NkVector<uint32>` produits ici (`objV[0]`,
> `objI[0]` dans `main`) sont **conservés par l'appelant** : le GPU n'en a
> qu'une copie-cache. On les réutilise en §4 pour la sélection exacte.

## 2. Un mesh qu'on peut ÉDITER : la structure demi-arête

Le tableau plat ci-dessus est très bien pour une forme figée, mais il ne sait
pas répondre à « quelles faces touchent ce sommet ? » ou « extrude cette
face ». Pour ça, le moteur fournit `NkEditMesh` : une structure
**demi-arête** (half-edge, la même famille que Blender/BMesh), qui modélise
explicitement sommets, arêtes et faces (n-gones, pas seulement des
triangles) et leurs relations.

```cpp
static void ResetEditMesh(NkEditMesh &em) {
    NkVertex3D v[4] = { /* les 4 coins d'un quad 1×1, posé à plat */ };
    const uint32 faceStart[2] = {0, 4};       // 1 face : sommets [0..4[
    const uint32 faceVerts[4] = {0, 1, 2, 3};
    em.BuildFromPolygons(v, 4, faceStart, 1, faceVerts);
}
```

`BuildFromPolygons` prend un format CSR (compressed sparse row) classique :
un tableau de sommets, et pour chaque face son point de départ dans
`faceVerts`. Ici, une seule face à 4 sommets.

### Éditer, puis re-trianguler

`NkEditMesh` n'est PAS directement dessinable : le GPU ne comprend que des
triangles. Après chaque édition, on reconstruit le mesh GPU :

```cpp
static NkMeshHandle UploadEditMesh(NkMeshSystem *meshes, NkEditMesh &em, NkMeshHandle old,
                                   NkVector<NkVertex3D> &outV, NkVector<uint32> &outIdx) {
    if (old.IsValid()) meshes->Release(old);   // l'ancien mesh GPU est obsolète
    em.RecomputeNormals();
    NkVector<NkEmId> triFace;                   // relie chaque triangle à sa face d'origine
    em.Triangulate(outV, outIdx, triFace);
    NkMeshDesc desc = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(),
                                         outV.Data(), (uint32)outV.Size(), outIdx.Data(), (uint32)outIdx.Size());
    return meshes->Create(desc);
}
```

Et les touches d'édition, dans le callback clavier :

```cpp
if (k == NkKey::NK_E) {
    editMesh.SelectAll();               // sélectionne TOUTES les faces…
    NkExtrudeParams p; p.offset = 0.35f;
    if (editMesh.ExtrudeSelectedFaces(p))
        meshEdit = UploadEditMesh(meshes, editMesh, meshEdit, objV[1], objI[1]);
} else if (k == NkKey::NK_C) {
    editMesh.SelectAll();
    if (editMesh.SubdivideSelectedFaces())
        meshEdit = UploadEditMesh(meshes, editMesh, meshEdit, objV[1], objI[1]);
} else if (k == NkKey::NK_R) {
    ResetEditMesh(editMesh);            // repart du quad d'origine
    meshEdit = UploadEditMesh(meshes, editMesh, meshEdit, objV[1], objI[1]);
}
```

`E` extrude toutes les faces sélectionnées le long de leurs normales
(un quad devient une boîte), `C` les subdivise, `R` réinitialise. C'est
exactement le vocabulaire d'un Edit Mode façon Blender — parce que c'est la
même structure de données dessous.

## 3. Convertir un clic écran en rayon 3D

Pour savoir ce que l'utilisateur vise, il faut transformer un pixel (x, y)
en une **demi-droite** partant de la caméra dans la scène :

```cpp
NkVec3f fwd = (tg - ro).Normalized();          // direction regardée
NkVec3f rgt = fwd.Cross({0,1,0}).Normalized(); // "droite" caméra
NkVec3f upv = rgt.Cross(fwd);

const float32 thY = math::NkTan(fovY * 0.5f * kDegToRad);  // demi-tangente verticale
const float32 thX = thY * aspect;                            // … et horizontale

const float32 nx = px / W * 2.f - 1.f;   // pixel -> [-1, 1] horizontal
const float32 ny = 1.f - py / H * 2.f;   // pixel -> [-1, 1] vertical (Y inversé : écran vs monde)

NkVec3f rd = (fwd + rgt * (nx * thX) + upv * (ny * thY)).Normalized();
```

C'est la même géométrie que la matrice de projection de la caméra, mais
« à l'envers » : au lieu de projeter un point 3D vers l'écran, on part d'un
pixel et on reconstruit la direction qui y mène. Les demi-tangentes
(`tan(fov/2)`) écartent le rayon proportionnellement au champ de vision.

## 4. Sélectionner : boîte englobante D'ABORD, triangles ENSUITE

Deux tests successifs, pour deux raisons différentes :

```cpp
for (int32 i = 0; i < 2; i++) {
    NkAABB world = {aabbLocal.min + pos, aabbLocal.max + pos};
    float32 tBox;
    if (!RayHitsAABB(ro, rd, world, tBox))
        continue;                                       // 1) rejet rapide

    float32 t;
    if (RayHitsTriangles(ro, rd, objV[i], objI[i], pos, t) && t < best) {
        best = t; selection = i;                          // 2) confirmation exacte
    }
}
```

1. **`RayHitsAABB`** (test des « slabs ») : très rapide, élimine d'un coup
   tous les objets clairement hors du chemin du rayon.
2. **`RayHitsTriangles`** (Möller–Trumbore) : teste le rayon contre **chaque
   triangle** du mesh et retourne l'intersection la plus proche.

### ⚠️ Pourquoi l'AABB seule ne suffit PAS

Une boîte englobante est, par définition, plus grande que l'objet qu'elle
contient — surtout pour une pyramide (la boîte est un pavé, l'objet un
triangle en coupe) ou un mesh fraîchement extrudé. Si on s'arrête au test 1,
cliquer légèrement **au-dessus** ou **en-dessous** du modèle — dans un coin
vide de la boîte — sélectionne quand même l'objet. Le test 2 élimine
exactement ce faux positif en vérifiant qu'un triangle **réel** est touché.

C'est pour ça que `CreatePyramidMesh` et `UploadEditMesh` renvoient leurs
tableaux CPU (`objV`/`objI`) : ce sont eux, pas le mesh GPU, la source de
vérité pour `RayHitsTriangles`.

## 5. Le tap, pas le drag

```cpp
float32 tx, ty;
if (camInput.ConsumeTap(tx, ty))
    pickAt(tx, ty);
```

`ConsumeTap` (étape 4) ne renvoie `true` que si le clic/toucher n'a
**presque pas bougé** — un vrai drag fait tourner la caméra, un tap
sélectionne. Les deux gestes cohabitent sans code de désambiguïsation ici :
tout est déjà réglé dans `camera_input.h`.

---

## Compiler et lancer

```bat
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target Tuto05Meshes --config Release
.\Build\Bin\Release-Windows\Tuto05Meshes\Tuto05Meshes.exe
```

Vous devez voir : une pyramide dorée et un quad bleu clair sur le sol.
Cliquez précisément dessus (pas à côté) : l'objet touché se surligne en
jaune et son nom s'affiche. `E`/`C`/`R` sur le mesh éditable pour
extruder/subdiviser/réinitialiser sa topologie.

## Pour aller plus loin (exercices)

1. **Nouvelle forme en dur** : un prisme ou une étoile, même pattern que la
   pyramide — un tableau de sommets, `pushTri` pour chaque face.
2. **Sélection de FACE** : `Triangulate` remplit aussi `outTriFace` (un
   `NkEmId` par triangle, vers sa face n-gon d'origine). Renvoyez l'index du
   triangle touché depuis `RayHitsTriangles` pour savoir **quelle face**
   viser (première brique d'un Edit Mode complet).
3. **Terrain** : générez une grille de sommets avec `pos.y = bruit(x, z)` —
   même fonction `NkMeshDesc::Simple`, juste plus de sommets.
4. **Undo** : `NkEditMesh.h` expose `NkEditHistory` — snapshotez avant
   chaque `E`/`C` et ajoutez `Ctrl+Z`.

## Dépannage

| Symptôme | Cause probable |
|---|---|
| Rien ne se sélectionne jamais | `ConsumeTap` ne renvoie jamais `true` — vérifiez `tapThreshold` (étape 4) |
| Ça sélectionne à côté de l'objet | Le test triangle est absent/cassé — l'AABB seule est trop permissive (voir §4) |
| `E`/`C` ne changent rien à l'écran | `UploadEditMesh` non rappelé après l'édition, ou l'ancien handle jamais relâché (fuite) |
| Le mesh édité a des normales bizarres après extrusion | `RecomputeNormals()` oublié avant `Triangulate` |
| Une face entière disparaît | Sommets dans le mauvais ordre (face orientée vers l'intérieur → backface-culling) |

---

🏁 **Fin de la série.** Vous savez ouvrir une fenêtre, initialiser le GPU,
bâtir une scène éclairée avec ombres, la parcourir à la souris/au tactile, et
créer/éditer votre propre géométrie avec une sélection précise. La suite
logique : matériaux texturés, import de modèles
(voir les loaders OBJ/glTF/FBX du moteur), et NKGui pour une interface riche.
