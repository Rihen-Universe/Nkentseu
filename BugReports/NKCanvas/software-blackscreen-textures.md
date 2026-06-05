# NKCanvas Software — Écran noir (résolu) puis textures/texte absents (ouvert)

- **Catégorie** : NKCanvas (backend Software / rasterizer CPU)
- **Sévérité** : élevée
- **Date** : 2026-06-05
- **Statut** : **partiel** — écran noir RÉSOLU ; textures/texte ENCORE absents

## Bug 1 — Écran noir (RÉSOLU)

### Symptôme
Backend Software : écran entièrement **noir**, rien ne s'affiche.

### Cause racine
**Identique au bug blanc de Vulkan** : `NkSoftwareRenderer2D::BeginBackend()` /
`EndBackend()` étaient **vides**, donc `NkSoftwareContext::BeginFrame()` (qui fait
`mBackBuffer.Clear(25,25,25)`) n'était **jamais appelé** → le back-buffer CPU
n'est ni alloué/cleared par frame, les draws ne landent pas, et `Present()`
(BitBlt) recopie un buffer noir.

### Solution
Piloter le cycle de frame dans les hooks backend (comme Vulkan) :
```cpp
void NkSoftwareRenderer2D::BeginBackend() { if (mCtx) mCtx->BeginFrame(); }
void NkSoftwareRenderer2D::EndBackend()   { if (mCtx) mCtx->EndFrame(); }
```
→ Les **formes vectorielles** (rects, cercles, lignes) s'affichent désormais.

Fichier : `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Software/NkSoftwareRenderer2D.cpp`.

## Bug 2 — Textures et texte ne rendent pas (OUVERT)

### Symptôme
Après le fix du Bug 1 : les **formes** s'affichent, mais **toute texture**
(images) et **tout texte** (atlas de police) restent **invisibles**.

### Analyse statique (code correct — bug runtime)
Tout semble correct à la lecture :
- `FontAtlas::Init` construit l'atlas en **RGBA8 blanc + alpha=couverture** et
  l'upload via **`NkTexture::LoadFromImage`** (PAS Create+Update) →
  `LoadFromImage` **copie les pixels dans `mCPUPixels`** (cf.
  `NkTexture.cpp` : `mCPUPixels.Resize(byteCount); memcpy(...)`).
- Le rasterizer Software (`RasterizeTriangle`) lit
  `tex->GetCPUPixels()` / `GetWidth()` / `GetHeight()` (lignes ~357-360) et
  échantillonne correctement : `texel.rgb * vertexColor`, `alpha = texel.a *
  vertexAlpha`, puis `BlendPixel` (alpha blend) — exactement le rendu attendu.

Donc en théorie le texte/textures devraient s'afficher. Le bug est **runtime**,
pas statique.

### Pistes à investiguer (prochaine session)
1. **Logger à l'exécution** dans `RasterizeTriangle` : pour un groupe avec
   `group.texture != null`, vérifier `tex->IsValid()` et
   `tex->GetCPUPixels() != null` et `texW/texH`. Si `GetCPUPixels()` est **null**
   → `mCPUPixels` n'a pas été peuplé pour CETTE texture (alors qu'OpenGL/Vulkan
   l'uploadent en GPU et marchent) → vérifier le chemin `LoadFromImage` /
   `NkImage::Pixels()` sur le path Software.
2. Si `texPix` est valide mais le texte reste invisible : vérifier l'ordre de
   canaux de `NkImage::Pixels()` (RGBA vs BGRA) et la valeur de `tp[3]` (alpha
   couverture) effectivement lue.
3. Vérifier que `group.texture` est bien renseigné dans les `NkBatchGroup` côté
   Software (le batcher pourrait perdre la texture entre `DrawVertices` et
   `SubmitBatches`).
4. Comparer : OpenGL sample la MÊME donnée d'atlas en GPU et rend le texte → la
   donnée est bonne ; isoler donc ce qui diffère côté CPU.

## Liens
- `Kernel/Runtime/NKCanvas/src/NKCanvas/Backend/Software/NkSoftwareRenderer2D.cpp` (BeginBackend/EndBackend, RasterizeTriangle, texture backend no-op IDs)
- `Applications/Pong/src/Pong/Render/FontAtlas.cpp` (création atlas RGBA + LoadFromImage)
- `Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkTexture.cpp` (LoadFromImage → mCPUPixels)
- Bug jumeau : [vulkan-rendering-white-yflip-srgb.md](vulkan-rendering-white-yflip-srgb.md) (même cause BeginFrame pour le noir/blanc)
