# NKMath — Roadmap

État actuel (mai 2026) : bibliothèque mathématique livrée et utilisée en
production par NkRenderer. Couvre vecteurs / matrices / quaternions / angles /
couleurs / random / SIMD (skeleton). Reste à durcir le SIMD côté implémentation
batch et à fiabiliser les conversions entre représentations.

---

## 📊 Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Scalaires + constantes (`NkFunctions.h`) | ✅ Livré | — | — |
| Angles (`NkAngle`, `NkEulerAngle`) | ✅ Livré | — | — |
| Vecteurs `NkVec2/3/4T<f/d/i/u>` | ✅ Livré | — | — |
| Matrices `NkMat2/3/4T<f/d>` | ✅ Livré | — | — |
| Quaternions `NkQuatT<f/d>` | ✅ Livré | — | — |
| Géométrie (`NkRange`, `NkRectangle`, `NkSegment`) | ✅ Livré | — | — |
| Couleurs (`NkColor` 8-bit, `NkColorF`, HSV) | ✅ Livré | — | — |
| Random (`NkRandom`, alias `NkRand`) | ✅ Livré | — | — |
| Formatage (`NkMathFormat`) | ✅ Livré | — | — |
| SIMD batch (`NkSIMD`) | 🔶 Partiel | L | Haute |
| Thread-safety `NkRandom` | ❌ TODO | S | Moyenne |
| Tests couverture étendue | 🔶 Partiel | M | Haute |
| Module sœur `NKMathSIMD` séparé | 🔶 Partiel | M | Basse |

Légende : ✅ Livré · 🔶 Partiel · ⏳ En cours · ❌ TODO · 🚫 Abandonné

---

## ✅ Livré

### Scalaires et constantes
- [NkFunctions.h/.cpp](src/NKMath/NkFunctions.h) : `NkSin`, `NkCos`, `NkTan`,
  `NkSqrt`, `NkPowInt`, `NkClamp`, `NkLerp`, `NkSmoothstep`, `NkSmootherstep`,
  `NkToDegrees`, `NkToRadians`, `NkIsPowerOf2`, `NkNextPowerOf2`, `NkClz`,
  `NkCtz`, `NkPopcount`, `NkDivI64` (avec gestion division par zéro)
- Constantes : `PI_F`, `NkPi`, `NkPis2`, `NkEpsilon`, `NkVectorEpsilon`
- Couvert par `tests/test_smoke.cpp` : ScalarFunctions, BitAndIntegerUtilities,
  DivisionAndInterpolationEdges

### Angles
- [NkAngle.h/.cpp](src/NKMath/NkAngle.h) : wrapper avec wrapping automatique
  `(-180°, 180°]`, conversion `Deg()` / `Rad()`, fabriques `FromDeg/FromRad`
- [NkEulerAngle.h/.cpp](src/NKMath/NkEulerAngle.h) : triplet pitch/yaw/roll
  avec détection du pôle de Gimbal

### Vecteurs génériques
- [NkVec.h/.cpp](src/NKMath/NkVec.h) : templates `NkVec2T`, `NkVec3T`,
  `NkVec4T` avec union d'accès flexible (`x/y`, `u/v`, `width/height`, `r/g/b`,
  `data[N]`)
- Alias livrés : `NkVec2f/d/i/u`, `NkVec3f/d/i/u`, `NkVec4f/d/i/u`
- Opérations 2D/3D/4D : arithmétique composante par composante, Dot, Cross
  (3D), Length, Normalize, Lerp, NLerp, SLerp, Reflect, Refract
- Couvert par tests smoke (addition Vec2f)

### Matrices génériques
- [NkMat.h/.cpp](src/NKMath/NkMat.h) : templates `NkMat2T`, `NkMat3T`,
  `NkMat4T` en column-major (compat OpenGL/Vulkan natif), `ToRowMajor()` /
  `AsRowMajor()` pour DirectX
- Fabriques `NkMat4f::Translation/Scaling/RotationY/LookAt/Perspective`
- Inversion, déterminant, transposition, produit matriciel
- Extraction TRS via quaternion intermédiaire (Shoemake 1994) pour éviter le
  gimbal lock

### Quaternions
- [NkQuat.h/.cpp](src/NKMath/NkQuat.h) : `NkQuatT<float32>` / `NkQuatT<float64>`
- Construction depuis angle-axe, Euler, matrice, from-to, LookAt
- Rotation via formule de Rodrigues optimisée (15 multiplications, Vince 2011)
- Produit Hamilton convention GLM (r appliqué APRÈS this)
- SLerp via exponentiation (Shoemake 1985) avec chemin court garanti
- Conversion Euler bidirectionnelle avec détection singularité ±90° yaw

### Géométrie
- [NkRange.h/.cpp](src/NKMath/NkRange.h) : `NkRangeT<T>` intervalle fermé avec
  Contains, Overlaps, Intersect, Union, Split, Shift, Expand, Clamp ; alias
  `NkRange`, `NkRangeInt`, `NkRangeUInt`, `NkRangeFloat`
- [NkRectangle.h/.cpp](src/NKMath/NkRectangle.h) : AABB 2D float32
  (corner + size), Clamp, Corner, Enlarge, `AABB()` enveloppe, test SAT
  (`SeparatingAxis`) ; couvert par tests smoke (`NkRect.Contains`)
- [NkSegment.h/.cpp](src/NKMath/NkSegment.h) : segment 2D bi-point avec union
  d'accès, Length, Project, Equivalent

### Couleurs
- [NkColor.h/.cpp](src/NKMath/NkColor.h) :
  - `NkColor` : RGBA 8-bit (4 bytes) — constructeurs from-hex / from-normalized,
    Lerp, conversions HSV, couleurs nommées statiques (Red, Green, etc.),
    `FromName` lookup
  - `NkColorF` : RGBA float32 (16 bytes) — pour calculs / shaders /
    interpolation continue
  - Conversion bidirectionnelle `NkColor ↔ NkColorF`
  - `NkHSV` : Hue/Saturation/Value pour variations intuitives
  - `RandomRGB` / `RandomRGBA` via NkRandom

### Random
- [NkRandom.h/.cpp](src/NKMath/NkRandom.h) : singleton accessible via
  `NkRandom::Instance()` ou alias global `NkRand`
- API : `NextFloat32`, `NextInt32`, `NextVec3`, `NextColor`, `NextColorA`,
  `NextQuaternion`, etc.
- Backend `rand()` standard (suffisant pour usage jeu)

### Formatage
- [NkMathFormat.h](src/NKMath/NkMathFormat.h) : spécialisations
  `NkFormatter<T>` pour tous les types NKMath (Vec/Mat/Quat/Angle/Color/Range)
- Modes `v/p/c/n` pour NkVec4 (vecteur, position, couleur, normalisé)
- Modes `d/r` pour NkAngle (degrés/radians)
- Délègue à `ToString()` pour les autres types

### Header maître
- [NKMath.h](src/NKMath/NKMath.h) : umbrella header. Inclure ce seul fichier
  suffit pour utiliser tous les types NKMath.

### Build target sœur
- `NKMathSIMD.jenga` : cible séparée pour les compilations vectorielles
  optionnelles (existe en parallèle de `NKMath.jenga`).

---

## 🔄 En cours / TODO immédiat

### SIMD batch (`NkSIMD.h/.cpp`)
- L'infrastructure de détection est livrée : `NK_SIMD_AVAILABLE`,
  `NK_SIMD_HAS_SSE42`, `NK_SIMD_HAS_AVX2`, `NK_SIMD_HAS_NEON`, fallback scalaire
- Types `nk_simd_vec4f` (4 floats) / `nk_simd_vec8f` (8 floats) déclarés
- **Manque** : les implémentations batch effectives
  - Produit matriciel 4×4 vectorisé (cible : SSE4.2 + AVX2 + NEON)
  - Transformation batch de vecteurs (transform N points par une `NkMat4f`)
  - SLerp batch pour particules / animations
  - `NkSqrt`/`NkRsqrt` Newton-Raphson SIMD
  - Trigonométrie SIMD (sin/cos/exp via Chebyshev)
- Effort estimé : L. Priorité : Haute (NkRenderer / animation skeletal
  bénéficieraient directement).

### Thread-safety NkRandom
- Le singleton n'est **pas** thread-safe (doc explicite). Pour usage parallèle :
  soit ajouter un mutex interne (overhead), soit fournir un type
  `NkThreadLocalRandom` per-thread avec seed dérivée. Préférer la 2ᵉ option.

### Tests
- Smoke tests existants couvrent : ScalarFunctions, VectorAndRectTypes,
  BitAndIntegerUtilities, DivisionAndInterpolationEdges
- À ajouter :
  - Matrices : multiplication, inversion, déterminant, fabriques `Translation
    / Scaling / RotationY / LookAt / Perspective`, extraction TRS (gimbal-safe)
  - Quaternions : SLerp chemin court, conversion Euler round-trip,
    gimbal-detection
  - Couleurs : conversion `NkColor ↔ NkColorF`, conversion HSV round-trip,
    `FromName`, `FromHex`
  - SIMD : tests de précision ULP vs scalaire (une fois implémenté)
  - NkRange / NkSegment : opérations géométriques
- Effort estimé : M.

---

## ❌ À venir / À ajouter (futur proche)

### Géométrie 3D manquante
- Pas de `NkAABB3` (boîte alignée 3D) — actuellement NkRectangle est 2D
  uniquement. Nécessaire pour culling 3D, physics broadphase, octree.
- Pas de `NkSphere` ni `NkOBB` ni `NkPlane` ni `NkFrustum` — toutes
  fondamentales pour culling renderer et NkPhysics.
- Pas de `NkRay3` / `NkSegment3` — version 3D du segment livré. Nécessaire pour
  raycast (NkPhysics + GizmoSystem éditeur).

### Interpolation et splines
- `NkCubicBezier`, `NkCatmullRom`, `NkHermite` — animation et trajectoires
- `NkBSpline` (générique k-ordre) pour animation skeletal lissée

### Précision et noise
- Pas de noise procédural (Perlin, Simplex, Worley). Utile pour NKAnimation
  procédurale, génération de terrain, fog volumétrique.
- Pas de générateur Mersenne Twister ni PCG. `rand()` est limité pour
  simulations physiques.

### NKMath ↔ NKMath SIMD path automatique
- Idéalement `NkMat4f::operator*` choisit automatiquement le path SIMD si
  `NK_SIMD_AVAILABLE`. Aujourd'hui l'utilisateur doit appeler `NkSIMD::*`
  explicitement.

### Fixed-point optionnel
- Pour plateformes embarquées / consoles sans FPU performant. `NkFixed16_16`
  reste à concevoir. Effort important, priorité basse.

---

## Bugs / quirks connus

- `NkLegacySystem.h` est un compatibility header — à terme migration complète
  vers `NKCore/NkTypes.h` pour éliminer la duplication des alias `float32` /
  `int32`.
- Les exemples Doxygen de `NKMath.h` mentionnent `NkMat4f::Perspective(...,
  false)` avec un paramètre booléen `[-1,1]` pour OpenGL : à vérifier que
  l'API actuelle expose bien cette signature (sinon ajuster la doc).
- `NkRandom` partage la séquence globale `rand()` — un appel à `srand()`
  externe casse la reproductibilité du singleton.

---

## Dépendances

- **Couches en dessous (utilisées)** : NKPlatform (détection arch, SIMD,
  endianness, export), NKCore (types `nk_float32` / `nk_int32` / `nk_size`,
  traits), NKContainers (`String/NkFormat.h` pour `NkFormatter`)
- **Modules au-dessus qui en dépendent** : NKRHI (matrices upload GPU),
  NKRenderer (Vec/Mat/Quat partout), NKPhysics (Vec/Mat/Quat + AABB futur),
  NKAnimation (Quat + interpolation), NKUI (Color + Rect 2D), NKScene
  (TransformComponent), Noge éditeur (gizmos), PV3DE (Face/Body animation
  pose)
