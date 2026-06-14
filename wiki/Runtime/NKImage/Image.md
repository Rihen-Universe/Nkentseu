# Les images CPU

> Couche **Runtime** · NKImage · Charger, créer, transformer et dessiner des images **côté
> processeur** : le conteneur autonome `NkImage`, le tampon binaire des codecs `NkImageStream`,
> et le moteur DEFLATE/zlib `NkDeflate`.

Avant qu'une texture n'atterrisse sur le GPU, avant qu'un sprite ne soit empaqueté dans un atlas,
avant qu'une icône ne soit sauvée sur disque, il y a **un tableau de pixels en RAM**. C'est tout le
rôle de NKImage : posséder ce tableau, savoir le **décoder** depuis un fichier (PNG, JPEG, BMP, HDR,
QOI…), le **manipuler** (redimensionner, recadrer, retourner, convertir de format), le **dessiner**
en logiciel (lignes, cercles, ellipses), et le **réencoder** vers le disque ou la mémoire. Ce n'est
**pas** une texture GPU : NKImage ne connaît ni OpenGL ni Vulkan, il ne fait que tenir des octets et
les transformer sur le CPU — c'est la couche d'au-dessus (NKCanvas, NKRenderer) qui les téléverse.

Toute la difficulté pratique de NKImage tient en **un seul sujet** : *qui possède la mémoire*. Le
module offre **deux familles d'API** qui ne se libèrent pas de la même façon, et les confondre mène
droit à la corruption de tas Windows (`c0000374`). Cette page vous apprend à ne jamais vous tromper.

- **Namespace** : `nkentseu` (tout est directement dans ce namespace, pas de sous-namespace `image`)
- **Headers parapluie** : `#include "NKImage/Core/NkImage.h"` (API) et
  `#include "NKImage/Core/NkImageExport.h"` (macros)
- `NkImage` hérite de `NKIResource` (`NKStream/NKIResource.h`) ; dépend de `NKMath` (`math::NkColor`,
  `math::NkIntRect`) et `NKCore/NkTypes.h`.

---

## Les deux familles d'API : `NkImage*` contre `*this`

La même classe `NkImage` se manipule de deux manières opposées, et c'est **le** point à comprendre
en premier.

La **famille instance** opère sur un objet que vous avez déclaré vous-même, le plus souvent sur la
**pile**. Ses méthodes de chargement retournent un `bool` (succès/échec) et écrivent **dans
`*this`**, en libérant au passage l'ancien tampon de pixels s'il y en avait un. C'est l'usage
quotidien, le plus simple :

```cpp
NkImage img;                       // sur la pile, vide
if (img.Load("hero.png")) {        // remplit *this, retourne bool
    upload(img.Pixels(), img.Width(), img.Height());
}                                   // destructeur libère les pixels automatiquement
```

La **famille statique** est faite de **fabriques** qui allouent un `NkImage` **sur le tas** et vous
en rendent un **pointeur** `NkImage*`. Cette fois c'est **vous le propriétaire** : il faut le rendre
explicitement, non pas avec `delete`, mais avec la méthode `Free()` de l'objet — qui libère à la
fois les pixels **et** le struct lui-même.

```cpp
NkImage* tex = NkImage::Create(256, 256, NkImagePixelFormat::NK_RGBA32);
// ... usage ...
tex->Free();                       // libère pixels + struct (jamais delete, jamais sur une pile)
```

La règle à graver : **`Free()` ne se dit que sur une image issue d'une fabrique statique** (`Create`,
`Alloc`, `Wrap`, `Copy`, `CopyAs`, `Convert`, `Resize`, `Crop`, `ConvertToTexture`). L'appeler sur
une instance pile reviendrait à faire `nkFree(this)` sur la pile — crash garanti. Pour vider une
instance pile sans la détruire, il y a `Unload()`, sûr partout.

Enfin, la **copie par valeur est interdite** (`= delete`) pour empêcher tout double-free : on
duplique explicitement avec `Copy()`, ou on **déplace** (`NkImage&& `, move-assign).

> **En résumé.** API instance → retourne `bool`, écrit dans `*this`, le destructeur nettoie ;
> typique sur la pile (`NkImage img; img.Load(...)`). API statique → retourne `NkImage*` que **vous
> possédez** et rendez via `->Free()` (jamais `delete`, jamais sur une pile). Pas de copie par
> valeur : `Copy()` ou move.

---

## Décrire un pixel : formats et canaux

Un `NkImage` connaît son **format pixel** (`NkImagePixelFormat`), qui dit combien de canaux et sous
quelle précision. Côté LDR (8 bits par canal) on a `NK_GRAY8` (1 octet), `NK_GRAY_A16` (gris +
alpha, 2 o), `NK_RGB24` (3 o) et `NK_RGBA32` (4 o, le format de travail par défaut). Côté **HDR**
(flottant 32 bits par canal) on a `NK_RGB96F` (12 o) et `NK_RGBA128F` (16 o), pour les
environnements lumineux et les fichiers `.hdr`/`.exr`.

Deux fonctions libres répondent aux questions de base sans toucher d'objet : `ChannelsOf(fmt)` donne
le nombre de canaux logiques (RGBA32 → 4, GRAY8 → 1…) et `BytesPerPixelOf(fmt)` les octets par
pixel (RGBA128F → 16, RGB24 → 3…). Les deux sont `constexpr` et `O(1)`.

À ne pas confondre avec le **format de fichier** (`NkImageFormat`) : PNG, JPEG, BMP, TGA, HDR, PPM,
PGM, PBM, QOI, GIF, ICO, SVG (rastérisé), EXR. C'est le conteneur sur disque ; il est **détecté
automatiquement** par les *magic bytes* au chargement, et `SourceFormat()` vous rappelle d'où venait
l'image. La précision interne reste décrite par `NkImagePixelFormat`, indépendamment.

> **En résumé.** `NkImagePixelFormat` = précision/canaux en RAM (LDR 8-bit : GRAY8/GRAY_A16/RGB24/
> RGBA32 ; HDR float : RGB96F/RGBA128F). `NkImageFormat` = conteneur fichier (PNG, JPEG…), détecté
> par magic bytes. `ChannelsOf` / `BytesPerPixelOf` répondent en `O(1)`.

---

## Charger, sauvegarder, encoder en mémoire

Charger se fait soit depuis un **chemin** (`Load`, ou l'override `LoadFromFile` de `NKIResource`),
soit depuis un **bloc mémoire** déjà en RAM (`LoadFromMemory`, utile pour un asset embarqué ou reçu
du réseau), soit depuis un **flux** (`LoadFromStream`). Le paramètre `desiredChannels` permet de
**forcer** la conversion à l'arrivée : `0` garde le format natif du fichier, `1`–`4` impose le
nombre de canaux. Sur Android, `Load` tente automatiquement l'`AAssetManager` si l'ouverture fichier
échoue.

Sauvegarder sur disque passe par `Save(path, quality)`, qui **déduit le format de l'extension**
(`.png`, `.jpg`, `.bmp`, `.tga`, `.ppm`, `.hdr`, `.qoi`), `quality` n'ayant d'effet que pour le
JPEG. Des variantes nommées existent (`SavePNG`, `SaveJPEG`…) si vous voulez forcer un encodeur sans
dépendre de l'extension.

Pour produire les octets **sans toucher au disque** — empaqueter dans une archive, envoyer sur le
réseau, mettre en cache — la famille `EncodePNG/JPEG/BMP/TGA/QOI` remplit un buffer fraîchement
alloué. **Attention au point crucial de la mémoire** : ce buffer est alloué via `NkAlloc`, donc il
se libère avec `nkentseu::memory::NkFree(out)` — **jamais** `std::free` ni `delete[]`, sous peine de
corruption de tas (`c0000374`). C'est la même règle pour `SaveToMemory` et pour
`NkImageStream::TakeBuffer`.

```cpp
uint8* png = nullptr; usize n = 0;
if (img.EncodePNG(png, n)) {
    network.Send(png, n);
    nkentseu::memory::NkFree(png);     // surtout PAS std::free / delete[]
}
```

> **En résumé.** Charger : `Load`/`LoadFromMemory`/`LoadFromStream`, `desiredChannels=0` natif ou
> `1–4` forcé. Sauver disque : `Save` (format par extension) ou variantes nommées. Encoder en RAM :
> `EncodePNG/JPEG/BMP/TGA/QOI` → buffer **NkAlloc** à libérer avec **`memory::NkFree`**, jamais
> `std::free`.

---

## Transformer, blitter, dessiner

Deux styles de transformation cohabitent. Celles qui **fabriquent une nouvelle image** retournent un
`NkImage*` que vous possédez (`->Free()`) : `Convert` (changer de format pixel), `Resize` (avec un
filtre `NkResizeFilter` : nearest, bilinéaire, bicubique, Lanczos3), `Crop` (sous-région), `Copy`
(clone profond) et `CopyAs` (clone + conversion). Celles qui agissent **en place** modifient `*this`
sans rien allouer : `FlipVertical`, `FlipHorizontal` et `PremultiplyAlpha` (à faire **avant** de
téléverser au GPU pour un *blending* correct — destructif, RGBA32 seulement).

Le **blit** copie des régions d'une image vers une autre. `Blit` recopie une source entière à une
position ; `BlitRegion` est la version générale qui sait **redimensionner** (rescale bilinéaire) si
les régions source et destination diffèrent en taille. La contrainte invariante : **même format
pixel** des deux côtés, débordements clippés silencieusement. `CopyTo` est plus strict — il exige
mêmes **format ET dimensions**.

Enfin, NKImage embarque un petit **rastériseur logiciel** : `SetPixel` (écrase), `BlendPixel`
(composite *src-over*), `Fill`, `DrawLine` (Bresenham), `DrawRect`/`FillRect`, `DrawCircle`/
`FillCircle`, `DrawEllipse`/`FillEllipse`. Tout cela travaille **uniquement en LDR 8 bits** et est
*no-op* sur une image HDR ou hors bornes. Ce n'est **pas** un moteur de rendu 2D accéléré — c'est de
quoi générer une texture de debug, un atlas procédural, une icône, ou pré-dessiner un masque.

> **En résumé.** Nouvelles images → `Convert/Resize/Crop/Copy/CopyAs` (possédées, `->Free()`).
> En place → `Flip*`, `PremultiplyAlpha`. Blit → même format requis ; `BlitRegion` sait rescaler ;
> `CopyTo` exige format **et** dimensions identiques. Dessin CPU LDR-only, *no-op* sur HDR.

---

## Aperçu de l'API

Tous les éléments publics, en un coup d'œil. Chacun est détaillé dans la « Référence complète ».
Complexités entre crochets quand elles importent.

### Enums et fonctions libres

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Format pixel | `NkImagePixelFormat` : `NK_UNKNOWN/GRAY8/GRAY_A16/RGB24/RGBA32/RGBA128F/RGB96F` | Précision et canaux en RAM (LDR 8-bit / HDR float). |
| Format fichier | `NkImageFormat` : `PNG/JPEG/BMP/TGA/HDR/PPM/PGM/PBM/QOI/GIF/ICO/SVG/EXR` | Conteneur sur disque (détecté par magic bytes). |
| Filtre | `NkResizeFilter` : `NK_NEAREST/BILINEAR/BICUBIC/LANCZOS3` | Interpolation de `Resize`/`BlitRegion` (défaut bilinéaire). |
| Libre | `ChannelsOf(f)` `[O(1) constexpr noexcept]` | Nombre de canaux logiques. |
| Libre | `BytesPerPixelOf(f)` `[O(1) constexpr noexcept]` | Octets par pixel. |

### `NkImage` — conteneur image CPU

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Cycle de vie | `NkImage()`, `~NkImage()`, move-ctor, move-assign | Vide ; destructeur libère les pixels si owning ; déplacement `O(1)`. Copie **interdite**. |
| Création (instance) | `Create(w, h, color, desiredChannels=4)` `[O(w·h)]` | (Ré)initialise `*this`, rempli d'une couleur. |
| Chargement (instance) | `Load`, `LoadFromFile`, `LoadFromMemory` (×3), `LoadFromStream` | Décode dans `*this` ; `desiredChannels` 0=natif, 1–4=forcé. |
| Fabriques (statique) | `Create` (×2), `Alloc`, `Wrap`, `ConvertToTexture` | Allouent un `NkImage*` possédé → `->Free()`. `Wrap` = vue non-owning. |
| Sauvegarde disque | `Save`, `SavePNG/JPEG/BMP/TGA/PPM/HDR/QOI`, `SaveToFile/Stream` | Écrit sur disque ; format par extension pour `Save`. |
| Sauvegarde (stubs) | `SaveGIF`, `SaveWebP`, `SaveSVG` | **Non implémentés** (retournent false). |
| Encodage mémoire | `EncodePNG/JPEG/BMP/TGA/QOI`, `SaveToMemory` | Buffer **NkAlloc** → libérer avec `memory::NkFree`. |
| En place | `FlipVertical`, `FlipHorizontal`, `PremultiplyAlpha` | Retourne / pré-multiplie alpha (RGBA32, destructif). |
| Nouvelles images | `Convert`, `Resize`, `Crop`, `Copy`, `CopyAs` | Retournent un `NkImage*` possédé → `->Free()`. |
| Blit / copie | `Blit`, `BlitRegion` (×2), `Copy(src,…)`, `CopyTo` | Copies de régions ; même format requis. |
| Métadonnées | `Pixels`, `Width`, `Height`, `Channels`, `BytesPP`, `Stride`, `Format`, `SourceFormat`, `IsValid`, `IsHDR`, `TotalBytes`, `RowPtr` | Accesseurs inline. |
| Dessin CPU | `SetPixel`, `GetPixel`, `BlendPixel`, `Fill`, `DrawHLine/VLine/Line`, `DrawRect/FillRect`, `DrawCircle/FillCircle`, `DrawEllipse/FillEllipse` | Rastérisation logicielle LDR 8-bit (no-op sur HDR). |
| Mémoire | `Free`, `Unload` | `Free` = pixels **+** struct (fabriques statiques) ; `Unload` = pixels seuls (sûr sur pile). |

### `NkImageStream` — tampon binaire des codecs

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Construction | `NkImageStream(data, size)`, `NkImageStream()`, `~NkImageStream()` | Mode lecture (non-owning) / mode écriture (dynamique). |
| Lecture | `ReadU8/U16BE/U16LE/U32BE/U32LE/I16BE/I32LE`, `ReadBytes`, `Skip`, `Seek` | Lit des entiers (endianness explicite par méthode) et des octets. |
| État lecture | `Tell`, `Size`, `IsEOF`, `HasBytes`, `HasError`, `Ptr` | Position, taille, fin, erreur, pointeur courant. |
| Écriture | `WriteU8/U16BE/U16LE/U32BE/U32LE/I32LE`, `WriteBytes`, `WriteSize` | Écrit dans le buffer dynamique (doublage de capacité). |
| Transfert | `TakeBuffer(out, size)` | Cède la propriété du buffer → libérer avec `memory::NkFree`. |

### `NkDeflate` — compression DEFLATE/zlib

| Catégorie | Élément | Rôle |
|-----------|---------|------|
| Décompression | `Decompress(in, inSz, out, outCap, written)` `[static noexcept]` | zlib RFC 1950 (en-tête + Adler-32) ; `out` **pré-alloué**. |
| Décompression | `DecompressRaw(in, inSz, out, outCap, written)` `[static noexcept]` | DEFLATE brut RFC 1951 (sans en-tête ni checksum). |
| Compression | `Compress(in, inSz, out, outSz, level=6)` `[static noexcept]` | zlib *stored blocks* ; `out` **NkAlloc** → `memory::NkFree` ; `level` ignoré. |

---

## Référence complète

Chaque élément est repris ici en détail : comportement, complexité, et usages **par domaine** du
temps réel — rendu, ECS, animation, gameplay/IA, audio, UI/2D, IO/réseau, GPU, threading, outils.
Les éléments triviaux sont décrits brièvement ; ceux qui portent un piège ou un vrai choix le sont à
fond.

### Le modèle mémoire : stride, ownership, `Free` contre `Unload`

Avant toute méthode, le **modèle de données**. Un `NkImage` tient un pointeur `mPixels`, des
dimensions `mWidth`/`mHeight`, un **stride** (octets par ligne, **aligné à 4** : `stride =
(w*bpp+3)&~3`), un format pixel, le format fichier d'origine, et un drapeau `mOwning`. Le stride
aligné explique pourquoi il faut **toujours** parcourir l'image via `RowPtr(y)` et `Stride()`, et
jamais supposer `width*bpp` octets par ligne — sinon on désaligne tout le balayage.

L'**ownership** est binaire. Une image normale est *owning* : son destructeur libère `mPixels`. Une
vue créée par `Wrap()` est *non-owning* : ni le destructeur ni `Free()` ne touchent au buffer, c'est
à vous de le maintenir en vie.

La distinction `Free()` / `Unload()` est la source d'erreur n°1 du module :

- **`Free()`** libère les pixels (si owning) **et** le struct `NkImage` lui-même via `nkFree`. Il ne
  se dit que sur une image **issue d'une fabrique statique**. Sur une instance pile, ce serait un
  free de la pile (crash).
- **`Unload()`** (override `NKIResource`) libère seulement les pixels et remet `*this` en « image
  vide », **sans** libérer le struct. Sûr sur pile comme sur tas ; un `Load` ultérieur réinitialise
  proprement.

À noter : le **destructeur n'appelle pas `Free()`** — il libère les pixels mais pas le struct (le
struct est sur la pile ou détruit par celui qui l'a alloué). Le vptr de la vtable virtuelle est
garanti car les fabriques utilisent un *placement new*.

- **GPU / rendu** — après `Load`, on lit `Pixels()` + `Stride()` ligne par ligne pour téléverser une
  texture ; pour un *blending* correct, `PremultiplyAlpha()` d'abord.
- **Outils / éditeur** — un *asset manager* garde des `NkImage*` issus de fabriques et les
  `->Free()` au déchargement ; les vues `Wrap()` exposent une sous-image sans recopie.
- **Threading** — aucune synchronisation n'est fournie : une même instance n'est **pas** thread-safe.
  Plusieurs threads ne doivent pas écrire le même `NkImage` sans verrou externe.

### Cycle de vie : constructeurs, move, copie interdite

`NkImage()` construit une image **invalide** (pixels null, w=h=0). Le **move-ctor** transfère le
contenu sans copie et laisse la source valide-mais-vide (`IsValid()==false`) ; le **move-assign**
libère d'abord le buffer existant puis transfère, avec auto-affectation sûre (`this==&other` testé).
La **copie par valeur est `= delete`** (ctor et `operator=`) : c'est l'anti double-free. Pour
dupliquer, on passe par `Copy()` (clone profond) ou un *move*. Tout cela est `noexcept`.

- **ECS** — un composant qui contient un `NkImage` se déplace donc proprement quand l'archétype est
  réagencé ; il ne se copie jamais par accident.

### `Create` (instance) — fabriquer dans `*this`

`Create(w, h, color, desiredChannels=4)` (ré)initialise `*this`, libère l'ancien buffer, et le
remplit d'une `math::NkColor`. `desiredChannels` va de 1 à 4 (défaut 4 → RGBA32). Retourne `bool`,
`O(w·h)`. C'est la voie pour une image de travail neuve sur la pile (masque, *render target* CPU,
canevas de debug).

### Chargement (instance) : `Load`, `LoadFrom*`

`Load(path, desiredChannels=0)` lit depuis le disque (et tente l'`AAssetManager` Android si `fopen`
échoue). `LoadFromMemory` existe en plusieurs surcharges : la 2-args (`data`, `size`) garde les
canaux natifs et délègue ; la 3-args (`data`, `size`, `desiredChannels`) — en `void*` ou `uint8*` —
est l'implémentation réelle, qui attend `size >= 4`. Les overrides `NKIResource` (`LoadFromFile`,
`LoadFromMemory` 2-args, `LoadFromStream`) ne font que déléguer. `desiredChannels` : `0` natif, `1`
à `4` conversion forcée.

> **Piège de résolution de surcharge.** Les versions « riches » (3-args) **n'ont volontairement pas
> de valeur par défaut** : c'est ce qui permet à `LoadFromMemory(data, size)` (2-args) de résoudre
> proprement vers l'override d'interface, sans ambiguïté ni *name-hiding* (la classe serait abstraite
> sinon).

- **IO / réseau** — un asset reçu en mémoire (téléchargé, décompressé d'une archive) se décode
  directement via `LoadFromMemory`, sans passer par un fichier temporaire.
- **Rendu** — chargement de textures depuis le pack d'assets au démarrage de la scène.

### Fabriques statiques : `Create`, `Alloc`, `Wrap`, `ConvertToTexture`

Toutes rendent un `NkImage*` **possédé** (`->Free()`), sauf la sémantique particulière de `Wrap`.

- `Create(w, h, desiredChannels=0, color=0)` — image sur le tas, remplie d'une couleur RGBA packée
  *big-endian* `0xRRGGBBAA`. `desiredChannels` : 0/4 → RGBA32, 1 → GRAY8, 2 → GRAY_A16, 3 → RGB24.
  `0x00000000` = transparent (*zero-fill*). nullptr si échec.
- `Create(w, h, fmt, color=0)` — même chose avec un `NkImagePixelFormat` explicite.
- `Alloc(w, h, fmt)` — image vide (pixels zeroed), pensée pour les **codecs** qui vont remplir
  ensuite. Possédée → `->Free()`.
- `Wrap(pixels, w, h, fmt, stride=0)` — **vue non-owning** sur un buffer existant : ni le destructeur
  ni `Free()` ne le libèrent (`stride=0` → calculé `w*bpp`). C'est vous qui gérez la durée de vie du
  buffer. Idéal pour exposer une zone mémoire (un *render target*, un mapping fichier) comme image
  sans recopie.
- `ConvertToTexture(hdrImage, exposure=1, gamma=2.2)` — **tone-mapping** HDR → RGBA32 LDR :
  `ldr = pow(clamp(hdr*exposure), 1/gamma)*255`. `hdrImage` doit être `IsHDR()` ; nullptr sinon.

- **Rendu / GPU** — `Alloc` pour reconstruire une image décodée, `ConvertToTexture` pour ramener un
  environnement `.hdr`/`.exr` en LDR affichable, `Wrap` pour donner une vue image d'un *framebuffer*
  logiciel sans copie.
- **Outils** — `Create` produit une texture de test (damier, couleur unie) en une ligne.

### Sauvegarde sur disque

`Save(path, quality=90)` déduit le format de l'extension (png, jpg/jpeg, bmp, tga, ppm/pgm, hdr,
qoi) ; `quality` n'agit que sur le JPEG. Les variantes nommées `SavePNG/SaveJPEG/SaveBMP/SaveTGA/
SavePPM/SaveHDR/SaveQOI` forcent un encodeur précis. Les overrides `NKIResource` `SaveToFile`
(délègue à `Save(path, 90)`) et `SaveToStream` (encode en PNG) complètent. **Non implémentés** :
`SaveGIF`, `SaveWebP`, `SaveSVG` (retournent false).

- **Outils / éditeur** — export d'une capture, d'un atlas généré, d'une *lightmap* bakée ; PNG pour
  le lossless, JPEG (`quality`) pour les photos, HDR pour les environnements.

### Encodage en mémoire

`EncodePNG/EncodeJPEG/EncodeBMP/EncodeTGA/EncodeQOI` (toutes `const noexcept`) remplissent un buffer
`out` **alloué via NkAlloc** : à libérer impérativement avec `nkentseu::memory::NkFree(out)`. Pareil
pour `SaveToMemory` (encode en PNG, lossless) et `SaveToStream`. Utiliser `std::free`/`delete[]`
provoque une **corruption de tas** (`c0000374`).

- **IO / réseau** — sérialiser une image pour l'envoyer sur un socket, l'empaqueter dans une archive,
  la mettre en cache disque/mémoire.
- **GPU** — capturer un *render target* (lu côté CPU) et l'encoder en PNG pour une *screenshot*.

### Transformations en place : `Flip*`, `PremultiplyAlpha`

`FlipVertical()` (axe horizontal) et `FlipHorizontal()` (axe vertical) retournent l'image, `O(w·h)`.
`PremultiplyAlpha()` pré-multiplie RGB par alpha — **destructif**, et ne s'applique qu'à `NK_RGBA32`.

- **GPU / rendu** — `FlipVertical` recale une image dont l'origine diffère entre le format fichier et
  la convention de texture (top-left vs bottom-left selon l'API). `PremultiplyAlpha` **avant
  l'upload** garantit un *blending* correct des bords semi-transparents (anti-halo).
- **UI / 2D** — pré-multiplier les sprites et glyphes alpha avant de les passer à l'atlas.

### Transformations renvoyant une nouvelle image

Toutes retournent un `NkImage*` possédé (`->Free()`), nullptr si l'image source est invalide.

- `Convert(newFmt)` — conversion de format ; si `newFmt==mFormat`, c'est un clone pur. HDR↔LDR par
  troncature / normalisation.
- `Resize(nw, nh, filter=NK_BILINEAR)` — redimensionne ; `NK_NEAREST` (rapide, pixel-art),
  `NK_BILINEAR` (défaut), `NK_BICUBIC`, `NK_LANCZOS3` (meilleure qualité, plus lent).
- `Crop(x, y, w, h)` — sous-région ; coords **entièrement** dans les bornes, sinon nullptr.
- `Copy()` — clone profond ; nullptr si `*this` invalide.
- `CopyAs(fmt)` — clone + conversion ; équivaut à `Copy()` si `fmt==mFormat` ; nullptr si invalide
  ou format inconnu.

- **Rendu** — `Resize` pour générer les niveaux de *mip* CPU, ou adapter une texture à une taille
  matérielle ; `Convert` pour ramener tout en RGBA32 avant l'upload.
- **Outils** — `Crop` découpe les sous-tuiles d'une *sprite sheet* ; `CopyAs` normalise des assets
  hétérogènes en un format unique.

### Blit et copies de régions

- `Blit(src, dstX, dstY)` — copie `src` **entière** dans `*this` à (dstX,dstY). Même format pixel
  requis, débordements clippés silencieusement.
- `BlitRegion(src, srcRegion, dstRegion[, filter])` — blit **général**. `srcRegion` vide
  (`w==0&&h==0`) → image src entière ; `dstRegion` vide → copie sans *scale* depuis son coin ;
  dimensions différentes → **rescale bilinéaire** (ou `filter`) dans la zone dst. Pré-condition :
  `*this` et `src` valides + même format ; clipping aux bornes ; rien à copier après clip → retourne
  `true` (pas une erreur) ; `false` si invalides ou formats différents.
- `Copy(src, dstX, dstY, area, clip=true)` — version **instance, sans allocation**. `area` vide →
  image entière ; `clip=true` clippe les débordements, `clip=false` échoue si ça dépasse.
  Pré-condition : valides + même format + `*this` assez grand.
- `CopyTo(dst)` — copie `*this` dans un `dst` existant ; exige **même format ET mêmes dimensions** ;
  laisse `dst` intact si échec.

- **UI / 2D** — composer un **atlas** : on `Blit` chaque glyphe/sprite à sa case ; `BlitRegion` quand
  il faut redimensionner à la volée.
- **Outils** — assemblage de planches, copie de patches, recomposition d'images.

### Accès aux métadonnées et aux pixels

Accesseurs inline `O(1)` : `Pixels()` (pointeur brut, mutable ou const), `Width()`, `Height()`,
`Channels()` (= `ChannelsOf(format)`), `BytesPP()` (= `BytesPerPixelOf(format)`), `Stride()`,
`Format()`, `SourceFormat()` (format du fichier d'origine), `IsValid()` (pixels non-null + w,h>0),
`IsHDR()` (RGB96F ou RGBA128F), `TotalBytes()` (= `stride*height`), et `RowPtr(y)` (début de la ligne
`y`, **tenant compte du stride**).

- **GPU** — le couple `Pixels()`/`Stride()` + `Width()`/`Height()` est tout ce qu'il faut pour
  décrire un *upload* ; `Format()`/`Channels()` choisit le format GPU cible.
- **Tout domaine** — `IsValid()` avant usage, `IsHDR()` pour brancher sur `ConvertToTexture`,
  `RowPtr` pour tout balayage manuel sûr.

### Dessin CPU (rastérisation logicielle)

Toutes ces méthodes travaillent **uniquement en LDR 8 bits** et sont *no-op* si l'image est invalide,
hors bornes, ou HDR. La couleur est une `math::NkColor`.

- `SetPixel(x, y, c)` — **écrase** (pas de blend), écrit jusqu'à `Channels()` octets, `O(1)`.
- `GetPixel(x, y)` — lit ; (0,0,0,0) si hors bornes/invalide/HDR ; pour <4 canaux, g/b répliquent r
  et a vaut 255, `O(1)`.
- `BlendPixel(x, y, c)` — composite **src-over** sur l'existant ; raccourci `SetPixel` si `c.a==255`,
  no-op si `c.a==0`, `O(1)`.
- `Fill(c)` — remplit toute l'image, `O(w·h)`.
- `DrawHLine`/`DrawVLine` — segments alignés (HLine ordonne x0/x1 si inversés).
- `DrawLine(x0,y0,x1,y1,c)` — Bresenham.
- `DrawRect`/`FillRect` — contour / plein (scanlines HLine) ; *no-op* si `w<=0||h<=0`.
- `DrawCircle`/`FillCircle` — contour *midpoint* 8-way / disque par scanlines (`math::NkSqrt`) ;
  *no-op* si `r<0`.
- `DrawEllipse`/`FillEllipse` — contour paramétrique (`math::NkCos`/`NkSin`, pas adaptatifs) /
  pleine par scanlines ; *no-op* si `rx<=0||ry<=0`.

- **Outils / éditeur** — générer une texture de debug procédurale (damier, grille, dégradé), un
  curseur, une icône simple, un nuancier.
- **UI / 2D** — pré-rendre un masque, un cadre, une *spark* avant de l'envoyer comme texture.
- **Gameplay** — dessiner une *minimap* CPU, un graphe de debug, une heatmap, qu'on téléverse ensuite.

### Mémoire : `Free`, `Unload`

Détaillés en tête de référence. `Free()` = pixels (si owning) **+** struct, réservé aux fabriques
statiques. `Unload()` (override `NKIResource`) = pixels seuls + remise à vide, sûr partout. Ne jamais
`Free()` une instance pile (double-free / free de pile).

### `NkImageStream` — le tampon des codecs

`NkImageStream` est le flux binaire bas niveau sur lequel s'appuient les codecs ; vous le croiserez
surtout en écrivant un nouveau codec. Il a **deux modes**. En **lecture**, on le construit sur
`(data, size)` : il est **non-owning** et une lecture hors bornes lève `mError=true` et renvoie 0.
En **écriture**, on le construit vide : il fait croître un buffer dynamique via `nkRealloc`
(doublage de capacité dès 4096 o) ; on récupère le résultat avec `TakeBuffer(out, size)` (qui cède
la propriété → libérer avec `memory::NkFree`).

Les lectures couvrent toutes les largeurs et les deux endianness, **explicites par méthode** : `BE`
(*big-endian*) pour PNG/JPEG, `LE` (*little-endian*) pour BMP/TGA/QOI/EXR — `ReadU8`, `ReadU16BE/LE`,
`ReadU32BE/LE`, `ReadI16BE`, `ReadI32LE`, plus `ReadBytes(dst, n)` (`dst` peut être nullptr pour
**avancer le curseur**), `Skip(n)` et `Seek(pos)`. Les accesseurs d'état (`Tell`, `Size`, `IsEOF`,
`HasBytes`, `HasError`, `Ptr`) renseignent la position et l'erreur. Côté écriture, symétriquement :
`WriteU8`, `WriteU16BE/LE`, `WriteU32BE/LE`, `WriteI32LE`, `WriteBytes`, plus `WriteSize()`.

- **IO / outils** — c'est l'unité de base pour parser ou émettre un en-tête de fichier image : on lit
  un magic, une largeur *big-endian*, on saute un chunk, etc. L'endianness explicite évite les bugs
  de portabilité.

> **Attention.** Le destructeur d'un stream **écriture** ne libère **pas** son buffer : il faut le
> récupérer avec `TakeBuffer` (puis `memory::NkFree`), sinon fuite mémoire.

### `NkDeflate` — DEFLATE / zlib

`NkDeflate` est le moteur de compression sans perte utilisé par le codec PNG (adapté de stb_image
v2.16, domaine public). L'*inflate* est **complet** (DEFLATE RFC 1951 + zlib RFC 1950) ; le *deflate*
est **minimal en blocs « stored »** (BTYPE=00, **pas de compression réelle**) — suffisant pour
produire des PNG valides, mais sans gain de taille. Trois méthodes publiques, toutes `static
noexcept` :

- `Decompress(in, inSz, out, outCap, written)` — décompresse du **zlib** (en-tête CMF/FLG +
  checksum Adler-32). Le buffer `out` est **pré-alloué par l'appelant** ; `written` reçoit le nombre
  d'octets écrits.
- `DecompressRaw(in, inSz, out, outCap, written)` — décompresse du **DEFLATE brut** (sans en-tête
  zlib ni checksum). Buffer `out` également pré-alloué.
- `Compress(in, inSz, out, outSz, level=6)` — encode en zlib *stored blocks*. Le buffer `out` est
  **alloué par NkDeflate** via `NkAlloc` → à libérer avec `memory::NkFree`. `level` est **ignoré**
  (stored uniquement).

- **IO / réseau** — réutiliser `Decompress`/`Compress` pour un format maison qui réemploie le
  conteneur zlib, ou pour décompresser des chunks PNG hors du codec. Penser que la **sortie de
  décompression est pré-allouée** : il faut connaître ou majorer la taille décompressée à l'avance.

> **Mémoire.** Décompression : `out` **pré-alloué** par vous (pas d'allocation interne).
> Compression : `out` alloué en interne via `NkAlloc`, à rendre avec `memory::NkFree`.

### Le socle commun

- **Tout passe par NKMemory.** Buffers encodés (`Encode*`, `SaveToMemory`), `NkImageStream::TakeBuffer`
  et `NkDeflate::Compress` allouent via `NkAlloc` → libérer **uniquement** avec
  `nkentseu::memory::NkFree`. Jamais `std::free`/`delete[]` (corruption de tas `c0000374`).
- **Deux familles, deux libérations.** Instance (pile) → destructeur ou `Unload()`. Fabrique
  statique (tas) → `->Free()`. Vue `Wrap()` → ne libère **rien** (buffer externe).
- **Pas de copie par valeur.** Toujours `Copy()` ou un *move*.
- **Pas de thread-safety.** Aucune primitive de synchronisation : une instance partagée nécessite un
  verrou externe.
- **`NkImage` est un `NKIResource`.** Les overrides `LoadFromFile/Memory/Stream`,
  `SaveToFile/Memory/Stream`, `IsValid`, `Unload` le rendent interchangeable partout où le moteur
  attend une ressource chargeable (voir [NKIResource](../../System/NKStream.md)).

---

### Exemple récapitulatif

```cpp
#include "NKImage/Core/NkImage.h"
using namespace nkentseu;

// 1. API instance (pile) : charger, manipuler, le destructeur nettoie tout seul.
NkImage img;
if (img.Load("hero.png", 4)) {          // forcé RGBA32
    img.FlipVertical();                 // recale l'origine pour la convention GPU
    img.PremultiplyAlpha();             // blending correct des bords
    upload(img.Pixels(), img.Width(), img.Height(), img.Stride());
}                                        // ~NkImage() libère les pixels

// 2. API statique (tas) : fabrique → on possède → on Free().
NkImage* thumb = img.Resize(64, 64, NkResizeFilter::NK_BILINEAR);
if (thumb) {
    uint8* png = nullptr; usize n = 0;
    if (thumb->EncodePNG(png, n)) {     // buffer NkAlloc
        cache.Store(png, n);
        nkentseu::memory::NkFree(png);  // surtout PAS std::free
    }
    thumb->Free();                       // pixels + struct
}

// 3. Texture de debug procédurale (rastériseur CPU LDR).
NkImage* dbg = NkImage::Create(128, 128, NkImagePixelFormat::NK_RGBA32);
dbg->Fill(math::NkColor{ 30, 30, 30, 255 });
dbg->DrawRect(8, 8, 112, 112, math::NkColor{ 255, 255, 0, 255 });
dbg->FillCircle(64, 64, 40, math::NkColor{ 0, 160, 255, 255 });
// ... upload puis ...
dbg->Free();

// 4. HDR → LDR affichable (tone-mapping).
NkImage env;
if (env.Load("studio.hdr") && env.IsHDR()) {
    NkImage* ldr = NkImage::ConvertToTexture(env, /*exposure*/ 1.2f, /*gamma*/ 2.2f);
    if (ldr) { upload(ldr->Pixels(), ldr->Width(), ldr->Height(), ldr->Stride()); ldr->Free(); }
}
```

---

[← Index NKImage](README.md) · [Récap NKImage](../NKImage.md) · [Couche Runtime](../README.md)
