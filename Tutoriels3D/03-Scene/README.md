# Étape 3 — Une vraie scène 3D : caméra, lumière, ombres

> **Cible jenga** : `Tuto03Scene` · **Source** : [main.cpp](main.cpp) (~240 lignes)
> **Nouveaux systèmes** : NkRender3D · NkCamera3D · NkLightDesc · NkMeshSystem · ombres (VSM)
> **Prérequis** : [Étape 2](../02-Renderer/README.md) — device + renderer + boucle de rendu

C'est ici que la 3D commence : trois objets (un sol, un cube doré qui tourne,
une sphère), un soleil qui projette de **vraies ombres**, une caméra, et le
panneau de texte de l'étape 2 par-dessus. Tout tient dans la boucle de rendu
que vous connaissez déjà — on ne fait qu'ajouter des choses entre `BeginFrame`
et `Present`.

## Ce que vous saurez faire à la fin

- décrire une caméra 3D (fov, aspect, near/far) et la placer ;
- déclarer une lumière directionnelle avec ombres ;
- soumettre des objets avec `NkDrawCall3D` : mesh + transform + matériau PBR ;
- composer des transformations (`Translate * Rotation * Scale`) ;
- éviter les deux pièges classiques des ombres (vibration, cache).

---

## 1. La caméra : une description, une position, une cible

```cpp
NkCamera3DData camData;
camData.up        = {0.f, 1.f, 0.f}; // Y vers le haut
camData.fovY      = 60.f;            // champ de vision vertical, en degrés
camData.aspect    = (float32)W / (float32)H;
camData.nearPlane = 0.1f;            // plans de clipping
camData.farPlane  = 100.f;

NkCamera3D cam(camData);
cam.SetPosition({3.5f, 2.5f, 4.5f}); // d'où on regarde
cam.SetTarget({0.f, 0.5f, 0.f});     // ce qu'on regarde
```

Deux détails qui comptent :
- **`aspect` utilise W/H courants** — comme on recalcule la caméra chaque
  frame, le redimensionnement de la fenêtre ne déforme jamais l'image ;
- **near/far** bornent la profondeur : un `nearPlane` trop petit (0.0001)
  détruit la précision du depth buffer → z-fighting.

La caméra est **fixe** dans cette étape ; elle devient interactive (orbite à
la souris) à l'étape 4 — et vous verrez qu'on ne changera que deux lignes ici.

## 2. Le contexte de scène et le soleil

Chaque frame, on décrit la scène au renderer : caméra, temps, lumières.

```cpp
NkSceneContext sctx;
sctx.camera = cam;
sctx.time = total;                 // secondes depuis le lancement (animations)
sctx.ambientIntensity = 0.15f;     // lumière ambiante résiduelle

NkLightDesc sun;
sun.type      = NkLightType::NK_DIRECTIONAL;  // soleil : direction, pas de position
sun.direction = {-0.4f, -1.f, -0.3f};         // vient d'en haut à droite
sun.color     = {1.f, 0.95f, 0.85f};          // blanc chaud
sun.intensity = 3.f;
sun.castShadow    = true;
sun.shadowStatic  = false;                    // voir le piège ci-dessous
sctx.lights.PushBack(sun);

r3d->BeginScene(sctx);   // la frame 3D commence
```

### ⚠️ Les deux pièges des ombres (vécus dans ce tutoriel)

1. **`shadowStatic = true` avec des objets qui bougent** : le cache d'ombre
   n'est rendu qu'une fois — l'ombre du cube resterait figée pendant que le
   cube tourne. Le cache ne convient qu'aux scènes 100 % immobiles.
2. **Ombres qui « vibrent » quand la caméra bouge** : par défaut la zone
   couverte par l'ombre directionnelle est recadrée sur la caméra ; chaque
   mouvement re-échantillonne la shadow map → scintillement des bords. Le
   moteur a un mode qui ancre le cadrage sur les objets du monde :

```cpp
if (auto *shadow = renderer->GetShadow())
    shadow->GetConfig().autoFitDirectional = true;   // une fois, à l'init
```

## 3. Les objets : le `NkDrawCall3D`

Pas de « scène graph » à ce stade : chaque frame, on **soumet** chaque objet.
Un objet = un mesh + une matrice de transformation + un matériau.

```cpp
// Le cube doré, posé au centre, en rotation continue :
NkDrawCall3D dc;
dc.mesh      = meshCube;
dc.transform = NkMat4f::Translate({0.f, 0.5f, 0.f}) *
               NkMat4f::RotationY(NkAngle::FromRad(total * 0.8f)) *
               NkMat4f::Scale({0.8f, 0.8f, 0.8f});
dc.aabb      = {{-0.6f, -0.1f, -0.6f}, {0.6f, 1.1f, 0.6f}}; // boîte englobante (culling + ombres)
dc.tint      = {1.f, 0.8f, 0.3f};  // couleur de base : or
dc.metallic  = 1.f;                // matériau PBR : métal…
dc.roughness = 0.2f;               // …assez poli
r3d->Submit(dc);
```

### Lire une composition de matrices

`Translate * RotationY * Scale` s'applique **de droite à gauche** au mesh :
d'abord l'échelle (cube de 0.8), puis la rotation (sur lui-même), enfin la
translation (posé en hauteur 0.5). Inverser l'ordre donnerait un cube qui
tourne AUTOUR de l'origine au lieu de tourner sur lui-même — essayez, c'est
le meilleur moyen de comprendre.

### Le matériau PBR en deux curseurs

| | `metallic = 0` | `metallic = 1` |
|---|---|---|
| `roughness → 0` | plastique brillant | miroir / chrome |
| `roughness → 1` | craie, tissu | métal brossé sombre |

Le sol du tutoriel : `metallic 0, roughness 0.9` (mat). Le cube : `1 / 0.2`
(or poli). La sphère : `0 / 1.0` (craie blanche).

### Les primitives sont fournies

```cpp
NkMeshHandle meshCube   = renderer->GetMeshSystem()->GetCube();
NkMeshHandle meshSphere = renderer->GetMeshSystem()->GetSphere();
NkMeshHandle meshPlane  = renderer->GetMeshSystem()->GetPlane();
```

Aucun fichier à charger : le `NkMeshSystem` génère cube/sphère/plan à la
demande (et les partage entre tous les utilisateurs). À l'étape 5, on créera
nos **propres** meshes sommet par sommet.

### Le sol : récepteur d'ombres, pas caster

```cpp
dc.castShadow = false;   // le sol REÇOIT les ombres mais n'en projette pas
```

Un plan de 24 m qui projette une ombre = toute la scène à l'ombre de
lui-même, et une shadow map gaspillée. Les récepteurs purs coupent leur
`castShadow`.

## 4. L'ordre d'une frame complète

```
BeginFrame
 ├─ BeginScene(sctx)          caméra + lumières de la frame
 │   ├─ Submit(sol)
 │   ├─ Submit(cube)
 │   └─ Submit(sphère)
 ├─ BeginOverlay … EndOverlay panneau de texte 2D (étape 2)
 ├─ Present
 └─ EndFrame
```

Le renderer se charge du reste : rendu des shadow maps, tri, PBR, tone
mapping. Vous décrivez, il dessine.

---

## Compiler et lancer

```bat
cd D:\Projets\2026\Nkentseu\Nkentseu
jenga build --target Tuto03Scene --config Release
.\Build\Bin\Release-Windows\Tuto03Scene\Tuto03Scene.exe
```

Vous devez voir : un cube doré qui tourne sur un sol gris, une sphère blanche
mate à sa droite, leurs deux ombres portées au sol, et le panneau de texte en
haut à gauche. `Échap` quitte.

Sous Linux (WSL2) : lancer **depuis la racine du repo** (résolution des
shaders) et préfixer `MESA_GL_VERSION_OVERRIDE=4.6` sous WSLg.

## Pour aller plus loin (exercices)

1. Ajoutez un deuxième cube à gauche (`Translate({-1.8f, 0.5f, 0.f})`).
2. Faites **orbiter** la sphère autour du cube :
   `Translate({cos(total)*1.8f, 0.5f, sin(total)*1.8f})`.
3. Passez la sphère en chrome (`metallic 1, roughness 0.05`) et comparez.
4. Changez `sun.direction` et observez les ombres suivre.

## Dépannage

| Symptôme | Cause probable |
|---|---|
| Objets noirs | Aucune lumière dans `sctx.lights`, ou intensité nulle |
| Pas d'ombres | `sun.castShadow` faux, ou le sol a `castShadow=false` **et** rien d'autre ne les reçoit |
| L'ombre du cube ne bouge pas | `shadowStatic = true` (cache) avec un objet animé |
| Ombres qui scintillent quand on bougera la caméra (étape 4) | `autoFitDirectional` non activé |
| Objet qui disparaît de l'ombre ou du rendu | `dc.aabb` trop petite (culling) — elle doit englober le mesh transformé |

**Étape suivante → [04-Camera](../04-Camera/README.md)** : la caméra devient
interactive (orbite, pan, zoom, WASD) — et le code d'entrées part dans son
propre fichier.
