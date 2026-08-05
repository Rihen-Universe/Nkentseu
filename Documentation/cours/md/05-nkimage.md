# NKImage : charger, fabriquer, afficher des images

Jusqu'ici, tout ce que nous avons affiché était fabriqué par le moteur :
rectangles, cercles, atlas de police. Ce chapitre ouvre la porte du monde
extérieur. `NKImage` est le module qui transforme un fichier
`.png` posé sur un disque en un tableau d'octets que le GPU sait
consommer — et, dans l'autre sens, un tableau d'octets en fichier.

C'est le premier des cinq modules d'intégration, et ce n'est pas un hasard :
c'est le plus simple, le plus stable, et surtout *les autres en dépendent*.
`NKMedia` s'en sert pour son codec MJPEG (qui n'est rien d'autre que le
codec JPEG de NKImage appliqué image par image), et `NKAudio` dépend de
`NKMedia`. L'ordre d'apprentissage est donc imposé par les dépendances :

**`Ordre imposé par les dépendances (voir les .jenga de chaque module)`**

```
NKImage  ─┬─> NKMedia ──> NKAudio
          │      (NKMedia depend de NKImage : codec MJPEG = codec JPEG de NKImage)
          │      (NKAudio depend de NKMedia : decodeur Opus)
          └─> NKFont     (independant : ne depend ni de NKImage ni de NkCanvas)

NKNetwork (totalement independant — peut venir n'importe ou)
```

## Ce qu'est NKImage, et ce qu'il n'est pas

### Aucune dépendance externe

La première chose à regarder, comme toujours, c'est le fichier de build :

**`Kernel/Runtime/NKImage/NKImage.jenga:25-29`**

```cpp
    nkentseudependson(
        ["NKPlatform", "NKCore", "NKMemory", "NKMath", "NKContainers", "NKLogger", "NKThreading", "NKFileSystem", "NKStream"],
        selfexport="NKImage",
        extra_includes=["src"],
    )
```

Que des modules de base. Pas de `libpng`, pas de `libjpeg`, pas de
`stb_image`. Le fichier de build le dit en toutes lettres
(`NKImage.jenga:6`) : « Bibliothèque self-contained sans dépendance
externe ». Chaque codec — PNG, JPEG, BMP, TGA, HDR, EXR, PPM, QOI, GIF, ICO,
WebP, SVG — est écrit dans le dépôt, y compris le `deflate` du PNG.

Cela a une conséquence pratique immédiate : **il n'y a rien à installer**.
Si le moteur compile, NKImage décode.

### NKImage ne connaît aucun GPU

Relisez la liste des dépendances : pas de `NKCanvas`, pas de
`NKRHI`, pas de `NKWindow`. NKImage produit et consomme des
*octets en mémoire vive*. Il ne sait pas ce qu'est une texture.

> **✅ Ce qu'il faut retenir**
>
> **NKImage produit des octets CPU ; le renderer produit un identifiant
> GPU.** Le point de couture entre les deux est toujours le même : un pointeur
> `const uint8*`, une largeur, une hauteur. Tout le reste de ce chapitre
> découle de cette phrase.

### L'arborescence

**`Kernel/Runtime/NKImage/ — arborescence`**

```
NKImage/
  NKImage.jenga
  src/NKImage/
    NKImage.h                         <- en-tete PARAPLUIE (tout inclure d'un coup)
    Core/
      NkImage.h        (1218 l.)      <- LE conteneur + NkImageStream + NkDeflate
      NkImage.cpp      (3034 l.)
      NkImageExport.h  (  58 l.)      <- macros NKENTSEU_IMAGE_API / NKIMG_INLINE
    Codecs/
      PNG/  JPEG/  BMP/  TGA/  HDR/  EXR/
      PPM/  QOI/   GIF/  ICO/  WEBP/  SVG/
```

Une seule classe compte pour ce chapitre : `NkImage`, dans
`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h`. Les codecs sont
appelables directement, mais on n'en a presque jamais besoin : `NkImage`
choisit le bon selon le contenu du fichier.

> **⚠️ L'exemple en tête de `NKImage.h` est périmé**
>
> Le commentaire de documentation en tête de l'en-tête parapluie
> (`Kernel/Runtime/NKImage/src/NKImage/NKImage.h:12-33`) contient un
> exemple `@code` qui utilise `NkSVGRenderer::RenderFromFile`,
> `NkSVGDOM` et `NkSVGDOMBuilder`. **Ces trois classes
> n'existent plus.** Le pipeline SVG a été remplacé par un codec unique, et le même
> fichier le dit vingt lignes plus bas :
>
> **`Kernel/Runtime/NKImage/src/NKImage/NKImage.h:53-54`**
>
> ```cpp
> // SVG : NkSVGDOM/Renderer ont ete remplaces par NkSVGCodec (codec unique).
> ```
>
> Ne recopiez jamais cet exemple : il ne compile pas. C'est le premier cas d'une
> règle que nous retrouverons dans chacun des cinq chapitres qui suivent :
> **quand un commentaire d'en-tête contredit le `.cpp`, c'est le
> `.cpp` qui a raison.**

## Les deux vocabulaires de formats

Avant toute chose, il faut distinguer deux choses que le débutant confond
systématiquement : le format des *pixels en mémoire* et le format du
*fichier sur le disque*.

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:67-75`**

```cpp
enum class NkImagePixelFormat : uint8 {
    NK_UNKNOWN = 0, NK_GRAY8 = 1, NK_GRAY_A16 = 2,
    NK_RGB24 = 3, NK_RGBA32 = 4, NK_RGBA128F = 5, NK_RGB96F = 6,
};
```

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:127-142`**

```cpp
enum class NkImageFormat : uint8 {                    // format de FICHIER
    NK_UNKNOWN=0, NK_PNG, NK_JPEG, NK_BMP, NK_TGA, NK_HDR,
    NK_PPM, NK_PGM, NK_PBM, NK_QOI, NK_GIF, NK_ICO, NK_SVG, NK_EXR,
};
```

Le premier décrit comment un pixel est rangé en RAM : un octet de gris
(`NK_GRAY8`), trois octets (`NK_RGB24`), quatre
(`NK_RGBA32`), ou quatre flottants 32 bits
(`NK_RGBA128F`, pour le HDR). Le second décrit l'enveloppe du fichier.
Un `.png` peut contenir du `NK_GRAY8` comme du
`NK_RGBA32` ; un `.hdr` contient forcément du flottant.

Deux fonctions `constexpr` font le pont :

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:78, 98`**

```cpp
constexpr int32 ChannelsOf(NkImagePixelFormat f) noexcept;
constexpr int32 BytesPerPixelOf(NkImagePixelFormat f) noexcept;
```

Enfin, le troisième énuméré du module gouverne le rééchantillonnage :

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:157-162`**

```cpp
enum class NkResizeFilter : uint8 {
    NK_NEAREST, NK_BILINEAR, NK_BICUBIC, NK_LANCZOS3,
};
```

Retenez ces quatre noms, nous y reviendrons : deux d'entre eux
mentent.

## Deux familles d'API, et pourquoi il faut choisir

C'est *le* point structurant du module, et la source de la moitié des
plantages des débutants. `NkImage` propose deux façons complètement
différentes de faire la même chose. Le commentaire de tête du fichier les nomme
lui-même :

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:7-36 (abrégé)`**

```
 1. API STATIQUE  -> retourne `NkImage*`  (ownership explicite, heap-alloue)
 2. API INSTANCE  -> retourne `bool`  (opere sur *this, aucune allocation visible)
```

### L'API instance : l'objet vit sur la pile

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:232, 242, 262, 282`**

```cpp
bool Create(uint32 width, uint32 height, math::NkColor color,
            int32 desiredChannels = 4) noexcept;
bool LoadFromFile(const char* path) override;
bool Load(const char* path, int32 desiredChannels = 0) noexcept;
bool LoadFromMemory(const void* data, usize size, int32 desiredChannels) noexcept;
```

Toutes ces méthodes renvoient un `bool` et travaillent sur
`*this`. On déclare une `NkImage` sur la pile, on l'utilise, et le
destructeur libère les pixels. **Il n'y a rien à écrire pour libérer.**

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:400-407 (abrégé)`**

```cpp
        NkImage img;
        bool ok = false;
        for (const char *p : kCandidates)
            if (img.Load(p, 4)) {
                ok = true;
                break;
            }
        if (ok && img.Width() > 0 && img.Height() > 0 && img.Pixels()) {
```

### L'API statique : vous possédez le pointeur

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:308, 317, 792, 804, 815`**

```cpp
static NkImage* Create(uint32 w, uint32 h, int32 desiredChannels = 0, uint32 color = 0) noexcept;
static NkImage* Create(uint32 w, uint32 h, NkImagePixelFormat fmt, uint32 color = 0) noexcept;
static NkImage* Alloc(int32 w, int32 h, NkImagePixelFormat fmt) noexcept;
static NkImage* Wrap(uint8* pixels, int32 w, int32 h, NkImagePixelFormat fmt, int32 stride=0) noexcept;
static NkImage* ConvertToTexture(const NkImage& hdrImage, float exposure=1.0f, float gamma=2.2f) noexcept;
```

Chacune renvoie un `NkImage*` alloué sur le tas. Vous devez appeler
`->Free()` dessus. Et ce n'est pas tout : **plusieurs méthodes
membres renvoient elles aussi une image neuve**, avec la même obligation :

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:396, 404, 475, 485, 523`**

```cpp
NkImage* Convert(NkImagePixelFormat newFmt) const noexcept;
NkImage* Resize(int32 nw, int32 nh, NkResizeFilter f = NkResizeFilter::NK_BILINEAR) const noexcept;
NkImage* Crop(int32 x, int32 y, int32 w, int32 h) const noexcept;
NkImage* Copy() const noexcept;
NkImage* CopyAs(NkImagePixelFormat fmt) const noexcept;
```

C'est le piège le plus courant du module : on charge sur la pile — bien —, puis
on appelle `Resize` — et on oublie que le résultat est un pointeur qu'il
faut libérer.

> **✅ La règle en une phrase**
>
> **Si la fonction renvoie `bool`, il n'y a rien à libérer. Si elle
> renvoie `NkImage*`, vous devez appeler `->Free()`.** Aucune
> exception dans tout le module.

### Copie interdite, déplacement autorisé

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:202-215`**

```cpp
NkImage(const NkImage&) = delete;                 // copie INTERDITE
NkImage& operator=(const NkImage&) = delete;
NkImage(NkImage&& other) noexcept;
NkImage& operator=(NkImage&& other) noexcept;
```

La raison est écrite juste au-dessus (`Core/NkImage.h:178-180`) : « La
copie par valeur est désactivée (`= delete`) pour éviter les
doubles-*free* accidentels. Utiliser `Copy()` (*deep clone*) ou
le *move constructor*. » Vous ne pouvez donc pas ranger une
`NkImage` dans un `NkVector` par copie ; utilisez le déplacement,
ou stockez des pointeurs.

## Charger une image

### La signature qui compte

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:262`**

```cpp
bool Load(const char *path, int32 desiredChannels = 0) noexcept;
```

Le second paramètre est le seul qui demande réflexion. `0` signifie
« garde le nombre de canaux du fichier » : un PNG en niveaux de gris donnera une
image `NK_GRAY8`, un PNG avec transparence donnera du
`NK_RGBA32`. C'est économe, mais cela veut dire que *vous ne savez
pas* ce que vous obtenez avant d'interroger `Format()`.

Passer `4` force la conversion en `NK_RGBA32`. C'est ce que fait
la démo, et c'est ce que vous ferez presque toujours dès qu'il s'agit d'envoyer
l'image au GPU : les backends veulent du RGBA, et faire la conversion à la
lecture évite d'écrire une deuxième passe.

### Les accesseurs

Une fois l'image chargée, tout se lit par des accesseurs `inline`
(`Core/NkImage.h:527-585`) :

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:527-585 (liste)`**

```
Pixels()       // uint8* : le premier octet de la premiere ligne
Width()  Height()
Channels()     // 1, 2, 3 ou 4
BytesPP()      // octets par pixel
Stride()       // octets par LIGNE  <- lire la section suivante
Format()       // NkImagePixelFormat courant
SourceFormat() // NkImageFormat du fichier d'origine
IsValid()  IsHDR()  TotalBytes()
RowPtr(y)      // uint8* sur la ligne y
```

### Le stride : la cause numéro un des images « penchées »

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:182-184`**

```
 STRIDE :
   Le stride (bytes par ligne) est aligne sur 4 octets : stride = (w*bpp+3)&~3.
   Utiliser RowPtr(y) pour acceder a la ligne y de facon portable.
```

Traduisons. Pour une image RGB24 de 5 pixels de large, une ligne fait
5 × 3 = 15 octets — mais le module en réserve 16, pour que chaque ligne
commence à une adresse multiple de 4. Il y a donc **un octet de bourrage
invisible à la fin de chaque ligne**.

Conséquence : `Pixels()` n'est pas un tableau contigu de
`width * height * bpp` octets. Un `memcpy` global qui suppose le
contraire produit une image décalée d'un pixel de plus à chaque ligne — l'effet
« penché » caractéristique. La parade est de recopier ligne par ligne :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:410-419`**

```cpp
            static NkVector<uint8> rgba;
            rgba.Resize(static_cast<usize>(imgW) * imgH * 4u);
            for (int32 y = 0; y < imgH; ++y) {
                const uint8 *src = img.RowPtr(y);
                uint8 *dst = rgba.Data() + static_cast<usize>(y) * imgW * 4u;
                for (int32 x = 0; x < imgW * 4; ++x)
                    dst[x] = src[x];
            }
```

> **⚠️ Quand le stride ne se voit pas**
>
> En RGBA32, `bpp = 4`, donc `w*4` est *toujours* multiple de 4
> et `stride == w*4`. Un `memcpy` global marchera. Puis un jour vous
> chargerez un JPEG en trois canaux, et tout se penchera — dans un code que vous
> n'avez pas touché. **Utilisez `RowPtr(y)` dès la première ligne que
> vous écrivez**, même quand ce n'est pas nécessaire ce jour-là.

## Fabriquer une image de toutes pièces

### Le squelette canonique de l'API statique

L'application `NKImageCodecTest` est le meilleur exemple court du dépôt.
Elle alloue une image, écrit les pixels à la main, et la sauvegarde dans neuf
formats :

**`Applications/NKImageCodecTest/src/main.cpp:15-27 (abrégé)`**

```cpp
    NkImage *img = NkImage::Alloc(W, H, NkImagePixelFormat::NK_RGB24);
    if (!img) {
        printf("[ERREUR] Alloc\n");
        return 1;
    }
    nk_uint8 *db = img->Pixels();
    const int st = img->Stride();
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            /* ... remplissage motif ... */
            nk_uint8 *p = db + (nk_size)y * st + x * 3;
            p[0] = r; p[1] = g; p[2] = b;
        }
    }
```

Remarquez l'arithmétique : `db + y * st + x * 3`. C'est `Stride()`
qui sert de pas vertical, jamais `Width() * 3`. Puis :

**`Applications/NKImageCodecTest/src/main.cpp:70-78 (abrégé)`**

```cpp
    for (const Fmt &f : formats) {
        const bool ok = img->Save(f.file, 100);
        printf("  %-12s : %s\n", f.file, ok ? "ecrit" : "ECHEC SAVE");
    }
    img->Free();
```

`Alloc` → `Pixels()` + `Stride()` →
`Save` → `Free`. Voilà le squelette complet.

### Créer une image remplie d'une couleur

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:232`**

```cpp
bool Create(uint32 width, uint32 height, math::NkColor color,
            int32 desiredChannels = 4) noexcept;
```

Cette signature a une histoire, et le dépôt l'a documentée sur les lieux du
crime :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkTexture.cpp:72-75`**

```cpp
                // NkImage::Create(w, h, NkColor color, int32 channels=4) : la COULEUR
                // d'abord, les canaux ensuite. (Avant : args inverses -> channels=0
                // pour un fill transparent -> Create echouait -> texture jamais creee.)
```

> **⚠️ Couleur d'abord, canaux ensuite**
>
> Il existe aussi une `Create` *statique* dont le troisième paramètre
> est le nombre de canaux et le quatrième la couleur — l'ordre inverse
> (`Core/NkImage.h:308`). Les deux compilent. L'une des deux ne fait pas ce
> que vous croyez. Vérifiez la famille d'API que vous employez : `bool` ou
> `NkImage*`.

### Dessiner dans une image (sur le processeur)

`NkImage` embarque un petit rasteriseur logiciel, entièrement
`inline` dans l'en-tête (`Core/NkImage.h:587-765`) — vous pouvez
donc lire son code sans quitter la déclaration :

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:587-765 (liste)`**

```
SetPixel  GetPixel  BlendPixel  Fill
DrawHLine  DrawVLine  DrawLine        // Bresenham
DrawRect   FillRect
DrawCircle FillCircle
DrawEllipse FillEllipse
```

Toutes prennent une `math::NkColor`. Un exemple complet, écrit pour ce
cours :

**`Exemple écrit pour le cours (API : Core/NkImage.h:232, 587-765, 349)`**

```cpp
#include "NKImage/Core/NkImage.h"
#include "NKMath/NkColor.h"
using namespace nkentseu;

NkImage canvas;
canvas.Create(640, 480, math::NkColor(20, 20, 28, 255), 4);
canvas.FillRect(40, 40, 200, 120, math::NkColor(255, 90, 0, 255));
canvas.DrawCircle(320, 240, 100, math::NkColor(0, 245, 255, 255));
canvas.DrawLine(0, 0, 639, 479, math::NkColor(255, 255, 255, 255));
canvas.SavePNG("dessin.png");
```

Deux limites, énoncées par le module lui-même :

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:587-590`**

```
 Formats LDR 8-bit (RGBA/RGB/RG/R). No-op si image invalide, hors
 bornes, ou HDR. Couleur = math::NkColor (= NkColor2D). SetPixel ECRASE
 (pas de blending) ; BlendPixel fait un src-over alpha.
```

« *No-op* si hors bornes » : dessiner à côté de l'image ne plante pas et ne
prévient pas. Et sur une image HDR, ces méthodes ne font strictement rien —
silencieusement.

## Écrire sur le disque

### Le dispatch par extension

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:348-360`**

```cpp
bool Save(const char* path, int32 quality = 90) const noexcept;   // dispatch par extension
bool SavePNG (const char* path) const noexcept;
bool SaveJPEG(const char* path, int32 quality = 90) const noexcept;
bool SaveBMP (const char* path) const noexcept;
bool SaveTGA (const char* path) const noexcept;
bool SavePPM (const char* path) const noexcept;
bool SaveHDR (const char* path) const noexcept;
bool SaveEXR (const char* path) const noexcept;   // EXR scanline FLOAT, compression NONE
bool SaveQOI (const char* path) const noexcept;
bool SaveGIF (const char* path) const noexcept;   // palette 256 (median-cut) + LZW
bool SaveWebP(const char* path, bool lossless = true, int32 quality = 90) const noexcept;
bool SaveSVG (const char* path) const noexcept;
```

`Save("photo.png")` appelle `SavePNG` ; `Save("photo.jpg", 80)` appelle `SaveJPEG` avec une qualité de 80. Le dispatch se lit dans
`Core/NkImage.cpp:2010-2030`.

### Les deux dernières lignes sont des mensonges

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.cpp:2119-2134`**

```cpp
    /**
     * SaveWebP — non implémenté.
     * Nécessiterait libwebp ou une implémentation custom VP8/VP8L.
     */
    bool NkImage::SaveWebP(const char *, bool, int32) const noexcept {
        return false;
    }

    /**
     * SaveSVG — non implémenté.
     * L'encodage raster → SVG (vectorisation) n'est pas dans le scope.
     */
    bool NkImage::SaveSVG(const char *) const noexcept {
        return false;
    }
```

> **⚠️ `SaveWebP` et `SaveSVG` renvoient toujours `false`**
>
> Ces deux méthodes sont déclarées, exportées, documentées — et vides. Elles ne
> plantent pas : elles renvoient `false` sans rien écrire. Si vous ne testez
> pas la valeur de retour, votre programme fera comme si tout allait bien.
>
> Corollaire moins évident : `Save("sortie.webp")` échoue aussi, parce que
> l'extension `webp` n'est pas dans le dispatch de `Save()`
> (`Core/NkImage.cpp:2010-2030`). Le fichier de test du dépôt le prouve
> malgré lui : `NKImageCodecTest` liste `ic_out.webp` dans ses
> formats attendus, et cette ligne affiche `ECHEC SAVE` à chaque exécution.
>
> Nuance importante : le *codec* WebP, lui, existe et fonctionne
> (`NkWebPCodec::Encode`, `Codecs/WEBP/NkWebPCodec.h:32`, chemin
> VP8L sans perte). C'est uniquement la méthode de commodité
> `NkImage::SaveWebP` qui est vide. Si vous avez vraiment besoin de WebP en
> écriture, appelez le codec directement.

### Encoder en mémoire

Parfois on ne veut pas de fichier : on veut les octets, pour les envoyer sur le
réseau ou les ranger dans une base.

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:369-373`**

```cpp
bool EncodePNG (uint8*& out, usize& size) const noexcept;
bool EncodeJPEG(uint8*& out, usize& size, int32 quality = 90) const noexcept;
bool EncodeBMP (uint8*& out, usize& size) const noexcept;
bool EncodeTGA (uint8*& out, usize& size) const noexcept;
bool EncodeQOI (uint8*& out, usize& size) const noexcept;
```

Le paramètre `out` est une *référence de pointeur* : la fonction
alloue le tampon et vous le rend. La question devient donc : qui le libère, et
avec quoi ?

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:31-36 (abrégé)`**

```
 REGLE DE MEMOIRE :
   Les buffers alloues via l'API statique DOIVENT etre liberes avec img->Free().
   Les buffers encodes (EncodePNG, EncodeJPEG, ...) DOIVENT etre liberes avec
   nkentseu::memory::NkFree(ptr).
   [...] l'allocateur custom NKMemory n'est pas compatible avec le heap CRT
   standard (crash c0000374 sur Windows en cas de melange).
```

`c0000374` est le code d'un tas Windows corrompu. Le plantage n'arrive
d'ailleurs pas au moment de la libération fautive, mais bien plus tard, dans une
allocation parfaitement innocente — d'où la difficulté à le diagnostiquer.

**`Exemple écrit pour le cours (API : Core/NkImage.h:369, NKMemory)`**

```cpp
uint8 *buf = nullptr;
usize  n   = 0;
if (src.EncodePNG(buf, n)) {
    /* ... envoyer buf/n sur le reseau, dans une base, ... */
    memory::NkFree(buf);          // et JAMAIS free() ni delete[]
}
```

## Transformer une image

### Redimensionner

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:404`**

```cpp
NkImage* Resize(int32 nw, int32 nh, NkResizeFilter f = NkResizeFilter::NK_BILINEAR) const noexcept;
```

Rappel : cela renvoie un `NkImage*`. L'usage complet :

**`Exemple écrit pour le cours (API : Core/NkImage.h:262, 404, 349, 775)`**

```cpp
NkImage src;
if (!src.Load("photo.jpg", 4))            // 4 = force RGBA32
    return 1;

NkImage *thumb = src.Resize(256, 256, NkResizeFilter::NK_BILINEAR);
if (!thumb) return 1;

thumb->Save("photo_thumb.png");           // format deduit de l'extension
thumb->Free();                            // OBLIGATOIRE (tas)
// ~NkImage() libere src : rien a ecrire.
```

> **⚠️ `NK_BICUBIC` et `NK_LANCZOS3` ne font pas ce qu'ils disent**
>
> Les quatre filtres compilent. Deux seulement existent.
>
> **`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.cpp:2901-2903`**
>
> ```cpp
>             // ── Bilinéaire (défaut pour NK_BILINEAR, NK_BICUBIC, NK_LANCZOS3) ─────
>             //
>             // Note : NK_BICUBIC et NK_LANCZOS3 ne sont pas encore implémentés ici.
> ```
>
> Et la documentation de la méthode le répète (`Core/NkImage.cpp:3009`) :
> « `NK_BICUBIC`, `NK_LANCZOS3` : *fallback* bilinéaire (non
> encore implémentés). »
>
> Demander du Lanczos ne plante pas, ne journalise rien, et vous rend du
> bilinéaire. C'est la pire catégorie de bug : celui qui produit un résultat
> plausible. Si la qualité de vos vignettes compte, sachez que vous avez du
> bilinéaire, quoi que dise votre code.

### Recadrer, convertir, recopier

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:378-388, 415-464, 505-514`**

```cpp
void FlipVertical()   noexcept;
void FlipHorizontal() noexcept;
void PremultiplyAlpha() noexcept;                                 // RGBA32 seulement
void Blit(const NkImage& src, int32 dstX, int32 dstY) noexcept;
bool BlitRegion(const NkImage& src, const math::NkIntRect& srcRegion,
                const math::NkIntRect& dstRegion) noexcept;
bool BlitRegion(const NkImage& src, const math::NkIntRect& srcRegion,
                const math::NkIntRect& dstRegion, NkResizeFilter filter) noexcept;
bool Copy(const NkImage& src, int32 dstX, int32 dstY,
          const math::NkIntRect& area, bool clip = true) noexcept;
bool CopyTo(NkImage& dst) const noexcept;
```

Notez le jeu de noms : `Copy(...)` avec arguments est une méthode
*instance* qui renvoie `bool` et copie *dans* `*this` ;
`Copy()` sans argument renvoie un `NkImage*` neuf. Deux méthodes
homonymes, deux régimes de mémoire opposés. C'est exactement pourquoi la règle
« regardez le type de retour » n'est pas une coquetterie.

### Le cas HDR

Une image `.hdr` ou `.exr` contient des flottants dont les valeurs
dépassent 1,0 — c'est tout l'intérêt. La convertir naïvement en 8 bits détruit
cette information :

**`Applications/NkImageDemo/src/Demo/ViewerApp.cpp:369-372`**

```
 NkImage::Load(.hdr, 4) ferait un Convert lineaire-clamp qui
 crame toutes les valeurs > 1.0. On passe par [le codec HDR]
 pour preserver le float, puis ConvertToTexture(exposure,gamma).
```

La bonne séquence est donc : décoder en flottant (le codec HDR le fait), puis
appliquer un *tone mapping* explicite avec
`ConvertToTexture(image, exposure, gamma)`
(`Core/NkImage.h:815`), qui rend une image 8 bits affichable.

## Le cas particulier du GIF animé

Le codec GIF a une API à part, parce qu'un GIF peut contenir plusieurs images :

**`Kernel/Runtime/NKImage/src/NKImage/Codecs/GIF/NkGIFCodec.h:36-53`**

```cpp
struct NkGIFFrame {
        NkImage *image = nullptr; ///< Frame RGBA32, taille canvas global
        uint32 delayMs = 0;       ///< Duree d'affichage avant frame suivante
        uint16 left = 0;          ///< Position originale (info, deja composee)
        uint16 top = 0;
        uint8 disposal = 0;       ///< 0=undef,1=keep,2=clear,3=restore (info)
};
struct NkGIFAnimation {
        uint32 width = 0; uint32 height = 0; uint32 frameCount = 0;
        NkGIFFrame *frames = nullptr;  ///< Array de frameCount entries
        uint16 loopCount = 0;          ///< 0 = infini (NETSCAPE2.0)
};
```

Le mot important est « déjà composée » : vous n'avez pas à gérer vous-même les
modes de *disposal* du format GIF, chaque `frames[i].image` est une
image RGBA32 complète, à la taille du canevas global. Les champs
`left`, `top` et `disposal` ne sont là que pour information.

**`Exemple écrit pour le cours (API : Codecs/GIF/NkGIFCodec.h:64, 67)`**

```cpp
#include "NKImage/Codecs/GIF/NkGIFCodec.h"

// data/bytes = contenu brut du .gif lu en memoire
NkGIFAnimation *anim = NkGIFCodec::DecodeAnimation(data, bytes);
if (anim) {
    for (uint32 i = 0; i < anim->frameCount; ++i) {
        NkImage *f = anim->frames[i].image;      // RGBA32, deja composee
        uint32   d = anim->frames[i].delayMs;    // duree d'affichage
        /* ... uploader f ... */
    }
    NkGIFCodec::FreeAnimation(anim);   // libere le struct ET toutes les NkImage*
}
```

Un seul appel à `FreeAnimation` libère tout. N'appelez pas
`Free()` sur les images individuelles : elles appartiennent à
l'animation.

## Libérer correctement : les quatre cas

Récapitulons, parce que c'est ici que les débutants perdent leurs soirées.

| **Origine de l'image** | **Libération** | **Exemple** |
|---|---|---|
| `NkImage img;` sur la pile | rien (destructeur) | `img.Load(...)` |
| Fabrique statique / `Resize` | `img->Free()` | `NkImage::Alloc(...)` |
| Tampon `Encode*` | `memory::NkFree(buf)` | `EncodePNG(buf,n)` |
| `NkGIFCodec::DecodeAnimation` | `FreeAnimation(anim)` | animation entière |

Et une cinquième méthode, qui n'appartient à aucune de ces lignes :

**`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:769-783`**

```
 Libere les pixels (si owning) ET libere le struct NkImage lui-meme via nkFree.
 A utiliser UNIQUEMENT sur les images creees via les fabriques statiques
 (Alloc, Wrap, Create statique, Copy, CopyAs, Convert, Resize, Crop, ...).
 Ne jamais appeler Free() sur une image allouee sur la stack.

 [NKIResource] Unload() : libere les pixels (si owning) et remet *this dans
 l'etat « image vide » SANS liberer le struct (contrairement a Free()).
```

`Unload()` sert quand vous voulez réutiliser une `NkImage` de pile
pour charger autre chose sans attendre la fin de la portée. `Free()` sur
une image de pile, en revanche, tente de libérer une adresse qui n'a jamais été
allouée : plantage garanti.

> **⚠️ `Wrap()` ne possède rien**
>
> **`Kernel/Runtime/NKImage/src/NKImage/Core/NkImage.h:795-800`**
>
> ```
>  Cree une vue non-owning sur un buffer pixel externe.
>  Le buffer n'est PAS libere par le destructeur ni par Free().
>  L'appelant reste responsable de la duree de vie du buffer.
> ```
>
> `Wrap` est très pratique : il permet d'appliquer les méthodes de
> `NkImage` (dessin, `Save`, `Encode`) à des pixels que vous
> possédez déjà, sans copie. Mais la `NkImage` obtenue est une *vue* :
> si le tampon d'origine meurt avant elle, toute lecture est une lecture après
> libération.

## Afficher une image dans une interface NKGui

Nous y voilà. C'est la partie que vous attendez, et elle tient en trois gestes :
**charger**, **uploader**, **déclarer un widget**.

### Le widget

**`Kernel/Runtime/NKGui/src/NKGui/Widgets/NkGuiWidgets.h:96-105`**

```cpp
        // ── Image / Icône (quad texturé) ──────────────────────────────────────
        // Dessine la texture `texId` (id backend) à la taille w×h. `tint` multiplie
        // (blanc = telle quelle) ; `uv0/uv1` = sous-région (atlas/sprite). Auto-layout.
        void Image(NkGuiContext &ctx, uint32 texId, float32 w, float32 h,
                   NkColor tint = NkColor{255, 255, 255, 255}, NkVec2 uv0 = NkVec2{0.f, 0.f},
                   NkVec2 uv1 = NkVec2{1.f, 1.f}) noexcept;
        // Bouton-image (icône cliquable) : fond + image centrée + survol. Retourne true au clic.
        bool ImageButton(NkGuiContext &ctx, const char *idStr, uint32 texId, float32 w, float32 h,
                         NkColor tint = NkColor{255, 255, 255, 255}, NkVec2 uv0 = NkVec2{0.f, 0.f},
                         NkVec2 uv1 = NkVec2{1.f, 1.f}) noexcept;
```

Le premier paramètre après le contexte est un `uint32 texId`. Pas un
pointeur, pas une `NkImage` : **un simple entier**. NKGui ne sait
rien de votre image ; il écrit ce nombre dans sa liste de commandes et laisse le
backend le résoudre. C'est la conséquence directe de l'architecture décrite au
chapitre 1.

### L'upload

Le pont vers NkCanvas expose la méthode qui associe un `texId` à des
pixels :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/UI/NkGuiCanvasBackend.h:94`**

```cpp
bool UploadImageRGBA(uint32 texId, const uint8 *rgba, int32 w, int32 h);
```

Le nom dit l'exigence : **RGBA**, et implicitement *contigu* (pas de
stride). D'où la recopie ligne par ligne que nous avons vue plus haut.

### La séquence complète

Voici le bloc réel de la démo, dans son intégralité. C'est l'exemple à recopier :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:290-322`**

```cpp
    // ── VRAIE image chargee depuis le disque (NKImage) -> texture backend ──────
    uint32 imgTexId = 0u;
    int32 imgW = 0, imgH = 0;
    {
        static const char *kCandidates[] = {
            "Applications/Mou/assets/brand/rihen-logo.png",
            "Applications/Mou/assets/brand/noge-logo.png",
            "Resources/Icons/ContentBrowser/FileIcon.png",
            "../../../Applications/Mou/assets/brand/rihen-logo.png",
        };
        NkImage img;
        bool ok = false;
        for (const char *p : kCandidates)
            if (img.Load(p, 4)) {
                ok = true;
                break;
            }
        if (ok && img.Width() > 0 && img.Height() > 0 && img.Pixels()) {
            imgW = img.Width();
            imgH = img.Height();
            static NkVector<uint8> rgba;
            rgba.Resize(static_cast<usize>(imgW) * imgH * 4u);
            for (int32 y = 0; y < imgH; ++y) {
                const uint8 *src = img.RowPtr(y);
                uint8 *dst = rgba.Data() + static_cast<usize>(y) * imgW * 4u;
                for (int32 x = 0; x < imgW * 4; ++x)
                    dst[x] = src[x];
            }
            imgTexId = 0x494D4731u; // 'IMG1'
            backend.UploadImageRGBA(imgTexId, rgba.Data(), imgW, imgH);
            logger.Info("Image chargee : {0}x{1}", imgW, imgH);
        } else {
            logger.Info("Image demo introuvable (cwd) -> section image vide");
        }
    }
```

Six observations, dans l'ordre où elles comptent :

1. **Une liste de chemins candidats.** L'application ne sait pas
   depuis quel répertoire on la lance ; elle essaie plusieurs chemins et
   prend le premier qui marche. Ce n'est pas de la paresse, c'est la
   réponse honnête à un vrai problème.
2. **`NkImage img;` sur la pile**, donc aucun `Free()` à
   écrire. C'est la voie recommandée.
3. **`img.Load(p, 4)`** — on force quatre canaux, parce que
   `UploadImageRGBA` attend du RGBA.
4. **La recopie ligne par ligne** avec `RowPtr(y)`, pour la
   raison expliquée plus haut.
5. **`imgTexId = 0x494D4731u`** — soit `'IMG1'` en
   ASCII. L'identifiant est choisi *par vous*, arbitrairement ; il
   suffit qu'il soit stable et distinct des autres (l'atlas de police, par
   exemple, utilise `'NKFT'`). Un entier lisible en hexadécimal aide
   énormément au débogage.
6. **Le `else` journalise.** Sans lui, une image manquante
   donne simplement... rien. Aucun message. C'est le silence dont parlait
   le chapitre 1.

Puis, dans la frame, le widget :

**`Applications/NKGuiDemo/src/NKGuiDemo/main.cpp:626-637`**

```cpp
            Text(ctx, "Image reelle (chargee via NKImage) :");
            if (imgTexId != 0u && imgW > 0 && imgH > 0) {
                // Affiche l'image a une largeur fixe en preservant le ratio.
                const float32 dispW = 150.f;
                const float32 dispH = dispW * static_cast<float32>(imgH) / static_cast<float32>(imgW);
                Image(ctx, imgTexId, dispW, dispH);
                ctx.SameLine();
                static int32 ibClicks = 0;
                if (ImageButton(ctx, "ib", imgTexId, 44.f, 44.f))
                    ++ibClicks;
            } else {
                Text(ctx, "(aucune image trouvee)");
            }
```

Le calcul de `dispH` mérite d'être noté : `Image` ne préserve
*pas* le rapport d'aspect, elle dessine exactement le rectangle que vous
demandez. Si vous lui donnez une taille carrée pour une image rectangulaire,
elle l'écrase sans se plaindre.

> **✅ Les trois gestes**
>
> 1. `NkImage img; img.Load(chemin, 4);` — charge et force le RGBA ;
> 2. `backend.UploadImageRGBA(monId, pixelsContigus, w, h);` — une
>    fois, à l'initialisation ;
> 3. `Image(ctx, monId, w, h);` — chaque frame, dans la déclaration
>    d'interface.
>
> La `NkImage` peut mourir après l'étape 2 : le backend a copié les pixels
> côté GPU. C'est l'entier `monId` qui doit survivre.

### Le chemin plus court : `NkTexture` de NkCanvas

Si vous n'êtes pas dans une interface NKGui mais dans une scène NkCanvas, il
existe un raccourci qui fait tout :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkTexture.h:53-58`**

```cpp
bool Create(NkIRenderer2D &renderer, uint32 width, uint32 height, const NkColor2D &fillColor);
bool LoadFromFile(NkIRenderer2D &renderer, const char *path);
bool LoadFromImage(NkIRenderer2D &renderer, const NkImage &image, const NkRect2i &area = NkRect2i{});
bool LoadFromMemory(NkIRenderer2D &renderer, const void *data, usize sizeBytes);
```

Son implémentation montre qu'il n'y a aucune magie — c'est exactement ce que
vous auriez écrit :

**`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkTexture.cpp:78-86`**

```cpp
        bool NkTexture::LoadFromFile(NkIRenderer2D &renderer, const char *path) {
            NkImage img;
            if (!img.Load(path))
                return false;
            return LoadFromImage(renderer, img);
        }
```

> **⚠️ Créer les textures *après* `Initialize()`**
>
> **`Kernel/Runtime/NKCanvas/src/NKCanvas/Renderer/Resources/NkTexture.cpp:17-19`**
>
> ```
>  Active backend dispatch table. Initialement vide (callbacks null) :
>  les operations sur NkTexture sont des no-ops tant qu'aucun renderer
>  n'a appele NkTextureSetBackend() a la fin de son Initialize().
> ```
>
> Autrement dit : une `NkTexture` créée avant que le renderer soit
> initialisé ne fait **rien**, et ne dit rien. C'est un invariant d'ordre pur.
> Créez vos textures après `renderer.Initialize()`, toujours.

## Décoder ailleurs, uploader ici

Décoder un JPEG de huit mégapixels prend des dizaines de millisecondes : bien
plus qu'une image à 60 Hz. La tentation est de le faire sur un fil de travail.

Le module ne l'interdit pas, mais il ne le promet pas non plus : le
`.jenga` liste `NKThreading` en dépendance, et aucun verrou n'est
exposé dans l'API publique. Aucun commentaire du module ne garantit la
*thread-safety*. La règle prudente, qui est celle de la visionneuse du
dépôt (`Applications/NkImageDemo/src/Demo/ViewerApp.cpp:463`, « Upload sur
le main thread (libere l'image apres) »), tient en une ligne :

> **✅ Ce qu'il faut retenir**
>
> **Décodez dans un fil de travail, uploadez sur le fil principal.** Le
> décodage produit des octets — c'est du calcul pur, isolé. L'upload touche au
> contexte graphique, qui appartient au fil principal.

## État réel du module

Terminons par l'inventaire honnête, parce que c'est ce qui vous fera gagner du
temps.

### Ce qui fonctionne

| **Format** | **Lecture** | **Écriture** |
|---|---|---|
| PNG | oui | oui |
| JPEG | oui | oui |
| BMP | oui | oui |
| TGA | oui | oui |
| PPM / PGM / PBM | oui | oui |
| QOI | oui | oui |
| HDR | oui | oui |
| EXR | oui | oui (scanline float, compression `NONE`) |
| GIF | oui (statique *et* animé) | oui (palette 256 + LZW) |
| ICO | oui | **aucun encodeur** |
| WebP | oui | par le codec seulement, pas par `Save` |
| SVG | oui (rastérisation) | non |

S'y ajoutent : le redimensionnement en `NK_NEAREST` et
`NK_BILINEAR`, et toutes les primitives de dessin CPU.

### Ce qui ne fonctionne pas

- `NkImage::SaveWebP` et `NkImage::SaveSVG` renvoient
  `false` sans rien faire (`Core/NkImage.cpp:2119-2134`) ;
- `NK_BICUBIC` et `NK_LANCZOS3` retombent silencieusement
  sur du bilinéaire (`Core/NkImage.cpp:2901-2903`) ;
- l'exemple en tête de `src/NKImage/NKImage.h` cite des classes
  supprimées.

> **⚠️ `NkImageDemo` ne compile plus**
>
> Vous trouverez dans le dépôt une application `Applications/NkImageDemo`,
> déclarée dans `Nkentseu.jenga:1447`. Ne la prenez pas pour modèle
> d'*appel* :
>
> **`Applications/NkImageDemo/src/Demo/ViewerApp.cpp:450`**
>
> ```cpp
>             NkImage *img = NkImage::Load(path.CStr(), 4);
> ```
>
> Il n'existe **aucune** `static NkImage* Load(...)`. Le seul
> `Load` du module est la méthode d'instance `bool Load(const char*, int32)` (`Core/NkImage.h:262`). Cette démo n'apparaît d'ailleurs pas dans
> `Build/Bin/Debug-Windows/` : elle ne compile plus contre l'API actuelle.
>
> Sa *logique* en revanche reste excellente à lire — parcours de dossier,
> lecture de GIF animé, *tone mapping* HDR. Lisez-la comme un plan, pas comme
> une référence d'API.

### Les deux classes utilitaires exportées

Pour mémoire, parce que vous les croiserez en lisant les codecs :

- `NkImageStream` (`Core/NkImage.h:933-1043`) — un
  lecteur/écrivain binaire qui sait lire en *big endian* comme en
  *little endian* : `ReadU16BE`, `ReadU32LE`,
  `ReadBytes`, `Skip`, `Seek`, `Tell`,
  `HasError`, et les `Write*` symétriques. C'est l'outil
  avec lequel tous les codecs du module sont écrits ;
- `NkDeflate` (`Core/NkImage.h:1085-1217`) — la compression
  *deflate*/*inflate* du PNG, exposée publiquement :
  `Decompress`, `DecompressRaw`, `Compress`. Utile
  bien au-delà des images.

## Exercices

> **✏️ 1 — Le round-trip, et le format qui échoue**
>
> Écrivez un petit programme console qui :
>
> 1. alloue une image RGB24 de 256 × 256 via `NkImage::Alloc` ;
> 2. la remplit d'un dégradé à la main, en utilisant `Stride()` ;
> 3. la sauvegarde en `.png`, `.bmp`, `.tga`,
>    `.qoi` et `.webp`, en **testant chaque valeur de
>    retour** et en affichant le résultat ;
> 4. recharge le `.png` et compare octet par octet avec l'original.
>
> Une seule des cinq sauvegardes doit échouer. Laquelle, et pourquoi ? Inspirez-vous
> de `Applications/NKImageCodecTest/src/main.cpp`, qui fait exactement cela
> en 79 lignes.

> **✏️ 2 — Rendre le stride visible**
>
> Chargez le même PNG deux fois : une fois avec `Load(path, 3)`, une fois
> avec `Load(path, 4)`. Pour chacune, affichez `Width()`,
> `Channels()`, `BytesPP()` et `Stride()`. Choisissez une
> largeur qui n'est pas multiple de 4 (par exemple 101 pixels) et vérifiez que
> `Stride() != Width() * BytesPP()` dans le cas à trois canaux.
>
> Puis, volontairement, écrivez la conversion en RGBA avec un `memcpy`
> global au lieu de `RowPtr`, affichez le résultat, et regardez la
> diagonale apparaître. Une erreur qu'on a vue une fois ne se refait pas.

> **✏️ 3 — Une vignette dans une interface**
>
> Reprenez l'application NKGui du chapitre 4 et ajoutez-lui un panneau qui :
>
> - charge une image du dépôt (essayez plusieurs chemins candidats) ;
> - en fabrique une vignette de 128 pixels de large avec `Resize`,
>   **en préservant le rapport d'aspect** ;
> - uploade la vignette sous un `texId` de votre choix ;
> - l'affiche avec `Image`, et affiche à côté ses dimensions
>   d'origine avec `Text`.
>
> Vérifiez ensuite avec un débogueur — ou un simple compteur — que vous avez bien
> appelé `Free()` exactement une fois sur le résultat de `Resize`,
> et zéro fois sur l'image de pile.

> **✏️ 4 — Mesurer le mensonge**
>
> Chargez une photographie assez grande, puis produisez deux vignettes de la même
> taille : une avec `NK_BILINEAR`, une avec `NK_LANCZOS3`.
> Sauvegardez les deux en PNG, puis comparez-les octet par octet.
>
> Elles doivent être **rigoureusement identiques**. Vous venez de démontrer
> vous-même, sans lire le `.cpp`, que le filtre Lanczos n'existe pas. C'est
> exactement la démarche à appliquer chaque fois qu'une API vous promet quelque
> chose que vous n'avez pas vu marcher.
