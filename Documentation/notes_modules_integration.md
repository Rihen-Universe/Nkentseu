# Cinq modules d'intégration — notes de référence pour le cours

> Document de travail rédigé **en lecture seule** sur le dépôt
> `D:\Projets\2026\Nkentseu\Nkentseu` (moteur C++17 « Nkentseu », commentaires en
> français, **règle absolue : zéro STL** — on n'utilise que `NkString`, `NkVector`,
> `NkFileSystem`, `math::Nk*`, et la mémoire passe par **NKMemory**).
>
> **Destination** : matière première des cinq chapitres d'intégration du cours
> débutant (après NkCanvas, NKGui, et « bâtir une application finie »).
> Tout est sourcé (`chemin:ligne`). Aucune source n'a été modifiée, rien n'a été
> compilé ni exécuté.
>
> Complément de `Documentation/notes_nkcanvas_nkgui.md` (mêmes conventions).

## Verdict global (à lire avant tout)

| Module | Chemin | Verdict pour le cours |
|---|---|---|
| **NKImage** | `Kernel\Runtime\NKImage` | ✅ **Utilisable** — API stable, très documentée, démo bâtie |
| **NKMedia** | `Kernel\Runtime\NKMedia` | 🟡 **Partiel** — écriture vidéo/audio solide, lecture inégale |
| **NKFont** | `Kernel\Runtime\NKFont` | ✅ **Utilisable** — atlas + métriques + polices embarquées |
| **NKAudio** | `Kernel\Runtime\NKAudio` | ✅ **Utilisable** — API façade `NkAudio` complète |
| **NKNetwork** | `Kernel\System\NKNetwork` | 🟡 **Partiel** — sockets/UDP fiable OK, HTTP/Lobby à vérifier |

*(Les verdicts détaillés, avec ce qui est vide ou stub, sont dans chaque section.)*

---

# 1. NKImage — images

## 1.1 Organisation

Racine : `D:\Projets\2026\Nkentseu\Nkentseu\Kernel\Runtime\NKImage`

```
NKImage/
  NKImage.jenga                       ← recette de build
  pch/pch.h · pch/pch.cpp
  src/NKImage/
    NKImage.h                         ← en-tête PARAPLUIE (tout inclure d'un coup)
    Core/
      NkImage.h        (1218 l.)      ← LE conteneur d'image + NkImageStream + NkDeflate
      NkImage.cpp      (3034 l.)
      NkImageExport.h  (  58 l.)      ← macros NKENTSEU_IMAGE_API / NKIMG_INLINE
    Codecs/
      PNG/NkPNGCodec.{h,cpp}          JPEG/NkJPEGCodec.{h,cpp}
      BMP/NkBMPCodec.{h,cpp}          TGA/NkTGACodec.{h,cpp}
      HDR/NkHDRCodec.{h,cpp}          EXR/NkEXRCodec.{h,cpp}
      PPM/NkPPMCodec.{h,cpp}          QOI/NkQOICodec.{h,cpp}
      GIF/NkGIFCodec.{h,cpp}          ICO/NkICOCodec.{h,cpp}
      WEBP/NkWebPCodec.{h,cpp}
      SVG/NkSVGCodec.{h,cpp} + NkXMLParser.{h,cpp}
  tests/TestEXR.cpp
```

**Dépendances déclarées** — `Kernel\Runtime\NKImage\NKImage.jenga:25-29` :

```python
    nkentseudependson(
        ["NKPlatform", "NKCore", "NKMemory", "NKMath", "NKContainers", "NKLogger", "NKThreading", "NKFileSystem", "NKStream"],
        selfexport="NKImage",
        extra_includes=["src"],
    )
```

Aucune dépendance externe : `NKImage.jenga:6` — « Bibliothèque self-contained sans
dépendance externe. »

Point d'attention build : `NKImage.jenga:47-51` indique que le pipeline SVG DOM
(`NkSVGDOM.cpp`, `NkSVGRenderer.cpp`) a été **retiré** ; il ne reste qu'un codec
unique `NkSVGCodec.cpp`. Le commentaire de tête de `src/NKImage/NKImage.h:12-33`
décrit encore l'ancien pipeline (`NkSVGRenderer::RenderFromFile`, `NkSVGDOM`,
`NkSVGDOMBuilder`) : **cette documentation d'en-tête est périmée**, ces classes
n'existent plus. La ligne 53-54 du même fichier le corrige :

```cpp
// SVG : NkSVGDOM/Renderer ont ete remplaces par NkSVGCodec (codec unique).
```

→ **Pour le cours : ne jamais recopier l'exemple `@code` du haut de `NKImage.h`.**

## 1.2 Surface d'API publique

### 1.2.1 Les enums (`Core/NkImage.h:67-162`)

```cpp
enum class NkImagePixelFormat : uint8 {
    NK_UNKNOWN = 0, NK_GRAY8 = 1, NK_GRAY_A16 = 2,
    NK_RGB24 = 3, NK_RGBA32 = 4, NK_RGBA128F = 5, NK_RGB96F = 6,
};                                                    // NkImage.h:67-75

enum class NkImageFormat : uint8 {                    // format de FICHIER
    NK_UNKNOWN=0, NK_PNG, NK_JPEG, NK_BMP, NK_TGA, NK_HDR,
    NK_PPM, NK_PGM, NK_PBM, NK_QOI, NK_GIF, NK_ICO, NK_SVG, NK_EXR,
};                                                    // NkImage.h:127-142

enum class NkResizeFilter : uint8 {
    NK_NEAREST, NK_BILINEAR, NK_BICUBIC, NK_LANCZOS3,
};                                                    // NkImage.h:157-162

constexpr int32 ChannelsOf(NkImagePixelFormat f) noexcept;      // NkImage.h:78
constexpr int32 BytesPerPixelOf(NkImagePixelFormat f) noexcept; // NkImage.h:98
```

### 1.2.2 `class NkImage : public NKIResource` (`Core/NkImage.h:186-905`)

**Deux familles d'API cohabitent** — c'est LE point pédagogique du chapitre. Le
commentaire de tête l'explique (`Core/NkImage.h:7-36`) :

> ```
>  1. API STATIQUE  → retourne `NkImage*`  (ownership explicite, heap-alloué)
>  2. API INSTANCE  → retourne `bool`  (opère sur *this, aucune allocation visible)
> ```

**Cycle de vie**

```cpp
NkImage() noexcept = default;                                   // :191
~NkImage() noexcept override;                                   // :199
NkImage(const NkImage&) = delete;                               // :202  copie INTERDITE
NkImage& operator=(const NkImage&) = delete;                    // :203
NkImage(NkImage&& other) noexcept;                              // :209
NkImage& operator=(NkImage&& other) noexcept;                   // :215
```

**API INSTANCE (retourne `bool`, opère sur `*this`)**

```cpp
bool Create(uint32 width, uint32 height, math::NkColor color,
            int32 desiredChannels = 4) noexcept;                          // :232
bool LoadFromFile(const char* path) override;                             // :242 (→ Load(path,0))
bool LoadFromMemory(const void* data, usize size) override;               // :247
bool LoadFromStream(NkStream& stream) override;                           // :252
bool Load(const char* path, int32 desiredChannels = 0) noexcept;          // :262
bool LoadFromMemory(const uint8* data, usize size) noexcept;              // :268
bool LoadFromMemory(const void* data, usize size, int32 desiredChannels) noexcept;  // :282
bool LoadFromMemory(const uint8* data, usize size, int32 desiredChannels) noexcept; // :293
bool Copy(const NkImage& src, int32 dstX, int32 dstY,
          const math::NkIntRect& area, bool clip = true) noexcept;        // :505
bool CopyTo(NkImage& dst) const noexcept;                                 // :514
```

**API STATIQUE (retourne `NkImage*`, à libérer par `->Free()`)**

```cpp
static NkImage* Create(uint32 w, uint32 h, int32 desiredChannels = 0, uint32 color = 0) noexcept; // :308
static NkImage* Create(uint32 w, uint32 h, NkImagePixelFormat fmt, uint32 color = 0) noexcept;    // :317
static NkImage* Alloc(int32 w, int32 h, NkImagePixelFormat fmt) noexcept;                          // :792
static NkImage* Wrap(uint8* pixels, int32 w, int32 h, NkImagePixelFormat fmt, int32 stride=0) noexcept; // :804
static NkImage* ConvertToTexture(const NkImage& hdrImage, float exposure=1.0f, float gamma=2.2f) noexcept; // :815
```

**Méthodes membres qui retournent une NOUVELLE image (à `->Free()`)**

```cpp
NkImage* Convert(NkImagePixelFormat newFmt) const noexcept;                          // :396
NkImage* Resize(int32 nw, int32 nh, NkResizeFilter f = NkResizeFilter::NK_BILINEAR) const noexcept; // :404
NkImage* Crop(int32 x, int32 y, int32 w, int32 h) const noexcept;                    // :475
NkImage* Copy() const noexcept;                                                      // :485
NkImage* CopyAs(NkImagePixelFormat fmt) const noexcept;                              // :523
```

**Sauvegarde disque** (`Core/NkImage.h:348-360`)

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
bool SaveWebP(const char* path, bool lossless = true, int32 quality = 90) const noexcept; ///< Non implémenté.
bool SaveSVG (const char* path) const noexcept;                                          ///< Non implémenté.
```

**Encodage en mémoire** (`Core/NkImage.h:369-373`) — `out` alloué par `NkAlloc` :

```cpp
bool EncodePNG (uint8*& out, usize& size) const noexcept;
bool EncodeJPEG(uint8*& out, usize& size, int32 quality = 90) const noexcept;
bool EncodeBMP (uint8*& out, usize& size) const noexcept;
bool EncodeTGA (uint8*& out, usize& size) const noexcept;
bool EncodeQOI (uint8*& out, usize& size) const noexcept;
```

**Manipulation & blit**

```cpp
void FlipVertical()   noexcept;                                   // :378
void FlipHorizontal() noexcept;                                   // :381
void PremultiplyAlpha() noexcept;                                 // :388 (RGBA32 seulement)
void Blit(const NkImage& src, int32 dstX, int32 dstY) noexcept;   // :415
bool BlitRegion(const NkImage& src, const math::NkIntRect& srcRegion,
                const math::NkIntRect& dstRegion) noexcept;       // :461
bool BlitRegion(const NkImage& src, const math::NkIntRect& srcRegion,
                const math::NkIntRect& dstRegion, NkResizeFilter filter) noexcept; // :464
```

**Accesseurs inline** (`Core/NkImage.h:527-585`) : `Pixels()`, `Width()`, `Height()`,
`Channels()`, `BytesPP()`, `Stride()`, `Format()`, `SourceFormat()`, `IsValid()`,
`IsHDR()`, `TotalBytes()`, `RowPtr(y)`.

**Dessin CPU logiciel** (`Core/NkImage.h:587-765`) — tout est `NKIMG_INLINE` dans
le header, donc directement lisible en cours :
`SetPixel`, `GetPixel`, `BlendPixel`, `Fill`, `DrawHLine`, `DrawVLine`, `DrawLine`
(Bresenham), `DrawRect`, `FillRect`, `DrawCircle`, `FillCircle`, `DrawEllipse`,
`FillEllipse`. Tous prennent une `math::NkColor`.

**Libération**

```cpp
void Free()   noexcept;            // :775  libère pixels ET le struct (heap uniquement)
void Unload() noexcept override;   // :783  libère pixels, garde le struct (pile OK)
```

### 1.2.3 Les codecs (appel direct, tous statiques)

| Codec | Fichier | Decode | Encode |
|---|---|---|---|
| PNG | `Codecs/PNG/NkPNGCodec.h:20,35` | `static NkImage* Decode(const uint8*, usize)` | `static bool Encode(const NkImage&, uint8*&, usize&)` |
| JPEG | `Codecs/JPEG/NkJPEGCodec.h:23,39` | idem | `Encode(const NkImage&, uint8*&, usize&, int32 quality=90)` |
| BMP | `Codecs/BMP/NkBMPCodec.h:21,27` | idem | idem |
| TGA | `Codecs/TGA/NkTGACodec.h:20,21` | idem | idem |
| QOI | `Codecs/QOI/NkQOICodec.h:7,8` | idem | idem |
| PPM | `Codecs/PPM/NkPPMCodec.h:20,26` | idem | `Encode(const NkImage&, const char* path)` (fichier direct) |
| HDR | `Codecs/HDR/NkHDRCodec.h:29,31,37,44` | idem | `Encode(img, path)`, `EncodeToMemory(img, out, size)`, `ConvertToTexture(img, exposure, gamma)` |
| EXR | `Codecs/EXR/NkEXRCodec.h:55,60` | idem | idem |
| GIF | `Codecs/GIF/NkGIFCodec.h:59,64,67,71,74` | `Decode` (1re frame), `DecodeAnimation`, `FreeAnimation` | `Encode`, `Save` |
| ICO | `Codecs/ICO/NkICOCodec.h:20` | `Decode` seulement | **aucun encodeur** |
| WebP | `Codecs/WEBP/NkWebPCodec.h:31,32` | `Decode` | `Encode(img, out, size, bool lossless=true, ...)` |
| SVG | `Codecs/SVG/NkSVGCodec.h:254,257,262,263` | `Decode(data,size,outW=0,outH=0)`, `DecodeFromFile(path,outW,outH)` | `Encode`, `EncodeToFile` |

**GIF animé** — structures publiques (`Codecs/GIF/NkGIFCodec.h:36-53`) :

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

### 1.2.4 Classes utilitaires exportées

- `class NkImageStream` (`Core/NkImage.h:933-1043`) — lecteur/écrivain binaire
  BE/LE utilisé par les codecs. `ReadU8/U16BE/U16LE/U32BE/U32LE/I16BE/I32LE`,
  `ReadBytes`, `Skip`, `Seek`, `Tell`, `Size`, `IsEOF`, `HasBytes`, `HasError`,
  `Ptr`, `WriteU8/…`, `WriteBytes`, `TakeBuffer(uint8*& , usize&)`, `WriteSize()`.
- `class NkDeflate` (`Core/NkImage.h:1085-1217`) — inflate/deflate :
  `static bool Decompress(in,inSz,out,outCap,written)`,
  `static bool DecompressRaw(...)`,
  `static bool Compress(in,inSz,uint8*& out,usize& outSz,int32 level=6)`.

## 1.3 Implémenté vs déclaré-mais-vide

### ✅ Réellement implémenté

- Chargement : PNG, JPEG, BMP, TGA, HDR, PPM/PGM/PBM, QOI, GIF (statique **et**
  animé), ICO, WebP, SVG, EXR.
- Écriture : PNG, JPEG, BMP, TGA, PPM, HDR, EXR, QOI, GIF (dispatch confirmé
  `Core/NkImage.cpp:2010-2030`).
- `Resize` en `NK_NEAREST` et `NK_BILINEAR`.
- Toutes les primitives de dessin CPU (elles sont inline dans le header, donc
  visiblement complètes).

### ❌ Déclaré mais vide — à NE PAS enseigner

`Core/NkImage.cpp:2119-2134` (recopié mot pour mot) :

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

Note : `NkWebPCodec::Encode` existe et est implémenté (`Codecs/WEBP/NkWebPCodec.h:32`,
chemin VP8L sans perte) — c'est seulement **`NkImage::SaveWebP` qui est le stub**.
De même `Save("x.webp")` renvoie `false` : l'extension `webp` n'est pas dans le
dispatch de `Save()` (`Core/NkImage.cpp:2010-2030`).

### ⚠️ Silencieusement dégradé (piège classique)

`Core/NkImage.cpp:2901-2903` :

```cpp
			// ── Bilinéaire (défaut pour NK_BILINEAR, NK_BICUBIC, NK_LANCZOS3) ─────
			//
			// Note : NK_BICUBIC et NK_LANCZOS3 ne sont pas encore implémentés ici.
```

et `Core/NkImage.cpp:3009` :

```cpp
	 *   NK_BICUBIC, NK_LANCZOS3 : fallback bilinéaire (non encore implémentés).
```

→ Demander `NK_LANCZOS3` **ne plante pas**, mais donne du bilinéaire. Le cours
doit le dire, sinon l'élève croit avoir de la qualité Lanczos.

### 🚨 Démo obsolète — piège pour le cours

`Applications\NkImageDemo\src\Demo\ViewerApp.cpp:450` écrit :

```cpp
			NkImage *img = NkImage::Load(path.CStr(), 4);
```

Or **il n'existe aucune `static NkImage* Load(...)`** : le seul `Load` est la
méthode d'instance `bool Load(const char*, int32)` (`Core/NkImage.h:262`,
définie `Core/NkImage.cpp:1757`). `NkImageDemo` n'apparaît d'ailleurs pas dans
`Build/Bin/Debug-Windows/` alors qu'il est bien déclaré dans
`Nkentseu.jenga:1447` → **cette démo ne compile plus contre l'API actuelle**.
Ne pas s'en servir comme exemple d'appel ; en revanche sa *logique* (scan de
dossier, GIF animé, HDR tonemap) reste un excellent plan de chapitre.

## 1.4 Exemples d'utilisation réels tirés du dépôt

### 1.4.1 `NKImageCodecTest` — encodage round-trip (application **bâtie**)

`Applications\NKImageCodecTest\src\main.cpp` (79 lignes, présente dans
`Build/Bin/Debug-Windows/NKImageCodecTest`). Extrait `main.cpp:15-27` puis `:62-79` :

```cpp
int main() {
	const int W = 512, H = 512;

	// Motif synthétique déterministe (RGB24).
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
	/* ... */
	const Fmt formats[] = {
		{"ic_out.png", true}, {"ic_out.bmp", true}, {"ic_out.tga", true}, {"ic_out.qoi", true},
		{"ic_out.ppm", true}, {"ic_out.gif", false}, {"ic_out.hdr", false}, {"ic_out.webp", true}, {"ic_out.exr", false},
	};
	for (const Fmt &f : formats) {
		const bool ok = img->Save(f.file, 100);
		printf("  %-12s : %s%s\n", f.file, ok ? "ecrit" : "ECHEC SAVE", f.lossless ? "  (lossless attendu)" : "");
	}
	img->Free();
	return 0;
}
```

Remarquer : `Alloc` (statique) → `Pixels()` + `Stride()` pour écrire ligne par
ligne → `Save()` → `Free()`. C'est **le squelette canonique de l'API statique**.
(À noter honnêtement : la ligne `{"ic_out.webp", true}` **échouera toujours**,
cf. §1.3.)

### 1.4.2 `NKGuiDemo` — charger une image et l'envoyer au backend NKGui

`Applications\NKGuiDemo\src\NKGuiDemo\main.cpp:290-322` — c'est **l'exemple à
recopier dans le chapitre « afficher une image dans une interface »** :

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

Trois leçons à en tirer :
1. `NkImage img;` sur la **pile** + `img.Load(...)` → aucun `Free()` à écrire
   (le destructeur s'en charge). C'est la voie recommandée aux débutants.
2. On force `4` canaux à la lecture pour garantir du RGBA32 côté GPU.
3. **On recopie ligne par ligne avec `RowPtr(y)`** parce que le stride est aligné
   sur 4 octets : un `memcpy` global du buffer serait faux dès que
   `width*4 != stride` (voir §1.6).

### 1.4.3 Le pont officiel NKImage → NkCanvas

`Kernel\Runtime\NKCanvas\src\NKCanvas\Renderer\Resources\NkTexture.cpp:68-131` :

```cpp
		bool NkTexture::Create(NkIRenderer2D &renderer, uint32 width, uint32 height, const NkColor2D &fillColor) {
			if (width == 0 || height == 0)
				return false;
			NkImage img;
			// NkImage::Create(w, h, NkColor color, int32 channels=4) : la COULEUR
			// d'abord, les canaux ensuite. (Avant : args inverses -> channels=0
			// pour un fill transparent -> Create echouait -> texture jamais creee.)
			if (!img.Create(width, height, fillColor, 4))
				return false;
			return LoadFromImage(renderer, img);
		}

		bool NkTexture::LoadFromFile(NkIRenderer2D &renderer, const char *path) {
			NkImage img;
			if (!img.Load(path))
				return false;
			return LoadFromImage(renderer, img);
		}
```

API du pont (`Kernel\Runtime\NKCanvas\src\NKCanvas\Renderer\Resources\NkTexture.h:53-58`) :

```cpp
bool Create(NkIRenderer2D &renderer, uint32 width, uint32 height, const NkColor2D &fillColor);
bool LoadFromFile(NkIRenderer2D &renderer, const char *path);
bool LoadFromImage(NkIRenderer2D &renderer, const NkImage &image, const NkRect2i &area = NkRect2i{});
bool LoadFromMemory(NkIRenderer2D &renderer, const void *data, usize sizeBytes);
```

## 1.5 Squelette minimal pour une application

**(a) Charger un fichier et l'afficher (voie NkCanvas — la plus courte)**

```cpp
#include "NKCanvas/Renderer/Resources/NkTexture.h"
using namespace nkentseu;
using namespace nkentseu::renderer;

NkTexture logo;
// À faire APRÈS l'Initialize() du renderer (voir invariant §1.6).
if (!logo.LoadFromFile(renderer, "Resources/Icons/logo.png"))
    logger.Error("logo introuvable");
// ... dessiner avec logo.GetGPUId() ...
logo.Destroy();     // ou laisser le destructeur
```

**(b) Charger, retailler, sauvegarder (NKImage pur, aucune fenêtre)**

```cpp
#include "NKImage/Core/NkImage.h"
#include "NKMemory/NKMemory.h"
using namespace nkentseu;

int main() {
    // 1) API instance : l'objet vit sur la pile, rien à libérer.
    NkImage src;
    if (!src.Load("photo.jpg", 4))          // 4 = force RGBA32
        return 1;

    // 2) API statique : Resize alloue une NOUVELLE image -> c'est à moi de la libérer.
    NkImage *thumb = src.Resize(256, 256, NkResizeFilter::NK_BILINEAR);
    if (!thumb) return 1;

    thumb->Save("photo_thumb.png");         // format déduit de l'extension
    thumb->Free();                          // OBLIGATOIRE (heap)

    // 3) Encodage en mémoire : le buffer sort de NkAlloc -> NkFree, jamais free().
    uint8 *buf = nullptr; usize n = 0;
    if (src.EncodePNG(buf, n)) {
        /* ... envoyer buf/n sur le réseau, dans une base, ... */
        memory::NkFree(buf);
    }
    return 0;                               // ~NkImage() libère src
}
```

**(c) Créer une image de toutes pièces et dessiner dedans (CPU)**

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

**(d) GIF animé**

```cpp
#include "NKImage/Codecs/GIF/NkGIFCodec.h"

// data/bytes = contenu brut du .gif lu en mémoire
NkGIFAnimation *anim = NkGIFCodec::DecodeAnimation(data, bytes);
if (anim) {
    for (uint32 i = 0; i < anim->frameCount; ++i) {
        NkImage *f = anim->frames[i].image;      // RGBA32, déjà composée
        uint32   d = anim->frames[i].delayMs;    // durée d'affichage
        /* ... uploader f ... */
    }
    NkGIFCodec::FreeAnimation(anim);             // libère struct + toutes les NkImage*
}
```

## 1.6 Pièges et invariants (citations du dépôt)

**1. Ne jamais mélanger les allocateurs.** `Core/NkImage.h:31-36` :

> ```
>  RÈGLE DE MÉMOIRE :
>    Les buffers alloués via l'API statique DOIVENT être libérés avec img->Free().
>    Les buffers encodés (EncodePNG, EncodeJPEG, …) DOIVENT être libérés avec
>    nkentseu::memory::NkFree(ptr).  Ne jamais utiliser std::free / delete[] :
>    l'allocateur custom NKMemory n'est pas compatible avec le heap CRT standard
>    (crash c0000374 sur Windows en cas de mélange).
> ```

**2. `Free()` vs `Unload()`.** `Core/NkImage.h:769-783` :

> ```
>  Libère les pixels (si owning) ET libère le struct NkImage lui-même via nkFree.
>  À utiliser UNIQUEMENT sur les images créées via les fabriques statiques
>  (Alloc, Wrap, Create statique, Copy, CopyAs, Convert, Resize, Crop, …).
>  Ne jamais appeler Free() sur une image allouée sur la stack.
> ```
> ```
>  [NKIResource] Libère les pixels (si owning) et remet *this dans l'état
>  « image vide » SANS libérer le struct (contrairement à Free()).
> ```

**3. Le stride, pas la largeur.** `Core/NkImage.h:182-184` :

> ```
>  STRIDE :
>    Le stride (bytes par ligne) est aligné sur 4 octets : stride = (w*bpp+3)&~3.
>    Utiliser RowPtr(y) pour accéder à la ligne y de façon portable.
> ```

C'est la cause n°1 d'images « penchées » à l'upload GPU pour un débutant.

**4. `Wrap()` ne possède rien.** `Core/NkImage.h:795-800` :

> ```
>  Crée une vue non-owning sur un buffer pixel externe.
>  Le buffer n'est PAS libéré par le destructeur ni par Free().
>  L'appelant reste responsable de la durée de vie du buffer.
> ```

**5. Copie interdite, move autorisé.** `Core/NkImage.h:178-180` :

> ```
>  La copie par valeur est désactivée (= delete) pour éviter les doubles-free
>  accidentels.  Utiliser Copy() (deep clone) ou le move constructor.
> ```

**6. Ordre des arguments de `Create`** — bug réel corrigé, cité tel quel
(`NKCanvas\...\NkTexture.cpp:72-75`) :

> ```
>  NkImage::Create(w, h, NkColor color, int32 channels=4) : la COULEUR
>  d'abord, les canaux ensuite. (Avant : args inverses -> channels=0
>  pour un fill transparent -> Create echouait -> texture jamais creee.)
> ```

**7. Dessin CPU : LDR seulement.** `Core/NkImage.h:587-590` :

> ```
>  Formats LDR 8-bit (RGBA/RGB/RG/R). No-op si image invalide, hors
>  bornes, ou HDR. Couleur = math::NkColor (= NkColor2D). SetPixel ECRASE
>  (pas de blending) ; BlendPixel fait un src-over alpha.
> ```

**8. HDR : ne pas charger un `.hdr` en 4 canaux.**
`Applications\NkImageDemo\src\Demo\ViewerApp.cpp:369-372` :

> ```
>  NkImage::Load(.hdr, 4) ferait un Convert lineaire-clamp qui
>  crame toutes les valeurs > 1.0. On passe par [le codec HDR]
>  pour preserver le float, puis ConvertToTexture(exposure,gamma).
> ```

**9. Ordre d'initialisation côté texture.** `NkTexture.cpp:17-19` :

> ```
>  Active backend dispatch table. Initialement vide (callbacks null) :
>  les operations sur NkTexture sont des no-ops tant qu'aucun renderer
>  n'a appele NkTextureSetBackend() a la fin de son Initialize().
> ```

→ **Invariant : créer les textures APRÈS `renderer.Initialize()`.**

**10. Ordre `Destroy()` puis copie CPU** (`NkTexture.cpp:103-108`) :

> ```
>  Copie CPU des pixels — DOIT etre APRES Destroy() : Destroy() appelle
>  mCPUPixels.Clear(), donc faire la copie avant l'effacait aussitot
>  (=> GetCPUPixels()==null). Le rasterizer Software echantillonne
>  directement mCPUPixels ; sans cette copie -> textures/texte
>  INVISIBLES en Software. Fix bug 2026-06-05.
> ```

**11. Threads / allocateurs.** Aucun commentaire du module ne promet la
thread-safety. `NKImage.jenga:26` liste `NKThreading` en dépendance mais aucun
mutex n'est exposé dans l'API publique. Le cours doit poser la règle simple :
**décoder dans un worker, uploader sur le thread principal** — c'est ce que fait
`ViewerApp.cpp:463` (« Upload sur le main thread (libere l'image apres) »).

## 1.7 Raccordement NkCanvas / NKGui

Deux chemins, à présenter dans cet ordre :

1. **Chemin haut niveau (NkCanvas)** — `NkTexture::LoadFromFile(renderer, path)`
   fait tout : `NkImage img; img.Load(path);` puis upload GPU
   (`NkTexture.cpp:81-86`). C'est le chemin à enseigner en premier.
2. **Chemin bas niveau (NKGui)** — on charge soi-même, on aplatit en RGBA
   contigu, on appelle `backend.UploadImageRGBA(texId, data, w, h)` puis on passe
   le `texId` aux widgets image de NKGui (`NKGuiDemo\...\main.cpp:290-322`).

Le point de couture est toujours le même : **NKImage produit des octets CPU, le
renderer produit un identifiant GPU.** NKImage ne connaît ni OpenGL ni Vulkan.

**Verdict NKImage : ✅ utilisable, chapitre sans réserve** — à condition de
(a) sauter `SaveWebP`/`SaveSVG`, (b) prévenir sur `NK_BICUBIC`/`NK_LANCZOS3`,
(c) ne pas recopier `NkImageDemo` ni l'exemple périmé du haut de `NKImage.h`.

---

# 2. NKFont — polices, atlas, métriques, rendu de texte

## 2.1 Organisation

Racine : `D:\Projets\2026\Nkentseu\Nkentseu\Kernel\Runtime\NKFont`

```
NKFont/
  NKFont.jenga
  pch/pch.h · pch/pch.cpp
  src/NKFont/
    NkFont.h            ( 366 l.)  <- EN-TÊTE PUBLIC PRINCIPAL (NkFontAtlas + NkFont)
    NkFontAtlas.cpp     (~ 700 l.) <- packing + rastérisation
    NkFontMesh.cpp      (~ 400 l.) <- extrusion 3D des glyphes
    NkEarcut.h                     <- triangulation (pour le mesh 3D)
    Core/
      NkFontTypes.h     ( 112 l.)  <- alias nkft_*, NkFontCodepoint, NkGlyphId
      NkFontParser.h/.cpp          <- parser TTF/OTF/CFF/WOFF + rastériseur + SDF
      NkFontRasterizer.cpp
      NkFontDetect.h/.cpp          <- NkFontProfile / NkFontDetector (bitmap vs vectoriel)
      NkFontSizeCache.h/.cpp       <- cache multi-tailles + NkFontScaleRenderer
      NkUtils.h/.cpp
    Embedded/
      NkFontEmbedded.h/.cpp        <- 10 polices compressées DANS le binaire (2,1 Mo de .cpp)
      DejaVuSansMono_data.h · Inter_data.h
```

**Dépendances déclarées** — `Kernel\Runtime\NKFont\NKFont.jenga:35-39` :

```python
    nkentseudependson(
        ["NKPlatform", "NKCore", "NKMemory", "NKMath", "NKContainers", "NKThreading", "NKLogger"],
        selfexport="NKFont",
        extra_includes=["src"],
    )
```

**Aucune dépendance à NKImage ni NKCanvas** : NKFont produit un **bitmap alpha8**
en RAM, rien d'autre. C'est le point pédagogique central du chapitre.

⚠️ **La docstring du `.jenga` est fortement exagérée.** `NKFont.jenga:7-23` annonce
BDF, Type 1 (PFB/PFA), WOFF2/Brotli, hinting VM TrueType, GSUB (ligatures), Bidi
UAX#9. Vérification par recherche exhaustive sur `src/` : **aucune occurrence** de
`BDF`, `Type1`, `PFB`, `Brotli`, `Bidi`, `GSUB`, `Hinting`. La seule occurrence de
`WOFF2` est un avertissement, et `GPOS` n'apparaît qu'une fois, pour lire l'offset
de table sans jamais l'exploiter (`Core/NkFontParser.cpp:993`). **Ne pas recopier
cette docstring dans le cours.** La liste honnête est celle de l'en-tête du
parser, `Core/NkFontParser.h:5-14` :

```
// Formats supportés :
//   OK  TTF SimpleGlyph + CompositeGlyph
//   OK  TTC (TrueType Collection) via faceIndex
//   OK  OTF avec table glyf (contours TrueType)
//   OK  OTF/CFF (Type 2 charstrings — interpréteur intégré)
//   OK  WOFF (décompresseur zlib intégré)
//   /!\ WOFF2 (Brotli — non supporté, dépendance externe requise)
//   OK  cmap format 4 (BMP) et format 12 (full Unicode)
//   OK  kern table format 0
//   OK  SDF generation (Signed Distance Field)
```

## 2.2 Surface d'API publique

### 2.2.1 Types de base (`Core/NkFontTypes.h`)

```cpp
using NkFontCodepoint = nkentseu::uint32;                 // NkFontTypes.h:47
using NkGlyphId       = nkentseu::uint32;                 // NkFontTypes.h:52
static constexpr NkGlyphId NKFONT_INVALID_GLYPH_ID = 0xFFFFFFFFu;   // :59
static constexpr NkFontCodepoint NKFONT_CODEPOINT_REPLACEMENT = 0x0000FFFDu; // :62
// + les alias nkft_uint8/16/32, nkft_int32, nkft_float32, nkft_size (:31-42)
```

### 2.2.2 `struct NkFontGlyph` (`NkFont.h:32-38`)

```cpp
	struct NkFontGlyph {
			NkFontCodepoint codepoint = 0;
			nkft_float32 advanceX = 0.f;
			nkft_float32 x0 = 0, y0 = 0, x1 = 0, y1 = 0; ///< Position quad (pixels écran, relative au curseur).
			nkft_float32 u0 = 0, v0 = 0, u1 = 0, v1 = 0; ///< UV dans l'atlas [0..1].
			bool visible = true;
	};
```

C'est **la structure la plus importante du chapitre** : elle contient déjà tout ce
qu'il faut pour émettre un quad texturé. Le cours n'a rien d'autre à calculer.

### 2.2.3 `struct NkFontConfig` (`NkFont.h:44-62`)

```cpp
	struct NkFontConfig {
			const nkft_uint8 *fontData = nullptr;
			nkft_size fontDataSize = 0u;
			bool fontDataOwned = false;
			nkft_int32 fontIndex = 0;
			nkft_float32 sizePixels = 13.f;
			nkft_int32 oversampleH = 2;
			nkft_int32 oversampleV = 1;
			bool pixelSnapH = false;
			math::NkVec2f glyphOffset = {0.f, 0.f};
			nkft_float32 glyphMinAdvanceX = 0.f;
			nkft_float32 glyphMaxAdvanceX = 1e9f;
			nkft_float32 glyphExtraSpacing = 0.f;
			const nkft_uint32 *glyphRanges = nullptr;
			NkFontCodepoint glyphFallback = 0x003Fu;
			bool mergeMode = false;
			nkft_float32 rasterizerMultiply = 1.f;
			char name[40] = {};
	};
```

Les deux champs à comprendre en cours : `glyphRanges` (paires `{début, fin}`
terminées par un `0`) et `mergeMode` (fusionner une seconde police dans la même
`NkFont` pour combler les glyphes manquants).

### 2.2.4 `struct NkFontAtlas` (`NkFont.h:102-163`)

Constantes : `NK_FONT_ATLAS_MAX_FONTS = 16`, `NK_FONT_ATLAS_MAX_CUSTOM_RECTS = 64`
(`NkFont.h:99-100`).

Champs publics (le cours les lit directement, pas d'accesseurs) :

```cpp
			void *texID = nullptr;
			nkft_int32 texWidth = 0, texHeight = 0;
			nkft_uint8 *texPixels = nullptr;      // ALPHA8, 1 octet/pixel
			bool texReady = false;
			nkft_int32 texDesiredWidth = 0;
			nkft_int32 texGlyphPadding = 2;       ///< Padding recommandé >= 2 pour éviter le bleeding.
			bool sdfMode = false;
			nkft_int32 sdfSpread = 6;             ///< Rayon SDF en pixels (4-8 typique).
			NkFont *fonts[NK_FONT_ATLAS_MAX_FONTS] = {};
			NkFontConfig configs[NK_FONT_ATLAS_MAX_FONTS];
			nkft_uint32 fontCount = 0u;
```

Méthodes (`NkFont.h:134-159`) :

```cpp
NkFont* AddFontFromFile(const char* path, nkft_float32 sizePixels, const NkFontConfig* cfg = nullptr);      // :134
NkFont* AddFontFromMemory(const nkft_uint8* data, nkft_size dataSize,
                          nkft_float32 sizePixels, const NkFontConfig* cfg = nullptr);                      // :135
NkFont* AddFontFromMemoryOwned(nkft_uint8* data, nkft_size dataSize,
                               nkft_float32 sizePixels, const NkFontConfig* cfg = nullptr);                 // :137

nkft_int32 AddCustomRect(nkft_uint16 w, nkft_uint16 h);                                                     // :140
const NkFontAtlasCustomRect* GetCustomRect(nkft_int32 idx) const;                                           // :141

static const nkft_uint32* GetGlyphRangesDefault();       // :143
static const nkft_uint32* GetGlyphRangesLatinExtA();     // :144
static const nkft_uint32* GetGlyphRangesCyrillic();      // :145
static const nkft_uint32* GetGlyphRangesGreek();         // :146
static const nkft_uint32* GetGlyphRangesChineseFull();   // :147

bool Build();                                            // :149  <- rastérise TOUT

void GetTexDataAsAlpha8 (nkft_uint8** op, nkft_int32* ow, nkft_int32* oh, nkft_int32* ob = nullptr);  // :151
void GetTexDataAsRGBA32 (nkft_uint8** op, nkft_int32* ow, nkft_int32* oh, nkft_int32* ob = nullptr);  // :152

void ClearTexData();     // :154
void Clear();            // :155
bool IsBuilt() const;    // :157
```

Copie interdite : `NkFontAtlas(const NkFontAtlas&) = delete;` (`NkFont.h:129`).

### 2.2.5 `struct NkFont` (`NkFont.h:210-307`)

Constantes : `NK_FONT_MAX_GLYPHS = 4096`, `NK_FONT_INDEX_SIZE = 256` (`NkFont.h:169-170`).

```cpp
			NkFontAtlas *containerAtlas = nullptr;    // :211  (l'atlas POSSÈDE cette NkFont)
			NkFontConfig config;
			nkft_float32 fontSize = 0, ascent = 0, descent = 0, lineAdvance = 0, scale = 1;   // :213
			NkFontGlyph glyphs[NK_FONT_MAX_GLYPHS];
			nkft_uint32 glyphCount = 0u;
			const NkFontGlyph *fallbackGlyph = nullptr;
			nkft_float32 fallbackAdvanceX = 0;
```

Méthodes de lecture (celles du cours) :

```cpp
const NkFontGlyph* FindGlyph(NkFontCodepoint c) const;              // :226  (avec repli)
const NkFontGlyph* FindGlyphNoFallback(NkFontCodepoint c) const;    // :227
nkft_float32 GetCharAdvance(NkFontCodepoint c) const;               // :229 (inline)
nkft_float32 CalcTextSizeX(const char* text, const char* textEnd = nullptr) const;   // :234
static NkFontCodepoint DecodeUTF8(const char** text, const char* textEnd);           // :235
bool GetGlyphOutlinePoints(NkFontCodepoint cp, NkVector<NkFontOutlineVertex>& outVertices) const; // :247
nkft_int32 FindGlyphIndex(NkFontCodepoint c) const;                 // :253
void BuildLookupTable();  void AddGlyph(const NkFontGlyph&);  void AddGlyphSorted(const NkFontGlyph&); // :250-252
```

Extrusion 3D (implémentée dans `NkFontMesh.cpp:320-380`) :

```cpp
NkFontGlyphMesh3D GenerateGlyphMesh3D(NkFontCodepoint cp, nkft_float32 scale, nkft_float32 extrusionDepth,
                                      const math::NkMat4f& worldMatrix, const math::NkVec4f& color) const;  // NkFont.h:271
NkFontGlyphMesh3D GenerateTextMesh3D (const char* text, nkft_float32 scale, nkft_float32 extrusionDepth,
                                      const math::NkMat4f& worldMatrix, const math::NkVec4f& color) const;  // :283
void ForEachGlyph3DVertex(...NkFont3DVertexCallback callback, void* userData) const;                        // :297
void ForEachText3DVertex (...NkFont3DVertexCallback callback, void* userData) const;                        // :304
```

Fonctions libres UTF-8 (`NkFont.h:313-314`) :

```cpp
NkFontCodepoint NkFontDecodeUTF8(const char** str, const char* end);
nkft_int32      NkFontEncodeUTF8(NkFontCodepoint c, char* out, nkft_int32 outSize);
```

Shaders GLSL prêts à l'emploi, fournis en constantes (`NkFont.h:324` et `:343`) :
`kNkFontFragNormal` (atlas alpha) et `kNkFontFragSDF` (atlas SDF).

### 2.2.6 Polices embarquées — `NkFontEmbedded` (`Embedded/NkFontEmbedded.h:112-163`)

```cpp
enum class NkEmbeddedFontId : nkft_uint32 {
    ProggyClean = 0, ProggyTiny = 1, DroidSans = 2, Karla = 3, Roboto = 4,
    Cousine = 5, SourceCodePro = 6, DroidSerif = 7, DejaVuSansMono = 8, Inter = 9,
    Count = 10, Default = ProggyClean,
};                                                   // NkFontEmbedded.h:49-74

static bool IsAvailable(NkEmbeddedFontId id);                                       // :120
static const NkEmbeddedFontData* GetData(NkEmbeddedFontId id);                      // :126
static NkFont* AddToAtlas(NkFontAtlas& atlas, NkEmbeddedFontId id,
                          nkft_float32 sizePx = 0.f, const NkFontConfig* cfg = nullptr); // :137
static NkFont* AddDefaultFont(NkFontAtlas& atlas, const NkFontConfig* cfg = nullptr);    // :144
static const char* GetName(NkEmbeddedFontId id);                                    // :149
static const NkEmbeddedFontData* GetAll(nkft_int32* outCount);                      // :155
static nkft_uint8* DecompressData(const NkEmbeddedFontData& data, nkft_uint32* outSize = nullptr); // :157
static void FreeDecompressedData(nkft_uint8* ptr);                                  // :158
```

**Les 10 polices sont RÉELLEMENT présentes** — vérifié dans le registre
`Embedded/NkFontEmbedded.cpp:20265-20370` : chaque entrée pointe sur un tableau de
données non nul (`sProggyCleanCompressedData`, `sDroidSansCompressedData`,
`sRobotoRegularCompressedData`, `sCousineRegularCompressedData`,
`sSourceCodeProRegularCompressedData`, `sDroidSerifCompressedData`,
`sKarlaRegularCompressedData`, `sDejaVuSansMonoCompressedData`,
`sInterCompressedData`). `IsAvailable()` teste exactement ces trois champs
(`NkFontEmbedded.cpp:20380-20386`).

Licences déclarées dans le registre : ProggyClean/ProggyTiny = MIT,
DroidSans/Roboto/Cousine/DroidSerif = Apache 2.0, Karla/SourceCodePro/Inter = OFL 1.1,
DejaVuSansMono = Bitstream Vera / domaine public.

→ **Conséquence pédagogique majeure : on peut écrire du texte sans AUCUN fichier
de police sur le disque.** C'est la porte d'entrée idéale du chapitre.

### 2.2.7 Détection & cache

`Core/NkFontDetect.h` : `enum class NkFontKind` (`:35`), `struct NkFontProfile`
(`:54-76`), `class NkFontDetector` (`:92`) avec
`Analyze` (`:104`), `AnalyzeBuffer` (`:110`), `AnalyzeFile` (`:115`),
`ApplyOptimalConfig(profile, sizePx, outConfig)` (`:123`),
`RecommendsNearestFilter(profile)` (`:129`), `SnapSize(profile, sizePx)` (`:137`).

`Core/NkFontSizeCache.h` : `class NkFontSizeCache` (`:85`) —
`Init(atlas, fontPath, preloadSizes, preloadCount)` (`:106`),
`GetFont(sizePx, frameIdx)` (`:122`), `BuildAtlasIfNeeded()` (`:128`),
`NeedsGpuUpload()` / `ClearGpuUploadFlag()` (`:133`/`:138`), `MAX_CACHED_SIZES = 16` (`:87`).
Plus `struct NkFontScaleRenderer` (`:205`) : `ComputeScale`, `CalcTextSizeX`,
`GetScaledGlyphQuad` — trois helpers **entièrement inline dans le header**, donc
sûrement fonctionnels.

## 2.3 Implémenté vs déclaré-mais-vide

### Réellement implémenté
- Parsing TTF (simple + composite), TTC, OTF glyf, OTF/CFF Type 2, WOFF (zlib),
  cmap 4 et 12, table `kern` format 0 (`Core/NkFontParser.cpp:1223-1226`).
- Rastérisation scanline + oversampling, packing shelf, `Build()`
  (`NkFontAtlas.cpp:329`).
- SDF : `sdfMode` est réellement branché — `NkFontAtlas.cpp:496-535` alloue un
  buffer temporaire et appelle `nkfont::NkMakeSDFFromBitmap(dst, drawW, drawH, sdfDst, sdfSpread)`.
- `GetTexDataAsAlpha8` (`NkFontAtlas.cpp:640`) et `GetTexDataAsRGBA32`
  (`:662`, conversion « blanc + alpha = couverture », allouée à la demande).
- Les 10 polices embarquées.
- L'extrusion 3D (`NkFontMesh.cpp:320`, `:338`, `:370`, `:375`) — non vide.

### Annoncé mais absent — NE PAS enseigner
- **WOFF2 / Brotli** : `Core/NkFontParser.h:11` — « non supporté, dépendance
  externe requise ». Aucune ligne de code Brotli dans le module.
- **GSUB / ligatures / Bidi / hinting VM / BDF / Type 1** : promis par
  `NKFont.jenga:16-23`, totalement absents du code.
- **GPOS** : l'offset de table est lu (`Core/NkFontParser.cpp:993`) et jamais utilisé.
- **Kerning côté NKCanvas** : `NKCanvas\...\Resources\NkFont.h:87` le dit
  franchement — « Le module NKFont n'expose pas le kerning par paire -> 0. »
  (le kerning existe pourtant côté parser via `NkGetGlyphKernAdvance`,
  `Core/NkFontParser.h:171` — il n'est simplement pas remonté au wrapper).
- **Faux-gras** : `NKCanvas\...\Resources\NkFont.h:84-85` — « bold ignoré (le
  module ne fait pas de faux-gras ; charger une fonte bold dédiée si nécessaire). »
- `NkFontSdf.h` mentionné dans `Core/NkFontSizeCache.h:193` (« voir NkFontSdf.h
  (futur) ») **n'existe pas**.

### Limite dure à enseigner
`NkFont.h:169` : `NK_FONT_MAX_GLYPHS = 4096u` — un tableau **fixe** dans le
`struct NkFont`. Charger le range CJK complet dépasse largement 4096 glyphes.
De même `NK_FONT_ATLAS_MAX_FONTS = 16` limite le nombre de tailles/polices
empilables dans un atlas.

## 2.4 Exemples d'utilisation réels tirés du dépôt

### 2.4.1 `NkGuiFont` — le chemin officiel NKFont vers NKGui

`Kernel\Runtime\NKGui\src\NKGui\Core\NkGuiFont.cpp:118-133`, recopié tel quel :

```cpp
		bool NkGuiFont::LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback) noexcept {
			atlas.Clear();
			face = nullptr;
			pixels = nullptr; // rechargeable
			NkFontConfig cfg;
			cfg.glyphRanges = NkGlyphRanges();
			face = NkFontEmbedded::AddToAtlas(atlas, id, sizePx, &cfg);
			if (!face)
				return false;
			NkMergeFallback(atlas, sizePx, extFallback); // repli pour les glyphes manquants
			if (!atlas.Build())
				return false;
			int32 bpp = 0;
			atlas.GetTexDataAsAlpha8(&pixels, &atlasW, &atlasH, &bpp);
			dirty = (pixels != nullptr && atlasW > 0 && atlasH > 0);
			return dirty;
		}
```

C'est **la séquence canonique en 4 temps** : `Clear()` puis `Add*()` puis `Build()`
puis `GetTexDataAsAlpha8()`. Le mécanisme de repli (`NkGuiFont.cpp:98-113`) montre
`mergeMode` en action :

```cpp
		static void NkMergeFallback(NkFontAtlas &atlas, float32 sizePx, bool ext) noexcept {
			if (!ext)
				return;
			auto add = [&](const char *path, const uint32 *ranges) {
				if (!NkFileExists(path))
					return;
				NkFontConfig fb;
				fb.glyphRanges = ranges;
				fb.mergeMode = true;
				atlas.AddFontFromFile(path, sizePx > 0.f ? sizePx : 16.f, &fb);
			};
			add(gFbBroad, NkBroadRanges());
			add(gFbCjk, NkCjkRanges());
			add(gFbEmoji, NkEmojiRanges());
		}
```

Structure du wrapper (`NkGuiFont.h:19-69`) — à donner en modèle aux élèves :

```cpp
		struct NKENTSEU_NKGUI_CLASS_EXPORT NkGuiFont {
				NkFontAtlas atlas;			///< possède la texture + les glyphes
				NkFont *face = nullptr;		///< face produite (détenue par l'atlas)
				uint32 texId = 0x4E4B4654u; ///< 'NKFT' — id stable pour le backend
				uint8 *pixels = nullptr;	///< atlas alpha8 (détenu par l'atlas)
				int32 atlasW = 0;
				int32 atlasH = 0;
				bool dirty = false; ///< à (ré)uploader côté backend
				bool LoadEmbedded(NkEmbeddedFontId id, float32 sizePx, bool extFallback = true) noexcept;
				bool LoadFromFile(const char *path, float32 sizePx, bool extFallback = true) noexcept;
				/* Face() TexId() Valid() Ascent() Descent() LineHeight() MeasureWidth() */
		};
```

### 2.4.2 Émettre les quads de texte — `NkGuiDrawList::AddText`

`Kernel\Runtime\NKGui\src\NKGui\Core\NkGuiDrawList.cpp:237-283`. Le cœur de la
boucle (à recopier tel quel dans le cours, c'est **le** patron de rendu texte) :

```cpp
			while (p < end) {
				const NkFontCodepoint cp = NkFontDecodeUTF8(&p, end);
				if (cp == 0u)
					break;
				const NkFontGlyph *g = face->FindGlyph(cp);
				if (!g)
					continue;
				if (g->visible) {
					const float32 x0 = NkGuiPixelSnap(x + g->x0), y0 = NkGuiPixelSnap(y + g->y0);
					const float32 x1 = x0 + (g->x1 - g->x0), y1 = y0 + (g->y1 - g->y0);
					if (x1 > xEnd)
						break; // troncature simple
					const uint32 i0 = Vtx({x0 + sTop, y0}, {g->u0, g->v0}, c);
					const uint32 i1 = Vtx({x1 + sTop, y0}, {g->u1, g->v0}, c);
					const uint32 i2 = Vtx({x1 + sBot, y1}, {g->u1, g->v1}, c);
					const uint32 i3 = Vtx({x0 + sBot, y1}, {g->u0, g->v1}, c);
					Tri(i0, i1, i2, texId);
					Tri(i0, i2, i3, texId);
				}
				x += g->advanceX;
			}
```

Signature complète (`NkGuiDrawList.cpp:237-238`) :

```cpp
void NkGuiDrawList::AddText(const NkFont *face, uint32 texId, const NkVec2 &baseline, const char *text,
                            const NkColor &col, float32 maxWidth, float32 skew) noexcept;
void NkGuiDrawList::AddTextRange(const NkFont *face, uint32 texId, const NkVec2 &baseline, const char *begin,
                                 const char *end, const NkColor &col) noexcept;   // :286
```

### 2.4.3 `Pong` — atlas multi-tailles + upload via NkCanvas (application **bâtie**)

`Applications\Pong\src\Pong\Render\FontAtlas.cpp` (371 lignes ; `Pong` est présent
dans `Build/Bin/Debug-Windows/`). Extrait `FontAtlas.cpp:48-90` :

```cpp
		bool FontAtlas::Init(renderer::NkRenderer2D &r) {
			mAtlas = new NkFontAtlas();

			NkEmbeddedFontId fontId = NkEmbeddedFontId::ProggyClean;
			if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::Karla)) {
				fontId = NkEmbeddedFontId::Karla;
			} else if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::DroidSans)) {
				fontId = NkEmbeddedFontId::DroidSans;
			} else if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::Roboto)) {
				fontId = NkEmbeddedFontId::Roboto;
			}

			// Rasterise la police a chaque taille de slot.
			const int slotN = static_cast<int>(SlotCount);
			for (int s = 0; s < slotN; ++s) {
				mFonts[s] = NkFontEmbedded::AddToAtlas(*mAtlas, fontId, kSizePx[s]);
				/* ... */
			}

			// Construit l'atlas (packing des glyphes dans une seule image).
			if (!mAtlas->Build()) { /* ... */ return false; }

			nkft_uint8 *pixels = nullptr;
			int w = 0, h = 0;
			mAtlas->GetTexDataAsAlpha8(&pixels, &w, &h);
```

Les 5 tailles sont déclarées `FontAtlas.cpp:30-36` :

```cpp
		static const float kSizePx[FontAtlas::SlotCount] = {
			14.0f, // SmallSlot
			18.0f, // BodySlot
			28.0f, // SubtitleSlot
			48.0f, // HeadlineSlot
			72.0f  // DisplaySlot
		};
```

Puis la conversion alpha8 vers RGBA et l'upload, avec un commentaire du dépôt qui
vaut avertissement (`FontAtlas.cpp:92-121`) :

```cpp
			// L'atlas NKFont est alpha8 (1 canal = couverture du glyphe). Le
			// shader 2D de NKCanvas echantillonne la texture en RGBA puis la
			// multiplie par la couleur du vertex. On convertit donc l'atlas en
			// RGBA8 "blanc + alpha=couverture" : texture(1,1,1,cov) * color =
			// (color.rgb, color.a*cov) => exactement le rendu de texte voulu.
			const usize pixCount = static_cast<usize>(w) * static_cast<usize>(h);
			NkVector<uint8> rgba;
			rgba.Resize(pixCount * 4u);
			for (usize i = 0; i < pixCount; ++i) {
				rgba[i * 4 + 0] = 255;
				rgba[i * 4 + 1] = 255;
				rgba[i * 4 + 2] = 255;
				rgba[i * 4 + 3] = pixels[i]; // couverture du glyphe
			}

			// On enveloppe nos pixels RGBA dans une NkImage puis on upload en un
			// seul LoadFromImage (qui passe par gTextureBackend.Create — cable).
			// NB : surtout PAS Create()+Update() : Update exige gTextureBackend.Update
			// qui n'est pas cable sur tous les backends -> echec silencieux.
			NkImage atlasImg;
			if (!atlasImg.Create(static_cast<uint32>(w), static_cast<uint32>(h), math::NkColor(0, 0, 0, 0), 4)) { /* ... */ }
			std::memcpy(atlasImg.Pixels(), rgba.Data(), pixCount * 4);
			mTexture = new renderer::NkTexture();
			if (!mTexture->LoadFromImage(*r.GetBackend(), atlasImg)) { /* ... */ }
			mTexture->SetFilter(renderer::NkTextureFilter::NK_LINEAR);
```

**C'est le chapitre « NKFont + NKImage + NkCanvas » en un seul bloc.**

Et la boucle de dessin (`FontAtlas.cpp:210-240`) :

```cpp
			const char *p = text;
			while (*p != '\0') {
				const char *pBefore = p;
				NkFontCodepoint cp = NkFont::DecodeUTF8(&p, nullptr);
				if (cp == 0) {
					if (p == pBefore)
						++p;
					continue;
				}
				const NkFontGlyph *g = font->FindGlyph(cp);
				if (g == nullptr)
					continue;
				if (g->visible) {
					PushGlyphQuad(verts, idx, cursorX + g->x0, cursorY + g->y0, g->x1 - g->x0, g->y1 - g->y0, g->u0,
					              g->v0, g->u1, g->v1, color);
				}
				/* cursorX += g->advanceX; ... */
			}
```

### 2.4.4 Wrapper NkCanvas « SFML-like » — `renderer::NkFont`

`Kernel\Runtime\NKCanvas\src\NKCanvas\Renderer\Resources\NkFont.h:63-115` :

```cpp
		class NkFont {
			public:
				bool LoadFromFile(NkIRenderer2D &renderer, const char *path);
				bool LoadFromMemory(NkIRenderer2D &renderer, const void *data, usize sizeBytes);
				const NkGlyph &GetGlyph(uint32 codepoint, uint32 characterSize, bool bold = false) const;
				float32 GetKerning(uint32 first, uint32 second, uint32 characterSize) const;
				float32 GetLineHeight(uint32 characterSize) const;
				const NkTexture *GetAtlasTexture(uint32 characterSize) const;
				bool IsValid() const;
				void Destroy();
			private:
				struct Page {                       // une page = une TAILLE de caractère
						uint32 characterSize = 0;
						::nkentseu::NkFontAtlas *atlas = nullptr; // heap (placement new)
						::nkentseu::NkFont *moduleFont = nullptr; // possédé par atlas
						NkTexture texture;
				};
		};
```

avec l'explication d'architecture, `NkFont.h:55-62` :

> ```
>  Depuis le refactoring 2026-05-28, NKCanvas ne réimplémente plus la
>  rasterisation (ex-FreeType supprimé). renderer::NkFont délègue au module
>  NKFont (nkentseu::NkFontAtlas + nkentseu::NkFont) : une "page" (atlas
>  rasterisé + texture GPU) par taille de caractère demandée.
> ```

## 2.5 Squelette minimal pour une application

**(a) Le plus court possible — police embarquée, aucun fichier**

```cpp
#include "NKFont/NkFont.h"
#include "NKFont/Embedded/NkFontEmbedded.h"
using namespace nkentseu;

// 1) L'ATLAS possède tout. Une seule instance, vivante aussi longtemps que le texte.
NkFontAtlas atlas;

// 2) Ajouter une ou plusieurs polices/tailles AVANT Build().
NkFont *body  = NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::Inter, 18.f);
NkFont *title = NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::Inter, 32.f);

// 3) Rastériser : c'est ICI que les pixels apparaissent.
if (!atlas.Build())
    return false;

// 4) Récupérer le bitmap ALPHA8 (1 octet/pixel) et l'uploader.
uint8 *pixels = nullptr; int32 w = 0, h = 0, bpp = 0;
atlas.GetTexDataAsAlpha8(&pixels, &w, &h, &bpp);   // bpp == 1
// -> convertir en RGBA blanc+alpha (cf. Pong) puis créer la texture GPU.

// 5) Mesurer / dessiner.
float largeur = body->CalcTextSizeX("Bonjour");
float hauteurLigne = body->lineAdvance;

// 6) Rien à libérer : ~NkFontAtlas() appelle Clear() qui détruit fonts + pixels.
```

**(b) Depuis un fichier TTF**

```cpp
NkFontAtlas atlas;
NkFontConfig cfg;
static const uint32 kRanges[] = { 0x0020, 0x00FF, 0x2500, 0x257F, 0 };  // Latin-1 + box-drawing
cfg.glyphRanges = kRanges;
NkFont *f = atlas.AddFontFromFile("Resources/Fonts/Inter-Regular.ttf", 20.f, &cfg);
if (!f || !atlas.Build()) return false;
```

**(c) Dessiner une chaîne UTF-8 (patron universel)**

```cpp
float x = posX, y = posY + font->ascent;     // y = LIGNE DE BASE, pas le haut !
const char *p = texte;
const char *end = p; while (*end) ++end;
while (p < end) {
    const NkFontCodepoint cp = NkFontDecodeUTF8(&p, end);
    if (cp == 0u) break;
    const NkFontGlyph *g = font->FindGlyph(cp);
    if (!g) continue;
    if (g->visible) {
        // quad écran : (x + g->x0, y + g->y0) .. (x + g->x1, y + g->y1)
        // UV atlas   : (g->u0, g->v0) .. (g->u1, g->v1)
        EmettreQuad(x + g->x0, y + g->y0, x + g->x1, y + g->y1,
                    g->u0, g->v0, g->u1, g->v1, couleur);
    }
    x += g->advanceX;
}
```

**(d) Mode SDF (texte net à toute échelle)**

```cpp
NkFontAtlas atlas;
atlas.sdfMode   = true;      // AVANT Build()
atlas.sdfSpread = 6;         // 4-8 pixels
NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::Inter, 48.f);
atlas.Build();
// Utiliser le shader fourni : nkentseu::kNkFontFragSDF (NkFont.h:343)
// avec uSmoothing ~ 0.1 / fontSize.
```

## 2.6 Pièges et invariants (citations du dépôt)

**1. L'ORDRE est absolu : `Add*` puis `Build` puis `GetTexData`.** `Build()` renvoie
`false` et journalise si l'atlas est vide (`NkFontAtlas.cpp:330-333`) :

```cpp
		if (fontCount == 0) {
			logger.Info("[NkFontAtlas] Build():aucune fonte\n");
			return false;
		}
```

et `GetTexDataAsAlpha8` **renvoie `nullptr` en silence** si `Build()` n'a pas eu
lieu (`NkFontAtlas.cpp:640-642`) : `if (!texReady || !texPixels) { *op = nullptr; ... }`.

**2. L'atlas POSSÈDE les `NkFont*`. Ne jamais les `delete`.** `NkFont.h:88-90` :

> ```
>  NkFont (le struct produit) n'est pas auto-portant : il est detenu par
>  son `containerAtlas` qui parse le TTF, fournit les glyphes et la
>  texture. NkFont seul ne « se charge » pas.
> ```

**3. `mergeMode` produit des pointeurs DUPLIQUÉS — bug réel documenté.**
`NkFontAtlas.cpp:185-189`, cité mot pour mot :

> ```
>  Les polices FUSIONNÉES (mergeMode) partagent le MÊME NkFont* que leur police de
>  base (cf. AddFontFromMemoryOwned) : fonts[] contient donc des pointeurs DUPLIQUÉS.
>  Il faut ne détruire chaque pointeur unique QU'UNE fois — sinon double-free / tas
>  corrompu au rechargement (crash zoom).
> ```

**4. `Build()` utilise des tableaux `static` : NI réentrant, NI thread-safe, et
un second atlas invalide le premier.** `NkFontAtlas.cpp:336-337` :

```cpp
		static nkfont::NkFontFaceInfo faceInfos[NK_FONT_ATLAS_MAX_FONTS];
		static nkft_float32 scales[NK_FONT_ATLAS_MAX_FONTS];
```

et `NkFontAtlas.cpp:381-382` stocke un pointeur vers ce tableau statique dans
chaque police :

```cpp
				// Stockage du faceInfo pour l'extraction des contours
				font->m_FaceInfo = &faceInfos[i];
```

(les tableaux de packing `static NkPackRect packRects[NK_FONT_ATLAS_MAX_FONTS * 8192]`
et suivants, `NkFontAtlas.cpp:381-386`, sont eux aussi statiques.)

→ **Invariant à énoncer clairement : un seul `NkFontAtlas::Build()` à la fois,
sur un seul thread ; et `GetGlyphOutlinePoints` / les fonctions 3D d'un atlas A ne
sont valides que tant qu'aucun autre atlas n'a appelé `Build()`.** Le cours doit
recommander UN atlas par application.

**5. Le padding de l'atlas.** `NkFont.h:111` :
`nkft_int32 texGlyphPadding = 2; ///< Padding recommandé >= 2 pour éviter le bleeding.`

**6. Le rebuild invalide la texture GPU.** `Core/NkFontSizeCache.h:62-63` :

> ```
>  @note Le rebuild de l'atlas invalide toutes les textures GPU.
>        L'application doit être notifiée pour re-uploader la texture.
> ```

D'où le drapeau `dirty` de `NkGuiFont` (`NkGuiFont.h:26`) et
`NeedsGpuUpload()` / `ClearGpuUploadFlag()` du cache.

**7. `NkFontSizeCache::GetFont` peut renvoyer `nullptr` la première fois.**
`Core/NkFontSizeCache.h:114-117` :

> ```
>  Si la taille n'est pas dans le cache :
>    1. Ajoute la fonte à l'atlas.
>    2. Marque l'atlas comme nécessitant un rebuild.
>    3. Retourne nullptr jusqu'au prochain appel après BuildAtlas().
> ```

**8. L'atlas est en ALPHA8 — il faut le convertir.** Le shader 2D de NkCanvas veut
du RGBA ; la recette « blanc + alpha = couverture » est celle de Pong
(`FontAtlas.cpp:92-104`) ou celle intégrée de `GetTexDataAsRGBA32`
(`NkFontAtlas.cpp:662-680`) qui fait exactement la même chose.

**9. Ne pas confondre haut du texte et ligne de base.** Pong approxime
(`FontAtlas.cpp:206-208`) : `const float ascent = fontPx * 0.82f;` — mais la
valeur exacte est `font->ascent`, calculée par `Build()` (`NkFontAtlas.cpp:377`).
Le cours doit enseigner `font->ascent` / `font->lineAdvance`, pas la constante magique.

**10. Le pixel-snap est indispensable au texte net.** `NkGuiDrawList.cpp:225-231` :

> ```
>  Le curseur de texte accumule des avances FRACTIONNAIRES. Sans arrondi,
>  seul le PREMIER glyphe d'une chaîne tombe sur un pixel entier ; tous les
>  suivants dérivent et échantillonnent l'atlas ENTRE deux texels, ce qui
>  rend le texte uniformément flou.
> ```

Et l'invariant subtil, `NkGuiDrawList.cpp:257-266` :

> ```
>  On arrondit la POSITION du quad, jamais l'AVANCE : `x` continue
>  d'accumuler la valeur exacte, donc la largeur totale de la chaîne
>  est inchangée et MeasureWidth reste d'accord avec le rendu.
> ```

**11. Piège d'upload signalé par le dépôt** (`Pong\...\FontAtlas.cpp:107-109`) :

> ```
>  NB : surtout PAS Create()+Update() : Update exige gTextureBackend.Update
>  qui n'est pas cable sur tous les backends -> echec silencieux.
> ```

**12. Les polices de repli externes se posent AVANT le chargement.**
`NkGuiFont.h:76-77` :

> ```
>  A poser par l'APPLICATION (ex. NKCode, depuis son dossier data/fonts) AVANT
>  de charger les polices. Un chemin nullptr/vide = role desactive.
> ```

signature : `void NkSetFallbackFontPaths(const char* broad, const char* cjk, const char* emoji) noexcept;`
(`NkGuiFont.h:78`).

## 2.7 Raccordement NkCanvas / NKGui

Trois chemins, du plus simple au plus manuel :

1. **NKGui** — `NkGuiFont::LoadEmbedded(id, taille)`, puis
   `backend.UploadFontGray8(font.TexId(), font.pixels, font.atlasW, font.atlasH)`
   (vu dans `Applications\NKGuiDemo\src\NKGuiDemo\main.cpp:281-288`), puis
   `ctx.font = &font;`. Ensuite les widgets dessinent le texte tout seuls via
   `NkGuiDrawList::AddText`. **C'est le chemin du cours.**
2. **NkCanvas SFML-like** — `renderer::NkFont::LoadFromFile(renderer, path)` +
   `NkText` ; une page d'atlas par `characterSize`. Zéro code de glyphe à écrire,
   mais pas de kerning ni de gras.
3. **Manuel** — atlas NKFont + conversion RGBA + `NkTexture` + boucle de quads
   (le modèle Pong, section 2.4.3). C'est celui qui *explique* comment ça marche.

**Verdict NKFont : utilisable, chapitre solide** — à condition de
(a) remplacer la liste de fonctionnalités du `.jenga` par celle du parser,
(b) enseigner l'invariant « un seul atlas, un seul `Build()` à la fois »,
(c) annoncer les plafonds `NK_FONT_MAX_GLYPHS = 4096` / `NK_FONT_ATLAS_MAX_FONTS = 16`.

---

# 3. NKAudio — lecture, mixage, périphériques

## 3.1 Organisation

Racine : `D:\Projets\2026\Nkentseu\Nkentseu\Kernel\Runtime\NKAudio`

```
NKAudio/
  NKAudio.jenga
  pch/pch.h · pch/pch.cpp
  src/NKAudio/
    NkAudio.h            (1427 l.)  <- EN-TÊTE PUBLIC UNIQUE : tout est là
    NkAudioApi.h         ( 187 l.)  <- macros d'export NKENTSEU_AUDIO_API
    NkAudioExport.h                 <- alias historique
    NkAudioEngineCore.cpp           <- AudioEngine (PIMPL)
    NkAudioLoader.cpp               <- AudioLoader (WAV natif + dispatch codecs)
    NkAudioMixer.cpp · NkAudioGenerator.cpp · NkAudioAnalyzer.cpp
    NkAudioEffects.{h,cpp}          <- effets DSP concrets
    NkAudioBackends.{h,cpp}         <- WASAPI / CoreAudio / ALSA / AAudio / WebAudio / Null
    NkAudioBus.{h,cpp}              <- bus hiérarchiques Master -> SFX/Music/Voice/UI
    NkAudioCapture.{h,cpp}          <- MICRO (entrée)
    NkDenoiser.{h,cpp} · NkHrtfDataset.{h,cpp}
    Codecs/
      MP3/NkMP3Codec · OGG/NkOGGVorbisCodec · FLAC/NkFLACCodec · Opus/NkOpusCodec
    Streaming/
      NkAudioStream.{h,cpp}         <- IAudioStream (pull) + WavStream/AiffStream/MemoryStream
      NkAudioStreamPlayer.{h,cpp}   <- thread décodeur + ring buffer
      NkContainerAudioStream.{h,cpp}<- piste audio d'un conteneur vidéo
  tests/ NkAudioTests.cpp · TestAudioFormats.cpp · TestFLAC.cpp · TestMP3.cpp · TestStreaming.cpp
```

**Dépendances déclarées** — `Kernel\Runtime\NKAudio\NKAudio.jenga:30-35` :

```python
    nkentseudependson(
        # NKMedia : decodeur Opus from-scratch (codec .opus via NkOpusCodec).
        ["NKPlatform", "NKCore", "NKMemory", "NKContainers", "NKLogger", "NKThreading", "NKFileSystem", "NKStream", "NKMedia"],
        selfexport="NKAudio",
        extra_includes=["src"],
    )
```

→ **NKAudio dépend de NKMedia** (uniquement pour Opus). À signaler dans le cours :
l'ordre des chapitres NKMedia/NKAudio n'est donc pas indifférent côté build.

Un seul include suffit pour tout l'essentiel : `#include "NKAudio/NkAudio.h"`.
Tout vit dans le namespace **`nkentseu::audio`**.

## 3.2 Surface d'API publique

### 3.2.1 Enums (`NkAudio.h:95-206`)

```cpp
enum class AudioFormat   { UNKNOWN=0, WAV, MP3, OGG, FLAC, OPUS, AIFF, RAW };            // :95-107
enum class SampleFormat  { UNKNOWN=0, INT_8, INT_16, INT_24, INT_32, FLOAT_32, FLOAT_64 };// :109-119
enum class AudioBackendType { AUTO=0, DIRECTSOUND, WASAPI, CORE_AUDIO, ALSA, PULSE_AUDIO,
                              OPEN_SL_ES, AAUDIO, WEB_AUDIO, CUSTOM, NULL_OUTPUT };       // :122-136
enum class WaveformType  { SINE=0, SQUARE, TRIANGLE, SAWTOOTH, NOISE_WHITE, NOISE_PINK, PULSE }; // :139-147
enum class AudioEffectType { NONE=0, REVERB, ECHO, DELAY, CHORUS, FLANGER, PHASER,
                             DISTORTION, COMPRESSOR, LIMITER, GATE, EQ_3BAND, EQ_PARAMETRIC,
                             LOW_PASS, HIGH_PASS, BAND_PASS, NOTCH, PITCH_SHIFT,
                             TIME_STRETCH, AUTOPAN, TREMOLO, VIBRATO };                    // :152-176
enum class VoiceState    { FREE=0, PLAYING, PAUSED, STOPPING, FINISHED };                 // :180-186
enum class AttenuationModel { NONE=0, INVERSE, LINEAR, EXPONENTIAL };                     // :191-196
enum class ResamplingQuality { LINEAR = 0, SINC_4, SINC_8 };                              // :206
```

### 3.2.2 Structures (`NkAudio.h:227-420`)

```cpp
	struct NKENTSEU_AUDIO_API AudioHandle {          // :227
			uint32 id = AUDIO_INVALID_ID;
			bool IsValid() const;  operator bool() const;  operator== / !=
	};
	constexpr AudioHandle AUDIO_HANDLE_INVALID = {AUDIO_INVALID_ID};   // :249
```

```cpp
	struct NKENTSEU_AUDIO_API AudioSample {          // :262
			float32 *data = nullptr;   ///< Samples Float32 (interleaved)
			usize frameCount = 0;      ///< Nombre de frames (samples/channels)
			int32 sampleRate = 48000;
			int32 channels = 2;
			AudioFormat format = AudioFormat::UNKNOWN;
			float32 GetDuration() const;      // :274
			usize   GetSampleCount() const;   // :278
			bool    IsValid() const;          // :282
			memory::NkAllocator *mAllocator = nullptr; ///< Allocateur responsable
	};
```

Le commentaire d'en-tête (`NkAudio.h:258-260`) fixe l'invariant central du module :

> ```
>  @note Format interne normalisé : Float32 interleaved stéréo.
>        La conversion est faite au chargement.
> ```

```cpp
	struct NKENTSEU_AUDIO_API VoiceParams {          // :346
			float32 volume = 1.0f;      float32 pitch = 1.0f;    float32 pan = 0.0f;
			bool looping = false;       float32 loopStart = 0.0f; float32 loopEnd = -1.0f;
			float32 fadeInTime = 0.0f;  float32 startOffset = 0.0f;
			int32 priority = 128;
			AudioSource3D source3d;
			const char *bus = "SFX";    ///< Bus de routage (SFX/Music/Voice/UI)
	};
```

```cpp
	struct NKENTSEU_AUDIO_API AudioEngineConfig {    // :396
			AudioBackendType backend = AudioBackendType::AUTO;
			int32 sampleRate = AUDIO_DEFAULT_SAMPLE_RATE;   // 48000
			int32 channels   = AUDIO_DEFAULT_CHANNELS;      // 2
			int32 bufferSize = AUDIO_DEFAULT_BUFFER_SIZE;
			int32 maxVoices  = AUDIO_MAX_VOICES;
			bool enableHrtf = false;    bool enableDoppler = true;
			float32 masterVolume = 1.0f;
			memory::NkAllocator *allocator = nullptr;       ///< nullptr = allocateur global
			bool enableMasterLimiter = true;
			float32 masterLimiterThresholdDb = -0.5f;
			ResamplingQuality resamplingQuality = ResamplingQuality::LINEAR;
	};
```

Plus `AudioSource3D` (`:297`), `AudioListener3D` (`:334`), `AdsrEnvelope` (`:365`),
`FftResult` (`:377`).

### 3.2.3 `class AudioLoader` (`NkAudio.h:580-666`) — tout statique

```cpp
static AudioFormat DetectFormat(const uint8* data, usize size);                     // :583
static AudioFormat DetectFormatFromPath(const char* path);                          // :586
static AudioSample Load(const char* path, memory::NkAllocator* allocator = nullptr); // :595
static AudioSample LoadFromMemory(const uint8* data, usize size,
                                  AudioFormat format = AudioFormat::UNKNOWN,
                                  memory::NkAllocator* allocator = nullptr);        // :605
static bool SaveWAV(const char* path, const AudioSample& sample);                   // :615
static void Free(AudioSample& sample);                                              // :622
static void ConvertSampleFormat(AudioSample& sample, SampleFormat target);          // :631
static void Resample(AudioSample& sample, int32 targetSampleRate, bool highQuality = true); // :640
static void ConvertChannels(AudioSample& sample, int32 targetChannels);             // :648
```

### 3.2.4 `class AudioEngine` — singleton (`NkAudio.h:1088-1355`)

```cpp
static AudioEngine& Instance();                                              // :1091

bool Initialize(const AudioEngineConfig& config = AudioEngineConfig{});      // :1104
void Shutdown();                                                             // :1114
bool IsInitialized() const;                                                  // :1116

AudioHandle Play(const AudioSample& sample, const VoiceParams& params = VoiceParams{}); // :1127
using ProceduralCallback = NkFunction<void(float32* buffer, int32 frames, int32 channels)>; // :1140
AudioHandle PlayProcedural(ProceduralCallback callback, const VoiceParams& params = VoiceParams{}); // :1141

void Stop(AudioHandle handle, float32 fadeOutTime = 0.0f);                   // :1145
void Pause(AudioHandle handle);  void Resume(AudioHandle handle);            // :1146-1147
bool IsPlaying(AudioHandle) const; bool IsPaused(AudioHandle) const; bool IsLooping(AudioHandle) const; // :1149-1151
void SetVolume/SetPitch/SetPan/SetLooping(AudioHandle, ...);                 // :1153-1156
float32 GetVolume/GetPitch/GetPan(AudioHandle) const;                        // :1158-1160
float32 GetPlaybackPosition(AudioHandle) const;                              // :1165
void    SetPlaybackPosition(AudioHandle, float32 seconds);                   // :1166

// 3D
void SetSourcePosition/Velocity/Direction(AudioHandle, x, y, z);             // :1170-1172
void SetSourcePositional(AudioHandle, bool);                                 // :1173
void SetSourceMinDistance/MaxDistance(AudioHandle, float32);                 // :1174-1175
void SetSourceOcclusion(AudioHandle, float32 occlusion);                     // :1179
void SetSourceAirAbsorption(AudioHandle, bool enabled);                      // :1183
void SetSourceHRTF(AudioHandle, bool enabled);                               // :1187
bool LoadHrtfDataset(const char* path);                                      // :1199
bool GenerateSyntheticHrtf(int32 irLength=128, int32 nAzimuths=36, int32 nElevations=9); // :1211
void UnloadHrtfDataset();  bool IsHrtfLoaded() const;                        // :1214-1217
void SetListenerPosition/Velocity(float32 x,y,z);                            // :1219-1220
void SetListenerOrientation(fwdX,fwdY,fwdZ, upX,upY,upZ);                    // :1221

// Effets
bool AddEffect(AudioHandle, IAudioEffect* effect);                           // :1233
void RemoveEffect(AudioHandle, IAudioEffect*);  void ClearEffects(AudioHandle); // :1234-1235
bool AddMasterEffect(IAudioEffect*);  void RemoveMasterEffect(IAudioEffect*);
void ClearMasterEffects();                                                   // :1239-1241

// Bus
NkAudioBus* GetMasterBus();                                                  // :1255
NkAudioBus* GetBus(const char* name);                                        // :1266
NkAudioBus* GetOrCreateBus(const char* name, NkAudioBus* parent = nullptr);  // :1276

AudioHandle PlayMusicCrossfade(const AudioSample& newMusic, float32 fadeTime = 2.0f,
                               const VoiceParams& params = VoiceParams{});   // :1297

void SetMasterVolume(float32);  float32 GetMasterVolume() const;             // :1302-1303
void StopAll();  void PauseAll();  void ResumeAll();                         // :1305-1307
void RenderToBuffer(float32* outputBuffer, int32 frameCount, int32 channels = 2); // :1321

AudioBackendType GetBackendType() const;  const char* GetBackendName() const; // :1325-1326
int32 GetSampleRate/GetChannels/GetBufferSize() const;                       // :1327-1329
float32 GetLatencyMs() const;  int32 GetActiveVoices() const;  int32 GetMaxVoices() const; // :1330-1332
```

Le singleton est non copiable (`NkAudio.h:1338-1339`) et **PIMPL** (`struct Impl; Impl* mImpl;`, `:1350-1351`).

### 3.2.5 `class NkAudioBus` (`NkAudioBus.h:51-179`)

```cpp
static constexpr int32 MAX_EFFECTS = 8;     // :53
static constexpr int32 MAX_CHILDREN = 16;   // :54
static constexpr int32 MAX_NAME_LEN = 32;   // :55

NkAudioBus(const char* name, NkAudioBus* parent = nullptr) noexcept;  // :57
NkAudioBus* GetParent() const noexcept;                               // :69
NkAudioBus* GetChild(int32 idx) const noexcept;                       // :77
NkAudioBus* FindDescendant(const char* name) noexcept;                // :80
void SetVolume(float32) noexcept;  float32 GetVolume() const noexcept; // :84-86
float32 GetEffectiveVolume() const noexcept;                          // :91 (produit récursif)
void SetMute(bool) / IsMuted() / SetSolo(bool) / IsSoloed()           // :93-105
bool AddEffect(IAudioEffect*) / RemoveEffect / ClearEffects           // :115-117
void SetSidechainFromBus(NkAudioBus* sourceBus, float32 amount = 0.7f,
                         float32 threshold = 0.05f) noexcept;          // :133
void ClearSidechain() noexcept;  NkAudioBus* GetSidechainSource() const; // :135-137
```

La règle de calcul du volume est donnée par `NkAudio.h:1250-1251` :

> ```
>  Volume effectif d'une voix = voix.volume * bus.volume *
>                               bus.parent.volume * ... * Master.volume.
> ```

### 3.2.6 Streaming (`Streaming/NkAudioStream.h`, `Streaming/NkAudioStreamPlayer.h`)

```cpp
	class NKENTSEU_AUDIO_API IAudioStream {                             // NkAudioStream.h:42
			virtual ~IAudioStream() = default;
			virtual int32   ReadFrames(float32* outBuf, int32 maxFrames) noexcept = 0;  // :49
			virtual bool    Seek(nk_int64 frameIdx) noexcept = 0;                       // :53
			virtual nk_int64 GetFrameCount() const noexcept = 0;                        // :56
			virtual int32   GetSampleRate() const noexcept = 0;                         // :58
			virtual int32   GetChannels() const noexcept = 0;                           // :59
			virtual bool    IsEOF() const noexcept = 0;                                 // :60
	};
	NKENTSEU_AUDIO_API IAudioStream* OpenAudioStream(const char* path) noexcept;         // :68
	class WavStream : public IAudioStream;     // :73
	class AiffStream : public IAudioStream;    // :119
	class MemoryStream : public IAudioStream;  // :161
```

```cpp
	class NKENTSEU_AUDIO_API AudioStreamPlayer {                        // NkAudioStreamPlayer.h:41
			bool Init(int32 sampleRate, int32 channels, int32 ringBufferFrames = 88200) noexcept; // :50
			void Shutdown() noexcept;                                    // :53
			bool Play(IAudioStream* stream, bool loop = false) noexcept;  // :59
			void Stop() noexcept;  void Pause() noexcept;  void Resume() noexcept; // :62-69
			int32 ReadFrames(float32* outBuf, int32 maxFrames) noexcept;  // :76
			bool IsPlaying() const noexcept;                              // :79
			void SetVolume(float32) / GetVolume()                         // :84-88
			void SetSpeed(float32) / GetSpeed()                           // :95-99
			void SeekContent(float32 seconds) noexcept;                   // :115
			void FlushRing() noexcept;                                    // :119
			bool IsActive() const noexcept;  bool IsFinished() const noexcept; // :127-135
	};
```

### 3.2.7 Capture micro (`NkAudioCapture.h`)

```cpp
	struct NkCaptureDeviceInfo { /* ... */ bool isDefault = false; };    // :27
	struct NkCaptureConfig {
			int32 sampleRate = 48000;  int32 channels = 1;  int32 ringSeconds = 4;
	};                                                                    // :32-34
	using NkAudioInCallback = NkFunction<void(const float32* interleaved, int32 frames, int32 channels)>; // :39

	class NKENTSEU_AUDIO_API NkAudioCapture {                            // :42
			static NkVector<NkCaptureDeviceInfo> EnumerateDevices();       // :50
			bool Open(const NkCaptureConfig& config);  void Close();       // :53-54
			bool Start();  void Stop();  bool IsCapturing() const;         // :57-59
			int32 Read(float32* out, int32 maxFrames);                     // :63
			int32 Available() const;                                       // :65
			void SetCallback(NkAudioInCallback cb);                        // :68
			int32 SampleRate() const;  int32 Channels() const;             // :70-71
			static bool SelfTest();                                        // :75
	};
```

### 3.2.8 Autres classes exportées

- `class IAudioEffect` (`NkAudio.h:438`) — interface d'effet ; implémentations
  concrètes dans `NkAudioEffects.h`.
- `class IAudioBackend` (`NkAudio.h:511`) + `class AudioBackendFactory` (`NkAudio.h:1377`)
  + macro d'auto-enregistrement (`NkAudio.h:1408`).
- `class AudioGenerator` (`NkAudio.h:695`) — synthèse (`GenerateTone`, `ApplyEnvelope`, …).
- `class AudioMixer` (`NkAudio.h:839`), `class AudioAnalyzer` (`NkAudio.h:947`, FFT).
- `class NkDenoiser` (`NkDenoiser.h`, avec `static bool SelfTest()` `:58`).

## 3.3 Implémenté vs déclaré-mais-vide

### Réellement implémenté
- **Décodage** : WAV natif (`NkAudioLoader.cpp:284`), MP3 (`:411` → `NkMP3Codec::Decode`),
  OGG Vorbis (`:417` → `NkOGGVorbisCodec::Decode`), FLAC (`:424` → `NkFLACCodec::Decode`),
  Opus (`:429` → `NkOpusCodec::Decode`, qui passe par NKMedia). Tous « from scratch ».
  Le commentaire de tête du fichier (`NkAudioLoader.cpp:3`, « stubs MP3/OGG/FLAC »)
  **est périmé** : les quatre décodeurs sont branchés sur de vrais codecs.
- **Backends de sortie réels** : WASAPI (`NkAudioBackends.cpp:261`, CoInitializeEx + IAudioClient),
  CoreAudio (`:525`, AudioComponent), ALSA (`:663`, `snd_pcm_open`), AAudio (`:885`),
  WebAudio (`:1285`), Null.
- **Capture micro** : `NkAudioCapture` complet + `SelfTest()` (`NkAudioCapture.cpp:930`).
- **Streaming** : `IAudioStream` + `AudioStreamPlayer` (thread décodeur + ring buffer)
  + `NkContainerAudioStream` (piste audio d'un fichier vidéo).
- **Bus hiérarchiques**, **sidechain**, **crossfade musique**, **HRTF synthétique**.
- `RenderToBuffer` (rendu offline synchrone, sans thread audio) — précieux pour les
  exercices testables du cours.

### Déclaré mais VIDE — à ne pas enseigner
- **Backend DirectSound** : `NkAudioBackends.cpp:445-451`, recopié tel quel :

```cpp
		bool DirectSoundAudioBackend::Initialize(int32 sr, int32 ch, int32 buf) {
			mSampleRate = sr;
			mChannels = ch;
			mBufferSize = buf;
			mImpl = memory::NkGetDefaultAllocator().New<DsImpl>();
			// DirectSoundCreate8, CreateSoundBuffer, etc.
			return true;
		}
```

Il retourne `true` sans rien ouvrir. **Ne jamais mettre `AudioBackendType::DIRECTSOUND`
dans `AudioEngineConfig`** : laisser `AUTO` (→ WASAPI sous Windows).

- **`PULSE_AUDIO`, `OPEN_SL_ES`, `CUSTOM`** existent dans l'enum
  (`NkAudio.h:126-133`) sans classe backend correspondante dans `NkAudioBackends.h`.

- **Resampling haute qualité** : `NkAudioLoader.cpp:588-592`, recopié :

```cpp
		void AudioLoader::ResampleSinc(AudioSample &sample, int32 targetSampleRate) {
			// Pour l'instant, fallback linéaire
			// TODO: Implémenter Kaiser-windowed Sinc pour qualité studio
			ResampleLinear(sample, targetSampleRate);
		}
```

→ `AudioLoader::Resample(sample, rate, /*highQuality=*/true)` **fait du linéaire**.
Même piège que `NK_LANCZOS3` dans NKImage : ça ne plante pas, ça ment.
Par symétrie, `ResamplingQuality::SINC_4` / `SINC_8` dans `AudioEngineConfig` sont
à vérifier avant d'être enseignés comme différenciants.

- **`SelfTest()`** n'existe que pour **deux** classes : `NkAudioCapture::SelfTest`
  (`NkAudioCapture.cpp:930`) et `NkDenoiser::SelfTest` (`NkDenoiser.cpp:304`).
  Pas de self-test pour `AudioEngine`, `AudioLoader`, ni les codecs.

## 3.4 Exemples d'utilisation réels tirés du dépôt

### 3.4.1 `NkAudioPlayer` — LE patron canonique (application, 263 lignes)

`Applications\NkAudioPlayer\src\main.cpp`. Le fichier s'ouvre sur la recette,
`main.cpp:2-9`, à recopier telle quelle dans le cours :

```
// Le patron CORRECT pour lire un fichier audio vers les haut-parleurs, ET l'afficher :
//   1. AudioEngine::Instance().Initialize()  -> ouvre le device + thread de mixage
//   2. AudioLoader::Load(path)               -> décode WAV/MP3/OGG/FLAC/Opus en AudioSample
//   3. engine.Play(sample, params)           -> lance une voix (bus "Music")
//   4. boucle : GetPlaybackPosition -> tête de lecture sur la FORME D'ONDE dessinée
//   5. engine.Shutdown()
```

Le code correspondant, `main.cpp:113-131` :

```cpp
	// ── 1) Moteur audio ───────────────────────────────────────────────────────
	audio::AudioEngine &engine = audio::AudioEngine::Instance();
	audio::AudioEngineConfig cfg;
	if (!engine.Initialize(cfg)) {
		logger.Error("[NkAudioPlayer] AudioEngine::Initialize a echoue (device indisponible ?)");
		return -2;
	}
	logger.Info("[NkAudioPlayer] device reel : {0} Hz, {1} canaux", engine.GetSampleRate(), engine.GetChannels());

	// ── 2) Charge/decode le fichier ───────────────────────────────────────────
	audio::AudioSample sample = audio::AudioLoader::Load(path.CStr());
	if (!sample.IsValid()) {
		logger.Error("[NkAudioPlayer] impossible de charger/decoder : {0}", path.CStr());
		engine.Shutdown();
		return -3;
	}
	const double durSec = (sample.sampleRate > 0) ? (double)sample.frameCount / (double)sample.sampleRate : 0.0;
```

Lancement de la voix, `main.cpp:158-162` :

```cpp
	// ── 4) Joue + prepare la forme d'onde ─────────────────────────────────────
	audio::VoiceParams vp;
	vp.bus = "Music";
	vp.volume = 1.0f;
	audio::AudioHandle handle = engine.Play(sample, vp);
```

Contrôle pause/reprise et fin naturelle, `main.cpp:185-191` et `:254-255` :

```cpp
				else if (k->GetKey() == NkKey::NK_SPACE) {
					paused = !paused;
					if (paused)
						engine.Pause(handle);
					else
						engine.Resume(handle);
				}
```
```cpp
		if (!engine.IsPlaying(handle) && !paused)
			running = false; // fin naturelle
```

Position de lecture pour la tête de lecture, `main.cpp:212-215` :

```cpp
		// Position de lecture -> fraction [0..1].
		const float32 pos = engine.GetPlaybackPosition(handle);
		const float32 frac = (durSec > 0.0) ? math::NkClamp((float32)(pos / durSec), 0.0f, 1.0f) : 0.0f;
		const int32 headCol = (int32)(frac * (float32)cols);
```

Libération, `main.cpp:258-259` — **l'ordre compte** :

```cpp
	engine.Shutdown();
	audio::AudioLoader::Free(sample);
```

Et un avertissement explicite du dépôt sur le rééchantillonnage (`main.cpp:15-16`) :

> ```
>  NOTE (agent NKCode) : le mixage NKAudio convertit le TAUX du fichier vers celui du
>  device (fix mixBuffer/format-device). Un lecteur n'a RIEN à faire côté rééchantillonnage.
> ```

### 3.4.2 `NkAudioDemo` — streaming (`Applications\NkAudioDemo\src\main.cpp`, 431 l.)

Ouverture d'un flux et pull, `main.cpp:27-50` :

```cpp
static int RunStreamingTest(const char *path) {
	logger.Info("[NkAudioDemo] Mode streaming : {0}", path);
	IAudioStream *stream = OpenAudioStream(path);
	if (!stream) {
		logger.Error("[NkAudioDemo] OpenAudioStream echec");
		return 1;
	}
	int32 sampleRate = stream->GetSampleRate();
	int32 channels = stream->GetChannels();
	nk_int64 totalFrames = stream->GetFrameCount();

	AudioStreamPlayer player;
	if (!player.Init(sampleRate, channels, 88200)) {
		logger.Error("[NkAudioDemo] player.Init echec");
		memory::NkGetDefaultAllocator().Delete(stream); // voir note c0000374 dans RunDirectPullTest
		return 2;
	}
	if (!player.Play(stream, /*loop=*/false)) {
		logger.Error("[NkAudioDemo] player.Play echec");
		player.Shutdown();
		return 3;
	}
```

Noter le commentaire de libération : `memory::NkGetDefaultAllocator().Delete(stream)`,
**pas `delete`** — le stream vient de l'allocateur NKMemory.

Et le piège du ring buffer, `main.cpp:72-80` (cité mot pour mot) :

> ```
>  Compte les lectures vides CONSECUTIVES (buffer réellement à sec trop
>  longtemps), pas un cumul sur toute la durée du test : un hoquet isolé du
>  thread décodeur (quelques ms de retard CPU) est normal et ne doit PAS
>  s'additionner avec d'autres hoquets bien plus tard pour déclencher un
>  faux timeout alors que le flux reste parfaitement sain entre-temps.
> ```

### 3.4.3 `NkMicRecord` — micro + transcodage (`Applications\NkMicRecord\src\main.cpp`, 216 l.)

Le mode `--decode` est **l'exemple le plus court du dépôt pour tester tous les
codecs**, `main.cpp:76-89` :

```cpp
	// Mode décodage : NkMicRecord --decode <in.(wav|mp3|ogg|flac|opus)> <out.wav>
	// Charge via AudioLoader (auto-détection du format, dont Ogg-Opus) et
	// réécrit en WAV 16-bit — test end-to-end des codecs NKAudio.
	if (argc >= 4 && NkString(argv[1]) == NkString("--decode")) {
		audio::AudioSample smp = audio::AudioLoader::Load(argv[2]);
		if (!smp.IsValid()) {
			printf("[DECODE] ERREUR : chargement impossible : %s\n", argv[2]);
			return 1;
		}
		const bool ok = audio::AudioLoader::SaveWAV(argv[3], smp);
```

Le commentaire de tête (`main.cpp:1-8`) précise le rôle de la capture :

> ```
>  Ouvre le micro par défaut (NKAudio, backend WASAPI sous Windows), enregistre
>  `secondes` (défaut 5), écrit un WAV 16-bit PCM. À LANCER à la main pour tester
>  la capture end-to-end (le ring buffer est déjà validé headless par NkAudioCapture::SelfTest).
> ```

### 3.4.4 `NkVideoPlayer` — audio d'un conteneur vidéo + callback procédural

`Applications\NkVideoPlayer\src\main.cpp:371-408` — c'est **le pont NKAudio ↔ NKMedia** :

```cpp
	audio::AudioEngine &engine = audio::AudioEngine::Instance();
	audio::AudioStreamPlayer streamPlayer;
	audio::AudioHandle audioHandle{};
	bool audioOn = false;
	{
		audio::IAudioStream *astream = audio::OpenAudioStream(videoPath.CStr()); // piste du conteneur
		if (!astream && !audioPath.Empty())
			astream = audio::OpenAudioStream(audioPath.CStr()); // fichier externe (WAV/FLAC/MP3/OGG)
		if (astream) {
			audio::AudioEngineConfig acfg;
			const bool engineUp = engine.Initialize(acfg);
			const bool playerUp =
				engineUp && streamPlayer.Init(engine.GetSampleRate(), engine.GetChannels(), 88200);
			const bool played = playerUp && streamPlayer.Play(astream, /*loop=*/false);
			if (played) {
				audio::VoiceParams vp;
				vp.bus = "Music";
				audioHandle = engine.PlayProcedural(
					[&streamPlayer](float32 *buf, int32 frames, int32 /*channels*/) {
						streamPlayer.ReadFrames(buf, frames);
					},
					vp);
				audioOn = true;
```

## 3.5 Squelette minimal pour une application

**(a) Jouer un son (le strict nécessaire)**

```cpp
#include "NKAudio/NkAudio.h"
using namespace nkentseu;

int main() {
    // 1) UNE fois, au démarrage, depuis le thread principal.
    audio::AudioEngine &engine = audio::AudioEngine::Instance();
    audio::AudioEngineConfig cfg;                 // AUTO -> WASAPI/CoreAudio/ALSA
    if (!engine.Initialize(cfg))
        return 1;

    // 2) Décoder le fichier en mémoire (Float32 interleaved).
    audio::AudioSample smp = audio::AudioLoader::Load("clic.wav");
    if (!smp.IsValid()) { engine.Shutdown(); return 2; }

    // 3) Jouer. Le sample DOIT rester vivant tant que la voix joue.
    audio::VoiceParams vp;
    vp.bus    = "SFX";       // "Master" / "SFX" / "Music" / "Voice" / "UI"
    vp.volume = 0.8f;
    audio::AudioHandle h = engine.Play(smp, vp);

    // 4) ... boucle de l'application ...
    while (engine.IsPlaying(h)) { /* ... */ }

    // 5) Ordre OBLIGATOIRE : arrêter le moteur AVANT de libérer le sample.
    engine.Shutdown();
    audio::AudioLoader::Free(smp);
    return 0;
}
```

**(b) Jouer un son au clic d'un bouton NKGui/NkCanvas**

```cpp
// --- Au démarrage, une seule fois ---
audio::AudioEngine &engine = audio::AudioEngine::Instance();
engine.Initialize(audio::AudioEngineConfig{});
static audio::AudioSample gClic = audio::AudioLoader::Load("Resources/Audio/clic.wav");

// --- Dans la frame, quand le widget renvoie true ---
if (nkgui::Button(ctx, "Valider")) {
    audio::VoiceParams vp;
    vp.bus = "UI";                       // bus dédié : volume UI réglable à part
    engine.Play(gClic, vp);              // handle ignoré : son court, "fire and forget"
}

// --- À la fermeture ---
engine.Shutdown();
audio::AudioLoader::Free(gClic);
```

**(c) Musique de fond avec crossfade et volume par bus**

```cpp
audio::AudioSample menu   = audio::AudioLoader::Load("menu.ogg");
audio::AudioSample combat = audio::AudioLoader::Load("combat.ogg");

audio::VoiceParams mp; mp.bus = "Music"; mp.looping = true;
engine.Play(menu, mp);
// ... plus tard, changement de scène ...
engine.PlayMusicCrossfade(combat, 2.0f, mp);   // 2 s de fondu croisé

// Réglage global du volume musique sans toucher aux SFX :
if (audio::NkAudioBus *bus = engine.GetBus("Music"))
    bus->SetVolume(0.35f);
```

**(d) Fichier long : streaming plutôt que chargement complet**

```cpp
audio::IAudioStream *st = audio::OpenAudioStream("podcast_1h.mp3");
audio::AudioStreamPlayer player;
player.Init(engine.GetSampleRate(), engine.GetChannels(), 88200);
player.Play(st, /*loop=*/false);              // le player POSSÈDE `st` désormais

audio::VoiceParams vp; vp.bus = "Voice";
audio::AudioHandle h = engine.PlayProcedural(
    [&player](float32 *buf, int32 frames, int32) { player.ReadFrames(buf, frames); }, vp);

// Fermeture : moteur d'abord, player ensuite (cf. invariant §3.6-3).
engine.Shutdown();
player.Shutdown();
```

**(e) Micro**

```cpp
#include "NKAudio/NkAudioCapture.h"

audio::NkAudioCapture cap;
audio::NkCaptureConfig ccfg;      // 48000 Hz, mono, ring de 4 s
if (!cap.Open(ccfg) || !cap.Start()) return 1;

NkVector<float32> bloc; bloc.Resize(1024);
while (enregistre) {
    const int32 n = cap.Read(bloc.Data(), 1024);   // 0 si rien de dispo
    /* ... accumuler ... */
}
cap.Stop();
cap.Close();
```

**(f) Rendu offline, sans carte son (idéal pour un exercice vérifiable)**

```cpp
audio::AudioEngineConfig cfg;
cfg.backend = audio::AudioBackendType::NULL_OUTPUT;   // aucune sortie physique
engine.Initialize(cfg);
engine.Play(smp);
NkVector<float32> mix; mix.Resize(48000 * 2);         // 1 s de stéréo
engine.RenderToBuffer(mix.Data(), 48000, 2);          // appel SYNCHRONE
```

## 3.6 Pièges et invariants (citations du dépôt)

**1. Un seul `Initialize()` / `Shutdown()`, depuis le thread principal.**
`NkAudio.h:1071-1073` :

> ```
>  @note Initialize() et Shutdown() sont appelés une seule fois
>        depuis le thread principal. Toutes les autres méthodes
>        sont thread-safe via des atomics et queues lock-free.
> ```

Contrats formels : `Initialize` — `@pre !IsInitialized()` / `@post IsInitialized() == true si succès`
(`NkAudio.h:1100-1101`) ; `Shutdown` — `@pre IsInitialized()` (`:1111`).

**2. Le `AudioSample` doit survivre à la voix.** `NkAudio.h:1123` :

> `@param sample  Sample source (doit rester valide pendant la lecture)`

D'où l'ordre `engine.Shutdown();` **puis** `AudioLoader::Free(sample);`
(`NkAudioPlayer\...\main.cpp:258-259`). L'inverse = lecture dans de la mémoire libérée.

**3. Ordre de destruction avec un `AudioStreamPlayer`** — commentaire complet de
`Applications\NkVideoPlayer\src\main.cpp:672-676`, cité mot pour mot :

> ```
>  streamPlayer.Shutdown() joint le thread décodeur et libère le IAudioStream
>  qu'il possède (ContainerAudioStream/MemoryStream/WavStream) ; l'ordre importe :
>  arrêter l'engine (qui appelle le callback procédural depuis son thread audio)
>  AVANT de détruire streamPlayer, pour ne jamais mixer un stream en cours de
>  destruction.
> ```

**4. Le callback procédural tourne sur le THREAD AUDIO : interdit d'allouer ou de
verrouiller.** `NkAudio.h:1134-1139` :

> ```
>  Le callback est appelé depuis le thread audio pour remplir
>  les buffers à la demande. Idéal pour synthétiseur logiciel.
>  @note TEMPS RÉEL : callback ne doit pas allouer/locker
> ```

**5. Allocateur : `NkAllocator`, jamais `delete`.** `NkAudio.h:284` :
`memory::NkAllocator *mAllocator = nullptr; ///< Allocateur responsable (jamais nullptr si IsValid())`
et `NkAudio.h:620` : `@note Doit être appelé avec le même allocateur utilisé pour Load()`.
Un `IAudioStream` non confié à un player se détruit avec
`memory::NkGetDefaultAllocator().Delete(stream)` (`NkAudioDemo\...\main.cpp:44`, avec
la mention explicite du crash `c0000374` en cas de mélange).

**6. L'ownership des effets reste à l'appelant.** `NkAudio.h:1230` :
`@param effect Pointeur vers l'effet (ownership = appelant)`. L'effet doit donc
survivre à la voix ou au bus auquel il est attaché.

**7. Le format interne est imposé.** `NkAudio.h:258-260` : « Format interne
normalisé : Float32 interleaved stéréo. La conversion est faite au chargement. »
Et le device peut avoir un autre taux : lire `engine.GetSampleRate()` /
`engine.GetChannels()` **après** `Initialize()`, jamais avant
(`NkAudioPlayer\...\main.cpp:120`).

**8. Le rééchantillonnage est automatique côté moteur.** `NkAudioPlayer\...\main.cpp:15-16` :
« le mixage NKAudio convertit le TAUX du fichier vers celui du device […] Un lecteur
n'a RIEN à faire côté rééchantillonnage. »

**9. `Play()` peut échouer silencieusement.** `NkAudio.h:1125` :
`@return Handle de contrôle, invalide si pool plein`. Toujours tester `h.IsValid()`
quand on compte réutiliser le handle. Pool = `AUDIO_MAX_VOICES` (config `maxVoices`).

**10. Les bus standards sont créés automatiquement.** `NkAudio.h:1246-1248` :

> ```
>  Le bus Master est cree automatiquement a Initialize() avec ses
>  4 sous-buses standard : SFX (default), Music, Voice, UI.
> ```

Donc `vp.bus = "SFX"` (défaut) fonctionne sans rien créer ; `GetBus("Music")` renvoie
`nullptr` si le moteur n'est pas initialisé.

**11. Limiter master activé par défaut.** `NkAudio.h:409-411` :
`enableMasterLimiter = true`, `masterLimiterThresholdDb = -0.5f` — c'est pourquoi
un volume > 1.0 ne crache pas mais se compresse.

**12. Occlusion : c'est l'APPLICATION qui fait le raycast.** `NkAudio.h:1177-1178` :

> ```
>  Occlusion [0..1] (0 = aucun obstacle, 1 = totalement occlus).
>  Atténue volume + lowpass filter sur frequences hautes.
>  L'application fait le raycast pour calculer cette valeur.
> ```

**13. HRTF : dataset requis.** `SetSourceHRTF` — « Necessite un dataset charge via
`LoadHrtfDataset()` » (`NkAudio.h:1185-1186`). Sans fichier `.nkhrtf`, utiliser
`GenerateSyntheticHrtf()` (`:1211`), qui ne demande aucun fichier.

## 3.7 Raccordement NkCanvas / NKGui

NKAudio **ne dépend d'aucun module graphique** : la couture se fait entièrement
côté application.

- **Son au clic** : le widget renvoie `true` → `engine.Play(sample, vp)` avec
  `vp.bus = "UI"`. Un bus dédié permet à l'utilisateur de couper les sons d'interface
  sans toucher la musique (`bus->SetMute(true)`).
- **Visualiser le son** : `GetPlaybackPosition(handle)` donne la tête de lecture ;
  `AudioSample::data` + `frameCount` permettent de pré-calculer une enveloppe min/max
  par colonne de pixels — c'est exactement `ComputePeaks` de
  `Applications\NkAudioPlayer\src\main.cpp:56-87`, puis un quad par colonne
  (`PushQuad`, `main.cpp:90-101`) envoyé à `target.Draw(verts.Data(), n, NkPrimitiveType::NK_TRIANGLES)`
  (`main.cpp:242`). **C'est le meilleur exercice « audio + NkCanvas » du dépôt.**
- **Spectre temps réel** : `AudioAnalyzer` (`NkAudio.h:947`) renvoie un `FftResult`
  avec `magnitudes` et `BinToFrequency(bin)` (`NkAudio.h:389-393`) — une barre par bin.
- **Vidéo + audio** : voir le pont `OpenAudioStream(cheminVideo)` + `PlayProcedural`
  de `NkVideoPlayer` (§3.4.4), avec l'horloge audio comme horloge maîtresse
  (`NkVideoPlayer\...\main.cpp:563-566`).

**Verdict NKAudio : utilisable, chapitre solide** — à condition de
(a) ne jamais forcer `AudioBackendType::DIRECTSOUND` (stub vide),
(b) marteler l'ordre `Shutdown()` → `Free()` → (player) `Shutdown()`,
(c) ne pas promettre le resampling Sinc (`ResampleSinc` retombe sur le linéaire).

---

# 4. NKMedia — vidéo / audio (conteneurs, codecs, lecture, écriture)

> ⚠️ **Avertissement pédagogique en tête de chapitre.** NKMedia est de très loin le
> plus gros des cinq modules (décodeurs H.264, HEVC, VP8, VP9, AV1, MPEG-2, Theora,
> AAC, Opus/CELT/SILK, AMR — tous écrits **from scratch, sans ffmpeg**). Un chapitre
> pour débutants ne doit **surtout pas** essayer d'en faire le tour. Il doit se
> limiter aux **quatre classes de façade** : `NkMediaProbe`, `NkVideoReader`,
> `NkVideoWriter`, `NkVideoRecorder`. Tout le reste est de la plomberie interne.

## 4.1 Organisation

Racine : `D:\Projets\2026\Nkentseu\Nkentseu\Kernel\Runtime\NKMedia`

```
NKMedia/
  NKMedia.jenga · ROADMAP.md (1529 l. — la source de vérité sur l'état réel)
  src/NKMedia/
    NkMediaProbe.{h,cpp}        (  76 /  531 l.)  <- détection conteneur + pistes
    NkMediaDemux.{h,cpp}        (  50 /  746 l.)  <- extraction des paquets audio
    Video/
      NkVideoTypes.h            (  22 l.)  <- NkVideoInputFormat
      NkVideoReader.{h,cpp}     (  78 / 3200 l.)  <- LECTURE : conteneur -> RGBA8
      NkVideoWriter.{h,cpp}     ( 103 /  222 l.)  <- ÉCRITURE simple
      NkVideoRecorder.{h,cpp}   ( 112 /  231 l.)  <- CAPTURE A/V threadée
      NkVideoConverter.{h,cpp}  (  35 /  205 l.)  <- séquence/AVI -> vidéo
      NkImageSequenceWriter.{h,cpp} ( 57 / 144 l.)<- « à la Blender »
      NkBitWriter.h
      Containers/ NkAviWriter · NkMovWriter · NkMp4H264Writer · NkWebmWriter
    Audio/Containers/NkWavWriter.{h,cpp} (65 / 169 l.)
    Codecs/
      Video/ H264/ · HEVC/ · VP8/ · VP9/ · AV1/ · Mpeg1/ · Mpeg2/ · Theora/
      Aac/ · Opus/ (+ Celt/ + Silk/) · Audio/AMR/
```

**Dépendances déclarées** — `Kernel\Runtime\NKMedia\NKMedia.jenga:17-22` :

```python
    nkentseudependson(
        ["NKCore", "NKPlatform", "NKMemory", "NKContainers", "NKMath", "NKLogger",
         "NKStream", "NKFileSystem", "NKImage", "NKThreading", "NKTime"],
        selfexport="NKMedia",
        extra_includes=["src"],
    )
```

→ **NKMedia dépend de NKImage** (le codec JPEG de NKImage sert de codec MJPEG,
et les séquences d'images passent par les codecs PNG/BMP/TGA/QOI de NKImage).
Et **NKAudio dépend de NKMedia** (Opus). Ordre d'enseignement recommandé :
NKImage → NKMedia → NKAudio.

Namespace : **`nkentseu::media`**. Pas d'en-tête parapluie : on inclut le
sous-en-tête précis dont on a besoin.

## 4.2 Surface d'API publique (les quatre façades)

### 4.2.1 `NkMediaProbe` — « qu'y a-t-il dans ce fichier ? » (`NkMediaProbe.h`)

```cpp
enum class NkMediaContainer { NK_UNKNOWN, NK_MP4, NK_WEBM, NK_WAV, NK_OGG, NK_MP3, NK_FLAC }; // :21-29
enum class NkMediaTrackType { NK_UNKNOWN, NK_AUDIO, NK_VIDEO };                               // :31

struct NkMediaTrack {                        // :34
        NkMediaTrackType type = NkMediaTrackType::NK_UNKNOWN;
        NkString codec;          // "aac","opus","vorbis","vp8","vp9","h264","pcm","mp3",...
        int32 sampleRate = 0;    int32 channels = 0;      // audio
        int32 width = 0;         int32 height = 0;        // vidéo
        int32 bitsPerSample = 0; bool pcmBigEndian = false;
        NkVector<nk_uint8> codecPrivate;                  // OpusHead pour Opus, etc.
};
struct NkMediaInfo {                         // :51
        NkMediaContainer container = NkMediaContainer::NK_UNKNOWN;
        NkVector<NkMediaTrack> tracks;
        const char* ContainerName() const;
        const NkMediaTrack* FirstAudio() const;
        const NkMediaTrack* FirstVideo() const;
};
struct NkMediaProbe {                        // :62
        static bool Probe(const uint8* data, usize size, NkMediaInfo& out);
        static bool ProbeFile(const char* path, NkMediaInfo& out);
        static NkMediaContainer DetectContainer(const uint8* data, usize size);
        static bool SelfTest();
};
```

### 4.2.2 `NkMediaDemux` — sortir les paquets audio (`NkMediaDemux.h`)

```cpp
struct NkMediaPacket {                       // :22
        usize offset = 0;   // position dans le buffer SOURCE (ne copie pas !)
        usize size = 0;
        int64 timestampMs = 0;
        int64 granule = -1;             // OGG
        int64 discardPaddingNs = 0;     // WebM
};
struct NkMediaDemux {                        // :39
        static bool ExtractAudioPackets(const uint8* data, usize size, const NkMediaInfo& info,
                                        NkVector<NkMediaPacket>& out);
        static bool ExtractAudioPacketsFile(const char* path, NkVector<nk_uint8>& outBytes,
                                            NkMediaInfo& outInfo, NkVector<NkMediaPacket>& out);
        static bool SelfTest();
};
```

### 4.2.3 `NkVideoReader` — LECTURE image par image (`Video/NkVideoReader.h`)

```cpp
	struct NkVideoFrame {                    // :26
			NkVector<nk_uint8> rgba;         // width*height*4, row-major, haut->bas
			int32 width = 0;   int32 height = 0;
			int64 timestampMs = 0;
			int32 index = -1;
	};
	struct NkVideoReaderInfo {               // :35
			int32 width = 0;   int32 height = 0;
			int32 frameCount = -1;           // -1 = inconnu
			double fps = 0.0;
			NkString codec;                  // "mjpeg"|"rawrgb"|"h264"|"hevc"|"vp8"|"vp9"|"av1"|"mpeg2"|"theora"|"image"
			NkString container;              // "avi"|"mov"|"mp4"|"sequence"
	};
	class NkVideoReader {                    // :44
			NkVideoReader();  ~NkVideoReader();
			NkVideoReader(const NkVideoReader&) = delete;          // :49  NON COPIABLE
			NkVideoReader& operator=(const NkVideoReader&) = delete;
			bool Open(const char* path);                            // :54
			bool IsOpen() const;                                    // :56
			const NkVideoReaderInfo& Info() const;                  // :57
			bool ReadFrame(NkVideoFrame& out);                      // :60  false = fini
			bool SeekFrame(int32 index);                            // :63
			int32 CurrentIndex() const;                             // :65
			void Close();                                           // :66
			static bool SelfTest();                                 // :70
		private:
			struct Impl;  Impl* mImpl = nullptr;                    // PIMPL
	};
```

### 4.2.4 `NkVideoWriter` — ÉCRITURE simple (`Video/NkVideoWriter.h`)

```cpp
enum class NkVideoCodec { RAW_BGR, MJPEG, MPEG1 };                    // :26-30
enum class NkVideoContainer { AVI, MOV, ELEMENTARY };                 // :33-37
// (NkVideoInputFormat { RGB24, RGBA32, BGR24 } vit dans Video/NkVideoTypes.h:16-20)

struct NkVideoConfig {                                                // :41
        int32 width = 0;  int32 height = 0;
        int32 fpsNum = 30;  int32 fpsDen = 1;
        NkVideoCodec codec = NkVideoCodec::MJPEG;
        NkVideoContainer container = NkVideoContainer::AVI;
        int32 quality = 90;                 // MJPEG 1..100
        int32 audioSampleRate = 0;          // 0 = vidéo muette
        int32 audioChannels = 0;
};
class NkVideoWriter {                                                 // :57
        bool Open(const char* path, const NkVideoConfig& cfg);        // :60
        bool WriteFrame(const uint8* pixels, NkVideoInputFormat fmt); // :63
        bool AddAudioSamples(const int16* interleaved, usize frameCount); // :69
        bool Close();                                                 // :72
        bool IsOpen() const;   int32 FrameCount() const;              // :74-81
};
```

### 4.2.5 `NkVideoRecorder` — CAPTURE d'écran A/V, threadée (`Video/NkVideoRecorder.h`)

```cpp
enum class NkRecorderCodec { H264, MJPEG };                           // :39

struct NkVideoRecorder {                                              // :41
        bool Begin(const char* path, int32 width, int32 height, int32 fpsNum = 60, int32 fpsDen = 1,
                   int32 qp = 20, int32 maxQueuedFrames = 32,
                   NkRecorderCodec codec = NkRecorderCodec::H264, int32 mjpegQuality = 90); // :47
        int32 AddAudio(int32 sampleRate, int32 channels, const char* lang3 = nullptr);       // :52
        int32 AddSubtitleTrack(const char* lang3 = nullptr);                                 // :54
        bool PushVideo(const uint8* pixels, NkVideoInputFormat fmt, bool flipVertical = false); // :58
        void PushAudio(int32 trackIdx, const int16* interleaved, uint32 frames);             // :60
        void AddSubtitle(int32 trackIdx, const char* utf8, uint32 startMs, uint32 durMs);    // :62
        bool End();                                                                           // :65
        bool IsRecording() const;  int32 FrameCount() const;                                  // :67-72
        int32 QueueDepth();  uint64 DroppedFrames();  double EncodeFps();                     // :75-77
};
```

### 4.2.6 Les utilitaires secondaires

```cpp
// Video/NkImageSequenceWriter.h
enum class NkImageSeqFormat { PNG, JPEG, BMP, TGA, QOI };             // :21-27
struct NkImageSequenceWriter {                                        // :29
        bool Open(const char* dir, const char* basename, int32 width, int32 height,
                  NkImageSeqFormat fmt, int32 padding = 4, int32 quality = 90);   // :33
        bool WriteFrame(const uint8* pixels, NkVideoInputFormat fmt);             // :37
        bool Close();  int32 FrameCount() const;                                  // :39-42
};

// Video/NkVideoConverter.h  (deux fonctions statiques, renvoient le NOMBRE de trames)
static int32 ImageSequenceToVideo(const char* prefix, int32 digits, const char* suffix,
                                  int32 first, int32 count, const char* outPath,
                                  int32 fpsNum, int32 fpsDen, int32 quality = 90);  // :24
static int32 MjpegAviToVideo(const char* aviPath, const char* outPath, int32 quality = 90); // :30

// Audio/Containers/NkWavWriter.h
struct NkWavWriter {
        bool Open(const char* path, int32 sampleRate, int32 channels, int32 bitsPerSample,
                  bool isFloat = false);                              // :26
        bool WriteSamples(const void* data, usize size);              // :32
        bool Close();                                                 // :35
        bool IsOpen() const;  usize BytesWritten() const;             // :37-41
        static bool SelfTest();                                       // :48
};

// Video/Containers/NkWebmWriter.h
enum class NkWebmVideoCodec { NONE, VP8, VP9 };   enum class NkWebmAudioCodec { NONE, OPUS }; // :36-37
struct NkWebmWriter {
        bool Open(const char* path, const NkWebmConfig& config);                       // :60
        bool AddVideoFrame(const uint8* data, usize size, int64 timestampMs, bool isKeyframe); // :65
        bool AddAudioFrame(const uint8* data, usize size, int64 timestampMs);          // :70
        bool Finalize();                                                               // :73
        int32 VideoFrameCount() const;  int32 AudioFrameCount() const;                 // :78-81
        static bool SelfTest();                                                        // :89
};

// Codecs/Video/H264/NkH264Encoder.h  (encodeur MP4/H.264 direct, avec pistes A/V + sous-titres)
struct NkH264Encoder {
        bool Open(const char* path, int32 width, int32 height, int32 fpsNum = 25, int32 fpsDen = 1,
                  int32 qp = 26, int32 gop = 30);                                      // :34
        bool WriteFrame(const uint8* pixels, NkVideoInputFormat fmt);                   // :38
        bool Close();                                                                   // :40
        int32 AddAudioTrack(int32 sampleRate, int32 channels, const char* lang3 = nullptr); // :51
        void WriteAudioPcm(int32 trackIdx, const int16* interleaved, uint32 frames);    // :52
        int32 AddSubtitleTrack(const char* lang3 = nullptr);                            // :54
        void AddSubtitle(int32 trackIdx, const char* utf8, uint32 startMs, uint32 durMs); // :55
        static bool SelfTest();                                                         // :63
};
```

## 4.3 Implémenté vs déclaré-mais-vide

### 🚨 Les commentaires d'en-tête sont PÉRIMÉS et SOUS-ESTIMENT le module

C'est l'inverse du problème rencontré dans NKFont. Exemples :

- `Video/NkVideoReader.h:8-11` dit encore :

```
//   - Conteneur MOV/MP4 (ISOBMFF) : piste vidéo -> MJPEG (mjpa/jpeg). H264 = à venir.
// H264/AVC (MP4 courant) exige un décodeur complet (gros chantier séparé) : la
// lecture d'une piste avc1 renvoie une erreur claire tant que le décodeur n'existe pas.
```

Or `Video/NkVideoReader.cpp` affecte réellement `info.codec` à
`"hevc"` (`:955`, `:1563`), `"vp8"` (`:1572`, `:1650`), `"vp9"` (`:1578`, `:1635`),
`"av1"` (`:1586`, `:1644`), `"h264"` (`:1592`, `:1828`, `:2002`), `"mpeg2"` (`:1869`),
`"theora"` (`:1944`), `"mjpeg"` (`:1203`), `"rawrgb"` (`:1206`), `"image"` (`:2093`).

- `Video/NkVideoWriter.h:5-6` dit « AVI pour l'instant ; MP4/WebM à venir » alors que
  MOV/MP4 et WebM sont livrés.

**→ Pour le cours : la source de vérité est `Kernel\Runtime\NKMedia\ROADMAP.md`,
pas les en-têtes.** Extraits de `ROADMAP.md:14-26` :

| Brique | Statut ROADMAP |
|---|---|
| 1. Probe / démux conteneurs | ✅ MP4/WebM/WAV/OGG/MP3/FLAC |
| 2. Extraction de paquets | ✅ MP4 (`stbl` + fMP4 `moof/traf/trun`), WebM |
| 3/3bis. Opus CELT / SILK | ✅ (SILK **bit-exact** vs libopus) |
| 3ter. Dispatcher Opus | 🔶 **CELT-only et SILK-only OK, mode HYBRIDE non fait** |
| 3quater. Fichiers `.opus` (Ogg) | ✅ (hybride/stéréo « refusés proprement ») |
| 4. Décodeur AAC-LC | ✅ bit-exact vs ffmpeg |
| 5. Muxers (écriture) | ✅ AVI + MOV/MP4 + WAV + WebM |
| 6. Vidéo (décode) | ✅ H.264 Main+High, VP8, VP9, HEVC/H.265 — **tous branchés `NkVideoReader`** |
| 7. Vidéo (encode) | 🔶 **EN COURS** — RAW/MJPEG/MPEG-1 + H.264 baseline livrés |
| 8. VP8 | ✅ complet (325 images bit-exactes vs ffmpeg) |
| 9. HEVC | ✅ complet (I+P+B, déblocage, SAO, Main10) |

### Ce que le cours peut enseigner sans risque
- `NkMediaProbe::ProbeFile` / `NkMediaDemux::ExtractAudioPacketsFile` — stables, testés.
- `NkVideoWriter` en **MJPEG/AVI** et **MJPEG/MOV** — le chemin le plus sûr : il y a un
  round-trip prouvé (`NkVideoReader::SelfTest`, `NkVideoReader.cpp:3146`).
- `NkVideoReader::Open` + `ReadFrame` sur **AVI MJPEG**, **MOV MJPEG**, **séquence
  d'images** (un dossier de PNG).
- `NkImageSequenceWriter` — trivialement fiable (délègue aux codecs NKImage).
- `NkWavWriter` — round-trip bit-exact vérifié par son `SelfTest`.
- `NkVideoRecorder` en mode **MJPEG** (encodage léger).

### Ce qu'il faut annoncer comme fragile ou incomplet
- **Encodage H.264** : `ROADMAP.md:24` marque la brique 7 « 🔶 **EN COURS** ».
  L'encodeur est « baseline » seulement. Pour un débutant : rester en MJPEG.
- **Opus mode hybride** : `ROADMAP.md:19` — « Reste : **mode hybride** (config 12-15) » ;
  et « hybride/stéréo refusés proprement » (`ROADMAP.md:21`). Un `.opus` stéréo
  courant peut donc être **refusé**.
- **AV1 / MPEG-2 / Theora** : `ROADMAP.md:23` — « AV1/MPEG-2/Theora restent à faire ».
  Le code de décodage existe (`NkAv1Decoder.cpp` fait 286 Ko) mais la ROADMAP ne les
  déclare pas terminés.
- **HEVC** : restes refusés proprement — « tuiles, PCM (code écrit mais dormant),
  4:2:2/4:4:4, `ref_pic_lists_modification`, `scaling_list_data` » (`ROADMAP.md:26`).
- **VP8** : restes — « segmentation MB, partitions multiples, versions 1-3 (refus propre) »
  (`ROADMAP.md:25`).
- **`SeekFrame`** n'est un vrai seek que sur les conteneurs indexés. Sur H.264 il
  repositionne sur l'IDR précédente et il faut redécoder en avant — commentaire du
  harnais, `Applications\NkVideoReadTest\src\main.cpp:2762-2765` :

> ```
>  SeekFrame se contente de repositionner sur l'IDR précédente (voir implémentation) : il
>  faut ensuite redécoder EN AVANT jusqu'à la cible, exactement comme le fait la boucle de
>  rattrapage du lecteur (Applications/NkVideoPlayer).
> ```

- **Audio dans `NkVideoWriter`** : `NkVideoWriter.h:50-52` — « Supportée par AVI […]
  et MOV/MP4 […]. **Non supportée par ELEMENTARY/MPEG1 (Open échoue si demandée).** »
- **`NkVideoRecorder` en mode MJPEG** : `NkVideoRecorder.h:36-38` — « conteneur
  MOV/MP4, **VIDÉO SEULE (audio/sous-titres ignorés dans ce mode)** ».

### Classes sans `SelfTest()`
`NkVideoWriter`, `NkVideoRecorder`, `NkVideoConverter`, `NkImageSequenceWriter`,
`NkAviWriter`, `NkMovWriter`, `NkMp4H264Writer` n'ont **pas** de self-test.
Ils sont validés indirectement par le round-trip de `NkVideoReader::SelfTest()`.

## 4.4 Exemples d'utilisation réels tirés du dépôt

### 4.4.1 Écrire une vidéo — `NKVideoTest` (application **bâtie**, Debug + Release)

`Applications\NKVideoTest\src\main.cpp` (266 lignes). La fonction complète,
`main.cpp:83-112` — **c'est le squelette de référence** :

```cpp
	bool MakeVideo(const char *path, media::NkVideoCodec codec, media::NkVideoContainer container, int32 w, int32 h,
				   int32 fps, int32 nframes) {
		media::NkVideoConfig cfg;
		cfg.width = w;
		cfg.height = h;
		cfg.fpsNum = fps;
		cfg.fpsDen = 1;
		cfg.codec = codec;
		cfg.container = container;
		cfg.quality = 88;

		media::NkVideoWriter vw;
		if (!vw.Open(path, cfg)) {
			::printf("  [ERREUR] ouverture %s\n", path);
			return false;
		}
		uint8 *rgb = (uint8 *)memory::NkAlloc((usize)w * h * 3);
		for (int32 fr = 0; fr < nframes; ++fr) {
			RenderFrame(rgb, w, h, fr, nframes);
			if (!vw.WriteFrame(rgb, media::NkVideoInputFormat::RGB24)) {
				::printf("  [ERREUR] trame %d\n", fr);
				memory::NkFree(rgb);
				vw.Close();
				return false;
			}
		}
		memory::NkFree(rgb);
		vw.Close();
		return true;
	}
```

Includes correspondants, `main.cpp:8-11` :

```cpp
#include "NKMedia/Video/NkVideoWriter.h"
#include "NKMedia/Video/NkImageSequenceWriter.h"
#include "NKMedia/Codecs/Video/Mpeg1/NkMpeg1Encoder.h"
#include "NKMemory/NKMemory.h"
```

Séquence d'images (workflow Blender), `main.cpp:185-201` :

```cpp
		media::NkImageSequenceWriter seq;
		bool ok = seq.Open(outDir, "nkframe", W, H, media::NkImageSeqFormat::PNG, 4, 90);
		if (ok) {
			uint8 *rgb = (uint8 *)memory::NkAlloc((usize)W * H * 3);
			for (int32 fr = 0; fr < 5 && ok; ++fr) { // 5 images d'exemple
				RenderFrame(rgb, W, H, fr, N);
				ok = seq.WriteFrame(rgb, media::NkVideoInputFormat::RGB24);
			}
			memory::NkFree(rgb);
			seq.Close();
		}
```

### 4.4.2 Lire une vidéo — `NkVideoReadTest` (application **bâtie**)

`Applications\NkVideoReadTest\src\main.cpp:3041-3070` — ouverture + boucle de
décodage de TOUTES les images :

```cpp
	const char *path = argv[1];
	NkVideoReader rd;
	if (!rd.Open(path)) {
		printf("  [KO] impossible d'ouvrir/lire : %s\n", path);
		return 1;
	}
	const NkVideoReaderInfo &in = rd.Info();
	printf("  ouvert : %s\n", path);
	printf("  conteneur=%s codec=%s  %dx%d  %.2f fps  frames=%d\n", in.container.CStr(), in.codec.CStr(), in.width,
		   in.height, in.fps, in.frameCount);

	int32 count = 0;
	uint64 globalSum = 0;
	NkVideoFrame fr;
	while (rd.ReadFrame(fr)) {
		uint64 s = 0;
		for (uint64 i = 0; i < fr.rgba.Size(); ++i)
			s += fr.rgba[i];
		globalSum += s;
		/* ... */
		++count;
	}
```

Et la suite de self-tests headless (`main.cpp:252-276`) — c'est **la façon
officielle de vérifier que NKMedia marche sur une machine donnée** :

```cpp
	if (argc < 2) {
		printf("  [self-test] ecrire AVI MJPEG -> relire -> verifier...\n");
		bool ok = NkVideoReader::SelfTest();
		printf("  [ %s ] NkVideoReader::SelfTest (AVI MJPEG round-trip)\n", ok ? "OK " : "KO");
		bool okH264 = NkH264Decoder::SelfTest();
		bool okCavlc = NkH264Cavlc::SelfTest();
		bool okVp9 = NkVp9Decoder::SelfTest();
		bool okAv1 = NkAv1Decoder::SelfTest();
		bool okHevc = NkHevcDecoder::SelfTest();
		bool okHevcCabac = NkHevcCabacState::SelfTest();
		bool okWav = NkWavWriter::SelfTest();
		bool okWebm = NkWebmWriter::SelfTest();
		bool all = ok && okH264 && okCavlc && okVp9 && okAv1 && okHevc && okHevcCabac && okWav && okWebm;
		printf("=== %s ===\n", all ? "LECTURE VIDEO OPERATIONNELLE" : "ECHEC");
		return all ? 0 : 1;
	}
```

### 4.4.3 Probe + démux + décodage audio — `NKMediaTest` (application **bâtie**)

`Applications\NKMediaTest\src\main.cpp:331-353` (probe d'un fichier réel) :

```cpp
	if (argc >= 2) {
		media::NkMediaInfo info;
		if (!media::NkMediaProbe::ProbeFile(argv[1], info)) {
			printf("[ERREUR] probe echoue : %s\n", argv[1]);
			return 1;
		}
		printf("Fichier   : %s\n", argv[1]);
		printf("Conteneur : %s\n", info.ContainerName());
		printf("Pistes    : %d\n", (int)info.tracks.Size());
		for (uint64 i = 0; i < info.tracks.Size(); ++i) {
			const media::NkMediaTrack &t = info.tracks[i];
			printf("  [%d] %-5s codec=%-6s", (int)i, TrackTypeName(t.type), t.codec.CStr());
			if (t.type == media::NkMediaTrackType::NK_AUDIO)
				printf(" %d Hz, %d canal(aux)", t.sampleRate, t.channels);
			if (t.type == media::NkMediaTrackType::NK_VIDEO)
				printf(" %dx%d", t.width, t.height);
			printf("\n");
		}
```

Démux + décodage AAC, `main.cpp:178-211` (extrait) — **montre bien que les paquets
pointent DANS `bytes`** :

```cpp
		NkVector<nk_uint8> bytes;
		media::NkMediaInfo info;
		NkVector<media::NkMediaPacket> packets;
		if (!media::NkMediaDemux::ExtractAudioPacketsFile(argv[2], bytes, info, packets))
			return 1;
		const media::NkMediaTrack *tr = info.FirstAudio();
		const int sr = tr ? tr->sampleRate : 44100;
		const int ch = tr ? tr->channels : 1;
		media::NkAacDecoder dec;
		if (!dec.Init(sr, ch)) { /* ... */ return 1; }
		NkVector<nk_int16> pcm;
		nk_int16 frame[1024 * 2];
		for (uint64 p = 0; p < packets.Size(); ++p) {
			const int32 n = dec.DecodeFrame(bytes.Data() + packets[p].offset, (int32)packets[p].size, frame);
			if (n <= 0)
				continue;
			for (int32 i = 0; i < n * ch; ++i)
				pcm.PushBack(frame[i]);
		}
```

Capture A/V multipiste avec sous-titres, `main.cpp:863-882` :

```cpp
		media::NkVideoRecorder rec;
		if (rec.Begin("engine_capture.mp4", W, H, fps, 1, 22)) {
			const int32 aFr = rec.AddAudio(kRate, 1, "fre"); // 2 langues audio
			const int32 aEn = rec.AddAudio(kRate, 1, "eng");
			const int32 sFr = rec.AddSubtitleTrack("fre");
			const int32 sGh = rec.AddSubtitleTrack("bbj");
			/* ... pour chaque frame ... */
				rec.PushVideo(fb.Data(), media::NkVideoInputFormat::RGBA32);
				rec.PushAudio(aFr, af.Data(), (uint32)aPerFrame);
				rec.PushAudio(aEn, ae.Data(), (uint32)aPerFrame);
			rec.AddSubtitle(sFr, "Rendu du moteur Nkentseu", 0, 500);
			rec.AddSubtitle(sGh, "Wa Nkentseu", 0, 500);
			rec.End();
```

### 4.4.4 Le lecteur complet — `NkVideoPlayer`

`Applications\NkVideoPlayer\src\main.cpp` (684 lignes). ⚠️ **Présent seulement dans
`Build/Bin/Release-Windows/`, absent de Debug.**

Includes du pont NKMedia ↔ NkCanvas ↔ NKAudio, `main.cpp:36-45` :

```cpp
#include "NKCanvas/Renderer/Targets/NkRenderWindow.h"
#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkSprite.h"

#include "NKMedia/Video/NkVideoReader.h"
#include "NKAudio/NkAudio.h"
#include "NKAudio/Streaming/NkAudioStream.h"
#include "NKAudio/Streaming/NkAudioStreamPlayer.h"
```

Ouverture, `main.cpp:316-327` :

```cpp
	// ── 1) Ouvre la vidéo (décodage image par image en RGBA) ──────────────────
	media::NkVideoReader reader;
	if (!reader.Open(videoPath.CStr())) {
		logger.Error("[NkVideoPlayer] impossible d'ouvrir/lire : {0}", videoPath.CStr());
		return -2;
	}
	const media::NkVideoReaderInfo &vin = reader.Info();
	const int32 vidW = vin.width > 0 ? vin.width : 1;
	const int32 vidH = vin.height > 0 ? vin.height : 1;
	const float32 fps = vin.fps > 1.0f ? vin.fps : 25.0f;
```

Texture cible + sprite, `main.cpp:355-362` :

```cpp
	// ── 4) Texture RGBA de la taille vidéo + sprite d'affichage ───────────────
	NkTexture frameTex;
	if (!frameTex.Create(*target.GetRenderer(), (uint32)vidW, (uint32)vidH, NkColor2D{0, 0, 0, 255})) {
		logger.Error("[NkVideoPlayer] echec creation texture {0}x{1}", vidW, vidH);
		window.Close();
		return -5;
	}
	NkSprite sprite(frameTex);
```

Upload d'une image décodée, `main.cpp:424-431` :

```cpp
	auto pushFrame = [&]() {
		rawW = fr.width;
		rawH = fr.height;
		rawFrame = fr.rgba;
		applyLook(fr.rgba.Data(), (uint64)fr.width * fr.height);
		frameTex.Update(fr.rgba.Data(), (uint32)fr.width, (uint32)fr.height, 0, 0);
		haveFrame = true;
	};
```

Cadencement + rattrapage, `main.cpp:562-620` (extrait clé) :

```cpp
		if (!paused && !ended) {
			if (audioOn && !streamPlayer.IsFinished())
				mediaClock = streamPlayer.GetPositionSeconds();
			else
				mediaClock += (float64)(dt * speed);
			const int32 targetIdx = (int32)(mediaClock * (float64)fps);
			/* resynchronisation dure si on a plus de 1,5 s de retard */
			const int32 kHardResyncLagFrames = (int32)(fps * 1.5f);
			if (!firstFrame && (targetIdx - reader.CurrentIndex()) > kHardResyncLagFrames) {
				if (reader.SeekFrame(targetIdx) && reader.ReadFrame(fr))
					havePendingFrame = true;
			}
			NkChrono burstTimer;
			while ((firstFrame || reader.CurrentIndex() < targetIdx) &&
				   burstTimer.Elapsed().ToMilliseconds() < 150.0) {
				if (reader.ReadFrame(fr)) { havePendingFrame = true; firstFrame = false; }
				else if (loop) { /* SeekFrame(0) + ReadFrame */ }
				else { paused = true; break; }
			}
			if (havePendingFrame)
				pushFrame(); // upload + dessin UNE SEULE FOIS, avec la dernière image de la rafale
		}
```

Rendu, `main.cpp:664-668` :

```cpp
		target.Clear(NkColor2D{0, 0, 0, 255});
		if (haveFrame)
			target.Draw(static_cast<const NkDrawable &>(sprite));
		target.Display();
```

### 4.4.5 Lecteur vidéo dans une interface — `NKCode`

`Applications\NKCode\src\NKCode\Shell\NkVideoViewer.h` (360 lignes, appli bâtie
Debug **et** Release). Upload dans le backend d'interface, `NkVideoViewer.h:70-82` :

```cpp
		inline void NkVideoUpload(editorkit::NkEditorShell *shell, NkVideoClip *c) {
			if (!shell || c->frame.rgba.Empty() || c->frame.width <= 0 || c->frame.height <= 0)
				return;
			const uint8 *px = c->frame.rgba.Data();
			if (c->texId == 0 || c->texW != c->frame.width || c->texH != c->frame.height) {
				c->texId = shell->UploadRGBA(px, c->frame.width, c->frame.height);
				c->texW = c->frame.width;
				c->texH = c->frame.height;
			} else {
				shell->UpdateRGBA(c->texId, px, c->frame.width, c->frame.height);
			}
			c->haveFrame = c->texId != 0;
		}
```

Cadencement dans une boucle d'interface, `NkVideoViewer.h:153-170` :

```cpp
			float32 dt = ctx.input.dt;
			if (dt <= 0.f || dt > 0.25f)
				dt = 1.f / 60.f; // garde-fou (onglet revenu au premier plan / hoquet)
			const float32 frameDur = 1.f / (c->fps * (c->speed > 0.01f ? c->speed : 0.01f));
			if (c->playing && !c->ended) {
				c->acc += dt;
				if (c->acc > frameDur * 4.f)
					c->acc = 0.f; // evite le rattrapage explosif
				while (c->acc >= frameDur) {
					c->acc -= frameDur;
					if (c->reader->ReadFrame(c->frame)) {
						c->index = c->frame.index;
						NkVideoUpload(shell, c);
					}
					/* ... loop / fin ... */
				}
			}
```

### 4.4.6 Enregistrer le rendu — `NKViewportDemo`

`Applications\NKViewportDemo\src\NKViewportDemo\main.cpp:329-362`, avec la
robustesse de finalisation (`:358-362`) :

```cpp
		if (record && target->CaptureToImage(capImg) && capImg.IsValid()) {
			if (!rec.IsRecording()) {
				if (rec.Begin("viewport_capture.mp4", capImg.Width(), capImg.Height(), 30, 1, 24)) {
					recAudio = rec.AddAudio(kRate, 1, "fre");
					const int32 st = rec.AddSubtitleTrack("fre");
					rec.AddSubtitle(st, "Capture live NKCanvas (DX11) - Nkentseu", 0, 4000);
				}
			}
			if (rec.IsRecording()) {
				rec.PushVideo(capImg.Pixels(), media::NkVideoInputFormat::RGBA32);
				rec.PushAudio(recAudio, recTone.Data(), (uint32)kAudioPerFrame);
				if (recFrames >= kRecTotal) { rec.End(); running = false; }
			}
		}
	}
	// Robustesse : si la fenêtre est fermée AVANT la fin de l'enregistrement, on finalise quand même
	// (écrit le moov) → le MP4 reste lisible (sinon "moov atom not found").
	if (rec.IsRecording()) {
		rec.End();
	}
```

### 4.4.7 Auto-régulation d'un enregistrement — `Applications\Sandbox\src\Demo\main.cpp:1025-1050`

```cpp
					const bool encoderBusy = recorder && recorder->QueueDepth() >= 24;
					if (!encoderBusy)
						(void)recordCapture.EnqueueCopy(...);
```

## 4.5 Squelette minimal pour une application

**(a) Écrire une vidéo (chemin le plus sûr : MJPEG/AVI)**

```cpp
#include "NKMedia/Video/NkVideoWriter.h"
#include "NKMemory/NKMemory.h"
using namespace nkentseu;

media::NkVideoConfig cfg;
cfg.width  = 640;  cfg.height = 480;
cfg.fpsNum = 25;   cfg.fpsDen = 1;
cfg.codec     = media::NkVideoCodec::MJPEG;      // sûr, universel
cfg.container = media::NkVideoContainer::AVI;
cfg.quality   = 90;

media::NkVideoWriter vw;
if (!vw.Open("sortie.avi", cfg)) return 1;

uint8 *rgb = (uint8 *)memory::NkAlloc((usize)cfg.width * cfg.height * 3);
for (int32 f = 0; f < 100; ++f) {
    DessineMaFrame(rgb, cfg.width, cfg.height, f);         // haut-en-bas, R,G,B
    if (!vw.WriteFrame(rgb, media::NkVideoInputFormat::RGB24)) break;
}
memory::NkFree(rgb);
vw.Close();          // OBLIGATOIRE : écrit l'index et rapièce les tailles
```

**(b) Lire une vidéo image par image**

```cpp
#include "NKMedia/Video/NkVideoReader.h"

media::NkVideoReader rd;
if (!rd.Open("film.avi")) return 1;                 // ou un DOSSIER de PNG

const media::NkVideoReaderInfo &in = rd.Info();
// in.width, in.height, in.fps, in.frameCount (-1 si inconnu), in.codec, in.container

media::NkVideoFrame fr;
while (rd.ReadFrame(fr)) {
    // fr.rgba = width*height*4 octets RGBA8, haut-en-bas
    // fr.index, fr.timestampMs
    Utilise(fr.rgba.Data(), fr.width, fr.height);
}
rd.Close();
```

**(c) Afficher la vidéo dans une fenêtre NkCanvas**

```cpp
// -- Init, APRÈS la création du renderer --
media::NkVideoReader rd;  rd.Open(chemin);
const auto &in = rd.Info();
renderer::NkTexture tex;
tex.Create(*target.GetRenderer(), (uint32)in.width, (uint32)in.height, NkColor2D{0,0,0,255});
renderer::NkSprite sprite(tex);

// -- Chaque frame, cadencé sur in.fps --
accum += dt;
const float dureeImage = 1.0f / (float)(in.fps > 1.0 ? in.fps : 25.0);
while (accum >= dureeImage) {
    accum -= dureeImage;
    if (rd.ReadFrame(frame))
        tex.Update(frame.rgba.Data(), (uint32)frame.width, (uint32)frame.height, 0, 0);
    else break;   // fin (ou rd.SeekFrame(0) pour boucler)
}
target.Clear(NkColor2D{0,0,0,255});
target.Draw(static_cast<const NkDrawable &>(sprite));
target.Display();
```

**(d) Enregistrer ce que l'application affiche**

```cpp
#include "NKMedia/Video/NkVideoRecorder.h"

media::NkVideoRecorder rec;
// MJPEG = encodage léger, recommandé pour un cours. H264 = plus compact mais lourd.
rec.Begin("capture.mp4", w, h, 30, 1, 22, 32, media::NkRecorderCodec::MJPEG, 92);
const int32 pisteAudio = rec.AddAudio(48000, 2, "fre");     // ignorée en mode MJPEG !

// chaque frame :
if (target.CaptureToImage(img) && img.IsValid())
    rec.PushVideo(img.Pixels(), media::NkVideoInputFormat::RGBA32, /*flipVertical=*/false);
rec.PushAudio(pisteAudio, pcm, nbFrames);

rec.End();          // draine la file + finalise. TOUJOURS l'appeler, même sur fermeture.
```

**(e) Inspecter un fichier sans le décoder**

```cpp
#include "NKMedia/NkMediaProbe.h"

media::NkMediaInfo info;
if (media::NkMediaProbe::ProbeFile("clip.mp4", info)) {
    printf("conteneur = %s, %d piste(s)\n", info.ContainerName(), (int)info.tracks.Size());
    if (const media::NkMediaTrack *v = info.FirstVideo())
        printf("video %s %dx%d\n", v->codec.CStr(), v->width, v->height);
    if (const media::NkMediaTrack *a = info.FirstAudio())
        printf("audio %s %d Hz %d ch\n", a->codec.CStr(), a->sampleRate, a->channels);
}
```

**(f) Séquence d'images → vidéo (une seule ligne)**

```cpp
#include "NKMedia/Video/NkVideoConverter.h"
// rendus/frame_001.png ... rendus/frame_120.png -> film.mp4 (H.264) à 25 fps
const int32 n = media::NkVideoConverter::ImageSequenceToVideo(
        "rendus/frame_", 3, ".png", 1, 120, "film.mp4", 25, 1);
```

## 4.6 Pièges et invariants (citations du dépôt)

**1. Les paquets du démux POINTENT dans le buffer source.** `NkMediaDemux.h:22` et
`:41-43`, cités mot pour mot :

> ```
>  // Un paquet encodé (pointe dans le buffer d'origine ; ne copie pas).
> ```
> ```
>  Les paquets référencent `data`
>  (rester valide tant qu'on les lit).
> ```

Et pour la variante fichier (`NkMediaDemux.h:47`) : « `outBytes` reçoit le buffer
(**à garder vivant** car les paquets pointent dedans) ». Un débutant qui laisse
sortir `bytes` de portée lit de la mémoire libérée.

**2. `Close()` / `End()` / `Finalize()` ne sont pas optionnels.** Sans eux, l'index
AVI ou l'atome `moov` MP4 n'est jamais écrit → fichier illisible. Le dépôt le
formule directement (`NKViewportDemo\...\main.cpp:358-360`) :

> ```
>  Robustesse : si la fenêtre est fermée AVANT la fin de l'enregistrement, on finalise quand même
>  (écrit le moov) → le MP4 reste lisible (sinon "moov atom not found").
> ```

**3. Sens des pixels.** Les encodeurs veulent du **haut-en-bas** (`NkVideoTypes.h:16-20`).
OpenGL rend bottom-up → `NkVideoRecorder::PushVideo(..., flipVertical=true)`
(`NkVideoRecorder.h:56-57` : « `flipVertical` pour framebuffer bottom-up (OpenGL) »).
`NkVideoFrame::rgba` en lecture est déjà « row-major, haut->bas » (`NkVideoReader.h:25`).

**4. Le recorder ABANDONNE des trames plutôt que de gonfler.** `NkVideoRecorder.h:45-46` :

> ```
>  `maxQueuedFrames` BORNE la file video : si l'encodeur (H.264 lourd) prend du retard,
>  les nouvelles trames sont ABANDONNEES (drop-newest) au lieu de gonfler la memoire
>  (temps reel : perdre une trame vaut mieux que geler). Abandons comptes (DroppedFrames()).
> ```

→ Toujours surveiller `DroppedFrames()` et `QueueDepth()`, comme le fait
`Applications\Sandbox\src\Demo\main.cpp:1025`.

**5. Ordre d'appel du recorder.** `NkVideoRecorder.h:51-54` : « À appeler **juste après
Begin (avant les trames)** » pour `AddAudio` et `AddSubtitleTrack`.

**6. Le mode MJPEG du recorder ignore l'audio.** `NkVideoRecorder.h:36-38` :
« conteneur MOV/MP4, **VIDÉO SEULE (audio/sous-titres ignorés dans ce mode)** ».
C'est un piège silencieux : `AddAudio` renvoie un index, `PushAudio` ne fait rien.

**7. Audio impossible en MPEG-1/ELEMENTARY.** `NkVideoWriter.h:52` :
« Non supportée par ELEMENTARY/MPEG1 (**Open échoue si demandée**) ».

**8. `NkVideoReader` est NON COPIABLE et PIMPL.** `NkVideoReader.h:49-50` :
`NkVideoReader(const NkVideoReader&) = delete;`. Le passer par référence.

**9. `SeekFrame` ≠ seek exact sur les codecs inter-frame.** Voir le commentaire
recopié en §4.3. Sur H.264, après `SeekFrame(n)`, il faut boucler `ReadFrame`
jusqu'à ce que `CurrentIndex() >= n`.

**10. La copie profonde du DPB était un bug de performance réel.** `ROADMAP.md:1447-1458` :

> ```
>  le vrai coupable était `NkVideoReader::ReadFrame` […] : la gestion du DPB (liste de
>  références) COPIAIT EN PROFONDEUR (au lieu de déplacer) jusqu'à 16 `NkH264Frame`
>  (plans Y/Cb/Cr + grilles de mouvement, ~380 Ko/image) DEUX FOIS par frame décodée
>  […] coût mesuré ~80 ms/frame et CROISSANT […] >90% du temps total. Fix : traits::NkMove
> ```

Leçon transférable pour le cours : `NkVideoFrame::rgba` est un `NkVector` — le
copier par valeur à chaque frame coûte cher. Le lecteur du dépôt le fait pourtant
(`NkVideoPlayer\...\main.cpp:427` : `rawFrame = fr.rgba;`) mais **uniquement** parce
qu'il a besoin d'une copie non étalonnée.

**11. Ne pas confondre le PTS et le temps de décodage.** `ROADMAP.md:1449-1452` :

> ```
>  (mesure fiable : chrono mur autour d'un run borné, PAS le champ `t=` de `NkVideoReadTest`
>  qui affiche le PTS vidéo, pas le temps de décodage réel — piège rencontré une fois)
> ```

**12. Décoder est CHER : ne jamais décoder plus d'une image par frame affichée.**
La boucle de rattrapage de `NkVideoPlayer` est bornée à 150 ms
(`main.cpp:606-607`) et n'upload qu'une seule fois (`main.cpp:618-619` :
« upload + dessin UNE SEULE FOIS, avec la dernière image de la rafale »).
Côté interface, `NKCode` borne l'accumulateur (`NkVideoViewer.h:159-160` :
« evite le rattrapage explosif »).

**13. Les self-tests écrivent dans le répertoire courant.** `nkvideoreader_selftest.avi`,
`nkwavwriter_selftest.wav`, `nkwebmwriter_selftest.webm`. À signaler si le cours
demande de lancer `NkVideoReadTest.exe`.

**14. `NkVideoRecorder` lance un THREAD.** `NkVideoRecorder.h:5-8` :

> ```
>  **Encodage sur un THREAD de fond** : la boucle de rendu pousse les trames capturées
>  dans une file (rapide) et un worker encode en arrière-plan → l'application reste
>  FLUIDE pendant la capture (l'encodeur H.264 est lourd).
> ```

`End()` joint ce thread : ne jamais détruire le recorder sans avoir appelé `End()`.

**15. Variable d'environnement HEVC.** `NkVideoReadTest\...\main.cpp:249-250` :
`NK_HEVC_THREADS` (0/absent = auto, 1 = séquentiel) →
`NkHevcDecoder::SetThreadCount(...)`.

## 4.7 Raccordement NkCanvas / NKGui

Le point de couture est toujours le même : **`NkVideoFrame::rgba` est un buffer RGBA8
CPU** — exactement le même format que ce que `NkTexture` attend.

1. **NkCanvas** (`NkVideoPlayer`) : `NkTexture::Create(renderer, w, h, ...)` **une
   fois**, puis `tex.Update(fr.rgba.Data(), w, h, 0, 0)` à chaque nouvelle image,
   et un `NkSprite` pour le dessin. Ne **jamais** recréer la texture par frame.
2. **Backend d'interface** (`NKCode`) : `shell->UploadRGBA(px, w, h)` la première
   fois (récupère un `texId`), puis `shell->UpdateRGBA(texId, px, w, h)` ensuite —
   avec re-upload complet si les dimensions changent (`NkVideoViewer.h:75-80`).
3. **Sens inverse (capturer l'interface)** : `NkRenderWindow::CaptureToImage(NkImage&)`
   côté NkCanvas (`NKViewportDemo\...\main.cpp:329`) ou
   `NkOffscreenTarget::ReadbackPixels` côté NKRHI (`NkVideoRecorder.h:8-9`), puis
   `rec.PushVideo(img.Pixels(), RGBA32)`. **C'est ici que NKImage, NkCanvas et
   NKMedia se rejoignent** : la capture est une `NkImage`.
4. **Audio de la vidéo** : `audio::OpenAudioStream(cheminVideo)` (NKAudio ouvre la
   piste audio du conteneur via `NkContainerAudioStream`) + `PlayProcedural` —
   voir §3.4.4. L'horloge audio sert d'horloge maîtresse
   (`NkVideoPlayer\...\main.cpp:563-566`).

**Verdict NKMedia : 🟡 partiel — chapitre possible mais à périmètre STRICTEMENT borné.**
Le cours doit :
(a) se limiter à `NkMediaProbe`, `NkVideoReader`, `NkVideoWriter` (MJPEG),
`NkImageSequenceWriter`, `NkVideoRecorder` (MJPEG) ;
(b) dire explicitement que les en-têtes sont périmés et que la vérité est dans
`ROADMAP.md` ;
(c) ne rien promettre sur l'encodage H.264 (brique 7 « EN COURS »), sur AV1/MPEG-2/
Theora, ni sur l'Opus hybride/stéréo ;
(d) prévenir que `SeekFrame` n'est exact que sur les conteneurs indexés.

---

# 5. NKNetwork — réseau (sockets, protocole, clients/serveurs)

## 5.1 Organisation

Racine : `D:\Projets\2026\Nkentseu\Nkentseu\Kernel\System\NKNetwork`
(**attention : `Kernel\System`, pas `Kernel\Runtime`** comme les quatre autres.)

```
NKNetwork/
  NKNetwork.jenga · ROADMAP.md
  pch/pch.h · pch/pch.cpp
  tests/.jenga/                 <- DOSSIER VIDE (voir §5.3)
  src/NKNetwork/
    NKNetwork.h        (7,7 Ko)  <- en-tête PARAPLUIE + alias de commodité
    NkNetworkApi.h     ( 24 Ko)  <- macros d'export
    Core/NkNetDefines.{h,cpp}    (31 / 12 Ko)  <- types, enums, constantes
    Transport/
      NkSocket.{h,cpp}           (45 / 38 Ko)  <- socket UDP/TCP brut
      NkReliableUDP.{h,cpp}      (46 / 34 Ko)  <- ACK, retransmit, fenêtre
    Protocol/
      NkBitStream.{h,cpp}        (39 / 19 Ko)  <- NkBitWriter / NkBitReader
      NkConnection.{h,cpp}       (56 / 56 Ko)  <- NkConnection + NkConnectionManager
    Replication/NkNetWorld.{h,cpp} (25 / 19 Ko)
    RPC/NkRPC.{h,cpp}            (35 / 13 Ko)
    Lobby/NkLobby.{h,cpp}        (67 / 39 Ko)
    HTTP/NkHTTPClient.{h,cpp}    (64 / 46 Ko)
```

**Dépendances déclarées** — `Kernel\System\NKNetwork\NKNetwork.jenga:27-42` :

```python
_NET_DEPS = ["NKCore", "NKPlatform", "NKMemory", "NKContainers", "NKLogger", "NKThreading", "NKMath", "NKTime", "NKFileSystem"]
_NET_INCLUDES = ["src", "pch"]
_NET_DEFINES = []
if _WANT_MBEDTLS:
    _NET_DEPS.append("NKMbedTLS")
    _NET_INCLUDES.append("%{wks.location}/Externals/Libs/NKMbedTLS/include")
    _NET_DEFINES.append("NKENTSEU_HTTP_USE_MBEDTLS")

with project("NKNetwork"):
    nkentseudependson(
        _NET_DEPS,
        selfexport="NKNetwork",
        extra_includes=_NET_INCLUDES,
        extra_defines=_NET_DEFINES,
    )
```

TLS **opt-in par variable d'environnement**, `NKNetwork.jenga:21-25` :

```python
# TLS/HTTPS opt-in : NK_ENABLE_TLS=1 (ou mbedtls) active le backend mbed-TLS
# (bibliotheque NKMbedTLS, C pur, cross-compilee pour toutes les plateformes y
# compris iOS). Desactive par defaut -> HTTP simple uniquement, aucune dependance.
_TLS_OPT = os.getenv("NK_ENABLE_TLS", "").strip().lower()
_WANT_MBEDTLS = _TLS_OPT in ("1", "true", "on", "yes", "mbedtls")
```

⚠️ **La docstring du `.jenga` est un copier-coller d'un AUTRE module.**
`NKNetwork.jenga:3-15` décrit « Système de réflexion runtime […] `NkType.h/cpp`,
`NkClass.h/cpp`, `NkProperty.h/cpp`, `NkMethod.h/cpp`, `NkRegistry.h/cpp` » —
c'est la description de **NKReflection**. Aucun de ces fichiers n'existe dans
NKNetwork. **Ne pas la recopier dans le cours.**

Côté application, il faut **lier Winsock sur Windows** :
`Applications\NkNetWorldDemo\NkNetWorldDemo.jenga:35-40` ajoute `"ws2_32"` à `links()`,
et `Sandbox\System\NKNetwork\NKNetworkSandbox.jenga:74-78` fait de même.

Namespace : **`nkentseu::net`**.

## 5.2 Surface d'API publique

### 5.2.1 Constantes et enums (`Core/NkNetDefines.h`)

```cpp
static constexpr uint32 kNkMaxConnections  = 256u;   // :145
static constexpr uint32 kNkMaxPacketSize   = 1400u;  // :148  (taille MTU-safe)
static constexpr uint32 kNkMaxPayloadSize  = 1380u;  // :151
static constexpr uint32 kNkMaxChannels     = 8u;     // :160
static constexpr uint32 kNkMaxRetransmits  = 5u;     // :169
static constexpr uint32 kNkMaxFragments    = 16u;    // :175

enum class NkNetResult : uint8 {                     // :268-283
    NK_NET_OK = 0, NK_NET_INVALID_ARG, NK_NET_NOT_CONNECTED, NK_NET_ALREADY_CONNECTED,
    NK_NET_CONNECTION_REFUSED, NK_NET_TIMEOUT, NK_NET_PACKET_TOO_LARGE, NK_NET_SOCKET_ERROR,
    NK_NET_BUFFER_FULL, NK_NET_NOT_INITIALIZED, NK_NET_PLATFORM_UNSUPPORTED,
    NK_NET_AUTH_FAILED, NK_NET_BANNED, NK_NET_UNKNOWN
};
inline const char* NkNetResultStr(NkNetResult r) noexcept;   // :291  -> texte lisible

enum class NkNetChannel : uint8 {                    // :505-525
    NK_NET_CHANNEL_UNRELIABLE = 0,       ///< UDP pur — positions, inputs, animations
    NK_NET_CHANNEL_RELIABLE_ORDERED,     ///< TCP-like — chat, événements gameplay
    NK_NET_CHANNEL_RELIABLE_UNORDERED,   ///< livraison garantie, ordre libre
    NK_NET_CHANNEL_SEQUENCED,            ///< seul le plus récent est livré
    NK_NET_CHANNEL_SYSTEM                ///< RÉSERVÉ AU MOTEUR — ne pas utiliser
};
```

Le commentaire de chaque canal donne son usage — c'est **le meilleur support
pédagogique du chapitre** (`NkNetDefines.h:505-525`, recopié ci-dessus).

Plus les types `NkPeerId`, `NkNetId`, `NkTimestampMs`.

### 5.2.2 `NkAddress` (`Transport/NkSocket.h:172-360`)

```cpp
class NkAddress {                                                     // :172
        NkAddress(const char* ip, uint16 port);                       // (ctor utilisé partout)
        static NkAddress Loopback(uint16 port, Family f = Family::NK_IP_V4) noexcept;  // :258
        static NkAddress Any(uint16 port, Family f = Family::NK_IP_V4) noexcept;       // :267
        static NkAddress Broadcast(uint16 port) noexcept;                              // :275
        bool IsValid() const noexcept;                                                 // :327
        NkString ToString() const noexcept;                                            // :353
};
```

### 5.2.3 `NkSocket` — la couche brute (`Transport/NkSocket.h:504-755`)

```cpp
class NkSocket {                                                      // :504
        enum class Type : uint8 { NK_UDP, NK_TCP /* ... */ };          // :513
        NkNetResult Create(const NkAddress& localAddr, Type type = Type::NK_UDP) noexcept; // :570
        void Close() noexcept;                                         // :577
        NkNetResult SetNonBlocking(bool v) noexcept;                   // :590
        NkNetResult SetBroadcast(bool v) noexcept;                     // :598
        NkNetResult SendTo(const void* data, uint32 size, const NkAddress& to) noexcept;  // :644
        NkNetResult RecvFrom(void* buf, uint32 bufSize, uint32& outSize,
                             NkAddress& outFrom) noexcept;             // :655
        bool IsValid() const noexcept;                                 // :715
        static NkNetResult PlatformInit() noexcept;                    // :746
        static void PlatformShutdown() noexcept;                       // :753
};
```

### 5.2.4 `NkConnectionManager` — LA classe du chapitre (`Protocol/NkConnection.h:733+`)

```cpp
struct NkReceiveMsg {                                                 // :229
        uint8 data[kNkMaxPayloadSize] = {};   ///< buffer INLINE (1380 octets), pas un pointeur
        uint32 size = 0;
        NkPeerId from;
        NkNetChannel channel = NkNetChannel::NK_NET_CHANNEL_UNRELIABLE;
        NkTimestampMs receivedAt = 0;
};

class NkConnectionManager {                                           // :733
        NkConnectionManager() noexcept = default;                     // :743
        ~NkConnectionManager() noexcept;                              // :749  (Shutdown auto)
        NkConnectionManager(const NkConnectionManager&) = delete;     // :752  NON COPIABLE

        NkNetResult StartServer(uint16 port, uint32 maxClients = 64) noexcept;  // :767
        NkNetResult Connect(const NkAddress& serverAddr, uint16 localPort = 0) noexcept; // :777
        void Shutdown() noexcept;                                     // :790

        NkNetResult SendTo(NkPeerId peer, const uint8* data, uint32 size, NkNetChannel ch) noexcept;
        NkNetResult Broadcast(const uint8* data, uint32 size, NkNetChannel ch) noexcept;
        void DrainAll(NkVector<NkReceiveMsg>& out) noexcept;
        void Disconnect(NkPeerId peer, const char* reason = nullptr) noexcept;
        void DisconnectAll(const char* reason = nullptr) noexcept;
        bool IsServer() const noexcept;   bool IsRunning() const noexcept;
        uint32 ConnectedPeerCount() const noexcept;
        bool GetConnectionStats(NkPeerId peer, NkConnectionStats& outStats) const noexcept;

        // Callbacks — appelés DEPUIS LE THREAD RÉSEAU.
        /* NkFunction */ onPeerConnected;      ///< (NkPeerId)
        /* NkFunction */ onPeerDisconnected;   ///< (NkPeerId, const char* reason)
        uint32 maxConnections = kNkMaxConnections;
};
```

Contrats documentés (`Protocol/NkConnection.h:762-786`) :

> ```
>  @note Crée un socket UDP et le lie à l'adresse Any(port).
>  @note Démarre le thread réseau interne pour polling automatique.
> ```
> ```
>  @note Crée une NkConnection interne et démarre le handshake.
> ```

et pour `Shutdown` (`:785-788`) :

> ```
>  @note Envoie une déconnexion gracieuse à tous les pairs connectés.
>  @note Attend la fin du thread réseau avant retour (join).
> ```

### 5.2.5 `NkBitWriter` / `NkBitReader` (`Protocol/NkBitStream.h`)

Sérialisation **bit-précise** (pas octet). Écriture (`:141+`) :

```cpp
void WriteBool(bool) ;  WriteU8/U16/U32/U64 ;  WriteI8/I16/I32 ;  WriteF32       // :165-216
void WriteF32Q(float32 v, float32 minV, float32 maxV, float32 prec) noexcept;    // :237 quantifié
void WriteInt(int32 v, int32 minV, int32 maxV) noexcept;                         // :252 borné
void WriteVec3f(const NkVec3f&) ;  WriteVec3fQ(v, minV, maxV, prec) ;  WriteQuatf(const NkQuatf&); // :264-288
void WriteString(const char* s, uint32 maxLen = 256) noexcept;                    // :298
void WriteBytes(const uint8* data, uint32 size) noexcept;                         // :307
void WriteBits(uint32 v, uint32 numBits) noexcept;                                // :321
void AlignToByte() noexcept;                                                      // :328
bool IsOverflowed() const noexcept;                                               // :352
usize BytesWritten() const;
```

Lecture — mêmes noms en `Read*` (`:455-635`), plus `ReadF32Q`, `ReadInt`,
`ReadBytes`, `ReadBits`, `AlignToByte`, `IsOverflowed`.

**Les quantificateurs (`WriteF32Q`, `WriteInt`, `WriteVec3fQ`) sont l'argument
pédagogique du module** : au lieu de 32 bits par flottant, on n'envoie que ce que
la précision exige.

### 5.2.6 `NkNetWorld` — réplication d'entités (`Replication/NkNetWorld.h`)

```cpp
struct NkNetEntity {                                    // :128
        NkNetId netId;  uint32 prefabId;  void* user;
        WriteStateFn writeState;   ///< sérialise l'état — appelé côté AUTORITÉ   // :152
        ReadStateFn  readState;    ///< applique un état reçu — côté réplique      // :155
};
struct NkNetInput { /* sequence, tick, data[], from */ };  // :199

class NkNetWorld {                                      // :343
        struct Config { float32 tickRate; uint32 keyframeInterval; /* ... */ };
        void Init(NkConnectionManager* mgr, bool isServer, Config cfg = {}) noexcept;
        NkNetId AllocateNetId() noexcept;                // :411
        bool RegisterEntity(const NkNetEntity& desc) noexcept;                    // :420
        bool UnregisterEntity(NkNetId netId, bool notifyPeers = true) noexcept;   // :428
        NkNetEntity* FindEntity(NkNetId netId) noexcept;                          // :432
        uint32 EntityCount() const noexcept;                                      // :435
        void Update(float32 dt) noexcept;                                         // :449
        bool HandleMessage(const NkReceiveMsg& msg) noexcept;                     // :457
        uint32 CurrentTick() const noexcept;                                      // :464
        void DrainInputs(NkPeerId peer, NkVector<NkNetInput>& out) noexcept;      // :486
        SpawnCb   onEntitySpawn;      ///< (NkNetId, uint32 prefabId, NkPeerId owner)  // :512
        DespawnCb onEntityDespawn;    ///< (NkNetId)                                    // :515
};
```

Le modèle est décrit `Replication/NkNetWorld.h:20-22` :

> ```
>  HandleMessage(). Entité inconnue → callback onEntitySpawn (l'application
>  crée son objet et l'enregistre) ; état reçu → readState ; destruction →
>  onEntityDespawn. Les inputs locaux remontent via SendInput().
> ```

### 5.2.7 `NkHTTPClient` (`HTTP/NkHTTPClient.h`)

```cpp
struct NkHTTPRequest  { /* ... */ NkString body; };          // :214, :234
struct NkHTTPResponse {                                       // :383
        uint32 statusCode = 0;      // :390
        NkString body;              // :407
        NkString error;             // :416
        bool IsOK() const noexcept; // :436
};
class NkHTTPClient {                                          // :549
        NkHTTPResponse Get(const char* url) noexcept;         // :700
        NkHTTPResponse Post(const char* url, const char* json) noexcept;  // :717
};
```

## 5.3 Implémenté vs déclaré-mais-vide

### 🚨 Le dossier `tests/` est VIDE — mais le `.jenga` déclare une cible de test

`Kernel\System\NKNetwork\tests\` ne contient qu'un sous-dossier vide `.jenga/`.
Le glob `testfiles(["tests/**.cpp"])` de `NKNetwork.jenga:92-95` ne matche **aucun
fichier** : le bloc `with test():` est un squelette mort.

**Il n'existe AUCUNE fonction `SelfTest()` dans NKNetwork** (recherche insensible à
la casse sur tout le module : 0 résultat) — contrairement à NKMedia (52 SelfTest)
et NKAudio (2).

Le runner officiel est une **application séparée**, et la ROADMAP le dit
explicitement (`Kernel\System\NKNetwork\ROADMAP.md:131-138`) :

> ```
>  ### Tests (2026-07-12)
>  - `Sandbox/System/NKNetwork` (cible `SandboxNKNetwork`) : **67 checks verts** —
>    NkBitStream round-trip + overflow, NkNetId Pack/Unpack, NkNetInterpolator,
>    réplication client/serveur sur messages forgés, **bout-en-bout loopback réel**
>    (StartServer + Connect 127.0.0.1, handshake, snapshot → spawn → delta →
>    despawn). Mode manuel `--https <url>` pour valider TLS.
>    (`jenga test` restant bloqué par la policy workspace `disableunittestexecution`,
>    le sandbox est le runner officiel du module.)
> ```

→ **Attention au chemin** : le sandbox réseau est dans
`D:\Projets\2026\Nkentseu\Nkentseu\Sandbox\System\NKNetwork\` (racine `Sandbox/`),
**pas** `Applications\Sandbox\`.
Binaires bâtis : `Build\Bin\Debug-Windows\SandboxNKNetwork\SandboxNKNetwork.exe`
et `Build\Bin\Release-Windows\SandboxNKNetwork\SandboxNKNetwork.exe`.

### Réellement exercé par du code du dépôt (donc sûr à enseigner)

| API | Utilisée par |
|---|---|
| `NkSocket::PlatformInit/PlatformShutdown` | SandboxNKNetwork, NkNetWorldDemo, Pong |
| `NkSocket::Create/SetNonBlocking/SetBroadcast/SendTo/RecvFrom` | **Pong uniquement** (`NetworkDiscovery`) |
| `NkConnectionManager` (`StartServer`/`Connect`/`Broadcast`/`SendTo`/`DrainAll`/`Disconnect`/`Shutdown`/`ConnectedPeerCount`) | SandboxNKNetwork, NkNetWorldDemo, Pong, Noge |
| `NkAddress(ip,port)` / `Any` / `Broadcast` / `Loopback` / `IsValid` / `ToString` | SandboxNKNetwork, NkNetWorldDemo, Pong |
| `NkBitWriter` / `NkBitReader` | SandboxNKNetwork, Noge |
| `NkNetWorld` | SandboxNKNetwork, Noge (donc NkNetWorldDemo) |
| `NkNetInterpolator`, `NkNetId::Pack/Unpack` | **SandboxNKNetwork uniquement** |
| `NkHTTPClient::Get` | **SandboxNKNetwork uniquement** (mode `--https`) |

### Jamais utilisé par aucune application — code mort côté consommateurs
- **`NkLobby` / `NkSession` / `NkMatchmaker` / `NkDiscovery`** (67 Ko de header !)
- **`NkRPC` / `NkRPCRouter`** — la ROADMAP le classe « **Livré (déclaratif)** »
  (`ROADMAP.md:22`), ce qui n'est pas la même chose que « prouvé ».
- **`NkReliableUDP`** en usage direct (il n'est employé qu'en interne par `NkConnection`).

→ **Le cours ne doit PAS enseigner Lobby / Matchmaker / RPC** : rien dans le dépôt
ne prouve qu'ils fonctionnent bout en bout.

### Non fait, d'après `ROADMAP.md:32-40`
| Élément | Statut ROADMAP |
|---|---|
| Prédiction client + réconciliation serveur | **TODO** |
| Relais des entités client-authoritative | **TODO** |
| WebAssembly — `emscripten_fetch` pour HTTP | **Partiel** |
| WebSocket / WebRTC (transport navigateur) | **TODO** |
| Compression LZ4/Zstd des snapshots | **TODO** |
| Chiffrement bout-en-bout (DTLS) | **TODO** |
| NAT traversal (STUN/TURN/ICE) | **TODO** |
| Stats/métriques runtime (RTT, perte, bande passante) | **Partiel** |

### HTTPS : désactivé par défaut
`HTTP/NkHTTPClient.cpp:1050` renvoie littéralement :

```cpp
			response.error = "HTTPS not compiled in (rebuild with NK_ENABLE_TLS=1)";
```

→ Sans `NK_ENABLE_TLS=1` au build, **toute URL `https://` échoue proprement**.
Le cours doit le dire, ou se limiter à `http://`.

## 5.4 Exemples d'utilisation réels tirés du dépôt

### 5.4.1 `SandboxNKNetwork` — le bout-en-bout complet (application **bâtie** Debug + Release)

`D:\Projets\2026\Nkentseu\Nkentseu\Sandbox\System\NKNetwork\src\main.cpp` (465 lignes).

Includes, `main.cpp:16-22` :

```cpp
#include "NKNetwork/NKNetwork.h"
#include "NKTime/NkChrono.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::net;
```

**Init plateforme + serveur + client + attente du handshake**, `main.cpp:313-334` :

```cpp
	void TestLoopback() {
		NK_CHECK(NkSocket::PlatformInit() == NkNetResult::NK_NET_OK);

		constexpr uint16 kPort = 48213;

		NkConnectionManager server;
		NK_CHECK(server.StartServer(kPort, 8) == NkNetResult::NK_NET_OK);

		NkConnectionManager client;
		NK_CHECK(client.Connect(NkAddress("127.0.0.1", kPort)) == NkNetResult::NK_NET_OK);

		// Attente de l'établissement de la connexion (handshake 3-way).
		bool connected = false;
		for (int i = 0; i < 500; ++i) {
			if (server.ConnectedPeerCount() >= 1 && client.ConnectedPeerCount() >= 1) {
				connected = true;
				break;
			}
			NkChrono::SleepMilliseconds(static_cast<int64>(10));
		}
		NK_CHECK(connected);
```

**Boucle de pompage : le serveur émet, le client draine et route**, `main.cpp:397-410` :

```cpp
		bool applied = false;
		NkVector<NkReceiveMsg> msgs;
		for (int i = 0; i < 500 && !applied; ++i) {
			serverWorld.Update(0.02f);
			msgs.Clear();
			client.DrainAll(msgs);
			for (usize m = 0; m < msgs.Size(); ++m) {
				(void)clientWorld.HandleMessage(msgs[m]);
			}
			applied = spawned && clientPawn.x == 12.5f && clientPawn.y == -7.25f;
			NkChrono::SleepMilliseconds(static_cast<int64>(10));
		}
```

**Fermeture — ORDRE À RETENIR**, `main.cpp:430-433` :

```cpp
		client.Shutdown();
		server.Shutdown();
		NkSocket::PlatformShutdown();
```

**Round-trip `NkBitStream`**, `main.cpp:47-89` (extrait) — c'est **l'exercice idéal
du chapitre, sans réseau du tout** :

```cpp
	void TestBitStream() {
		uint8 buffer[256];
		NkBitWriter writer(buffer, sizeof(buffer));

		writer.WriteBool(true);
		writer.WriteU8(0xAB);
		writer.WriteU16(0x1234);
		writer.WriteU32(0xDEADBEEF);
		writer.WriteU64(0x0123456789ABCDEFull);
		writer.WriteI32(-42);
		writer.WriteF32(3.5f);
		writer.WriteBits(5u, 3);
		writer.AlignToByte();
		const uint8 raw[4] = {1, 2, 3, 4};
		writer.WriteBytes(raw, 4);
		NK_CHECK(!writer.IsOverflowed());

		NkBitReader reader(buffer, writer.BytesWritten());
		NK_CHECK(reader.ReadBool() == true);
		NK_CHECK(reader.ReadU8() == 0xAB);
		/* ... symétrique ... */
		NK_CHECK(!reader.IsOverflowed());

		// Overflow en écriture comme en lecture.
		uint8 tiny[2];
		NkBitWriter tinyWriter(tiny, sizeof(tiny));
		tinyWriter.WriteU32(0xFFFFFFFFu);
		NK_CHECK(tinyWriter.IsOverflowed());
	}
```

**Réplication : callback de spawn + `readState`**, `main.cpp:171-203` (extrait) :

```cpp
		world.onEntitySpawn = [&](NkNetId netId, uint32 prefabId, NkPeerId /*owner*/) {
			spawnedPrefab = prefabId;
			NkNetEntity desc;
			desc.netId = netId;
			desc.prefabId = prefabId;
			desc.user = &pawn;
			desc.readState = [](void *user, NkBitReader &r) {
				TestPawn *p = static_cast<TestPawn *>(user);
				p->x = r.ReadF32();
				p->y = r.ReadF32();
			};
			world.RegisterEntity(desc);
		};
```

Et le pendant côté serveur, `main.cpp:349-361` :

```cpp
		NkNetEntity desc;
		desc.netId = serverWorld.AllocateNetId();
		desc.prefabId = 99;
		desc.user = &serverPawn;
		desc.writeState = [](void *user, NkBitWriter &w) {
			TestPawn *p = static_cast<TestPawn *>(user);
			w.WriteF32(p->x);
			w.WriteF32(p->y);
		};
		NK_CHECK(serverWorld.RegisterEntity(desc));
```

**Mode HTTPS manuel**, `main.cpp:441-451` :

```cpp
int main(int argc, char **argv) {
	// Mode HTTPS manuel : SandboxNKNetwork --https <url>
	// Nécessite un build avec NK_ENABLE_TLS=1 (backend mbedTLS), sinon le
	// client renvoie proprement "HTTPS not compiled in".
	if (argc >= 3 && NkString(argv[1]) == NkString("--https")) {
		NkHTTPClient http;
		const NkHTTPResponse resp = http.Get(argv[2]);
		std::printf("[HTTPS] %s -> status=%u bodyLen=%u error='%s'\n", argv[2], resp.statusCode,
					resp.body.Length(), resp.error.CStr());
		return (resp.statusCode >= 200 && resp.statusCode < 400) ? 0 : 1;
	}
```

### 5.4.2 `NkNetWorldDemo` — réplication liée à l'ECS (application **bâtie** Debug)

`Applications\NkNetWorldDemo\src\main.cpp` (213 lignes).

Le bloc d'includes contient **le piège le plus important du module**,
`main.cpp:20-37` — recopié mot pour mot :

```cpp
// PAS l'umbrella NKNetwork/NKNetwork.h : il injecte des alias de commodité
// dans `nkentseu` (dont `using NkNetSystem = net::NkNetSystem;` et
// `using NkNetEntity = net::NkNetEntity;`) qui entrent en collision directe
// avec l'adaptateur ECS de Noge (`nkentseu::NkNetSystem`, composant
// `nkentseu::ecs::NkNetEntity`). On inclut donc uniquement les en-têtes
// précis nécessaires.
#include "Noge/ECS/Replication/NkNetWorld.h"
#include "NKECS/World/NkWorld.h"
#include "NKNetwork/Transport/NkSocket.h"
#include "NKTime/NkChrono.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;
using namespace nkentseu::ecs;
// PAS de `using namespace nkentseu::net;` : net::NkNetEntity (descripteur de
// réplication NKNetwork) et ecs::NkNetEntity (composant ECS) partagent le
// même nom -- même piège que Noge/ECS/Replication/NkNetWorld.cpp.
```

La boucle de tick générique, `main.cpp:61-73` :

```cpp
	template <typename DoneFn>
	bool Pump(NkNetSystem &serverNet, NkWorld &serverWorld, NkNetSystem &clientNet, NkWorld &clientWorld,
			  int maxSteps, DoneFn done) noexcept {
		for (int i = 0; i < maxSteps; ++i) {
			serverNet.Execute(serverWorld, 0.02f);
			clientNet.Execute(clientWorld, 0.02f);
			if (done()) {
				return true;
			}
			NkChrono::SleepMilliseconds(static_cast<int64>(10));
		}
		return done();
	}
```

Le nettoyage complet, `main.cpp:205-211` :

```cpp
	serverNet.Shutdown();
	clientNet.Shutdown();
	client.Shutdown();
	server.Shutdown();
	net::NkSocket::PlatformShutdown();
```

### 5.4.3 `Pong` — la seule application « produit » réellement en réseau

`Applications\Pong\src\Pong\Net\NetworkSession.{h,cpp}` (246 + 483 l.),
`NetworkDiscovery.{h,cpp}` (140 + 241 l.), `NetProtocol.h` (387 l.).
Bâti en **Release-Windows** (le binaire Debug est absent).

Init/shutdown plateforme, `NetworkSession.cpp:28-40` :

```cpp
		void NetworkSession::PlatformInit() {
			const auto r = net::NkSocket::PlatformInit();
			if (r != net::NkNetResult::NK_NET_OK) {
				logger.Error("[Net] PlatformInit failed: {0}", (int)r);
			} else {
				logger.Info("[Net] PlatformInit OK");
			}
		}

		void NetworkSession::PlatformShutdown() {
			net::NkSocket::PlatformShutdown();
			logger.Info("[Net] PlatformShutdown");
		}
```

Envoi avec choix de canal, `NetworkSession.cpp:234-243` — **le passage à recopier
dans le cours pour expliquer fiable vs non-fiable** :

```cpp
		bool NetworkSession::Broadcast(const uint8 *data, uint32 size, uint8 reliable) {
			if (mConnMgr == nullptr)
				return false;
			if (mState.load() != NetworkState::Connected)
				return false;
			const auto ch = reliable ? net::NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED
									 : net::NkNetChannel::NK_NET_CHANNEL_UNRELIABLE;
			const auto r = mConnMgr->Broadcast(data, size, ch);
			return r == net::NkNetResult::NK_NET_OK;
		}
```

Réception + routage par type de message, `NetworkSession.cpp:245-260` :

```cpp
		void NetworkSession::DrainInternal() {
			if (mConnMgr == nullptr)
				return;
			// Drain brut depuis NkConnectionManager.
			NkVector<net::NkReceiveMsg> all;
			mConnMgr->DrainAll(all);
			for (uint32 i = 0; i < all.Size(); ++i) {
				const auto &msg = all[i];
				if (msg.size >= sizeof(netproto::PktHello) && msg.data[0] == netproto::kMsgHello) {
					netproto::PktHello pkt;
					std::memcpy(&pkt, msg.data, sizeof(pkt));
					/* ... */
					continue;
				}
				/* ... */
				// Message non-interne : reserve pour le caller via DrainReceived.
				mPendingForUser.PushBack(msg);
			}
		}
```

Câblage dans la boucle de jeu, `Applications\Pong\src\Pong\Game\PongApp.cpp:105`,
`:162-169`, `:136-139` :

```cpp
			// Init reseau (sockets plateforme). Idempotent — peut etre rappele.
			NetworkSession::PlatformInit();
```
```cpp
			// Tick reseau (drain interne du thread reseau). Aucun cout si
			// la session est Idle.
			mNetwork.Tick(dt);
			// Tick decouverte LAN : emet beacon (host) + drain scan (client).
			mDiscovery.Tick(dt);
```
```cpp
			// Ferme la session reseau et libere les sockets plateforme.
			mNetwork.Shutdown();
			NetworkSession::PlatformShutdown();
```

### 5.4.4 Découverte LAN — le seul usage RAW de `NkSocket` du dépôt

`Applications\Pong\src\Pong\Net\NetworkDiscovery.cpp`. Socket d'émission
(broadcast), `:44-63` :

```cpp
			mBeaconSock = new net::NkSocket();
			// Bind sur une socket UDP IPv4 quelconque (port 0 = laisse l'OS choisir),
			// on n'a besoin que d'envoyer. SetBroadcast active SO_BROADCAST
			// necessaire pour pouvoir SendTo vers 255.255.255.255.
			const auto rc = mBeaconSock->Create(net::NkAddress::Any(0), net::NkSocket::Type::NK_UDP);
			if (rc != net::NkNetResult::NK_NET_OK) { /* ... */ return false; }
			(void)mBeaconSock->SetNonBlocking(true);
			const auto rb = mBeaconSock->SetBroadcast(true);
			if (rb != net::NkNetResult::NK_NET_OK) { /* ... */ return false; }
```

Socket d'écoute, `:84-96` :

```cpp
			mScanSock = new net::NkSocket();
			// Bind sur kBeaconPort, INADDR_ANY : on reçoit les broadcasts
			// emis vers 255.255.255.255:kBeaconPort par d'autres machines du LAN.
			const auto rc = mScanSock->Create(net::NkAddress::Any(kBeaconPort), net::NkSocket::Type::NK_UDP);
			/* ... */
			(void)mScanSock->SetNonBlocking(true);
			(void)mScanSock->SetBroadcast(true);
```

Le tick (émission + drain non bloquant), `:115-160` (extrait) :

```cpp
			if (mBeaconSock != nullptr) {
				mBeaconTimer -= dt;
				if (mBeaconTimer <= 0.0f) {
					const auto dst = net::NkAddress::Broadcast(kBeaconPort);
					const auto rs = mBeaconSock->SendTo(&mBeaconPkt, sizeof(mBeaconPkt), dst);
					/* ... */
					mBeaconTimer = kBeaconIntervalSec;
				}
			}
			if (mScanSock != nullptr) {
				// Boucle : on lit tant qu'il y a des datagrammes pendants.
				// En non-bloquant, RecvFrom OK avec outSize=0 = rien a lire.
				constexpr int kMaxPerTick = 32;
				for (int i = 0; i < kMaxPerTick; ++i) {
					uint8 buf[256];
					uint32 received = 0;
					net::NkAddress from;
					const auto rr = mScanSock->RecvFrom(buf, sizeof(buf), received, from);
					if (rr != net::NkNetResult::NK_NET_OK) break;
					if (received == 0) break;
					/* filtre magic + version, puis UpdateHost(pkt, from) */
				}
			}
```

## 5.5 Squelette minimal pour une application

**(a) Serveur**

```cpp
// N'inclure QUE ce dont on a besoin (voir l'invariant §5.6-1 sur l'umbrella).
#include "NKNetwork/Transport/NkSocket.h"
#include "NKNetwork/Protocol/NkConnection.h"
#include "NKTime/NkChrono.h"
using namespace nkentseu;

int main() {
    // 1) UNE fois par processus.
    if (net::NkSocket::PlatformInit() != net::NkNetResult::NK_NET_OK)
        return 1;

    net::NkConnectionManager server;

    // 2) Callbacks AVANT StartServer — ils sont appelés DEPUIS LE THREAD RÉSEAU.
    server.onPeerConnected    = [](net::NkPeerId p) { /* ne rien faire de lourd ici */ };
    server.onPeerDisconnected = [](net::NkPeerId p, const char *raison) { /* ... */ };

    // 3) Démarre : crée le socket UDP + lance le thread réseau interne.
    if (server.StartServer(7777, 64) != net::NkNetResult::NK_NET_OK)
        return 2;

    // 4) Boucle : on DRAINE, on ne "reçoit" pas soi-même.
    NkVector<net::NkReceiveMsg> msgs;
    while (running) {
        msgs.Clear();
        server.DrainAll(msgs);
        for (uint32 i = 0; i < msgs.Size(); ++i) {
            const net::NkReceiveMsg &m = msgs[i];
            TraiterMessage(m.data, m.size, m.from);
        }
        // ... réponse ...
        server.Broadcast(payload, taille, net::NkNetChannel::NK_NET_CHANNEL_UNRELIABLE);
        NkChrono::SleepMilliseconds(16);
    }

    // 5) Ordre : manager d'abord (join du thread), plateforme ensuite.
    server.Shutdown();
    net::NkSocket::PlatformShutdown();
    return 0;
}
```

**(b) Client**

```cpp
net::NkSocket::PlatformInit();
net::NkConnectionManager client;
const net::NkAddress addr("192.168.1.20", 7777);
if (!addr.IsValid())                       // TOUJOURS tester : parse de l'IP
    return 1;
if (client.Connect(addr) != net::NkNetResult::NK_NET_OK)
    return 2;

// Le handshake 3-way est ASYNCHRONE : attendre que le pair apparaisse.
for (int i = 0; i < 500 && client.ConnectedPeerCount() == 0; ++i)
    NkChrono::SleepMilliseconds(10);
if (client.ConnectedPeerCount() == 0)
    return 3;                              // serveur injoignable

// ... même boucle DrainAll que le serveur ...

client.Shutdown();
net::NkSocket::PlatformShutdown();
```

**(c) Composer un message avec `NkBitWriter` (sans réseau : testable seul)**

```cpp
#include "NKNetwork/Protocol/NkBitStream.h"

uint8 buf[256];
net::NkBitWriter w(buf, sizeof(buf));
w.WriteU8(kMonTypeDeMessage);
w.WriteInt(scoreJoueur, 0, 999);              // 10 bits au lieu de 32
w.WriteF32Q(posX, -100.f, 100.f, 0.01f);      // quantifié, ~15 bits
w.AlignToByte();
if (!w.IsOverflowed())
    client.SendTo(peer, buf, (uint32)w.BytesWritten(),
                  net::NkNetChannel::NK_NET_CHANNEL_RELIABLE_ORDERED);

// Lecture — MÊME ORDRE, MÊMES BORNES, sinon tout est décalé.
net::NkBitReader r(m.data, m.size);
const uint8 type = r.ReadU8();
const int32 score = r.ReadInt(0, 999);
const float32 x   = r.ReadF32Q(-100.f, 100.f, 0.01f);
if (r.IsOverflowed()) { /* message tronqué : jeter */ }
```

**(d) Découverte LAN (annoncer un serveur, le trouver)**

```cpp
#include "NKNetwork/Transport/NkSocket.h"

// -- Côté hôte : émettre une balise --
net::NkSocket beacon;
beacon.Create(net::NkAddress::Any(0), net::NkSocket::Type::NK_UDP);
beacon.SetNonBlocking(true);
beacon.SetBroadcast(true);                              // SANS ça, SendTo échoue
beacon.SendTo(&paquet, sizeof(paquet), net::NkAddress::Broadcast(kPortBalise));

// -- Côté client : écouter --
net::NkSocket scan;
scan.Create(net::NkAddress::Any(kPortBalise), net::NkSocket::Type::NK_UDP);
scan.SetNonBlocking(true);
uint8 buf[256]; uint32 recu = 0; net::NkAddress from;
if (scan.RecvFrom(buf, sizeof(buf), recu, from) == net::NkNetResult::NK_NET_OK && recu > 0) {
    // from.ToString() = adresse de l'hôte -> proposer de s'y connecter
}
```

## 5.6 Pièges et invariants (citations du dépôt)

**1. NE PAS inclure l'en-tête parapluie `NKNetwork/NKNetwork.h` dans une
application qui utilise l'ECS.** Il injecte des `using` dans le namespace
`nkentseu` (`NKNetwork.h:102-147`) qui collisionnent. Le dépôt le documente deux
fois, `NkNetWorldDemo\...\main.cpp:20-26` :

> ```
>  PAS l'umbrella NKNetwork/NKNetwork.h : il injecte des alias de commodité
>  dans `nkentseu` (dont `using NkNetSystem = net::NkNetSystem;` et
>  `using NkNetEntity = net::NkNetEntity;`) qui entrent en collision directe
>  avec l'adaptateur ECS de Noge […]. On inclut donc uniquement les en-têtes
>  précis nécessaires.
> ```

et `main.cpp:34-37` :

> ```
>  PAS de `using namespace nkentseu::net;` : net::NkNetEntity (descripteur de
>  réplication NKNetwork) et ecs::NkNetEntity (composant ECS) partagent le
>  même nom
> ```

**2. `PlatformInit()` avant tout, `PlatformShutdown()` après tout.** C'est
`WSAStartup` sur Windows. `ROADMAP.md:52-54` : « `PlatformInit()` **requis avant
usage** ». Ordre de fermeture prouvé par `SandboxNKNetwork\...\main.cpp:430-433` :
`client.Shutdown(); server.Shutdown(); NkSocket::PlatformShutdown();`

**3. `StartServer`/`Connect` lancent un THREAD interne.**
`Protocol/NkConnection.h:764` et `:782` : « Démarre le **thread réseau interne**
pour polling automatique. » Conséquences :
- les callbacks `onPeerConnected` / `onPeerDisconnected` s'exécutent **sur ce
  thread**, pas sur le thread principal — n'y faire que de l'atomique/du léger
  (voir `Pong\...\NetworkSession.cpp:60-63` qui n'y fait qu'un `fetch_add`) ;
- `Shutdown()` **joint** ce thread (`NkConnection.h:787` : « Attend la fin du
  thread réseau avant retour (join) »).

**4. Le handshake est ASYNCHRONE.** `Connect()` renvoie `NK_NET_OK` **avant** que
la connexion soit établie. Le seul test valable est `ConnectedPeerCount() >= 1`
dans une boucle bornée — patron identique dans les deux démos
(`SandboxNKNetwork\...\main.cpp:324-332`, `NkNetWorldDemo\...\main.cpp:106-116`).

**5. On ne « reçoit » pas : on DRAINE.** `DrainAll(NkVector<NkReceiveMsg>&)` vide
la file remplie par le thread réseau. Toujours `msgs.Clear()` avant, sinon on
retraite les anciens (voir `SandboxNKNetwork\...\main.cpp:400-401`).

**6. `NkReceiveMsg::data` est un buffer INLINE de 1380 octets.**
`Protocol/NkConnection.h:235` : `uint8 data[kNkMaxPayloadSize] = {};`. Donc
`NkVector<NkReceiveMsg>` est **lourd** (1,4 Ko par élément) : le vider entre deux
frames et ne jamais le copier inutilement. Corollaire : **un message ne peut pas
dépasser 1380 octets** (`kNkMaxPayloadSize`, `NkNetDefines.h:151`) — au-delà,
`NK_NET_PACKET_TOO_LARGE`.

**7. Toujours tester `msg.data != nullptr` avant de déréférencer** — bug réel du
dépôt : `Pong\...\NetworkSession.cpp:253,274,279` déréfère `msg.data[0]` sans
garde, alors que la copie `Pong2\...\NetworkSession.cpp:253-254,275,280` ajoute
`&& msg.data != nullptr`. **C'est la seule différence entre les deux copies.**
Le cours doit enseigner la version corrigée.

**8. Choisir le bon canal.** `Core/NkNetDefines.h:505-525` donne la doctrine :
- `NK_NET_CHANNEL_UNRELIABLE` — « positions, inputs, animations — données éphémères » ;
- `NK_NET_CHANNEL_RELIABLE_ORDERED` — « chat, événements gameplay, commandes critiques » ;
- `NK_NET_CHANNEL_SEQUENCED` — « états continus (health, mana) — les anciens sont obsolètes » ;
- `NK_NET_CHANNEL_SYSTEM` — « **Usage interne uniquement — comportement non défini
  si utilisé.** » ← à interdire explicitement aux élèves.

**9. `SetBroadcast(true)` est obligatoire pour émettre vers 255.255.255.255.**
`Pong\...\NetworkDiscovery.cpp:47-49` :

> ```
>  Bind sur une socket UDP IPv4 quelconque (port 0 = laisse l'OS choisir),
>  on n'a besoin que d'envoyer. SetBroadcast active SO_BROADCAST
>  necessaire pour pouvoir SendTo vers 255.255.255.255.
> ```

**10. En non-bloquant, `RecvFrom` OK avec `outSize == 0` signifie « rien à lire ».**
`Pong\...\NetworkDiscovery.cpp:139-140` :

> ```
>  Boucle : on lit tant qu'il y a des datagrammes pendants.
>  En non-bloquant, RecvFrom OK avec outSize=0 = rien a lire.
> ```

Et la boucle est **bornée** (`kMaxPerTick = 32`) pour ne pas monopoliser la frame.

**11. `NkConnectionManager` est non copiable.**
`Protocol/NkConnection.h:752-753` : `NkConnectionManager(const NkConnectionManager&) = delete;`.
D'où le `new` dans Pong (`NetworkSession.cpp:43-51`).

**12. Toujours valider l'adresse avant `Connect`.**
`Pong\...\NetworkSession.cpp:122-129` teste `addr.IsValid()` et rend un message
utilisateur explicite. `NkAddress::IsValid()` (`Transport/NkSocket.h:327`).

**13. `writeState` doit être DÉTERMINISTE.** `Replication/NkNetWorld.h:124-126` :

> ```
>  @note writeState doit être DÉTERMINISTE vis-à-vis de l'état : deux
>  [appels sur le même état doivent produire les mêmes octets]
> ```

**14. `writeState`/`readState` doivent être des LAMBDAS, pas des pointeurs de
fonction bruts.** Documenté dans le pont ECS
(`Engine\Noge\src\Noge\ECS\Replication\NkNetWorld.cpp:132-138`) : un pointeur de
fonction nu résout mal la surcharge de `NkFunction`.

**15. `maxConnections` est plafonné.** `Protocol/NkConnection.h:174-176` :
« Doit être ≤ `kNkMaxConnections` (256) défini dans NkNetDefines.h. »

**16. Ne pas lier `ws2_32` = erreurs d'édition de liens obscures.** À rappeler dans
la partie build du chapitre (`NkNetWorldDemo.jenga:35-40`).

## 5.7 Raccordement NkCanvas / NKGui

NKNetwork n'a **aucun lien** avec le rendu — le pont est entièrement applicatif.
Le modèle éprouvé est celui de Pong :

1. **Une classe de session** possède le `NkConnectionManager` et expose une API
   simple à l'interface (`Pong\...\Net\NetworkSession.h`, 246 l.) : `StartHost`,
   `StartJoin`, `Broadcast`, `DrainReceived`, `Shutdown`, plus un `NetworkState`
   atomique (`Idle` / `Hosting` / `Connected`).
2. **Un `Tick(dt)` appelé une fois par frame** depuis la boucle principale
   (`PongApp.cpp:162-169`) draine le thread réseau et met à jour l'état.
3. **Les scènes d'interface lisent l'état, jamais le manager.**
   `NetworkLobbyScene.cpp:93` (`DrainReceived`), `:142` (`StartBeacon`),
   `:153`/`:210` (`StartScan`) ; `GameplayScene.cpp:404-406` (réception),
   `:705` (input joueur, canal non fiable), `:849` (snapshot d'état, non fiable),
   `:227`/`:293`/`:309` (pause / but / rejouer, canal fiable).

C'est **exactement le découpage à enseigner** : le réseau vit sur son thread, la
session fait le tampon, l'interface ne voit qu'un état et une file de messages.

**Verdict NKNetwork : 🟡 partiel — chapitre honnête possible, périmètre à borner.**
Le cours doit :
(a) se limiter à `NkSocket` (init/plateforme + UDP brut pour la découverte LAN),
`NkAddress`, `NkConnectionManager` et `NkBitWriter`/`NkBitReader` — les seules
briques réellement exercées ;
(b) **ne pas enseigner** `NkLobby` / `NkMatchmaker` / `NkRPC` (aucun consommateur
dans le dépôt) ni la prédiction client (TODO en ROADMAP) ;
(c) prévenir que HTTPS échoue sans `NK_ENABLE_TLS=1` ;
(d) marteler les trois invariants de thread : callbacks sur le thread réseau,
handshake asynchrone, `Shutdown()` avant `PlatformShutdown()` ;
(e) rappeler que la doc du `.jenga` décrit un autre module et que `tests/` est vide.

---

# 6. Récapitulatif transversal pour le cours

## 6.1 Ordre d'enseignement recommandé (imposé par les dépendances)

```
NKImage  ─┬─> NKMedia ──> NKAudio
          │      (NKMedia dépend de NKImage : codec MJPEG = codec JPEG de NKImage)
          │      (NKAudio dépend de NKMedia : décodeur Opus)
          └─> NKFont     (indépendant, ne dépend ni de NKImage ni de NkCanvas)

NKNetwork (totalement indépendant — peut venir n'importe où)
```

Chapitre 1 : **NKImage** (le plus simple, le plus sûr, et il alimente les autres).
Chapitre 2 : **NKFont** (indépendant, résultat visuel immédiat).
Chapitre 3 : **NKAudio** (façade propre, gratification immédiate : un son au clic).
Chapitre 4 : **NKMedia** (le plus gros — à borner sévèrement).
Chapitre 5 : **NKNetwork** (le plus abstrait — à borner aussi).

## 6.2 Les trois règles de mémoire à répéter dans chaque chapitre

1. **`NkImage`** : `Free()` uniquement sur les objets issus des fabriques statiques ;
   `Unload()` sur les instances de pile ; buffers `Encode*` → `memory::NkFree`.
2. **`NKAudio`** : `engine.Shutdown()` **avant** `AudioLoader::Free(sample)` ;
   les `IAudioStream` non confiés à un player → `memory::NkGetDefaultAllocator().Delete(...)`.
3. **Partout** : jamais `std::free` / `delete` sur de la mémoire NKMemory
   (« crash c0000374 sur Windows », `NKImage\...\NkImage.h:36`).

## 6.3 Les commentaires du dépôt auxquels il ne faut PAS se fier

| Fichier | Problème |
|---|---|
| `NKImage\src\NKImage\NKImage.h:12-33` | exemple `@code` avec `NkSVGRenderer`/`NkSVGDOM` — classes **supprimées** |
| `NKFont\NKFont.jenga:7-23` | promet BDF, Type 1, WOFF2, hinting, GSUB, Bidi — **rien de tout ça n'existe** |
| `NKAudio\...\NkAudioLoader.cpp:3` | « stubs MP3/OGG/FLAC » — **périmé**, les 4 codecs sont branchés |
| `NKMedia\...\NkVideoReader.h:8-11` | « H264 = à venir » — **périmé**, H264/HEVC/VP8/VP9 sont branchés |
| `NKMedia\...\NkVideoWriter.h:5-6` | « MP4/WebM à venir » — **périmé**, livrés |
| `NKNetwork\NKNetwork.jenga:3-15` | décrit **NKReflection**, pas NKNetwork |
| `Applications\NkImageDemo\...\ViewerApp.cpp:450` | appelle une `NkImage::Load` **statique inexistante** |
| `Pong\...\NetworkSession.cpp:253` | déréférence `msg.data[0]` sans garde nulle (corrigé dans Pong2) |

**Règle générale à donner à l'auteur du cours : quand un `.jenga` ou un en-tête
contredit le `.cpp` ou le `ROADMAP.md`, c'est le `.cpp`/`ROADMAP.md` qui a raison.**

## 6.4 Applications de référence et état de build (Windows)

| Application | Module | Debug | Release | Rôle pour le cours |
|---|---|---|---|---|
| `NKImageCodecTest` | NKImage | ✅ | ✅ | squelette API statique |
| `NKGuiDemo` | NKImage + NKFont + NKGui | ❌ | ❌ | **source à jour, mais non bâtie** — image + police dans une interface |
| `NkImageDemo` | NKImage | ❌ | ❌ | **ne compile plus** (`NkImage::Load` statique inexistante) — logique seulement |
| `Pong` | NKFont, NKAudio, NKMedia, NKNetwork | ❌ | ✅ | atlas + réseau LAN complet |
| `NkAudioPlayer` | NKAudio + NkCanvas | ❌ | ✅ | **le patron audio canonique** |
| `NkAudioDemo` | NKAudio (streaming) | ❌ | ✅ | `IAudioStream` |
| `NkMicRecord` | NKAudio (capture) | ❌ | ✅ | micro + transcodage |
| `NKMediaTest` | NKMedia | ✅ | ✅ | probe/démux/recorder |
| `NKVideoTest` | NKMedia | ✅ | ✅ | **écriture vidéo, 266 l.** |
| `NkVideoReadTest` | NKMedia | ✅ | ✅ | **lecture + tous les self-tests** |
| `NkVideoPlayer` | NKMedia + NkCanvas + NKAudio | ❌ | ✅ | lecteur complet |
| `NKCode` | NKMedia + NKGui | ✅ | ✅ | vidéo dans une interface |
| `SandboxNKNetwork` | NKNetwork | ✅ | ✅ | **le seul test réseau** |
| `NkNetWorldDemo` | NKNetwork + ECS | ✅ | ❌ | réplication ECS |

*(Chemins des binaires : `D:\Projets\2026\Nkentseu\Nkentseu\Build\Bin\Debug-Windows\<App>\` et `...\Release-Windows\<App>\`.)*
