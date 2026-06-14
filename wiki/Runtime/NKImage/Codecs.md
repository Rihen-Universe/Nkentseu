# Les codecs d'image

> Couche **Runtime** · NKImage · Lire et écrire des images dans **tous les formats** —
> `NkPNGCodec`, `NkJPEGCodec`, `NkBMPCodec`, `NkTGACodec`, `NkQOICodec`, `NkHDRCodec`,
> `NkEXRCodec`, `NkGIFCodec`, `NkICOCodec`, `NkPPMCodec`, `NkWebPCodec`, `NkSVGCodec` —
> autour du type pivot `NkImage`, sans aucune dépendance externe.

Charger une texture, exporter une capture d'écran, lire un *cubemap* HDR pour l'éclairage
ambiant, rasteriser une icône SVG dans une UI : tout cela revient à **décoder un format
d'octets vers des pixels**, ou l'inverse. NKImage écrit ces douze codecs *from scratch* — pas
de `libpng`, pas de `stb_image` lié, pas de `zlib` système : du PNG via un inflate maison, du
JPEG via une IDCT maison, du WebP via un décodeur VP8L maison. La conséquence pratique est
double : la chaîne d'assets ne traîne aucune bibliothèque tierce, et **tous** les codecs
partagent exactement le même contrat de mémoire et les mêmes types. Apprendre l'un, c'est
apprendre les douze.

Le principe est uniforme. Chaque codec est une **classe purement statique** (on ne l'instancie
jamais) qui expose au minimum `Decode` et, le plus souvent, `Encode`. Décoder produit toujours un
`NkImage*` — l'objet pivot du module, décrit en bas de page — ou `nullptr` en cas d'échec.
Encoder remplit soit un **buffer mémoire** que vous devrez libérer, soit directement un **fichier**
sur disque. Toutes ces méthodes sont `noexcept` : un format invalide ne lève jamais d'exception,
il renvoie `nullptr` ou `false`.

- **Namespace** : `nkentseu` (sous-namespace `nkentseu::math` pour `NkColor`/`NkIntRect`)
- **Header parapluie** : `#include "NKImage/NKImage.h"` (inclut `NkImage` + les douze codecs)

---

## La règle de mémoire, avant tout le reste

Avant le premier `Decode`, il faut intérioriser **deux règles de libération** — elles ne sont pas
des détails, elles sont la frontière entre un code qui marche et une *heap corruption* `c0000374`
sous Windows. La cause profonde est simple : NKImage alloue tout via l'allocateur maison NKMemory,
**jamais** via le tas du CRT. Rendre cette mémoire au mauvais gestionnaire (`std::free`, `delete[]`)
écrase des métadonnées qui n'appartiennent pas à ce gestionnaire.

**Première règle — les images.** Toute `NkImage*` qui sort d'un `Decode` (ou d'une fabrique
statique `Create`/`Alloc`) a été allouée par le module : on la rend par `img->Free()`, qui libère
les pixels **et** la structure. Ce n'est **pas** un `delete` : ne jamais `delete` une image
fabriquée, et symétriquement ne jamais `Free()` une `NkImage` posée sur la **pile** (`Free()`
appellerait `nkFree(this)` sur une adresse pile).

**Seconde règle — les buffers d'encodage.** Tout paramètre `uint8*& out` rempli par
`Encode`/`EncodeToMemory`/`SaveToMemory` a été alloué via `nkentseu::memory::NkAlloc`. On le libère
**exclusivement** par `nkentseu::memory::NkFree(out)`. Jamais `std::free`, jamais `delete[]`.

```cpp
uint8* out = nullptr; usize size = 0;
if (NkPNGCodec::Encode(*img, out, size)) {        // out alloué par NkAlloc
    file.Write(out, size);
    nkentseu::memory::NkFree(out);                // la SEULE libération correcte
}
img->Free();                                      // image fabriquée → Free(), pas delete
```

> **En résumé.** Image fabriquée → `img->Free()` (jamais `delete`, jamais sur une image pile).
> Buffer `out` d'un `Encode` mémoire → `nkentseu::memory::NkFree(out)` (jamais `std::free`/`delete[]`).
> Tout écart = corruption de tas `c0000374`.

---

## Les codecs « buffer mémoire » : PNG, JPEG, BMP, TGA, QOI

Le gros des formats matriciels suit le **même patron** : `Decode(data, size)` lit un buffer en
mémoire et rend une `NkImage*`, `Encode(img, out, outSize)` produit un buffer en mémoire. C'est le
patron qu'on veut presque toujours, parce qu'il ne touche pas le disque — on décode depuis un
*asset pack* déjà chargé, on encode vers un flux réseau ou un fichier ouvert par NKFileSystem.

`NkPNGCodec` est le pilier : PNG complet (RFC 2083), décode n'importe quel format entier, encode
tout format pixel entier. Son inflate/deflate vient du `NkDeflate` interne du module. `NkBMPCodec`
couvre le DIB Windows dans toute sa diversité historique (en-têtes v3/v4/v5, profondeurs 1 à 32
bits, indexé, RLE4/RLE8, BITFIELDS) et encode en 24 ou 32 bpp. `NkTGACodec` lit le TARGA
(types 1/2/3, RLE, 15/16 bits étendus en RGB24). `NkQOICodec` est le format *Quite OK Image* :
rapide, sans perte, idéal pour des caches d'assets internes.

`NkJPEGCodec` a deux particularités à retenir. Au décodage, il sort du **grayscale** (`NK_GRAY8`)
ou de la **couleur** (`NK_RGB24`), et il **rejette le JPEG progressif** (renvoie `nullptr`) —
seul le baseline DCT est lu. À l'encodage, il prend un paramètre `quality` dans `[1, 100]`
(défaut 90) et convertit l'image en RGB24/Gray8 si besoin.

```cpp
NkImage* tex = NkPNGCodec::Decode(asset.data, asset.size);   // nullptr si échec
if (tex) {
    uint8* jpg = nullptr; usize n = 0;
    NkJPEGCodec::Encode(*tex, jpg, n, 85);                    // recompresse en JPEG q=85
    nkentseu::memory::NkFree(jpg);
    tex->Free();
}
```

> **En résumé.** PNG/JPEG/BMP/TGA/QOI suivent le patron `Decode(data,size) → NkImage*` /
> `Encode(img, out, outSize) → bool`. JPEG sort `NK_GRAY8`/`NK_RGB24`, refuse le progressif, et
> prend un `quality` à l'encodage. Tout `out` se libère par `NkFree`.

---

## Les codecs « fichier » : HDR, PPM

Deux codecs s'écartent du patron mémoire et écrivent **directement un fichier** : leur `Encode`
prend un `const char* path` plutôt qu'un `uint8*& out`. C'est un piège classique — on cherche le
buffer de sortie, il n'existe pas.

`NkHDRCodec` lit et écrit le Radiance HDR (`.hdr`/`.rgbe`), le format des environnements pour
l'éclairage basé image. Il décode vers du **float32 RGB** (`NK_RGB96F`) : un *cubemap* HDR garde
toute sa plage dynamique. Pour l'encodage il offre **les deux variantes** : `Encode(img, path)`
écrit le fichier RLE compressé, et `EncodeToMemory(img, out, outSize)` produit un buffer mémoire
(à libérer par `NkFree`). Bonus utile : `ConvertToTexture(hdr, exposure, gamma)` *tone-mappe* une
image HDR vers du RGBA8 affichable (gamma ≤ 0 ⇒ pas de correction gamma), en rendant une **nouvelle
image** à libérer par `Free()`.

`NkPPMCodec` couvre le NetPBM (P1 à P6) : `Decode` mappe P1/P4/P2/P5 vers `NK_GRAY8` et P3/P6 vers
`NK_RGB24` ; `Encode(img, path)` écrit un fichier binaire P5 (gris) ou P6 (couleur). C'est le format
de débogage par excellence : trivial à écrire, lisible par n'importe quel outil.

> **En résumé.** HDR et PPM encodent vers un **fichier** (`Encode(img, path)`), pas vers un buffer.
> Pour du HDR en mémoire, utilisez `NkHDRCodec::EncodeToMemory`. HDR décode en float `NK_RGB96F` ;
> `ConvertToTexture` le ramène en RGBA8 affichable.

---

## Le décodage seul : EXR, ICO

Deux formats sont **lecture seule** dans cette version : il n'y a pas d'`Encode`.

`NkEXRCodec` décode l'OpenEXR 1.x *scanline single-part* — le format HDR professionnel. Il gère les
types de pixels HALF/FLOAT/UINT et les compressions NONE/RLE/ZIPS/ZIP (PIZ en bêta) ; il sort du
`NK_RGB96F` (3 canaux ou 1 canal répliqué) ou du `NK_RGBA128F` (4 canaux). Les variantes encore non
gérées (PXR24, B44, DWAA/DWAB, *tiles*, *multipart*) renvoient `nullptr`.

`NkICOCodec` décode l'ICO/CUR de Windows, qui contient souvent **plusieurs résolutions** dans un
même fichier : le codec **sélectionne automatiquement la plus grande**. Les images embarquées en
PNG comme en BMP sont reconnues.

> **En résumé.** EXR et ICO sont **décode seul** (pas d'`Encode`). EXR sort en float
> (`NK_RGB96F`/`NK_RGBA128F`), scanline single-part uniquement. ICO choisit tout seul l'entrée la
> plus grande résolution.

---

## L'animation : GIF

`NkGIFCodec` mérite sa propre section car il gère l'**animation**. Pour la compatibilité, son
`Decode` classique ne rend que la **première frame** (en RGBA32). Mais `DecodeAnimation` rend
**toutes** les frames, déjà composées sur le canvas global, sous forme d'un `NkGIFAnimation*`.

Cette structure porte les dimensions, le `frameCount`, le tableau `frames` et `loopCount`
(0 = boucle infinie, via l'extension NETSCAPE2.0). Chaque `NkGIFFrame` contient son `image` (RGBA32,
taille canvas), son `delayMs`, sa position `left`/`top` et son mode `disposal`. Point capital :
un `NkGIFAnimation` se libère **exclusivement** par `FreeAnimation`, qui détruit la structure **et**
toutes les images des frames — jamais `Free()`/`delete` à la main sur la struct ou ses frames.

Côté écriture, `Encode` produit un GIF89a (quantification médiane-coupure 256 couleurs,
transparence) dans un buffer mémoire, et `Save(img, path)` écrit directement un `.gif`.

```cpp
NkGIFAnimation* anim = NkGIFCodec::DecodeAnimation(data, size);
if (anim) {
    for (uint32 i = 0; i < anim->frameCount; ++i) {
        const NkGIFFrame& f = anim->frames[i];
        Upload(f.image, f.delayMs);          // f.image = RGBA32, taille canvas
    }
    NkGIFCodec::FreeAnimation(anim);         // libère struct + toutes les frames
}
```

> **En résumé.** `NkGIFCodec::Decode` = première frame seulement ; `DecodeAnimation` = toutes les
> frames composées dans un `NkGIFAnimation*`, libéré **uniquement** par `FreeAnimation`. À l'écriture :
> `Encode` (buffer GIF89a) ou `Save(img, path)` (fichier).

---

## Le lossless récent : WebP

`NkWebPCodec` décode et encode le WebP *from scratch*. Au décodage il gère le VP8L (sans perte,
complet) et le VP8 (avec perte, basique). À l'encodage, `Encode(img, out, outSize, lossless, quality)`
écrit du VP8L (sans perte par défaut) dans un buffer mémoire. Toute la machinerie interne (chunks
RIFF, décodeur VP8L, tables de Huffman) est `private` : la surface publique se réduit à `Decode` et
`Encode`.

> **En résumé.** WebP décode VP8L (lossless complet) + VP8 (lossy basique), encode VP8L vers un
> buffer mémoire (`out` → `NkFree`). Reste = interne.

---

## Le vectoriel : SVG et son écosystème

`NkSVGCodec` est à part : il **rasterise** du SVG (un format vectoriel) vers du RGBA32. Ce n'est pas
un parser complet de SVG — il gère les formes géométriques (`<svg> <g> <path> <rect> <circle>
<ellipse> <line> <polyline> <polygon>`) mais **pas** le texte, `<use>`, les styles CSS, gradients,
patterns, masques, *clipPath* ni filtres. `Decode(data, size, outW, outH)` produit l'image rasterisée
(dimensions 0 ⇒ taille naturelle du SVG/viewBox), `DecodeFromFile` lit d'abord le disque.

Attention au sens d'`Encode` : il **n'est pas** une vectorisation. Il enrobe une image matricielle
en `<image href="data:png;base64,…">` à l'intérieur d'un `<svg>` — pixel-perfect, mais ce n'est pas
une conversion bitmap→tracés.

Au-delà de la simple rasterisation, le module expose un **écosystème vectoriel** : pour ceux qui
veulent les formes elles-mêmes (et non leurs pixels), `NkSVGImage` représente le SVG comme une liste
de shapes rasterisable **à la demande** (`Rasterize(w, h)`) **et** triangulable pour le GPU
(`TriangulateAll`). On peut parcourir chaque shape via `NkSVGShapeView` (une vue read-only), lire ses
contours, sa couleur, et la trianguler en *triangle-list* prête à uploader. Les types de support
`NkSVGColor`, `NkSVGStyle` et `NkSVGTransform` décrivent respectivement une couleur (parseur CSS,
148 noms), un style cascadable et une matrice affine 2D.

> **En résumé.** `NkSVGCodec::Decode` rasterise les **formes** SVG (pas texte/CSS/gradients) en
> RGBA32. `Encode` enrobe un bitmap dans un SVG (≠ vectorisation). `NkSVGImage` garde le SVG
> **vectoriel** : `Rasterize` à la demande, `TriangulateAll` pour le GPU.

---

## Le DOM XML sous-jacent : NkXMLParser

Le rasteriseur SVG s'appuie sur un **parser XML maison** que vous pouvez réutiliser directement.
`NkXMLParser::Parse` remplit un `NkXMLDocument` — qu'il faut **`Init()` avant** — dont toute la
mémoire vit dans une **arena** (allocateur linéaire) libérée d'un coup par `Destroy()`. On navigue
ensuite l'arbre par `NkXMLNode` (attributs, enfants, frères), on cherche par `GetElementById`,
`GetElementsByTagName` ou `QuerySelector`. C'est un DOM léger, UTF-8 only, pensé pour configurer,
charger des descripteurs ou parser tout XML d'outillage sans tirer une bibliothèque externe.

> **En résumé.** `NkXMLParser` + `NkXMLDocument` (à `Init()` avant, `Destroy()` après) = un DOM XML
> UTF-8 à arena, réutilisable hors SVG pour tout fichier XML d'outillage.

---

## Aperçu de l'API

### Les douze codecs

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Mémoire | `NkPNGCodec::Decode` / `Encode` | PNG (RFC 2083), tout format entier. |
| Mémoire | `NkJPEGCodec::Decode` / `Encode(…, quality=90)` | JPEG baseline ; sort GRAY8/RGB24, refuse le progressif. |
| Mémoire | `NkBMPCodec::Decode` / `Encode` | DIB v3/v4/v5, 1–32 bpp, RLE/BITFIELDS ; encode 24/32 bpp. |
| Mémoire | `NkTGACodec::Decode` / `Encode` | TARGA types 1/2/3, RLE, 15/16 bpp. |
| Mémoire | `NkQOICodec::Decode` / `Encode` | QOI sans perte. |
| Fichier+mém | `NkHDRCodec::Decode` / `Encode(…, path)` / `EncodeToMemory` / `ConvertToTexture` | Radiance HDR float `NK_RGB96F` ; tone-map RGBA8. |
| Décode seul | `NkEXRCodec::Decode` | OpenEXR scanline single-part → float. |
| Animation | `NkGIFCodec::Decode` / `DecodeAnimation` / `FreeAnimation` / `Encode` / `Save` | GIF87a/89a multi-frame. |
| Décode seul | `NkICOCodec::Decode` | ICO/CUR ; choisit la plus grande résolution. |
| Fichier | `NkPPMCodec::Decode` / `Encode(…, path)` | NetPBM P1–P6. |
| Mémoire | `NkWebPCodec::Decode` / `Encode(…, lossless=true, quality=90)` | WebP VP8L/VP8. |
| Vectoriel | `NkSVGCodec::Decode(…, outW, outH)` / `DecodeFromFile` / `Encode` / `EncodeToFile` | Rasterise les formes SVG → RGBA32. |

### Structures GIF

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Frame | `NkGIFFrame` : `image`, `delayMs`, `left`, `top`, `disposal` | Une frame composée (RGBA32, taille canvas). |
| Animation | `NkGIFAnimation` : `width`, `height`, `frameCount`, `frames`, `loopCount` | Résultat de `DecodeAnimation` (libérer par `FreeAnimation`). |

### Écosystème vectoriel SVG

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Couleur | `NkSVGColor` : `r/g/b/a`, `none`, `Parse`, `Transparent`/`Black`/`White`/`None` | Couleur SVG, parseur CSS (148 noms). |
| Style | `NkSVGStyle` : `fill`, `stroke`, `strokeWidth`, `opacity`, `fillOpacity`, `strokeOpacity`, `fillEvenOdd`, `visible` | Style cascadable par `<g>`. |
| Transform | `NkSVGTransform` : `a..f`, `Identity`/`Translate`/`Scale`/`Rotate`/`Parse`, `operator*`, `Apply` | Matrice affine 2D. |
| Vue shape | `NkSVGShapeView` : `ContourCount`, `Contour{Point}Count`, `ContourXs`/`Ys`, `FillColor`/`StrokeColor`/`StrokeWidth`/`Opacity`/`FillEvenOdd`, `Triangulate`, `IsValid` | Vue read-only sur une shape parsée. |
| Image vecto | `NkSVGImage` : `LoadFromFile`/`LoadFromMemory`, `Rasterize`, `Natural{Width,Height}`, `ShapeCount`, `GetShape`, `TriangulateAll`, `Free` | SVG vectoriel rasterisable/triangulable à la demande. |

### DOM XML (`NkXMLParser.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Parser | `NkXMLParser::Parse` (×2) / `ParseFile` | Remplit un `NkXMLDocument` (déjà `Init()`). |
| Document | `NkXMLDocument` : `Init`/`Destroy`, `GetElementById`, `GetElementsByTagName`, `QuerySelector`, `ForEach`, `arena`, `docElement`, `isValid`, `errorMsg`/`errorLine` | Racine DOM (mémoire en arena). |
| Nœud | `NkXMLNode` : `type`, `tagName`/`localName`/`ns`, `GetAttr`/`GetAttrLocal`/`GetAttrF`/`HasAttr`, `Is`, `FirstChildNamed`/`NextSiblingNamed`, `GetTextContent` | Élément/texte de l'arbre. |
| Attribut | `NkXMLAttr` : `name`/`value`/`prefix`/`localName`/`ns`/`next` | Attribut (liste chaînée). |
| Arena / NS | `NkXMLArena` (`Init`/`InitView`/`Destroy`/`Reset`/`Alloc`/`Dup`), `NkXMLNamespaceCtx` (`Push`/`Resolve`), `enum NkXMLNodeType` | Allocateur linéaire, namespaces, types de nœud. |

### Le type pivot `NkImage` (`Core/NkImage.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Enums | `NkImagePixelFormat`, `NkImageFormat`, `NkResizeFilter` | Format pixel / conteneur / filtre de redimensionnement. |
| Helpers | `ChannelsOf(f)`, `BytesPerPixelOf(f)` `[constexpr]` | Canaux / octets par pixel d'un format. |
| Fabriques | `Create` (×2), `Alloc`, `Wrap`, `ConvertToTexture` | Images owning / vue non-owning / HDR→RGBA8. |
| Cycle de vie | `NkImage()`, `~NkImage`, move ctor/assign (copie supprimée) | Construction/déplacement (non copiable). |
| Charger | `Load`, `LoadFromMemory` (surcharges), overrides `NKIResource` | Depuis fichier / mémoire / flux. |
| Sauver fichier | `Save`, `SavePNG`/`SaveJPEG`/`SaveBMP`/`SaveTGA`/`SavePPM`/`SaveHDR`/`SaveQOI` ; `SaveGIF`/`SaveWebP`/`SaveSVG` (**non impl.**) | Écrit un fichier (extension déduite ou explicite). |
| Encoder mémoire | `EncodePNG`/`EncodeBMP`/`EncodeTGA`/`EncodeQOI`/`EncodeJPEG` | Buffer `out` (→ `NkFree`). |
| Transformer | `Convert`, `Resize`, `Crop`, `Copy`, `CopyAs`, `Blit`, `BlitRegion`, `FlipVertical`/`Horizontal`, `PremultiplyAlpha` | Conversion, redim, recadrage, composition. |
| Dessin CPU | `SetPixel`/`GetPixel`/`BlendPixel`, `Fill`, `DrawLine`/`HLine`/`VLine`, `DrawRect`/`FillRect`, `DrawCircle`/`FillCircle`, `DrawEllipse`/`FillEllipse` | Dessin LDR 8-bit (no-op si HDR). |
| Accès | `Pixels`, `Width`/`Height`/`Channels`/`BytesPP`/`Stride`, `Format`/`SourceFormat`, `IsValid`, `IsHDR`, `TotalBytes`, `RowPtr` | Métadonnées et accès aux pixels. |
| Mémoire | `Free` (pixels + struct), `Unload` (pixels seuls) | `Free` = images fabriquées ; `Unload` = sûr sur pile. |

### Briques internes réutilisables (`Core/NkImage.h`)

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Flux binaire | `NkImageStream` : ctor R/W, `ReadU8/16/32`/`ReadBytes`/`Skip`/`Seek`, `WriteU8/16/32`/`WriteBytes`, `TakeBuffer`, `Tell`/`Size`/`IsEOF`/`HasError` | Buffer binaire endian-aware des codecs. |
| Compression | `NkDeflate` : `Decompress` (zlib), `DecompressRaw` (DEFLATE brut), `Compress` (stored blocks) | inflate/deflate maison (PNG…). |

---

## Référence complète

Chaque élément est repris en détail : son comportement, sa complexité quand elle compte, et ses
usages concrets dans les différents domaines du moteur. Les codecs partageant le même patron sont
décrits ensemble ; ce qui les distingue est mis en avant.

### Le patron commun : `Decode` / `Encode`

Tous les `Decode` ont la signature `static NkImage* Decode(const uint8* data, usize size) noexcept`
(SVG ajoute `outW`/`outH`). Ils renvoient `nullptr` sur tout échec — données tronquées, format non
supporté, signature invalide — **jamais** d'exception. L'image rendue est **owning** : on la libère
par `img->Free()`. Tous les `Encode` mémoire ont la signature
`static bool Encode(const NkImage& img, uint8*& out, usize& outSize, …) noexcept`, allouent `out` via
NkAlloc, et le `false` signale l'échec sans rien allouer.

Domaines d'emploi de ce patron :
- **Rendu / GPU** — décoder une texture depuis un *asset pack* déjà en RAM, puis uploader
  `img->Pixels()` ; pas d'I/O disque dans la boucle de chargement.
- **IO / réseau** — réencoder une capture en PNG/JPEG/QOI vers un buffer envoyé sur le réseau ou
  écrit par NKFileSystem ; le `out` mémoire évite un fichier temporaire.
- **Outils / éditeur** — pipeline d'import d'assets : décoder n'importe quel format en entrée,
  réencoder dans le format interne (QOI rapide, ou PNG portable).

### `NkPNGCodec` — le format pivot

PNG est le format de référence : sans perte, alpha, portable partout. Le décodeur lit tout format
pixel entier (gris, palette, RGB, RGBA, avec ou sans alpha) ; l'encodeur accepte tout format pixel
entier en entrée. Sa compression repose sur le `NkDeflate` interne (inflate/deflate RFC 1950/1951).

- **Rendu / 2D** — le format par défaut des textures, sprites, atlas d'UI ; alpha exact pour le
  *blending*.
- **Outils / éditeur** — export de captures, de previews, de *thumbnails* ; format universellement
  relisible.
- **IO** — `NkImage::SaveToMemory` (override `NKIResource`) encode justement en PNG : c'est le
  format de sérialisation par défaut d'une image.

### `NkJPEGCodec` — la photo compressée

JPEG baseline DCT, avec deux contraintes structurantes. Au **décodage**, la sortie est `NK_GRAY8`
(monochrome) ou `NK_RGB24` (couleur) — **jamais d'alpha**, c'est le format qui ne le porte pas — et
le **JPEG progressif est rejeté** (`nullptr`). À l'**encodage**, l'image est convertie en RGB24/Gray8
au besoin et le paramètre `quality ∈ [1,100]` (défaut 90) règle le compromis taille/qualité.

- **Rendu** — textures de surfaces naturelles (terrain, ciel, photo) où l'absence d'alpha et la
  perte sont acceptables contre un gain de taille énorme.
- **IO / réseau** — transmettre des images photographiques (capture caméra via NKCamera) avec un
  `quality` modéré pour limiter la bande passante.
- **Piège** — un JPEG progressif renvoyé par un service externe donnera `nullptr` ; prévoir un
  *fallback* ou un transcodage en amont.

### `NkBMPCodec` — le DIB Windows complet

BMP couvre toute l'histoire du DIB : en-têtes BITMAPCOREHEADER (12), INFOHEADER (40), V4 (108),
V5 (124) ; profondeurs 1/2/4/8 (indexé), 16/24/32 ; compressions BI_RGB, BI_RLE4, BI_RLE8,
BI_BITFIELDS. L'encodage sort en 24 bpp (RGB) ou 32 bpp (RGBA) avec un BITMAPV4HEADER.

- **Outils / éditeur** — échanger avec des outils Windows hérités, lire des ressources `.bmp`
  embarquées (icônes, splash).
- **IO** — format non compressé pratique pour du débogage pixel-à-pixel (aucune perte ni filtre).

### `NkTGACodec` — le TARGA

TARGA lecture types 1 (palette), 2 (RGB), 3 (gris), avec RLE (types 9/10/11) ; les 15/16 bpp RGB555
sont étendus en RGB24. Format historique des textures de jeu.

- **Rendu** — lire d'anciens assets de jeu livrés en `.tga` (très répandu dans les pipelines
  *id Tech*/Source).
- **Outils** — interop avec des éditeurs d'art qui exportent en TGA non compressé ou RLE.

### `NkQOICodec` — le sans-perte rapide

QOI (*Quite OK Image*) : compression sans perte, encodage/décodage très rapides, format trivial.
Idéal pour ce qui doit être rapide à lire/écrire sans dépendre de la lourdeur de PNG.

- **Rendu / GPU** — cache de textures décodées sur disque : recharger en QOI est nettement plus
  rapide que redécoder un PNG.
- **Outils / éditeur** — sauvegarde intermédiaire d'assets dans un pipeline d'import.

### `NkHDRCodec` — le Radiance HDR

HDR (`.hdr`/`.rgbe`) porte la **haute dynamique** : il décode en float32 `NK_RGB96F`, préservant des
valeurs > 1.0. C'est le seul codec à proposer trois formes d'écriture et un convertisseur :

- `Decode(data, size)` — vers `NK_RGB96F`. Lit header EXPOSURE/FORMAT, nouveau et ancien RLE, raw,
  toutes orientations ±X/±Y.
- `Encode(img, path)` — écrit un **fichier** `.hdr` (RLE compressé) ; **pas** de buffer mémoire ici.
- `EncodeToMemory(img, out, outSize)` — la variante buffer (→ `NkFree`), pour qui veut éviter le
  disque.
- `ConvertToTexture(hdr, exposure=1, gamma=2.2)` — *tone-mappe* le HDR vers une **nouvelle** image
  RGBA8 affichable (à libérer par `Free()`) ; `gamma ≤ 0` désactive la correction.

Domaines :
- **Rendu / éclairage** — charger un environnement HDR pour l'IBL (*image-based lighting*) :
  `Decode` garde la plage dynamique, le moteur l'échantillonne pour l'ambiant et les reflets.
- **GPU** — uploader directement les float `NK_RGB96F` dans une texture HDR.
- **UI / debug** — `ConvertToTexture` pour afficher un aperçu *tone-mappé* dans l'éditeur, avec
  exposition réglable.

### `NkEXRCodec` — l'OpenEXR pro (lecture)

EXR scanline single-part *from scratch*, **décode seul**. Types HALF/FLOAT/UINT ; compressions
NONE/RLE/ZIPS/ZIP (PIZ bêta). La sortie suit le nombre de canaux : 3 → `NK_RGB96F`, 4 →
`NK_RGBA128F`, 1 → `NK_RGB96F` répliqué. Les variantes PXR24/B44/DWAA/DWAB, *tiles* et *multipart*
renvoient `nullptr`.

- **Rendu / éclairage** — importer des environnements ou des textures HDR de qualité film (EXR est
  le standard VFX), avec alpha float pour le RGBA128F.
- **Outils** — pipeline d'import depuis des DCC (Blender, Nuke) qui exportent en EXR ; vérifier le
  `nullptr` pour les compressions non gérées.

### `NkGIFCodec` — l'image animée

GIF87a/89a complet, multi-frame. Le décodage a deux portes : `Decode` (première frame seule, RGBA32,
rétrocompatible) et `DecodeAnimation` (toutes les frames). Ce dernier rend un `NkGIFAnimation*` dont
chaque `NkGIFFrame` est **déjà composée** sur le canvas global (on n'a pas à appliquer soi-même les
*disposal methods* — `image` est prête). `loopCount` à 0 signifie boucle infinie. La libération passe
**uniquement** par `FreeAnimation` (struct + toutes les frames). À l'écriture : `Encode` (GIF89a,
quantification médiane-coupure 256 couleurs + transparence, buffer mémoire) ou `Save(img, path)`
(fichier).

- **UI / 2D** — afficher un GIF animé (chargement, emote, tutoriel) : itérer `frames` en respectant
  `delayMs`.
- **Outils / éditeur** — exporter une animation courte en GIF pour partage (`Save`), ou en buffer
  pour l'attacher à un message.
- **Piège** — `Decode` ne donne **que** la première frame ; pour l'animation il faut
  `DecodeAnimation`, et ne **jamais** `Free()`/`delete` une frame individuelle.

### `NkICOCodec` — l'icône Windows (lecture)

ICO/CUR **décode seul**. Un `.ico` contient souvent plusieurs tailles ; le codec **sélectionne
automatiquement la plus grande résolution**. Les sous-images en PNG comme en BMP sont reconnues.

- **UI / outils** — charger l'icône d'application, une icône de fichier, un curseur (`.cur`) pour
  l'afficher dans l'éditeur.
- **Piège** — pas de choix de la taille : on récupère toujours la plus grande ; redimensionner
  ensuite via `NkImage::Resize` si une taille précise est voulue.

### `NkPPMCodec` — le NetPBM

NetPBM P1–P6 : `Decode` mappe P1/P4 et P2/P5 vers `NK_GRAY8`, P3/P6 vers `NK_RGB24` ;
`Encode(img, path)` écrit un **fichier** binaire P5 (gris) / P6 (couleur). Format minimaliste,
trivial à produire et à relire.

- **Outils / debug** — dump rapide d'un buffer (heightmap, masque, *G-buffer*) vers un fichier
  inspectable par n'importe quel visualiseur ; aucune compression à déboguer.
- **IO** — format d'échange ultra-simple entre étapes d'un pipeline de traitement d'image.

### `NkWebPCodec` — le WebP moderne

WebP *from scratch* : `Decode` gère VP8L (sans perte, complet) et VP8 (avec perte, basique) ;
`Encode(img, out, outSize, lossless=true, quality=90)` écrit du VP8L vers un buffer mémoire.

- **Rendu / 2D** — textures et atlas web compacts (WebP bat souvent PNG en taille à qualité égale).
- **IO / réseau** — servir/recevoir des assets web ; le mode lossless préserve l'exactitude pour
  des sprites à alpha net.

### `NkSVGCodec` — la rasterisation vectorielle

SVG → RGBA32. Le rasteriseur gère les **formes** (`<svg> <g> <path> <rect> <circle> <ellipse>
<line> <polyline> <polygon>`) mais **pas** `<text>`, `<use>`, `<defs><style>`, les gradients,
patterns, masques, *clipPath* ni filtres — ce sont les limites à connaître avant de choisir un asset
SVG. `Decode(data, size, outW, outH)` rasterise à la taille demandée (`0` ⇒ taille naturelle),
`DecodeFromFile` lit d'abord le disque. L'`Encode`/`EncodeToFile` **n'est pas une vectorisation** :
il enrobe l'image matricielle en `<image href="data:png;base64,…">` dans un `<svg>` (pixel-perfect).

- **UI / 2D** — rasteriser une icône vectorielle à la résolution exacte de l'affichage (net à
  n'importe quel DPI) ; c'est le grand intérêt du SVG côté éditeur/HUD.
- **Outils / éditeur** — charger des assets d'interface vectoriels, les redimensionner sans
  *aliasing* en re-rasterisant à la bonne taille.
- **Piège** — un SVG utilisant texte/gradients/filtres ne rendra pas ces éléments ; vérifier
  l'asset, ou pré-aplatir dans un éditeur vectoriel.

### Le type `NkColor` et le rectangle `NkIntRect`

Le dessin CPU et certaines API de `NkImage` prennent une couleur `math::NkColor` (et un rectangle
`math::NkIntRect` pour les régions) du module NKMath. Les couleurs *packed* `uint32` sont au format
`0xRRGGBBAA`.

### L'écosystème vectoriel : `NkSVGImage`, `NkSVGShapeView` et types de support

Là où `NkSVGCodec::Decode` ne rend que des **pixels**, `NkSVGImage` garde le SVG **vectoriel** —
une liste de shapes que l'on rasterise ou triangule à la demande. C'est l'API à utiliser dès qu'on
veut autre chose qu'un bitmap figé.

`NkSVGImage` (PIMPL, non copiable, ctor/dtor privés, **à libérer par `Free()`**) :
- `LoadFromFile` / `LoadFromMemory` — charge et parse le SVG en shapes.
- `Rasterize(outW, outH)` — re-rasterise depuis les shapes (une dimension à 0 ⇒ calculée par
  *aspect ratio*) ; rend une image RGBA32 owning. **Rendu / UI** : re-rasteriser une icône à
  plusieurs tailles depuis une seule source vectorielle.
- `NaturalWidth` / `NaturalHeight` — dimensions depuis le viewBox/width-height.
- `ShapeCount` / `GetShape(idx)` — accès aux shapes via `NkSVGShapeView`.
- `TriangulateAll(outXs, outYs, outIndices, outTriColors)` — produit un **mesh global** (triangle-list)
  avec une couleur RGBA8888 *packed* par triangle. **GPU / rendu** : uploader directement un SVG
  comme géométrie vectorielle remplie, sans passer par une texture.

`NkSVGShapeView` — vue **read-only** sur une shape, valide tant que le `NkSVGImage` parent vit ;
coordonnées en espace SVG (avant *scaling* viewBox→output) :
- `ContourCount`, `ContourPointCount(idx)`, `ContourXs(idx)`/`ContourYs(idx)` — la géométrie brute
  des contours.
- `FillColor`/`StrokeColor`/`StrokeWidth`/`Opacity`/`FillEvenOdd` — l'apparence.
- `Triangulate(outXs, outYs, outIndices, baseIndex=0)` — *ear-clipping* par contour (sans gestion
  des trous), renvoie le nombre de triangles ; `baseIndex` = taille de `outXs` avant l'appel, pour
  **chaîner** plusieurs shapes dans un même buffer. **GPU** : remplir une forme vectorielle en
  triangles uploadables.
- `IsValid` — la vue pointe-t-elle sur une shape ?

Les types de support décrivent l'apparence et la transformation :
- `NkSVGColor` (`r/g/b/a`, `none`) — couleur SVG ; `Parse` lit `#RGB`/`#RRGGBB`/`rgb()`/`rgba()` et
  148 noms CSS ; statics `Transparent`/`Black`/`White`/`None`.
- `NkSVGStyle` — style **cascadable** par `<g>` : `fill`/`stroke`, `strokeWidth`, `opacity` et ses
  variantes, `fillEvenOdd`, `visible`.
- `NkSVGTransform` — matrice affine 2D `(a,b,c,d,e,f)` ; fabriques `Identity`/`Translate`/`Scale`/
  `Rotate`/`Parse`, composition par `operator*`, application par `Apply(x, y)` (inline). **Outils** :
  composer les transforms d'un arbre `<g>` pour positionner les shapes.

### Le DOM XML : `NkXMLParser`, `NkXMLDocument`, `NkXMLNode`

Le parser XML qui alimente le SVG est utilisable seul, pour tout XML d'outillage (UTF-8 only).

`NkXMLParser` (statique) : `Parse(xml, size, doc)` / `Parse(cstr, doc)` / `ParseFile(path, doc)`.
Le `doc` doit avoir été `Init()` au préalable ; `Parse` renvoie `true` même avec des avertissements.

`NkXMLDocument` — racine DOM dont toute la mémoire vit dans une **arena** :
- `Init(arenaBytes=4 Mo)` / `Destroy()` — initialise / libère l'arena (**aucun** free individuel des
  nœuds : tout part d'un coup).
- `docElement` — le premier élément (ex. `<svg>`) ; `isValid`, `errorMsg`, `errorLine` pour le
  diagnostic.
- `GetElementById(id)` (DFS), `GetElementsByTagName(tag, out, maxOut)` (`"*"` = tous),
  `QuerySelector(selector)` (CSS minimal `#id`/`.class`/`tag`/`tag.class`), `ForEach(fn, ud)` (DFS,
  arrêt si `fn` retourne `false`). **Outils / IO** : charger un descripteur XML, le parcourir, en
  extraire des nœuds par id/tag/sélecteur.

`NkXMLNode` — un élément ou un texte de l'arbre : `type` (`NkXMLNodeType`), `tagName`/`localName`/`ns`,
liens `parent`/`firstChild`/`nextSibling`… Lecture d'attributs : `GetAttr(name)` (linéaire,
`O(numAttrs)`), `GetAttrLocal`, `GetAttrF(name, def)` (via `strtof`), `HasAttr`. Navigation : `Is(tag)`,
`FirstChildNamed(tag)`, `NextSiblingNamed(tag)`, `GetTextContent(buf, len)` (concatène les enfants
TEXT/CDATA). `NkXMLAttr` est l'attribut (liste chaînée `name`/`value`/`prefix`/`localName`/`ns`/`next`).

`NkXMLArena` — l'allocateur linéaire sous-jacent : `Init(bytes)` (owning, via `NkAlloc`), `InitView`
(vue non-owning), `Destroy`/`Reset`, `Alloc<T>(count)` (bump-pointer aligné 8, zéro-fill, `O(1)`),
`Dup(str, len)` (copie une chaîne dans l'arena). `NkXMLNamespaceCtx` (`Push`/`Resolve`) gère les
espaces de noms ; `enum NkXMLNodeType` distingue ELEMENT/TEXT/CDATA/COMMENT/PI/DOCUMENT.

### Le type pivot `NkImage`

C'est l'objet que **tous** les codecs produisent et consomment. `NkImage` dérive de `NKIResource`,
n'est **pas copiable** (copie supprimée) mais est *movable* (move ctor/assign self-safe), gère son
buffer automatiquement avec un *stride* aligné sur 4 octets (`(w*bpp+3)&~3`).

**Les enums** posent le vocabulaire :
- `NkImagePixelFormat` — `NK_UNKNOWN`, `NK_GRAY8`, `NK_GRAY_A16`, `NK_RGB24`, `NK_RGBA32`,
  `NK_RGBA128F` (float ×4), `NK_RGB96F` (float ×3). Les helpers `ChannelsOf(f)` et
  `BytesPerPixelOf(f)` (`constexpr`) donnent canaux et octets/pixel.
- `NkImageFormat` — le conteneur (`NK_PNG`, `NK_JPEG`, …, `NK_SVG`, `NK_EXR`).
- `NkResizeFilter` — `NK_NEAREST`/`NK_BILINEAR`/`NK_BICUBIC`/`NK_LANCZOS3` pour `Resize`.

**Les fabriques** (résultat owning, à libérer par `Free()`) :
- `Create(w, h, desiredChannels=0, color=0)` et `Create(w, h, fmt, color=0)` — image neuve remplie
  (`color` *packed* `0xRRGGBBAA`).
- `Alloc(w, h, fmt)` — allocation brute, usage interne des codecs.
- `Wrap(pixels, w, h, fmt, stride=0)` — **vue non-owning** sur un buffer externe : jamais libérée par
  `Free`/destructeur. **GPU / interop** : envelopper un buffer mappé sans copie.
- `ConvertToTexture(hdr, exposure=1, gamma=2.2)` — HDR → RGBA32 *tone-mappé*.

**Charger / sauver.** `Load(path, desiredChannels=0)` et les surcharges `LoadFromMemory` détectent le
format. Les overrides `NKIResource` (`LoadFromFile`/`LoadFromMemory`/`LoadFromStream`,
`SaveToFile`/`SaveToMemory`/`SaveToStream`, `IsValid`, `Unload`) intègrent l'image dans le système de
ressources — `SaveToMemory` encode en **PNG** par défaut. Les `Save*` fichier couvrent
PNG/JPEG/BMP/TGA/PPM/HDR/QOI ; `Save(path, quality)` déduit le format de l'extension. **Attention** :
`SaveGIF`, `SaveWebP` et `SaveSVG` **ne sont pas implémentés** (retournent `false`) — passer par les
codecs `NkGIFCodec`/`NkWebPCodec`/`NkSVGCodec`. Les `Encode*` mémoire (PNG/BMP/TGA/QOI/JPEG)
remplissent un `out` à libérer par `NkFree`.

**Transformer.** `Convert(fmt)`, `Resize(w, h, filter)`, `Crop`, `Copy`, `CopyAs(fmt)` rendent de
**nouvelles** images owning ; `Blit`/`BlitRegion`/`Copy(src, …)`/`CopyTo(dst)` composent une image dans
une autre ; `FlipVertical`/`FlipHorizontal`/`PremultiplyAlpha` opèrent **en place**.
- **Rendu / GPU** — `Resize` pour générer des mip-levels CPU, `PremultiplyAlpha` avant un *blending*
  *premultiplied*, `FlipVertical` pour accorder l'orientation à une convention de texture
  (OpenGL bas-gauche).
- **UI / 2D** — `Crop`/`BlitRegion` pour découper et composer un atlas de sprites.

**Dessin CPU** (LDR 8-bit, no-op si l'image est HDR ou hors bornes, couleur `math::NkColor`) :
`SetPixel`/`GetPixel`/`BlendPixel` (src-over), `Fill`, `DrawLine`/`DrawHLine`/`DrawVLine` (Bresenham),
`DrawRect`/`FillRect`, `DrawCircle`/`FillCircle` (midpoint), `DrawEllipse`/`FillEllipse`
(paramétrique).
- **Outils / éditeur** — générer une texture procédurale, dessiner une grille de debug, annoter une
  capture sans GPU.
- **UI** — composer un atlas simple ou un *placeholder* directement en CPU.

**Accès.** `Pixels()` (const/non), `Width`/`Height`/`Channels`/`BytesPP`/`Stride`,
`Format`/`SourceFormat`, `IsValid`, `IsHDR`, `TotalBytes`, `RowPtr(y)` — l'essentiel pour uploader au
GPU ou itérer ligne par ligne via `Stride`.

**Mémoire.** `Free()` libère pixels **et** struct (`nkFree`) — uniquement sur images **fabriquées**,
jamais sur une `NkImage` pile. `Unload()` (override) libère **les pixels seuls**, conserve la struct,
et est **sûr** sur la pile comme sur le tas.

### Les briques internes : `NkImageStream`, `NkDeflate`

Ces deux classes servent les codecs mais sont exposées et réutilisables.

`NkImageStream` — un buffer binaire *endian-aware*. Ctor lecture `NkImageStream(data, size)`
(non-owning) ou ctor écriture `NkImageStream()` (buffer dynamique qui croît, doublage à partir de
4096). Lecture : `ReadU8`/`ReadU16BE`/`ReadU16LE`/`ReadU32BE`/`ReadU32LE`/`ReadI16BE`/`ReadI32LE`,
`ReadBytes(dst, n)` (`dst` nullptr = skip), `Skip`/`Seek`. Écriture : `WriteU8`/`WriteU16BE/LE`/
`WriteU32BE/LE`/`WriteI32LE`/`WriteBytes`. `TakeBuffer(out, outSize)` transfère le buffer interne (à
libérer par `NkFree`). Accès : `Tell`/`Size`/`IsEOF`/`HasBytes(n)`/`HasError`/`Ptr`. Une lecture
hors-bornes met `HasError` à vrai et renvoie 0 (jamais de crash). **IO / outils** : écrire un parseur
de format binaire propriétaire sans réinventer la lecture big/little-endian.

`NkDeflate` — l'inflate/deflate maison (adapté de stb_image v2.16) :
- `Decompress(in, inSz, out, outCap, written)` — zlib RFC 1950 (en-tête CMF/FLG + Adler-32) ; `out`
  est **pré-alloué par l'appelant**.
- `DecompressRaw(…)` — même contrat, mais DEFLATE brut RFC 1951 (sans en-tête zlib).
- `Compress(in, inSz, out, outSz, level=6)` — produit du zlib en **stored blocks** (BTYPE=00, donc
  **pas de compression réelle**, `level` ignoré) ; `out` via NkAlloc → `NkFree`.

  **IO / réseau** : décompresser un flux zlib/DEFLATE arbitraire (pas seulement PNG) sans tirer
  `zlib`. À noter : `Compress` n'écrit que des blocs *stored*, à utiliser quand on veut le conteneur
  zlib sans en attendre un gain de taille.

---

### Exemple récapitulatif

```cpp
#include "NKImage/NKImage.h"
using namespace nkentseu;

// 1) Décoder une texture PNG depuis un buffer déjà en mémoire.
NkImage* tex = NkPNGCodec::Decode(asset.data, asset.size);   // nullptr si échec
if (tex) {
    renderer.Upload(tex->Pixels(), tex->Width(), tex->Height());

    // Réencoder en QOI (rapide) vers un buffer — out via NkAlloc.
    uint8* qoi = nullptr; usize qoiSize = 0;
    if (NkQOICodec::Encode(*tex, qoi, qoiSize))
        nkentseu::memory::NkFree(qoi);                       // SEULE libération correcte

    tex->Free();                                             // image fabriquée → Free()
}

// 2) Charger un environnement HDR pour l'éclairage, l'afficher tone-mappé.
NkImage* hdr = NkHDRCodec::Decode(envData, envSize);         // NK_RGB96F (float)
if (hdr) {
    NkImage* preview = NkHDRCodec::ConvertToTexture(*hdr, 1.0f, 2.2f); // → RGBA8
    // ... afficher 'preview' dans l'éditeur ...
    preview->Free();
    hdr->Free();
}

// 3) Rasteriser une icône SVG à la taille exacte de l'affichage.
NkImage* icon = NkSVGCodec::DecodeFromFile("icons/save.svg", 32, 32);
if (icon) { ui.UploadIcon(icon); icon->Free(); }

// 4) Lire toutes les frames d'un GIF animé.
NkGIFAnimation* anim = NkGIFCodec::DecodeAnimation(gifData, gifSize);
if (anim) {
    for (uint32 i = 0; i < anim->frameCount; ++i)
        Push(anim->frames[i].image, anim->frames[i].delayMs);
    NkGIFCodec::FreeAnimation(anim);                         // libère tout d'un coup
}
```

---

[← Index NKImage](README.md) · [Récap NKImage](../NKImage.md) · [Couche Runtime](../README.md)
