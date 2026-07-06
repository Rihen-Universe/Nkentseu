# Software — toute la scène vire au cyan (couleur de sommet packée uint lue comme float → NaN)

- **Catégorie** : Backends-Rendu
- **Sévérité** : majeur (fidélité + perf)
- **Date** : 2026-07-06
- **Statut** : résolu

## Symptôme

Backend Software (via NKRenderer, `renderdemo --demo=2 -bsw`) : **toute la scène tire vers le cyan/teal** — sol cyan (au lieu de gris), sphères toutes cyan/bleu (au lieu d'un dégradé varié), colonnes teal. Le **canal ROUGE** est quasi absent partout. Bonus : rasterisation anormalement lente (le math NaN est lent sur x86).

## Cause racine

Le format de sommet des meshes moteur **`NkVertex3D`** (stride **56**) stocke la couleur en **uint RGBA packé** (4 octets) à l'offset 52 — PAS en `vec4` de floats. Le fixed-function 3D software lisait ces 4 octets **comme un float** :

```cpp
const float cr=g(52,1.f), ...;   // BUG : lit l'uint comme float
```

Le bitpattern d'un uint couleur (ex. blanc opaque `0xFFFFFFFF`) interprété en `float` IEEE-754 est un **NaN**. Donc `vcol.r = NaN` → `albedo.r = vcol.r * tint.r = NaN` → toutes les contributions d'éclairage sur R (`ambient`, diffus, spéculaire de chaque lumière) deviennent NaN → `litR = NaN` → écrit `(uint8)(NaN*255) = 0`. Résultat : **rouge tué sur tout objet**, scène cyan. (Le vert/bleu survivaient car `g(56)`/`g(60)` dépassaient le stride 56 et retournaient le défaut 1.0.)

Diagnostic décisif : `[FLOOR-DIAG] vcol=(nan,1.000,1.000) tintRAW=(0.700,0.700,0.700)` puis, après fix, `stride=56 vcol=(1,1,1) albedo=(0.120,0.120,0.130) lit=(0.378,0.366,0.368)` (gris neutre).

## Fix

`NkSoftwareDevice::CreateShader`, vertFn du fixed-function 3D : **dépaqueter** l'uint couleur au lieu de le lire en float.

```cpp
float cr=1.f,cg=1.f,cb=1.f,ca=1.f;
if (52u+4u <= stride) { uint32 col; memcpy(&col, v+52, 4);
    cr=(col&0xFFu)/255.f; cg=((col>>8)&0xFFu)/255.f; cb=((col>>16)&0xFFu)/255.f; ca=((col>>24)&0xFFu)/255.f; }
```

(Le format NKRenderer 68 o à `vec4` float n'est pas utilisé par ces meshes.)

## Vérification

Sol : **(27,184,183) teal → (193,184,183) gris**. Sphères : dégradé varié correct (cyan/bleu/magenta/rose/blanc/jaune). Cubes instanciés arc-en-ciel, colonnes blanches, halo du point light rouge visible sur le sol. Composition et couleurs **fidèles à la référence OpenGL**. FPS légèrement amélioré (plus de math NaN).

## Piège annexe (build)

Pendant le débogage, le **binaire ne se relinkait pas** (exe périmé malgré « Build Successful ») — les fixes ne tournaient pas. Contournement : **supprimer l'exe avant `jenga build`** pour forcer le relink, et vérifier le timestamp de l'exe vs source.

## Règle générale

Attention aux **couleurs de sommet packées** (uint RGBA) vs vec4 float selon le format de vertex. En software, toujours dépaqueter selon le format réel. Un **NaN dans une couleur** se propage silencieusement et tue un canal (souvent lu comme 0) — surveiller les canaux « éteints ».

## Liens

- Correctif : `Kernel/Runtime/NKRHI/src/NKRHI/Software/NkSoftwareDevice.cpp` (vertFn fixed-function 3D).
- Format : `NkVertex3D` (stride 56, couleur uint @52) vs format packé 68 o (vec4 @52).
