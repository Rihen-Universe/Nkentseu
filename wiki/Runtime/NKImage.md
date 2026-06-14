# NKImage

> Couche **Runtime** · L'image CPU du moteur : un conteneur de pixels autonome (`NkImage`)
> et douze codecs from-scratch (PNG, JPEG, BMP, TGA, QOI, HDR, EXR, GIF, ICO, PPM, SVG, WebP).

Dès qu'une chose est faite de **pixels** — une texture à charger, une icône à décoder, une
capture à sauvegarder, un atlas à blitter, une vignette à générer — elle passe par NKImage.
C'est la couche qui transforme un fichier sur disque (ou un buffer en mémoire) en un tableau
de pixels manipulable côté CPU, et inversement. Le rendu y puise ses textures, NKFont y
rastérise ses glyphes, NKCamera y convertit ses frames, l'éditeur y charge ses thumbnails.

Tout tourne autour d'un seul type pivot : **`NkImage`**, un conteneur à gestion mémoire
autonome (stride aligné 4 octets, ownership explicite) qui hérite de `NKIResource` — donc
Load/Save uniformes avec le reste du moteur. Autour de lui gravitent les douze codecs, chacun
une classe statique pure exposant `Decode` (buffer → `NkImage*`) et, le plus souvent, `Encode`
(`NkImage` → buffer). Tous les codecs sont écrits **from scratch**, sans dépendance externe.

- **Namespace** : `nkentseu` (pas de sous-namespace ; `nkentseu::math` pour `NkColor`/`NkIntRect`)
- **Header parapluie** : `#include "NKImage/NKImage.h"`

---

## Par où commencer

Selon ce que vous cherchez à faire :

| Besoin | Partie |
|--------|--------|
| Charger une image, accéder aux pixels, sauvegarder | [Le conteneur image](NKImage/Image.md) |
| Créer / redimensionner / convertir / recadrer une image | [Le conteneur image](NKImage/Image.md) |
| Blitter, copier des régions, composer des atlas | [Le conteneur image](NKImage/Image.md) |
| Dessiner sur le CPU (lignes, rectangles, cercles) | [Le conteneur image](NKImage/Image.md) |
| Décoder/encoder un format précis (PNG, JPEG, WebP…) à la main | [Les codecs](NKImage/Codecs.md) |
| Lire toutes les frames d'un GIF animé | [Les codecs](NKImage/Codecs.md) |
| Rastériser ou trianguler un SVG | [Les codecs](NKImage/Codecs.md) |
| Décompresser/compresser du zlib/DEFLATE, parser du XML | [Les codecs](NKImage/Codecs.md) |

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec ses contraintes
et ses cas d'usage concrets (textures, atlas, icônes, animation, scientifique…).

---

## Aperçu des familles

- **Conteneur** (`Core/NkImage.h`) — `NkImage` : pixels, formats LDR (GRAY8/GRAY_A16/RGB24/
  RGBA32) et HDR (RGB96F/RGBA128F), Load/Save (héritage `NKIResource`), fabriques `Alloc`/
  `Wrap`/`Create`, transformations (`Convert`/`Resize`/`Crop`/`Copy`), blit/régions, dessin CPU.
- **Plomberie codec** (`Core/NkImage.h`) — `NkImageStream` (lecture/écriture binaire endian-
  explicite des codecs) et `NkDeflate` (inflate/deflate zlib & DEFLATE pour PNG).
- **Codecs image** (`Codecs/*`) — douze classes statiques `Decode`/`Encode` : PNG, JPEG, BMP,
  TGA, QOI, HDR, EXR (lecture seule), GIF (multi-frame), ICO (lecture seule), PPM, SVG, WebP.
- **Support SVG & XML** (`Codecs/SVG/*`) — `NkSVGImage`/`NkSVGShapeView` (vectoriel
  rasterisable + triangulation GPU), types de style (`NkSVGColor`/`NkSVGStyle`/
  `NkSVGTransform`) et un parser/DOM XML complet (`NkXMLParser`/`NkXMLDocument`).

> **Règle mémoire transversale** : un `NkImage*` issu d'une fabrique statique ou d'un `Decode`
> se libère par `img->Free()` (pixels + struct). Un buffer `out` d'`Encode`/`EncodeToMemory`/
> `SaveToMemory` est alloué via `NkAlloc` → se libère **exclusivement** par
> `nkentseu::memory::NkFree(out)`, jamais `std::free`/`delete[]` (heap corruption c0000374).
> Ne jamais `Free()` une `NkImage` allouée sur la pile (utiliser `Unload()`).

---

## Index des headers

| Header | Contenu | Documenté dans |
|--------|---------|----------------|
| `NKImage/NKImage.h` | Parapluie (inclut Core + les 12 codecs + SVG/XML). | — |
| `Core/NkImage.h` | `NkImage`, `NkImageStream`, `NkDeflate`, enums & helpers. | [Le conteneur image](NKImage/Image.md) |
| `Core/NkImageExport.h` | Macros d'export/attributs (`NKENTSEU_IMAGE_API`…). | [Le conteneur image](NKImage/Image.md) |
| `Codecs/PNG/NkPNGCodec.h` | PNG RFC 2083 (Decode/Encode). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/JPEG/NkJPEGCodec.h` | JPEG baseline DCT (Decode/Encode). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/BMP/NkBMPCodec.h` | BMP/DIB complet (Decode/Encode). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/TGA/NkTGACodec.h` | TARGA (Decode/Encode). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/QOI/NkQOICodec.h` | QOI (Decode/Encode). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/HDR/NkHDRCodec.h` | Radiance HDR float32 (Decode/Encode fichier+mémoire). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/EXR/NkEXRCodec.h` | OpenEXR scanline (Decode seul). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/GIF/NkGIFCodec.h` | GIF87a/89a multi-frame + animation. | [Les codecs](NKImage/Codecs.md) |
| `Codecs/ICO/NkICOCodec.h` | ICO/CUR (Decode seul). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/PPM/NkPPMCodec.h` | NetPBM P1–P6 (Decode/Encode fichier). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/SVG/NkSVGCodec.h` | Rasterisation SVG + `NkSVGImage` vectoriel. | [Les codecs](NKImage/Codecs.md) |
| `Codecs/WEBP/NkWebPCodec.h` | WebP VP8/VP8L (Decode/Encode lossless). | [Les codecs](NKImage/Codecs.md) |
| `Codecs/SVG/NkXMLParser.h` | Parser/DOM XML (support SVG). | [Les codecs](NKImage/Codecs.md) |

---

[← Couche Runtime](README.md) · [Index du wiki](../README.md)
