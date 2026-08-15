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
| Codec SVG (decode rasterizer scanline AA) | Livré | M | P2 |
| Codec SVG (stroke caps/joins + gradients linear/radial) | Livré | M | P2 |
| Codec SVG (text/use/defs-style/clipPath/patterns/filters) | TODO | L | P3 |
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

- **PPM binaire 8 bits — lecture hors bornes possible** (relevé le 2026-08-16 pendant la migration vers la valeur, **NON corrigé**, préexistant) : `NkPPMCodec.cpp` ~l.143-147, `p += lineBytes;` peut porter `p` au-delà de `end` ; au tour suivant `const usize avail = static_cast<usize>(end - p);` calcule un `ptrdiff_t` **négatif** casté en `usize` → valeur énorme → `toCopy = lineBytes` et `NkCopy` **lit hors du buffer source**. Déclenché par un fichier PPM tronqué. Sans rapport avec la migration : signalé, pas touché.
- **PNG profondeur < 8, types couleur 2/4/6** (relevé le 2026-08-16, **NON corrigé**, préexistant) : `NkPNGCodec.cpp` ~l.402, seul `ct == 0` écrit dans `dst` ; pour `ct == 2/4/6` en profondeur < 8 rien n'est écrit et la ligne reste à zéro. La spec PNG interdit ces combinaisons, mais **aucun rejet explicite en amont** → un fichier malformé donne une image noire en silence plutôt qu'une erreur.

## Bugs corrigés récemment

- **NkJPEGCodec::Encode — BUG K (2026-05-27)** : level shift `-128` manquant dans `FDCT8x8` avant DCT (spec ITU-T.81 §5.4). Le décodeur (`jIDCT`) appliquait bien le `+128` inverse → asymétrie → DC biaisé de +1024 → couleurs corrompues (rouge devient magenta, bleu devient magenta) visibles dans tout viewer externe. Fix : ajout du level shift sur les 8 samples (variables nommées `v0..v7` car `r0..r7` étaient réutilisés plus bas dans la passe 1).
- **Doc Encode trompeuse (2026-05-26)** : les headers `NkJPEGCodec.h`, `NkPNGCodec.h`, `NkHDRCodec.h` + bloc `NkImage::EncodePNG/JPEG/...` disaient "buffer alloué avec malloc, libérer avec free". Réalité : `NkAlloc` (allocateur custom NKMemory). Utiliser `std::free` → heap corruption Windows c0000374. Doc corrigée avec note explicite.

## Pièges connus (à NE PAS reproduire)

- **`NkImage::Alloc()` + `Free()` puis `delete img`** = double-free immédiat. Le wrapper NkImage est alloué via `nkMalloc + placement new` et `Free()` libère pixels + wrapper. Pattern correct : `NkImage::Alloc(...) → img->Free();` (PAS de `delete img`). Documenté dans `Pong/Render/Texture2D.cpp:86-87`, à hoister dans `NkImage.h` près de `Alloc()`.
- ⚠️ **`Free()` sur une instance VALEUR = `c0000374`, et c'est DÉJÀ ARRIVÉ deux fois** (relevé le 2026-08-15). L'entrée ci-dessus donnait le pattern du tas comme *le* pattern sans dire ce qui suit — c'est la moitié manquante, et c'est celle qui a coûté un crash. `Free()` fait `nkFree(mPixels)` **puis `nkFree(this)`** (`NkImage.cpp:1468-1473`) : appelée sur une `NkImage` de pile ou de membre, elle rend à l'allocateur **une adresse qui ne lui appartient pas**. Constaté deux fois par Rihen — voir le commentaire sur place `NK3DModeler/Viewport/NkDemo3D.cpp:11366` : l'application se fermait net juste après l'écriture du fichier. **Sur une valeur : `Unload()`, jamais `Free()`.** Aucun garde ne distingue les deux cas ; le contrat n'existe qu'en commentaire (`NkImage.cpp:1460-1462`).
- ⚠️ **Ce piège n'est pas évitable par discipline de site.** `Convert`, `Copy`, `CopyAs`, `Crop`, `Resize` sont des méthodes **d'instance** qui retournent un `NkImage *` **du tas**. Une instance valeur **fabrique donc des instances tas**, et les deux modèles cohabitent dans la même expression — c'est pourquoi 10 fichiers du dépôt sont mixtes, sans négligence de leur auteur. Recensement complet (2026-08-15, commit `4a940717`) : **120 appels `Free()` dans 66 fichiers** pour la voie tas, **29 fichiers** pour la voie valeur. ✅ **ARBITRÉ par Rodolf le 2026-08-16 : voie unique = la VALEUR, `Free()` supprimée.** Migration exécutée le 2026-08-16 — voir la section « Migration vers la valeur » ci-dessous, et `DETTE_LISIBILITE.md` chantier 12.
- **Buffer `out` de `NkXxxCodec::Encode`** : alloué via `NkAlloc`, libérer avec `nkentseu::memory::NkFree(out)` depuis `NKMemory/NkAllocator.h`. JAMAIS `std::free` / `delete[]`.

---

## Migration vers la valeur (2026-08-16) — ce qui a été tranché, et pourquoi

**Décision de Rodolf** : `NkImage` devient un **type valeur pur**. `Free()` est
**supprimée** de l'API ; l'expression fautive `img.Free()` n'existe donc plus
*comme expression*, et le compilateur signale tous les anciens sites.

### Les 5 productrices : `Convert` / `Copy()` / `CopyAs` / `Crop` / `Resize`

Question laissée ouverte par le recensement : elles retournaient un `NkImage *`
du tas depuis une méthode **d'instance `const`** — c'est *elles* qui faisaient
qu'une instance valeur fabriquait une instance tas, et donc elles qui ont produit
les fichiers mixtes. Deux issues possibles : **muter en place** (précédent
`sf::Image`) ou **rendre par valeur**.

**Retenu : le RETOUR PAR VALEUR.** Quatre raisons, toutes mesurées :

1. **C'est le seul choix qui respecte le critère de l'arbitrage** (« l'appel
   erroné ne doit pas compiler »). Avec un retour par valeur, `NkImage *r =
   img.Crop(...)` ne compile plus : *tous* les sites sont attrapés. Avec la
   mutation en place, l'affectation est attrapée elle aussi — **mais l'appel en
   instruction nue `img.Crop(0,0,w,h);` compile AVANT comme APRÈS, avec le sens
   INVERSE** : aujourd'hui il fuit un clone et laisse `img` intacte ; muté, il
   **détruit `img`**. Le compilateur ne peut pas voir ce renversement. La mutation
   troquerait 120 erreurs de compilation contre un nombre inconnu de changements
   de comportement silencieux.
2. **`Copy() const` n'a aucun sens en place** : elle clone `*this`: muter `*this`
   en une copie de lui-même est un no-op.
3. **⭐ Le précédent `sf::Image` est déjà honoré ailleurs dans la classe — le
   suivre ici SUPPRIMERAIT une capacité.** `NkImage` n'a jamais choisi entre les
   deux conceptions : elle porte **les deux**. La moitié « mutante » de SFML
   existe déjà, séparément, et rend `bool` : `Copy(const NkImage &src, x, y, area,
   clip)` (`NkImage.cpp:2417`) et `CopyTo(NkImage &dst) const` (`:2359`). Or
   `CopyTo` **exige le même format ET les mêmes dimensions** (`:2362-2365`), alors
   que `Crop`, `Resize` et `Convert` changent précisément la taille ou le format.
   La moitié mutante **ne peut donc pas exprimer** les 5 productrices. On garde
   les deux moitiés : `bool` pour muter, valeur pour produire.
4. **L'échec devient plus sûr, pas moins.** `sf::Image` laisse l'image *à moitié
   transformée* quand une transformation échoue. Par valeur, l'échec rend une
   image **invalide** (`IsValid()==false`, il n'y a plus de `nullptr` à tester) et
   **la source n'est jamais touchée**.

**Sûreté vérifiée avant de trancher, pas supposée** : **0** classe ne dérive de
`NkImage` dans le worktree → le retour par valeur d'un type polymorphe ne peut pas
trancher (*slicing*). Le move-ctor existe et transfère bien `mOwning`
(`NkImage.cpp:1417-1425`) ; la NRVO supprime la copie dans le cas courant.

&gt; Référence lue, non copiée : `SFML-master/include/SFML/Graphics/Image.hpp`
&gt; (`REFERENCES_OSS.md`). `sf::Image` est un type valeur pur, ce qui **confirme**
&gt; la décision de fond ; mais son choix de la *mutation en place* pour les
&gt; transformations est **explicitement refusé ici**, pour la raison 3.
&gt; SFML s'appuie sur la STL : rien n'est transposable mécaniquement en zero-STL.

### Effets de bord utiles constatés pendant la migration

- `NkImage::Create` (instance) et `LoadFromMemoryImpl` faisaient un **« vol de
  buffer » manuel** (recopie des 7 membres, `tmp->mOwning=false`, `tmp->Free()`).
  Les deux se réduisent à un `*this = traits::NkMove(tmp);`.
- Ces deux méthodes libéraient l'ancien buffer **avant** de savoir si la nouvelle
  image était bonne : en cas d'échec elles laissaient `*this` avec `mPixels=nullptr`
  mais `mWidth`/`mHeight` inchangés — un objet incohérent. **Corrigé au passage** :
  en cas d'échec `*this` est désormais laissée **intacte**.
- `CopyAs()` et `NkImage::ConvertToTexture()` n'ont **aucun appelant** dans tout le
  worktree (contre-épreuvé). `Copy()` n'en a **qu'un, interne**. Elles sont migrées
  quand même, mais leur coût de migration est nul.

---

## Dépendances
- **Couches en dessous (utilisées)** : NKCore (types), NKMemory (NkAllocator, NkAlloc/NkFree), NKContainers (NkVector), NKFileSystem (NkFile), NKPlatform (macros API)
- **Modules au-dessus qui en dépendent** : NKRenderer (chargement textures), NKUI (icônes, atlas), NKFont (atlas glyphes via NkImage), assets pipeline
