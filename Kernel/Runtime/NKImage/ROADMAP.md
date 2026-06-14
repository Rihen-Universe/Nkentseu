# NKImage — Roadmap

État actuel (mai 2026) : module image runtime mature, 12 codecs intégrés from-scratch (PNG, JPEG, BMP, TGA, HDR, EXR, PPM, QOI, GIF, ICO, WebP, SVG), sortie pixels LDR + HDR, pipeline complet stb_image-style sans dépendance externe. **2026-05-29** : `NkImage` implémente désormais `NKIResource` (interface CPU de NKStream) — `LoadFromFile/Memory/Stream` + `SaveToFile/Memory/Stream` + `IsValid/Unload`, sans casser les API riches `Load(path, channels)` ni `LoadFromMemory(d, s, channels)`. Manquent : formats GPU compressés (KTX2/BC/ASTC), mipmaps, atlasing et hot-reload.

---

## Synthèse

| Phase / Composant | Statut | Effort | Priorité |
|-------------------|--------|--------|----------|
| Core NkImage (alloc/wrap/convert/resize/blit/crop) | Livré | — | — |
| Pixel formats LDR (GRAY8, GRAY_A16, RGB24, RGBA32) | Livré | — | — |
| Pixel formats HDR (RGB96F, RGBA128F) | Livré | — | — |
| NkImageStream (read/write big/little endian) | Livré | — | — |
| NkDeflate (inflate/deflate zlib RFC 1950 + raw) | Livré | — | — |
| Codec PNG (decode + encode) | Livré | — | — |
| Codec JPEG (decode + encode baseline) | Livré | — | — |
| Codec BMP (decode + encode) | Livré | — | — |
| Codec TGA (decode + encode) | Livré | — | — |
| Codec HDR Radiance .hdr (decode + encode) | Livré | — | — |
| Codec EXR scanline | Partiel | M | P1 |
| Codec PPM/PGM/PBM (decode + encode) | Livré | — | — |
| Codec QOI (decode + encode) | Livré | — | — |
| Codec GIF (decode + encode + animation multi-frame) | Livré | — | — |
| Codec ICO (decode + encode) | Livré | — | — |
| Codec WebP VP8L lossless | Livré | — | — |
| Codec WebP VP8 lossy decode | Partiel | M | P2 |
| Codec WebP VP8 lossy encode | TODO | L | P3 |
| Codec SVG (decode rasterizer scanline AA) | Partiel | M | P2 |
| Codec SVG (text/use/defs/gradients/clipPath) | TODO | L | P3 |
| Codec SVG (encode wrapper) | Livré | — | — |
| Manipulation (Flip, Premultiply, Convert, Resize, Blit, Crop) | Livré | — | — |
| Filtres resize (Nearest/Bilinear/Bicubic/Lanczos3) | Livré | — | — |
| ConvertToTexture (tonemap HDR -> LDR) | Livré | — | — |
| Codec KTX2 (Khronos texture container) | TODO | L | P1 |
| Compression GPU BC1/BC3/BC5/BC7 | TODO | L | P1 |
| Compression GPU ETC2/ASTC (mobile) | TODO | L | P2 |
| Génération mipmaps (down-sampling box/Kaiser) | TODO | M | P1 |
| Texture array / cubemap container | TODO | M | P2 |
| Streaming d'images (lecture par tuiles) | TODO | L | P3 |
| Hot-reload watcher (lien NKFileSystem) | TODO | M | P2 |
| Atlasing (bin-packing rect / Skyline) | TODO | M | P2 |
| Animation framework générique (au-delà de GIF) | TODO | L | P3 |
| Tests unitaires complets par codec | Partiel | M | P2 |

Légende : Livré · Partiel · En cours · TODO · Abandonné

---

## Tableau formats supportés (lecture / écriture)

| Format | Lecture | Écriture | Pixel out | Notes |
|--------|:---:|:---:|-----------|-------|
| PNG    | Livré | Livré | RGBA32 / GRAY8 | Inflate maison, filtres 0-4, RFC 2083 |
| JPEG   | Livré | Livré | RGB24 / GRAY8 | Baseline 8-bit, qualité paramétrable |
| BMP    | Livré | Livré | RGB24 / RGBA32 | 24/32 bits, bottom-up |
| TGA    | Livré | Livré | RGBA32 | RLE + non compressé |
| HDR    | Livré | Livré | RGB96F | Radiance RLE, scanline |
| EXR    | Partiel | TODO | RGB96F / RGBA128F | Scanline only ; PIZ beta ; tiles/multipart/DWAA non supportés ; pas d'encode |
| PPM/PGM/PBM | Livré | Livré (PPM) | RGB24 / GRAY8 | ASCII + binary |
| QOI    | Livré | Livré | RGBA32 | Quite OK Image format |
| GIF    | Livré | Livré | RGBA32 | GIF87a/89a + multi-frame + disposal |
| ICO    | Livré | TODO encodage | RGBA32 | Décodage multi-image |
| WebP VP8L (lossless) | Livré | Livré | RGBA32 | Compression LZ77 + Huffman |
| WebP VP8 (lossy) | Partiel | TODO | RGB24 | Décodage DCT basique |
| SVG    | Partiel | Livré (wrapper) | RGBA32 | Pas de text/use/defs/gradients/filters |

EXR non supportés explicitement : PXR24, B44, B44A, DWAA, DWAB, tiles, multipart 2.0.

---

## Livré

### Phase 1 — Core image
- Classe `NkImage` avec ownership explicite (Alloc, Wrap, Free) — [NkImage.h](src/NKImage/Core/NkImage.h)
- Formats pixel LDR + HDR : GRAY8, GRAY_A16, RGB24, RGBA32, RGB96F, RGBA128F
- Détection automatique du format depuis les magic bytes (Dispatch)
- Conversion canaux (`ConvertChannels`) sur chargement
- `NkImageStream` : lecture/écriture LE/BE avec gestion erreurs
- `NkDeflate` : inflate (LSB-first stb_image-style) + deflate level 0-9 + raw inflate

### Phase 2 — Codecs from-scratch (aucune dépendance externe)
- PNG : décodeur complet (RFC 2083, filtres 0-4, interlacement Adam7), encodeur basique
- JPEG : décodeur DCT baseline + encodeur paramétrable qualité
- BMP / TGA / HDR / PPM / QOI / GIF / ICO / WebP / SVG : voir tableau
- GIF multi-frame avec composition automatique selon disposal methods
- EXR scanline single-part : ZIP, ZIPS, RLE, NONE, HALF/FLOAT/UINT, layouts R/G/B/A, Y/RY/BY, Z
- SVG : tokenizer XML, rasterizer scanline antialiasé (supersample 2x), beziers, arcs

### Phase 3 — Manipulation
- Flip vertical/horizontal, prémultiplication alpha
- `Resize` avec 4 filtres (Nearest, Bilinear, Bicubic, Lanczos3)
- `Crop`, `Blit` (copie rectangulaire avec clipping)
- `Convert` entre formats pixel arbitraires
- `ConvertToTexture` : tone-mapping HDR -> LDR (exposure + gamma)

### Phase 4 — Encodage mémoire
- `EncodePNG`, `EncodeJPEG`, `EncodeBMP`, `EncodeTGA`, `EncodeQOI` vers buffer en mémoire

---

## En cours / TODO immédiat

### Finalisation codecs
- **EXR encode** : aucune écriture EXR pour l'instant (Save EXR absent)
- **EXR PIZ** : décodage structurel correct mais corruption résiduelle, à finaliser
- **EXR compressions manquantes** : PXR24, B44, B44A, DWAA, DWAB
- **EXR tiles + multipart 2.0** : non supportés
- **WebP VP8 lossy encode** : actuellement seul VP8L lossless est en encode
- **SVG features avancées** : `<text>`, `<use>`, `<defs>`, `<style>`, gradients (linéaire + radial), patterns, masks, clipPath, filters

### Tests
- Un seul fichier test trouvé : [TestEXR.cpp](tests/TestEXR.cpp)
- Manquent : tests unitaires PNG/JPEG/BMP/TGA/HDR/QOI/GIF/ICO/WebP/SVG round-trip + golden images

---

## À venir / À ajouter (futur proche)

### Compression GPU (alignée NKRenderer Phase H)
- Format conteneur **KTX2** (Khronos) : multi-layer, multi-mip, supercompression Basis
- Encodage BC1/BC3/BC5/BC7 (desktop GPU)
- Encodage ETC2/ASTC (mobile GPU)
- Décompression CPU des blocs (fallback lecture)

### Pipeline texture moderne
- **Génération mipmaps** : down-sampling box / Kaiser-windowed
- **Texture array** : conteneur pour layers (sprite sheets, terrain blends)
- **Cubemap** : 6 faces packées dans un seul fichier
- **Atlasing** : bin-packing rect (Skyline / Guillotine) pour atlas runtime

### Hot-reload et streaming
- **Hot-reload** : watcher fichier via NKFileSystem -> callback de recharge
- **Streaming par tuiles** : lecture progressive grandes images (sparse textures, terrain)
- **Animation framework générique** : étendre au-delà de GIF (APNG, séquences PNG, WebP animé)

### Diagnostic & qualité
- Tests round-trip systématiques par format
- Comparaison golden images (PSNR / SSIM)
- Fuzzing décodeurs sur corpus de fichiers cassés
- Benchmark décodage vs libpng/libjpeg-turbo de référence

---

## Bugs / quirks connus
- EXR PIZ : décode mais avec artefacts résiduels (non bloquant, marqué BETA dans le code)
- Le codec EXR ne fait que de la lecture (pas d'écriture) — workaround : passer par HDR Radiance
- WebP lossy VP8 décode seulement (encode lossless uniquement)
- SVG ne gère ni le texte ni les gradients
- **NkJPEGCodec::Decode YCbCr 4:2:0 résiduel** (2026-05-27, non fixé) : sur un buffer JPEG produit par NkJPEGCodec::Encode lui-même, le Decode lit correctement la luma Y mais perd Cb/Cr → résultat grayscale au lieu de couleur. Le même buffer ouvert dans un viewer externe (Windows Photos, GIMP) s'affiche correctement en couleur — donc bug spécifique à notre Decode sur ce type de buffer. Decode marche sur des `.jpg` standards (Photoshop, appareil photo, etc.). Comparaison statique avec stb_image v2.16 local sur `decode_block / extend_receive / grow_buffer / huff_decode / idct_block / MCU loop` n'a pas révélé de différence. Demande debug runtime ciblé (prints sur coefs DC Cb/Cr décodés bloc par bloc).

## Bugs corrigés récemment

- **NkJPEGCodec::Encode — BUG K (2026-05-27)** : level shift `-128` manquant dans `FDCT8x8` avant DCT (spec ITU-T.81 §5.4). Le décodeur (`jIDCT`) appliquait bien le `+128` inverse → asymétrie → DC biaisé de +1024 → couleurs corrompues (rouge devient magenta, bleu devient magenta) visibles dans tout viewer externe. Fix : ajout du level shift sur les 8 samples (variables nommées `v0..v7` car `r0..r7` étaient réutilisés plus bas dans la passe 1).
- **Doc Encode trompeuse (2026-05-26)** : les headers `NkJPEGCodec.h`, `NkPNGCodec.h`, `NkHDRCodec.h` + bloc `NkImage::EncodePNG/JPEG/...` disaient "buffer alloué avec malloc, libérer avec free". Réalité : `NkAlloc` (allocateur custom NKMemory). Utiliser `std::free` → heap corruption Windows c0000374. Doc corrigée avec note explicite.

## Pièges connus (à NE PAS reproduire)

- **`NkImage::Alloc()` + `Free()` puis `delete img`** = double-free immédiat. Le wrapper NkImage est alloué via `nkMalloc + placement new` et `Free()` libère pixels + wrapper. Pattern correct : `NkImage::Alloc(...) → img->Free();` (PAS de `delete img`). Documenté dans `Pong/Render/Texture2D.cpp:86-87`, à hoister dans `NkImage.h` près de `Alloc()`.
- **Buffer `out` de `NkXxxCodec::Encode`** : alloué via `NkAlloc`, libérer avec `nkentseu::memory::NkFree(out)` depuis `NKMemory/NkAllocator.h`. JAMAIS `std::free` / `delete[]`.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (types), NKMemory (NkAllocator, NkAlloc/NkFree), NKContainers (NkVector), NKFileSystem (NkFile), NKPlatform (macros API)
- **Modules au-dessus qui en dépendent** : NKRenderer (chargement textures), NKUI (icônes, atlas), NKFont (atlas glyphes via NkImage), assets pipeline
