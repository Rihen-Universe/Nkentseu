# DX11 — Input layout : format d'attribut faux (indexation enum) → dernier triangle UV=(0,0)

- **Catégorie** : DirectX11
- **Sévérité** : majeur
- **Date** : 2026-06-05
- **Statut** : résolu

## Symptôme

Sur DX11 uniquement (DX12/GL/Vulkan OK), la texture d'un objet est correcte SAUF sur
le **dernier triangle** de son vertex buffer, qui s'affiche comme si toutes ses UV
valaient **(0,0)** :
- Un **plan** (sol) non-indexé de 6 sommets : le 2e triangle (T2) est noir/figé sur
  le coin (0,0) de la texture, le damier y est étiré/effondré.
- Un **cube** de 36 sommets : seul **un coin d'une face** (le dernier triangle) est
  touché — d'où "certaines faces ont l'artefact, pas toutes".
- Indépendant de l'angle de vue, de l'ombre, du slot de draw (UBO/descriptor), de la
  taille du triangle. Suit la **géométrie**, pas l'orientation.

## Contexte

- Backend DX11, démo `NkRHIDemoFullImage` (et `NkRHIDemoFullSL`).
- Vertex interleavé `Vtx3D { vec3 pos; vec3 normal; vec3 color; vec2 uv; }` (stride 44),
  layout `POSITION(RGB32,0) NORMAL(RGB32,12) COLOR(RGB32,24) TEXCOORD(RG32,36)`.

## Cause racine

`NkDirectX11Device::CreateGraphicsPipeline` construisait le `D3D11_INPUT_ELEMENT_DESC`
avec une **table de formats indexée par la valeur brute de l'enum** :
```cpp
static const DXGI_FORMAT fmtTable[] = { R32_FLOAT, R32G32_FLOAT, R32G32B32_FLOAT, ... };
elems[i].Format = fmtTable[ NkMin((uint32)a.format, 15u) ];
```
Or `NkVertexFormat = NkGPUFormat` (alias), et dans l'enum `NkGPUFormat` les formats 32-bit
ne valent PAS 0/1/2/3 : `NK_R32_FLOAT=22, NK_RG32_FLOAT=23, NK_RGB32_FLOAT=24,
NK_RGBA32_FLOAT=25` (il y a ~22 formats 8/16-bit avant). Donc `NkMin(23..25, 15) = 15`
→ **`fmtTable[15] = DXGI_FORMAT_R32G32B32A32_SINT` pour TOUS les attributs vertex**.

Conséquence : l'Input Assembler lisait **16 octets par attribut** au lieu de 8/12.
- Les formats 32-bit passent les bits bruts ; `float3 pos` / `float2 uv` ne prennent que
  les 2-3 premiers composants → pour la plupart des sommets, les octets en trop tombent
  dans le sommet suivant (dans le buffer) et sont ignorés → **ça "marche par chance"**.
- Mais pour le **dernier sommet**, lire 16 octets à l'offset UV **dépasse la fin du
  vertex buffer**. DX11 renvoie alors **0** pour l'élément → UV lue = (0,0).
- La position (offset 0) marche partout car `float3` ignore le 4e composant et l'offset 0
  a toujours de la marge.

C'est pour ça que **seul le dernier triangle** casse, et que la **position monde
s'interpole parfaitement** (le rasterizer et les varyings sont sains — c'est le *fetch*
de l'attribut qui est faux).

## Solution

Utiliser la vraie fonction de conversion `ToDXGIFormat(NkGPUFormat)` (déjà présente,
switch-case complet) au lieu du `fmtTable` indexé :
```cpp
// NkDirectX11Device.cpp, CreateGraphicsPipeline, boucle input layout :
elems[i].Format = ToDXGIFormat(a.format);   // au lieu de fmtTable[min(a.format,15)]
```
(et supprimer le `fmtTable` devenu inutile.)

## Vérification — protocole de diagnostic (gardé, très efficace)

Le bug était trompeur (ressemblait à de l'aliasing de minification / clamp). La séquence
de tests qui l'a isolé, du plus au moins informatif :
1. **Texture damier procédurale** plaquée au sol → rend l'artefact net (vs texture bruitée).
2. **Cube + plan côte à côte, même texture** → prouve que ce n'est pas que le plan
   (certaines faces du cube aussi) ni l'angle rasant.
3. **Fond magenta** (clear ≠ noir) → distingue "uv=(0,0) → noir" d'un pixel **non rendu**.
   Ici : pas de trou magenta → couverture/rasterization OK, c'est bien uv=(0,0).
4. **Désactiver l'ombre** (`shadow=1`) → écarte l'ombre comme cause du triangle sombre.
5. **Quadrants colorés** (TL rouge / TR vert / BL bleu clair / BR jaune) → montre quel
   coin UV mappe où ; révèle que le coin (0,1) du dernier triangle lit (0,0).
6. **UV-as-color** (`fragColor = vec4(uv,0,1)`) → isole interpolation vs sampling.
7. **World-pos-as-color** (`fragColor = vec4(wp*0.25+0.5,1)`) → DÉCISIF : la position
   monde s'interpole **parfaitement** sur le triangle fautif → ce n'est PAS le rasterizer.
8. **UV calculée depuis la position** (`o.uv = wp.xy` au lieu de `v.uv`) → DÉCISIF : la
   texture devient parfaite → le bug est la **lecture de l'attribut UV**, pas la sortie/varying.

Après fix : plan entièrement texturé, cube correct sous tous les angles, sur DX11.

## Liens

- `Kernel/Runtime/NKRHI/src/NKRHI/DirectX11/NkDirectX11Device.cpp` (CreateGraphicsPipeline
  input layout ; `ToDXGIFormat` ~ligne 1069).
- `Kernel/Runtime/NKRHI/src/NKRHI/Core/NkTypes.h` (enum `NkGPUFormat`, `using NkVertexFormat`).
- Même classe de fragilité à auditer sur les autres backends (table indexée par enum).
