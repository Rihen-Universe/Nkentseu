# Resources/NKRenderer

Assets externes consommés par NKRenderer à runtime — shaders, textures par
défaut, fonts, environnements IBL.

Cette arborescence existe pour deux raisons :

1. **User overrides** — l'utilisateur peut déposer ici ses propres versions
   des shaders/textures/fonts pour surcharger les défauts du moteur sans
   recompiler. Le loader file-system de NKRenderer regarde ce dossier en
   priorité, puis tombe en fallback sur les shaders embarqués (`raw string`
   compilés dans le binaire).
2. **Bibliothèque de matériaux Unity / Unreal-style** — chaque material
   (PBR, CarPaint, Anime, Toon, Hair, Skin, Glass, etc.) a son propre
   sous-dossier. Les utilisateurs peuvent ajouter de nouveaux materials en
   suivant la même convention de nommage.

## Convention de nommage

Pour un shader nommé `<MaterialName>` à l'étage `<Stage>` (vert/frag/comp),
on a une variante par backend RHI :

```
Resources/NKRenderer/Shaders/<MaterialName>/<Backend>/<material>.<stage>.<ext>
```

| Backend | Sous-dossier | Extension              | Notes                                |
|---------|--------------|------------------------|--------------------------------------|
| OpenGL  | `GL/`        | `.<stage>.gl.glsl`     | GLSL 460 core, layout(binding=N)     |
| Vulkan  | `VK/`        | `.<stage>.vk.glsl`     | GLSL Vulkan : layout(set=N,binding=M)|
| DX11    | `DX11/`      | `.<stage>.dx11.hlsl`   | HLSL Shader Model 5.0                |
| DX12    | `DX12/`      | `.<stage>.dx12.hlsl`   | HLSL SM 6.0+ (root signatures)       |
| Metal   | `MSL/`       | `.<stage>.msl`         | Metal Shading Language               |
| NkSL    | `NkSL/`      | `.<stage>.nksl`        | NkSL custom (transpilable vers tous) |

Exemple complet pour PBR vertex stage :

```
Resources/NKRenderer/Shaders/PBR/GL/pbr.vert.gl.glsl
Resources/NKRenderer/Shaders/PBR/VK/pbr.vert.vk.glsl
Resources/NKRenderer/Shaders/PBR/DX11/pbr.vert.dx11.hlsl
Resources/NKRenderer/Shaders/PBR/DX12/pbr.vert.dx12.hlsl
Resources/NKRenderer/Shaders/PBR/MSL/pbr.vert.msl
Resources/NKRenderer/Shaders/PBR/NkSL/pbr.vert.nksl
```

## Sous-dossiers

```
Resources/NKRenderer/
├── Shaders/
│   ├── PBR/         — Material PBR metallic-roughness standard
│   ├── Anime/       — Toon-shading anime style avec rim lighting
│   ├── CarPaint/    — Multi-layer carpaint avec flake + clearcoat
│   ├── Cloth/       — Cloth ASM avec sheen + subsurface
│   ├── Glass/       — Transparent avec IOR + roughness
│   ├── Hair/        — Marschner anisotropic hair
│   ├── Skin/        — Subsurface scattering preintégré
│   ├── Toon/        — Cel-shading classique 3 bandes
│   ├── Unlit/       — Pas de lighting, just texture * tint
│   ├── Water/       — Surface eau avec foam + caustics
│   ├── Foliage/     — Vegetation avec subsurface + wind
│   ├── Volume/      — Raymarching pour fog / clouds
│   ├── Particles/   — Sprites animés blend additive
│   ├── Render2D/    — Sprites 2D + shapes (overlay UI)
│   ├── Shadow/      — Depth-only pour les shadow maps
│   └── PostProcess/ — Tonemap, Bloom, FXAA, SSAO, ...
├── Textures/
│   └── Defaults/    — White1x1, Black1x1, Normal1x1, Magenta1x1
├── Fonts/           — Polices fallback (ProggyClean, NotoSans...)
└── Env/             — Cubemaps IBL, HDRI, BRDF LUTs précalculées
```

## Status courant

État (mai 2026) : la majorité des shaders existent dans
`Kernel/Runtime/NKRenderer/src/NKRenderer/Shaders/<Material>/<Backend>/` et
sont **embarqués en raw string** dans les sous-systèmes (Render2D, Render3D,
PostProcess, Shadow). Le **loader file-system** qui charge en priorité
depuis `Resources/NKRenderer/Shaders/` est sur la roadmap (Phase D.2c +).

En attendant, pour ajouter un nouveau material custom :
1. Créer le sous-dossier `Resources/NKRenderer/Shaders/MonMat/<Backend>/`
2. Y placer les fichiers shader selon la convention
3. (Une fois le loader implémenté) appeler
   `renderer->GetShaders()->LoadFromFile("MonMat")` pour compiler+enregistrer

## Override d'un shader stock

Pour surcharger un shader fourni par le moteur (ex : utiliser ton propre
PBR), dépose simplement un fichier au bon emplacement :

```
Resources/NKRenderer/Shaders/PBR/GL/pbr.frag.gl.glsl
```

Le moteur le détectera et l'utilisera à la place de l'embarqué.
