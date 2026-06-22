# 4. NKImage — charger, manipuler et enregistrer des images

> Module **Runtime** : `NKImage`. Couche image CPU du moteur Nkentseu.
> Pré-requis : [NKMemory](01-NKMemory.md) (la règle d'or mémoire est ici **vitale**).
> Suite logique : [NKCanvas](05-NKCanvas.md) (transformer une image en texture pour l'afficher).
> Retour à l'[index des guides](README.md).

---

## 4.1 Introduction

`NKImage` est la brique qui **lit et écrit des fichiers image** dans Nkentseu. Avant de
dessiner un sprite à l'écran (guide [NKCanvas](05-NKCanvas.md)), il faut d'abord faire entrer
les pixels en mémoire : c'est exactement le rôle de `NkImage`.

Sa particularité tient en deux mots : **12 codecs écrits from-scratch**, sans aucune
dépendance externe (pas de libpng, pas de libjpeg, pas de stb installé sur la machine —
tout est dans le moteur). Les formats lisibles sont :

> **PNG · JPEG · BMP · TGA · HDR · EXR · PPM/PGM/PBM · QOI · GIF · ICO · WebP · SVG**

En écriture, NKImage gère : **PNG, JPEG, BMP, TGA, HDR, PPM, QOI** (et un *wrapper* SVG).

La classe centrale est `nkentseu::NkImage`. Elle stocke :

- un buffer de pixels CPU (`Pixels()`),
- des dimensions (`Width()`, `Height()`),
- un **format pixel** (`Format()`), c'est-à-dire le nombre et le type de canaux.

Les formats pixel possibles (`NkImagePixelFormat`) :

| Format | Octets/pixel | Canaux | Usage |
|--------|:---:|:---:|-------|
| `NK_GRAY8`    | 1  | 1 | luminance |
| `NK_GRAY_A16` | 2  | 2 | luminance + alpha |
| `NK_RGB24`    | 3  | 3 | couleur opaque |
| `NK_RGBA32`   | 4  | 4 | couleur + transparence (le plus courant) |
| `NK_RGB96F`   | 12 | 3 | HDR (flottants 32 bits) |
| `NK_RGBA128F` | 16 | 4 | HDR + alpha |

Tout est dans le namespace `nkentseu`. L'include unique de la bibliothèque est :

```cpp
#include "NKImage/NKImage.h"   // tire tous les codecs + la classe NkImage
```

Si tu n'as besoin que de la classe et d'un ou deux codecs, tu peux inclure plus finement :

```cpp
#include "NKImage/Core/NkImage.h"              // la classe NkImage seule
#include "NKImage/Codecs/SVG/NkSVGCodec.h"     // le codec SVG seul
```

---

## 4.2 Charger une image

### 4.2.1 La méthode la plus simple : `Load`

`NkImage` est conçu pour vivre **sur la pile** (en variable locale). On crée l'objet, puis
on appelle `Load(chemin)` qui remplit l'instance :

```cpp
#include "NKImage/Core/NkImage.h"
using namespace nkentseu;

NkImage img;                          // image vide (sur la pile)
if (!img.Load("assets/hero.png")) {
    // échec : fichier introuvable, illisible, format non reconnu…
    return;
}

// À partir d'ici, img est valide.
```

`Load` retourne un `bool` : **toujours tester le retour**. Un chemin erroné, un fichier
corrompu ou un format non supporté renvoie `false` sans planter.

Tu peux aussi forcer le nombre de canaux à la lecture (utile pour garantir du RGBA en
sortie, quel que soit le fichier d'entrée) :

```cpp
img.Load("assets/photo.jpg", 4);     // 0 = canaux natifs, 1..4 = conversion forcée
```

- `0` → garde les canaux natifs du fichier (un JPEG donne du RGB24, un PNG transparent du RGBA32…).
- `4` → convertit toujours en RGBA32 après décodage. **C'est souvent le bon choix** quand
  l'image part ensuite vers le GPU, qui attend du RGBA.

### 4.2.2 Gérer l'échec proprement

Le pattern recommandé teste à la fois le retour de `Load` **et** `IsValid()` :

```cpp
NkImage img;
if (!img.Load("assets/hero.png", 4) || !img.IsValid()) {
    NK_LOG_WARN("Image introuvable ou invalide");
    return false;                     // on n'a rien à charger plus loin
}
```

`IsValid()` renvoie `true` seulement si l'image a des pixels et des dimensions > 0.

### 4.2.3 Accéder aux dimensions, canaux et pixels

Une fois chargée, on interroge l'image :

```cpp
int32  w   = img.Width();        // largeur en pixels
int32  h   = img.Height();       // hauteur en pixels
int32  ch  = img.Channels();     // nombre de canaux logiques (1..4)
int32  bpp = img.BytesPP();      // octets par pixel
int32  str = img.Stride();       // octets par ligne (aligné sur 4 — voir note)
const uint8* data = img.Pixels(); // pointeur brut vers le buffer pixel
```

> **Attention au stride.** Le buffer n'est pas forcément `width * bpp` octets par ligne :
> il est **aligné sur 4 octets** (`stride = (w*bpp + 3) & ~3`). Pour parcourir les lignes de
> façon portable, utilise `RowPtr(y)` plutôt que de calculer toi-même l'offset :

```cpp
for (int32 y = 0; y < img.Height(); ++y) {
    const uint8* ligne = img.RowPtr(y);   // début de la ligne y, stride pris en compte
    for (int32 x = 0; x < img.Width(); ++x) {
        const uint8* px = ligne + x * img.BytesPP();
        // px[0]=R, px[1]=G, px[2]=B, px[3]=A (si RGBA32)
    }
}
```

Pour un accès pixel ponctuel (lecture/écriture), des helpers existent aussi :

```cpp
math::NkColor c = img.GetPixel(10, 20);          // lit un pixel (LDR uniquement)
img.SetPixel(10, 20, math::NkColor(255,0,0,255)); // écrit un pixel (rouge opaque)
```

### 4.2.4 Charger depuis la mémoire

Si l'image arrive déjà en RAM (téléchargée, embarquée, lue d'une archive), on charge
directement le buffer **encodé** (le PNG/JPEG brut, pas les pixels décodés) :

```cpp
const uint8* fichierEncode = /* … */;   // octets PNG/JPEG/… bruts
usize        taille        = /* … */;

NkImage img;
if (img.LoadFromMemory(fichierEncode, taille, 4)) {
    // décodé, converti en RGBA32
}
```

Le format est **détecté automatiquement** à partir des *magic bytes* du buffer : pas besoin
de préciser PNG ou JPEG, NKImage reconnaît la signature seul.

---

## 4.3 Le SVG : une image vectorielle rastérisée à la demande

Le SVG est un cas à part : ce n'est pas une image en pixels mais une **description
vectorielle**. NKImage la **rastérise** (la transforme en pixels) à la taille que tu
demandes. L'intérêt est énorme pour le **multi-DPI** : un même fichier `.svg` donne une icône
nette à 48×48 comme à 512×512, sans flou ni crénelage.

### 4.3.1 Décodage direct vers une `NkImage`

Le chemin le plus simple passe par `NkSVGCodec` :

```cpp
#include "NKImage/Codecs/SVG/NkSVGCodec.h"
using namespace nkentseu;

// Rastérise logo.svg à 256×256 pixels.
NkImage* img = NkSVGCodec::DecodeFromFile("assets/logo.svg", 256, 256);
if (img) {
    // … utiliser img->Pixels(), img->Width(), img->Height() …
    img->Free();                 // ⚠️ image *heap* renvoyée par un codec : Free() obligatoire
}
```

> `DecodeFromFile` (et tous les codecs `Decode*`) renvoient un **`NkImage*` alloué sur le
> heap** : c'est toi qui en es propriétaire et tu dois appeler `img->Free()`. C'est différent
> de la `NkImage` sur la pile du §4.2. On y revient en détail au §4.5.

Tu peux passer `0` pour une dimension : NKImage calcule alors la taille manquante en
**préservant le ratio** d'aspect :

```cpp
NkImage* img = NkSVGCodec::DecodeFromFile("assets/logo.svg", 512, 0); // hauteur auto
```

Il existe aussi `NkSVGCodec::Decode(data, size, outW, outH)` pour rastériser depuis un
buffer XML déjà en mémoire.

### 4.3.2 Rastériser plusieurs tailles depuis un même SVG

Si tu as besoin de la **même icône en plusieurs résolutions**, ne re-parse pas le fichier à
chaque fois. Charge le SVG une fois sous forme vectorielle (`NkSVGImage`) puis appelle
`Rasterize` autant de fois que voulu — chaque appel re-rastérise depuis les vecteurs, **sans
perte** :

```cpp
NkSVGImage* svg = NkSVGImage::LoadFromFile("assets/logo.svg");
if (svg) {
    NkImage* petit = svg->Rasterize(48, 48);     // pour un écran 100 % DPI
    NkImage* grand = svg->Rasterize(96, 96);     // pour un écran 200 % DPI
    // … upload des deux vers le GPU …
    petit->Free();
    grand->Free();
    svg->Free();                 // libère la représentation vectorielle
}
```

`NaturalWidth()` / `NaturalHeight()` donnent la taille naturelle (depuis le `viewBox` ou
les attributs `width`/`height` du `<svg>`) si tu veux respecter la taille d'origine.

### 4.3.3 Ce que le codec SVG sait faire… et ne sait pas faire

Le rasteriseur SVG est antialiasé (supersample 2×) et gère beaucoup de choses, mais **pas
tout**. À connaître avant de fournir un fichier :

**Pris en charge :**

- Éléments : `<svg>`, `<g>`, `<path>`, `<rect>`, `<circle>`, `<ellipse>`, `<line>`,
  `<polyline>`, `<polygon>`.
- Cascade `<g>` (transform + style hérités), `transform`, `opacity`.
- `fill`, `stroke`, `stroke-width`, `fill-rule` (nonzero / evenodd).
- Terminaisons de trait : `stroke-linecap` (butt / round / square).
- Jointures : `stroke-linejoin` (miter / round / bevel), `stroke-miterlimit`.
- **Dégradés** : `<linearGradient>` et `<radialGradient>` (stops, `gradientUnits`,
  `gradientTransform`, `spreadMethod` pad/reflect/repeat, `href` pour hériter les stops).
- Couleurs : `#RGB`, `#RRGGBB`, `#RRGGBBAA`, `rgb()`, `rgba()`, et les 148 noms CSS.

**Non pris en charge** (à éviter dans tes `.svg`) :

- `<text>` (le texte n'est **pas** rendu — convertis-le en chemins dans ton éditeur),
- `<use>`, `<defs><style>` (classes CSS), `patterns`, `masks`, `clipPath`, `filters`.

---

## 4.4 Enregistrer une image

### 4.4.1 Sauvegarder par extension

Le plus simple : `Save(chemin)`. Le format est **déduit de l'extension** du fichier.

```cpp
img.Save("sortie/capture.png");          // PNG (lossless)
img.Save("sortie/capture.jpg", 85);      // JPEG, qualité 85 (1..100)
img.Save("sortie/capture.bmp");          // BMP
```

Extensions reconnues par `Save` : `png`, `jpg`/`jpeg`, `bmp`, `tga`, `ppm`/`pgm`, `hdr`,
`qoi`. Le second paramètre (`quality`, défaut 90) ne s'applique qu'au JPEG.

Des variantes explicites existent si tu préfères nommer le format directement :

```cpp
img.SavePNG("out.png");
img.SaveJPEG("out.jpg", 90);
img.SaveBMP("out.bmp");
img.SaveTGA("out.tga");
img.SavePPM("out.ppm");
img.SaveHDR("out.hdr");   // pour les images HDR (RGB96F)
img.SaveQOI("out.qoi");
```

> `SaveGIF`, `SaveWebP` et `SaveSVG` (sur `NkImage`) ne sont **pas implémentés** et renvoient
> `false`. Pour exporter en SVG enrobant des pixels, voir `NkSVGCodec::EncodeToFile` ci-dessous.

### 4.4.2 Encoder en mémoire

Pour obtenir le fichier encodé dans un **buffer** (pour l'envoyer sur le réseau, le stocker
dans une archive, etc.) sans passer par le disque, utilise les méthodes `Encode*`. Elles
remplissent un pointeur `out` et une taille `size` :

```cpp
uint8* out  = nullptr;
usize  size = 0;
if (img.EncodePNG(out, size)) {
    // out / size contiennent le PNG encodé.
    // … utiliser le buffer …
    nkentseu::memory::NkFree(out);   // ⚠️ LIBÉRATION OBLIGATOIRE avec NkFree (voir §4.5)
}
```

Méthodes disponibles : `EncodePNG`, `EncodeJPEG`, `EncodeBMP`, `EncodeTGA`, `EncodeQOI`.

Pour le SVG enrobant une image pixel (round-trip pixel-perfect, l'image est intégrée en PNG
base64 dans un `<image href="data:…">`) :

```cpp
NkSVGCodec::EncodeToFile(img, "out.svg");       // vers un fichier
// ou en mémoire :
uint8* svgOut = nullptr; usize svgSize = 0;
NkSVGCodec::Encode(img, svgOut, svgSize);
nkentseu::memory::NkFree(svgOut);               // même règle de libération
```

---

## 4.5 Gestion mémoire — **la section à ne pas survoler**

NKImage repose sur l'allocateur maison **NKMemory** (voir [guide 1](01-NKMemory.md)). Mélanger
cet allocateur avec le heap standard du C/C++ provoque une **corruption de tas** Windows
(exception `c0000374`). Il y a trois pièges classiques. Apprends-les une bonne fois.

### Règle 1 — Le buffer d'un `Encode*` se libère avec `NkFree`

Le `out` rempli par `EncodePNG`/`EncodeJPEG`/`EncodeBMP`/`EncodeTGA`/`EncodeQOI` (et par
`SaveToMemory`, `NkSVGCodec::Encode`) est alloué via `NkAlloc`. Il se libère **uniquement**
avec `NkFree` :

```cpp
uint8* out = nullptr; usize size = 0;
img.EncodePNG(out, size);

nkentseu::memory::NkFree(out);   // ✅ correct
// std::free(out);               // ❌ heap corruption c0000374
// delete[] out;                 // ❌ heap corruption c0000374
```

### Règle 2 — `NkImage*` renvoyé par un codec/fabrique : `Free()`, jamais `delete`

Toutes les fabriques **statiques** (`NkSVGCodec::Decode*`, `NkImage::Alloc`, `Create`
statique, `Copy`, `Convert`, `Resize`, `Crop`, …) renvoient un `NkImage*` dont **tu es
propriétaire**. Il se détruit avec `img->Free()` :

```cpp
NkImage* img = NkImage::Alloc(64, 64, NkImagePixelFormat::NK_RGBA32);
// … utiliser img …
img->Free();        // ✅ libère les pixels ET le struct NkImage
```

Le piège mortel : **`Alloc()` + `Free()` PUIS `delete img`** = double-free immédiat.
`Free()` libère déjà tout (le wrapper `NkImage` est lui-même alloué via NKMemory). Donc :

```cpp
NkImage* img = NkImage::Alloc(64, 64, NkImagePixelFormat::NK_RGBA32);
img->Free();
delete img;          // ❌ double-free → crash c0000374. NE JAMAIS faire ça.
```

### Règle 3 — Sur la pile : on ne touche pas à `Free()`

À l'inverse, une `NkImage` **locale** (sur la pile, §4.2) ne doit **jamais** recevoir
`Free()` : son destructeur libère les pixels tout seul quand elle sort du scope. Si tu veux
juste la « vider » pour la recharger, utilise `Unload()` :

```cpp
NkImage img;                 // pile
img.Load("a.png");
img.Unload();                // libère les pixels, garde l'objet réutilisable
img.Load("b.png");           // OK
// pas de Free() ici : le destructeur s'en charge à la fin du scope
```

| Origine de l'image | Comment la détruire |
|--------------------|---------------------|
| `NkImage img;` (pile) | rien à faire (destructeur), ou `Unload()` pour recharger |
| `NkImage* p = NkImage::Alloc/...` (heap) | `p->Free()` (jamais `delete`) |
| `NkImage* p = NkSVGCodec::Decode*(...)` | `p->Free()` |
| buffer `out` d'un `Encode*` | `nkentseu::memory::NkFree(out)` |

---

## 4.6 Convention de chemins (desktop vs Android)

Le même code tourne sur desktop et sur mobile, mais la **racine des assets diffère** :

- **Desktop** : les fichiers sont sur le disque, préfixés par ton dossier d'assets, ex.
  `"assets/logo.png"`.
- **Android** : les assets sont empaquetés dans l'APK et lus via l'**AAssetManager**. La
  racine est `""` (chaîne vide) — on passe le chemin relatif sans préfixe.

`NkImage::Load` tente d'ailleurs **automatiquement** l'AAssetManager sur Android si l'ouverture
fichier classique échoue. Le pattern multi-plateforme courant (tiré de `Applications/Mou`) :

```cpp
char full[512];
#if defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_ANDROID)
    std::snprintf(full, sizeof(full), "%s", relPath);          // racine "" sur Android
#else
    std::snprintf(full, sizeof(full), "assets/%s", relPath);   // "assets/" sur desktop
#endif
// puis : img.Load(full, 4)  ou  NkSVGCodec::DecodeFromFile(full, w, h)
```

---

## 4.7 Exemple complet : charger une image et la préparer pour le GPU

Cet exemple réunit les briques précédentes — détection SVG vs raster, conversion RGBA,
gestion mémoire correcte — sur le modèle réel de `Applications/Mou/src/Mou/Assets/MouAssets.cpp`.
Le résultat (`img->Pixels()`, `Width()`, `Height()`) est exactement ce qu'attend une texture
GPU (voir [NKCanvas](05-NKCanvas.md) pour la suite : transformer ces pixels en `NkTexture`).

```cpp
#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/SVG/NkSVGCodec.h"
#include <cstring>
using namespace nkentseu;

// Charge un asset (SVG rastérisé à w×h, ou image bitmap convertie en RGBA32)
// et renvoie une NkImage* heap dont l'appelant fait ->Free() après upload GPU.
NkImage* ChargerAsset(const char* full, int32 w, int32 h) {
    const char* dot   = std::strrchr(full, '.');
    const bool  isSvg = dot && (std::strcmp(dot, ".svg") == 0 ||
                                std::strcmp(dot, ".SVG") == 0);

    NkImage* img = nullptr;
    if (isSvg) {
        // SVG : rastérisé directement à la taille voulue (idéal multi-DPI).
        img = NkSVGCodec::DecodeFromFile(full, w, h);
    } else {
        // Bitmap : on alloue puis on charge en forçant le RGBA32.
        img = NkImage::Alloc(1, 1, NkImagePixelFormat::NK_RGBA32);
        if (img && (!img->Load(full, 4) || !img->IsValid())) {
            img->Free();
            img = nullptr;
        }
    }
    return img;   // l'appelant fait img->Free() après usage (upload GPU)
}

// Utilisation :
//   NkImage* img = ChargerAsset("assets/svg/star.svg", 128, 128);
//   if (img) {
//       backend->UploadTextureRGBA8(id, img->Pixels(), img->Width(), img->Height());
//       img->Free();
//   }
```

---

## 4.8 Manipulations utiles (bonus)

`NkImage` offre aussi des opérations courantes. Les fabriques renvoient un **nouveau**
`NkImage*` (à `Free()`), les manipulations *in-place* modifient l'objet courant :

```cpp
img.FlipVertical();        // retourne l'image (in-place)
img.PremultiplyAlpha();    // pré-multiplie RGB par alpha (RGBA32, avant upload GPU)

NkImage* petit = img.Resize(64, 64);                 // redimensionne -> nouvelle image
NkImage* zone  = img.Crop(10, 10, 32, 32);           // sous-région  -> nouvelle image
NkImage* rgb   = img.Convert(NkImagePixelFormat::NK_RGB24); // change de format
// … usage …
petit->Free(); zone->Free(); rgb->Free();
```

Filtres de `Resize` disponibles (paramètre `NkResizeFilter`) : `NK_NEAREST`, `NK_BILINEAR`
(défaut), `NK_BICUBIC`, `NK_LANCZOS3`.

---

## 4.9 Récapitulatif

- `NkImage` charge/enregistre des images via **12 codecs maison** (PNG, JPEG, BMP, TGA, HDR,
  EXR, PPM, QOI, GIF, ICO, WebP, SVG), zéro dépendance externe.
- **Charger** : `NkImage img; img.Load("assets/x.png", 4);` puis tester `IsValid()`. Lire
  les pixels via `RowPtr(y)` (attention au stride aligné 4).
- **SVG** : `NkSVGCodec::DecodeFromFile(path, w, h)` rastérise à la taille voulue (parfait
  pour le multi-DPI). Gère paths/formes/dégradés/caps/joins ; **pas** le texte ni le CSS.
- **Enregistrer** : `img.Save("out.png")` (format selon l'extension) ou `EncodePNG(out, size)`
  en mémoire.
- **Mémoire (règle d'or)** : un buffer `out` d'`Encode*` → `nkentseu::memory::NkFree`. Un
  `NkImage*` de codec/fabrique → `img->Free()` (**jamais** `delete`, sinon `c0000374`). Une
  `NkImage` sur la pile → ne rien faire (ou `Unload()`).
- **Chemins** : `"assets/…"` sur desktop, `"…"` (AAssetManager) sur Android.

### Dépendances Jenga

Ajoute `NKImage` à ton `nkentseudependson` :

```python
with project("MonJeu"):
    kind("app")
    files(["src/**.cpp"])
    nkentseudependson(
        ["NKImage", "NKMemory", "NKCore", "NKMath", "NKLogger"],
        extra_includes=["src"],
    )
```

Jenga propage transitivement les includes des modules dont NKImage dépend
(NKCore, NKMemory, NKContainers, NKFileSystem, NKMath) — tu n'as donc qu'à déclarer `NKImage`.

---

Étape suivante : afficher concrètement ces pixels à l'écran. Direction le guide
[5. NKCanvas](05-NKCanvas.md) pour transformer une `NkImage` en **texture** et la dessiner.
Pense à relire la [règle d'or mémoire (NKMemory)](01-NKMemory.md) — elle conditionne tout le
reste. ⟵ Retour à l'[index des guides](README.md).
