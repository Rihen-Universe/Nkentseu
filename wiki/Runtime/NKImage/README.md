# NKImage — documentation détaillée

Le module **NKImage**, partie par partie. Pour une vue d'ensemble et un guide « par où
commencer », voir le récap : [../NKImage.md](../NKImage.md).

Chaque page suit la même structure : un **tutoriel** narratif, un **aperçu** tabulaire de
toute l'API, puis une **référence-cours** où chaque élément est expliqué avec ses contraintes
mémoire et ses cas d'usage concrets (textures, atlas, icônes, animation, scientifique…).

| Page | Ce qu'on y apprend | Headers |
|------|--------------------|---------|
| [Image.md](Image.md) | Le conteneur CPU `NkImage` : pixels & formats (LDR/HDR), Load/Save via `NKIResource`, fabriques `Alloc`/`Wrap`/`Create`, `Free` vs `Unload`, transformations (`Convert`/`Resize`/`Crop`/`Copy`), blit & régions, dessin CPU ; plus la plomberie `NkImageStream` et `NkDeflate`. | `Core/NkImage.h`, `Core/NkImageExport.h` |
| [Codecs.md](Codecs.md) | Les 12 codecs `Decode`/`Encode` (PNG, JPEG, BMP, TGA, QOI, HDR, EXR, GIF, ICO, PPM, SVG, WebP) et leur règle mémoire (sortie `NkAlloc` → `NkFree`) ; le GIF animé (`NkGIFAnimation`), le SVG vectoriel (`NkSVGImage`/`NkSVGShapeView`) et le parser XML/DOM (`NkXMLParser`). | `Codecs/PNG/NkPNGCodec.h`, `Codecs/JPEG/NkJPEGCodec.h`, `Codecs/BMP/NkBMPCodec.h`, `Codecs/TGA/NkTGACodec.h`, `Codecs/QOI/NkQOICodec.h`, `Codecs/HDR/NkHDRCodec.h`, `Codecs/EXR/NkEXRCodec.h`, `Codecs/GIF/NkGIFCodec.h`, `Codecs/ICO/NkICOCodec.h`, `Codecs/PPM/NkPPMCodec.h`, `Codecs/SVG/NkSVGCodec.h`, `Codecs/WEBP/NkWebPCodec.h`, `Codecs/SVG/NkXMLParser.h` |

[← Récap NKImage](../NKImage.md) · [← Couche Runtime](../README.md)
