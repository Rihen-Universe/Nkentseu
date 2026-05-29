#pragma once
/**
 * @File    NkImage.h
 * @Brief   NkImage — chargement/sauvegarde/manipulation d'images, sans dépendance externe.
 *          Algorithmes inflate/deflate adaptés de stb_image v2.16 (public domain, Sean Barrett).
 *
 * ─── PHILOSOPHIE DE L'API ────────────────────────────────────────────────────
 *
 *  Il existe deux familles de méthodes :
 *
 *  1. API STATIQUE  → retourne `NkImage*`  (ownership explicite, heap-alloué)
 *     - NkImage::Alloc(...)       fabriques bas niveau pour les codecs
 *     - NkImage::Wrap(...)        vue non-owning sur un buffer externe
 *     - NkImage::Create(...)      création statique avec couleur de remplissage
 *     - NkImage::Convert(...)     conversion de format, retourne une nouvelle image
 *     - NkImage::Resize(...)      redimensionnement, retourne une nouvelle image
 *     - NkImage::Crop(...)        sous-région, retourne une nouvelle image
 *     - NkImage::Copy()           clone profond, retourne une nouvelle image
 *     - NkImage::CopyAs(fmt)      clone + conversion, retourne une nouvelle image
 *     - NkImage::ConvertToTexture(...)  tone-mapping HDR→LDR
 *     L'appelant est responsable d'appeler img->Free() sur le résultat.
 *
 *  2. API INSTANCE  → retourne `bool`  (opère sur *this, aucune allocation visible)
 *     - img.Create(...)           crée/réinitialise *this
 *     - img.Load(path)            charge un fichier dans *this
 *     - img.LoadFromMemory(...)   charge depuis un buffer mémoire dans *this
 *     - img.Copy(src, x, y, area, clip)  copie une région de src dans *this
 *     - img.CopyTo(dst)           copie *this dans une image existante
 *     Ces méthodes libèrent automatiquement l'ancien buffer avant de remplir *this.
 *
 *  RÈGLE DE MÉMOIRE :
 *    Les buffers alloués via l'API statique DOIVENT être libérés avec img->Free().
 *    Les buffers encodés (EncodePNG, EncodeJPEG, …) DOIVENT être libérés avec
 *    nkentseu::memory::NkFree(ptr).  Ne jamais utiliser std::free / delete[] :
 *    l'allocateur custom NKMemory n'est pas compatible avec le heap CRT standard
 *    (crash c0000374 sur Windows en cas de mélange).
 *
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Proprietary - Free to use and modify
 */

#include "NkImageExport.h"
#include "NKMath/NKMath.h"      // math::NkColor, math::NkIntRect
#include "NKStream/NKIResource.h" // interface ressource CPU commune
#include <cstdio>

namespace nkentseu {

    // ─────────────────────────────────────────────────────────────────────────────
    //  Formats pixel supportés
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @enum NkImagePixelFormat
     * Décrit le layout et la profondeur de chaque pixel stocké en mémoire CPU.
     *
     * LDR (Low Dynamic Range, entiers 8 bits par canal) :
     *   NK_GRAY8     — 1 octet/pixel  : luminance
     *   NK_GRAY_A16  — 2 octets/pixel : luminance + alpha
     *   NK_RGB24     — 3 octets/pixel : rouge, vert, bleu
     *   NK_RGBA32    — 4 octets/pixel : rouge, vert, bleu, alpha
     *
     * HDR (High Dynamic Range, flottants 32 bits par canal) :
     *   NK_RGB96F    — 12 octets/pixel : RGB flottant
     *   NK_RGBA128F  — 16 octets/pixel : RGBA flottant
     */
    enum class NkImagePixelFormat : uint8 {
        NK_UNKNOWN  = 0,
        NK_GRAY8    = 1,
        NK_GRAY_A16 = 2,
        NK_RGB24    = 3,
        NK_RGBA32   = 4,
        NK_RGBA128F = 5,
        NK_RGB96F   = 6,
    };

    /** Retourne le nombre de canaux logiques pour un format donné. */
    NKIMG_INLINE constexpr int32 ChannelsOf(NkImagePixelFormat f) noexcept {
        switch (f) {
            case NkImagePixelFormat::NK_GRAY8:    return 1;
            case NkImagePixelFormat::NK_GRAY_A16: return 2;
            case NkImagePixelFormat::NK_RGB24:    return 3;
            case NkImagePixelFormat::NK_RGBA32:   return 4;
            case NkImagePixelFormat::NK_RGBA128F: return 4;
            case NkImagePixelFormat::NK_RGB96F:   return 3;
            default:                              return 0;
        }
    }

    /** Retourne le nombre d'octets occupés par un pixel complet. */
    NKIMG_INLINE constexpr int32 BytesPerPixelOf(NkImagePixelFormat f) noexcept {
        switch (f) {
            case NkImagePixelFormat::NK_GRAY8:    return 1;
            case NkImagePixelFormat::NK_GRAY_A16: return 2;
            case NkImagePixelFormat::NK_RGB24:    return 3;
            case NkImagePixelFormat::NK_RGBA32:   return 4;
            case NkImagePixelFormat::NK_RGBA128F: return 16;
            case NkImagePixelFormat::NK_RGB96F:   return 12;
            default:                              return 0;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Formats de fichier supportés
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @enum NkImageFormat
     * Identifie le format de conteneur/compression d'un fichier image.
     * Détecté automatiquement depuis la signature binaire (magic bytes)
     * dans NkImage::DetectFormat().
     */
    enum class NkImageFormat : uint8 {
        NK_UNKNOWN = 0,
        NK_PNG,   ///< Portable Network Graphics (.png)
        NK_JPEG,  ///< JPEG (.jpg, .jpeg)
        NK_BMP,   ///< Windows Bitmap (.bmp)
        NK_TGA,   ///< Truevision TGA (.tga)
        NK_HDR,   ///< Radiance HDR (.hdr) — RGB96F
        NK_PPM,   ///< Portable Pixmap (.ppm)
        NK_PGM,   ///< Portable Graymap (.pgm)
        NK_PBM,   ///< Portable Bitmap (.pbm)
        NK_QOI,   ///< Quite OK Image (.qoi)
        NK_GIF,   ///< Graphics Interchange Format (.gif)
        NK_ICO,   ///< Windows Icon (.ico)
        NK_SVG,   ///< Scalable Vector Graphics (.svg) — rastérisé via NkSVGCodec
        NK_EXR,   ///< OpenEXR (.exr) — RGB96F / RGBA128F via NkEXRCodec
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  Filtres de redimensionnement
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @enum NkResizeFilter
     * Algorithme d'interpolation utilisé par NkImage::Resize().
     *
     *   NK_NEAREST   — plus proche voisin (rapide, effet pixelisé)
     *   NK_BILINEAR  — bilinéaire (bon compromis qualité/vitesse, défaut)
     *   NK_BICUBIC   — bicubique (meilleure qualité, plus lent)
     *   NK_LANCZOS3  — Lanczos-3 (qualité maximale, lent)
     */
    enum class NkResizeFilter : uint8 {
        NK_NEAREST,
        NK_BILINEAR,
        NK_BICUBIC,
        NK_LANCZOS3,
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkImage
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @class NkImage
     *
     * Conteneur d'image CPU avec gestion autonome de la mémoire.
     *
     * OWNERSHIP :
     *   Par défaut (mOwning=true), NkImage possède son buffer pixel et le libère
     *   dans le destructeur.  Les images créées via Wrap() sont non-owning
     *   (mOwning=false) : elles n'appellent jamais free sur les pixels.
     *
     * COPIE :
     *   La copie par valeur est désactivée (= delete) pour éviter les doubles-free
     *   accidentels.  Utiliser Copy() (deep clone) ou le move constructor.
     *
     * STRIDE :
     *   Le stride (bytes par ligne) est aligné sur 4 octets : stride = (w*bpp+3)&~3.
     *   Utiliser RowPtr(y) pour accéder à la ligne y de façon portable.
     */
    class NKENTSEU_IMAGE_API NkImage : public NKIResource {
        public:

            // ── Cycle de vie ──────────────────────────────────────────────────────────

            /** Constructeur par défaut : image invalide (pixels=null, w=h=0). */
            NkImage() noexcept = default;

            /**
             * Destructeur : libère mPixels si mOwning==true.
             * N'appelle PAS Free() (qui libérerait aussi le struct lui-même via nkFree).
             * Virtuel (hérité de NKIResource) — n'altère pas le pattern Alloc/Free
             * car Alloc/Wrap utilisent placement new (le vptr est donc initialisé).
             */
            ~NkImage() noexcept override;

            /** Copie désactivée — utiliser Copy() pour un clone explicite. */
            NkImage(const NkImage&)            = delete;
            NkImage& operator=(const NkImage&) = delete;

            /**
             * Move constructor : transfère le buffer sans copie ni allocation.
             * Après le move, `other` est dans un état valide mais vide (IsValid()==false).
             */
            NkImage(NkImage&& other) noexcept;

            /**
             * Move assignment : libère l'éventuel buffer existant puis transfère.
             * Self-assignment sécurisé (this==&other est testé).
             */
            NkImage& operator=(NkImage&& other) noexcept;

            // ── API INSTANCE : Create / Load — opèrent sur *this, retournent bool ────
            //    Ces méthodes libèrent automatiquement le buffer précédent avant
            //    d'initialiser *this.  Elles sont pensées pour une utilisation en valeur
            //    (NkImage img; img.Load("foo.png")).

            /**
             * Crée une image de dimensions (width × height) en mémoire, remplie avec
             * la couleur `color` (composantes RGBA dans math::NkColor).
             *
             * @param width            Largeur en pixels (> 0).
             * @param height           Hauteur en pixels (> 0).
             * @param color            Couleur de remplissage (RGBA).
             * @param desiredChannels  Nombre de canaux souhaité (1–4, défaut 4 → RGBA32).
             * @return true si l'allocation a réussi et *this est valide.
             */
            bool Create(uint32 width, uint32 height,
                        math::NkColor color,
                        int32 desiredChannels = 4) noexcept;

            // ── NKIResource : surcharges « minces » (signatures EXACTES de
            //    l'interface). Elles délèguent aux versions riches ci-dessous.
            //    Important : les versions riches n'ont PLUS de paramètre par
            //    défaut, afin que LoadFromMemory(data, size) (2 args) résolve
            //    sans ambiguïté vers l'override d'interface et non vers la
            //    version à 3 arguments (sinon name-hiding + classe abstraite).

            /** [NKIResource] Charge un fichier image (canaux natifs). Délègue à Load(path, 0). */
            bool LoadFromFile(const char* path) override { return Load(path, 0); }

            /** [NKIResource] Charge depuis un buffer mémoire (canaux natifs). */
            bool LoadFromMemory(const void* data, usize size) override {
                return LoadFromMemory(data, size, 0);
            }

            /** [NKIResource] Charge depuis un flux NkStream (lit tout le flux puis décode). */
            bool LoadFromStream(NkStream& stream) override;

            /**
             * Charge une image depuis un fichier sur disque.
             * Sur Android, tente automatiquement l'AAssetManager si fopen échoue.
             *
             * @param path             Chemin UTF-8 vers le fichier image.
             * @param desiredChannels  0 = canaux natifs du fichier, 1–4 = conversion forcée.
             * @return true si le chargement et le décodage ont réussi.
             */
            bool Load(const char* path, int32 desiredChannels = 0) noexcept;

            /**
             * Surcharge confort uint8* à 2 arguments (canaux natifs).
             * Évite un static_cast côté appelant tout en conservant un chemin direct.
             */
            bool LoadFromMemory(const uint8* data, usize size) noexcept {
                return LoadFromMemory(data, size, 0);
            }

            /**
             * Charge une image depuis un buffer mémoire brut (void*), canaux explicites.
             * Surcharge pratique pour les APIs C qui manipulent void* (ex: fread).
             * Délègue vers la surcharge const uint8* après un static_cast sécurisé.
             *
             * @param data             Pointeur vers les données encodées (PNG, JPEG, …).
             * @param size             Taille du buffer en octets.
             * @param desiredChannels  0 = canaux natifs, 1–4 = conversion forcée.
             * @return true si le chargement et le décodage ont réussi.
             */
            bool LoadFromMemory(const void* data, usize size,
                                int32 desiredChannels) noexcept;

            /**
             * Charge une image depuis un buffer mémoire typé uint8*, canaux explicites.
             * C'est l'implémentation réelle ; la surcharge void* délègue ici.
             *
             * @param data             Pointeur vers les données encodées.
             * @param size             Taille du buffer en octets (>= 4).
             * @param desiredChannels  0 = canaux natifs, 1–4 = conversion forcée.
             * @return true si le chargement et le décodage ont réussi.
             */
            bool LoadFromMemory(const uint8* data, usize size,
                                int32 desiredChannels) noexcept;

            // ── API STATIQUE : fabriques retournant NkImage* ──────────────────────────
            //    L'appelant possède le résultat et DOIT appeler img->Free().

            /**
             * Crée une image allouée sur le heap, remplie avec une couleur RGBA packed.
             *
             * @param width            Largeur en pixels.
             * @param height           Hauteur en pixels.
             * @param desiredChannels  0 ou 4 → RGBA32, 1 → GRAY8, 2 → GRAY_A16, 3 → RGB24.
             * @param color            Couleur RGBA packed big-endian : 0xRRGGBBAA.
             *                         0x00000000 = transparent black (buffer zero-fill).
             * @return Nouvelle image owning, ou nullptr en cas d'échec.
             */
            static NkImage* Create(uint32 width, uint32 height,
                                    int32 desiredChannels = 0,
                                    uint32 color = 0) noexcept;

            /**
             * Surcharge de Create() avec format pixel explicite.
             * Plus précise que la version par canaux quand on connaît le format exact.
             *
             * @param fmt   Format pixel cible.
             * @param color Couleur RGBA packed big-endian (0xRRGGBBAA).
             */
            static NkImage* Create(uint32 width, uint32 height,
                                    NkImagePixelFormat fmt,
                                    uint32 color = 0) noexcept;

            // ── NKIResource : surcharges Save « minces » (signatures exactes) ───────────
            //    Délèguent aux variantes riches. SaveToMemory encode en PNG par défaut
            //    (lossless universel) ; `out` est alloué via NkAlloc → NkFree (voir
            //    EncodePNG). Le format de SaveToFile/Stream est PNG si l'extension
            //    n'est pas explicite (SaveToStream) ou déduit de l'extension (SaveToFile).

            /** [NKIResource] Sauvegarde via l'extension du chemin (qualité JPEG 90 par défaut). */
            bool SaveToFile(const char* path) const override { return Save(path, 90); }

            /** [NKIResource] Encode en PNG dans un buffer mémoire (out à libérer avec NkFree). */
            bool SaveToMemory(uint8*& out, usize& size) const override { return EncodePNG(out, size); }

            /** [NKIResource] Encode en PNG et écrit le résultat dans le flux. */
            bool SaveToStream(NkStream& stream) const override;

            // ── Sauvegarde sur disque ──────────────────────────────────────────────────

            /**
             * Sauvegarde l'image dans le format déduit de l'extension du chemin.
             * Extensions reconnues : png, jpg/jpeg, bmp, tga, ppm/pgm, hdr, qoi.
             *
             * @param path    Chemin de sortie (l'extension détermine le format).
             * @param quality Qualité JPEG [1–100], ignoré pour les autres formats.
             * @return true si l'écriture a réussi.
             */
            bool Save    (const char* path, int32 quality = 90) const noexcept;
            bool SavePNG (const char* path)                     const noexcept;
            bool SaveJPEG(const char* path, int32 quality = 90) const noexcept;
            bool SaveBMP (const char* path)                     const noexcept;
            bool SaveTGA (const char* path)                     const noexcept;
            bool SavePPM (const char* path)                     const noexcept;
            bool SaveHDR (const char* path)                     const noexcept;
            bool SaveQOI (const char* path)                     const noexcept;
            bool SaveGIF (const char* path)                     const noexcept; ///< Non implémenté, retourne false.
            bool SaveWebP(const char* path, bool lossless = true, int32 quality = 90) const noexcept; ///< Non implémenté.
            bool SaveSVG (const char* path)                     const noexcept; ///< Non implémenté.

            // ── Encodage en mémoire ────────────────────────────────────────────────────
            //
            //    `out` est alloué via NkAlloc (allocateur NKMemory).
            //    L'appelant DOIT libérer avec : nkentseu::memory::NkFree(out)
            //    NE PAS utiliser std::free / delete[] — incompatible avec NkAlloc
            //    et provoque une corruption du heap sur Windows (exception c0000374).

            bool EncodePNG (uint8*& out, usize& size)                   const noexcept;
            bool EncodeJPEG(uint8*& out, usize& size, int32 quality=90) const noexcept;
            bool EncodeBMP (uint8*& out, usize& size)                   const noexcept;
            bool EncodeTGA (uint8*& out, usize& size)                   const noexcept;
            bool EncodeQOI (uint8*& out, usize& size)                   const noexcept;

            // ── Manipulation in-place ──────────────────────────────────────────────────

            /** Retourne l'image verticalement (flip autour de l'axe horizontal). */
            void FlipVertical()    noexcept;

            /** Retourne l'image horizontalement (flip autour de l'axe vertical). */
            void FlipHorizontal()  noexcept;

            /**
             * Pré-multiplie les canaux RGB par l'alpha.
             * Opération destructrice : ne s'applique qu'aux images NK_RGBA32.
             * Utile avant l'upload GPU pour le blending correct.
             */
            void PremultiplyAlpha() noexcept;

            /**
             * Convertit l'image vers un nouveau format pixel.
             * Retourne une nouvelle image allouée (API statique, l'appelant possède le résultat).
             * Si newFmt == mFormat, fait un clone pur.
             * Conversions HDR↔LDR supportées via troncature/normalisation.
             */
            NkImage* Convert(NkImagePixelFormat newFmt) const noexcept;

            /**
             * Redimensionne l'image à (nw × nh) pixels.
             * Retourne une nouvelle image allouée.
             *
             * @param f  Filtre d'interpolation (défaut : NK_BILINEAR).
             */
            NkImage* Resize(int32 nw, int32 nh,
                            NkResizeFilter f = NkResizeFilter::NK_BILINEAR) const noexcept;

            /**
             * Copie (blit) l'image `src` entière dans *this à la position (dstX, dstY).
             * Les deux images doivent avoir le même format pixel.
             * Les débordements sont clippés silencieusement.
             *
             * @param src   Image source (image entière).
             * @param dstX  Colonne de destination dans *this.
             * @param dstY  Ligne de destination dans *this.
             */
            void Blit(const NkImage& src, int32 dstX, int32 dstY) noexcept;

            /**
             * Copie (blit) une sous-région de `src` dans une sous-région de *this,
             * avec redimensionnement optionnel par interpolation bilinéaire.
             *
             * Cette méthode est la version la plus générale du blit :
             *
             *   - Si srcRegion est vide (width==0 && height==0), toute l'image src est utilisée.
             *   - Si dstRegion est vide (width==0 && height==0), les pixels sont copiés
             *     à partir de (dstRegion.left, dstRegion.top) sans redimensionnement
             *     (équivalent à Blit avec région source).
             *   - Si srcRegion et dstRegion ont des dimensions différentes, la région
             *     source est redimensionnée par interpolation bilinéaire pour s'adapter
             *     exactement à la région de destination.  Cela permet de faire du scaling
             *     ciblé sur une zone précise sans créer d'image intermédiaire.
             *
             * Pré-conditions :
             *   - *this et src doivent être valides (IsValid()==true).
             *   - *this et src doivent avoir le même format pixel.
             *
             * Comportement des débordements :
             *   - Les régions source et destination sont clippées aux bornes de leurs
             *     images respectives avant toute opération.
             *   - Si après clipping il ne reste rien à copier, la méthode retourne true
             *     sans effectuer de travail (pas une erreur).
             *
             * Exemple d'utilisation :
             * @code
             *   // Colle le sprite src[10,20,64,64] dans atlas[100,200,128,128]
             *   // en l'étirant (scale 1:2 sur chaque axe) :
             *   atlas.BlitRegion(sprite,
             *       math::NkIntRect{10, 20, 64, 64},   // srcRegion
             *       math::NkIntRect{100, 200, 128, 128} // dstRegion
             *   );
             * @endcode
             *
             * @param src        Image source.
             * @param srcRegion  Sous-région rectangulaire à lire dans src.
             *                   Si width==0 && height==0 : toute l'image src.
             * @param dstRegion  Sous-région rectangulaire de destination dans *this.
             *                   Si width==0 && height==0 : copie sans redimensionnement
             *                   à partir de (dstRegion.left, dstRegion.top).
             * @return true si l'opération s'est terminée sans erreur.
             *         false si les images sont invalides ou si les formats diffèrent.
             */
            bool BlitRegion(const NkImage& src,
                            const math::NkIntRect& srcRegion,
                            const math::NkIntRect& dstRegion) noexcept;

            bool BlitRegion(const NkImage& src,
                            const math::NkIntRect& srcRegion,
                            const math::NkIntRect& dstRegion,
                            NkResizeFilter filter) noexcept;

            /**
             * Retourne une sous-région de l'image comme nouvelle image allouée.
             * Les coordonnées doivent être entièrement dans les bornes.
             *
             * @param x, y  Coin supérieur gauche de la région.
             * @param w, h  Dimensions de la région.
             * @return Nouvelle image allouée, ou nullptr si hors bornes.
             */
            NkImage* Crop(int32 x, int32 y, int32 w, int32 h) const noexcept;

            // ── Copies ────────────────────────────────────────────────────────────────

            /**
             * Clone profond : retourne une nouvelle image avec les mêmes pixels/format.
             * API statique — l'appelant possède le résultat (appeler ->Free()).
             *
             * @return Nouvelle image owning, ou nullptr si *this est invalide.
             */
            NkImage* Copy() const noexcept;

            /**
             * Copie une région de `src` dans *this à la position (dstX, dstY).
             * API instance — opère sur *this, ne fait aucune allocation.
             *
             * Pré-conditions :
             *   - *this et src doivent être valides (IsValid()==true).
             *   - *this et src doivent avoir le même format pixel.
             *   - *this doit être suffisamment grand pour accueillir la région.
             *
             * @param src   Image source.
             * @param dstX  Colonne de destination dans *this.
             * @param dstY  Ligne de destination dans *this.
             * @param area  Région rectangulaire à copier dans src (NkIntRect).
             *              Si area.width==0 && area.height==0, copie l'image entière.
             * @param clip  true  → les débordements sont clippés silencieusement.
             *              false → retourne false si quoi que ce soit sort des bornes.
             * @return true si la copie a réussi (ou n'avait rien à faire après clip).
             */
            bool Copy(const NkImage& src,
                    int32 dstX, int32 dstY,
                    const math::NkIntRect& area,
                    bool clip = true) noexcept;

            /**
             * Copie *this dans une image existante `dst` sans allocation.
             * Exige : même format ET mêmes dimensions.  Ne modifie pas dst en cas d'échec.
             *
             * @return true si la copie a réussi.
             */
            bool CopyTo(NkImage& dst) const noexcept;

            /**
             * Retourne un clone éventuellement converti vers `fmt`.
             * Si fmt == mFormat, équivalent à Copy() (pas de conversion inutile).
             * API statique — l'appelant possède le résultat (appeler ->Free()).
             *
             * @return Nouvelle image owning, ou nullptr si *this invalide ou fmt inconnu.
             */
            NkImage* CopyAs(NkImagePixelFormat fmt) const noexcept;

            // ── Accès aux métadonnées ─────────────────────────────────────────────────

            NKIMG_INLINE uint8*       Pixels()       noexcept       { return mPixels; }
            NKIMG_INLINE const uint8* Pixels() const noexcept       { return mPixels; }
            NKIMG_INLINE int32        Width()  const noexcept       { return mWidth;  }
            NKIMG_INLINE int32        Height() const noexcept       { return mHeight; }
            NKIMG_INLINE int32        Channels() const noexcept     { return ChannelsOf(mFormat); }
            NKIMG_INLINE int32        BytesPP()   const noexcept    { return BytesPerPixelOf(mFormat); }
            NKIMG_INLINE int32        Stride()    const noexcept    { return mStride; }
            NKIMG_INLINE NkImagePixelFormat Format()       const noexcept { return mFormat;  }
            NKIMG_INLINE NkImageFormat      SourceFormat() const noexcept { return mSrcFmt;  }

            /** Retourne true si l'image contient des pixels valides. [NKIResource] */
            NKIMG_INLINE bool IsValid() const noexcept override {
                return mPixels && mWidth > 0 && mHeight > 0;
            }

            /** Retourne true si le format pixel est flottant (HDR). */
            NKIMG_INLINE bool IsHDR() const noexcept {
                return mFormat == NkImagePixelFormat::NK_RGBA128F
                    || mFormat == NkImagePixelFormat::NK_RGB96F;
            }

            /** Taille totale du buffer pixel en octets (stride * height). */
            NKIMG_INLINE usize TotalBytes() const noexcept {
                return usize(mStride) * mHeight;
            }

            /** Pointeur vers le début de la ligne y (tient compte du stride). */
            NKIMG_INLINE uint8* RowPtr(int32 y) noexcept {
                return mPixels + usize(y) * mStride;
            }
            NKIMG_INLINE const uint8* RowPtr(int32 y) const noexcept {
                return mPixels + usize(y) * mStride;
            }

            // ── Gestion mémoire ───────────────────────────────────────────────────────

            /**
             * Libère les pixels (si owning) ET libère le struct NkImage lui-même via nkFree.
             * À utiliser UNIQUEMENT sur les images créées via les fabriques statiques
             * (Alloc, Wrap, Create statique, Copy, CopyAs, Convert, Resize, Crop, …).
             * Ne jamais appeler Free() sur une image allouée sur la stack.
             */
            void Free() noexcept;

            /**
             * [NKIResource] Libère les pixels (si owning) et remet *this dans l'état
             * « image vide » SANS libérer le struct (contrairement à Free()).
             * Sûr sur une instance pile comme heap : c'est l'opération de « déchargement »
             * réutilisable (un Load ultérieur réinitialise *this proprement).
             */
            void Unload() noexcept override;

            // ── Fabriques bas niveau (usage interne / codecs) ─────────────────────────

            /**
             * Alloue une image vide (pixels zeroed) de dimensions (w × h) et de format fmt.
             * Utilisé par les codecs pour construire leur résultat.
             * L'appelant possède le résultat et DOIT appeler ->Free().
             */
            static NkImage* Alloc(int32 w, int32 h, NkImagePixelFormat fmt) noexcept;

            /**
             * Crée une vue non-owning sur un buffer pixel externe.
             * Le buffer n'est PAS libéré par le destructeur ni par Free().
             * L'appelant reste responsable de la durée de vie du buffer.
             *
             * @param pixels  Pointeur vers les données pixel.
             * @param w, h    Dimensions de l'image.
             * @param fmt     Format pixel.
             * @param stride  Stride en octets (0 = calculé automatiquement w*bpp).
             */
            static NkImage* Wrap(uint8* pixels, int32 w, int32 h,
                                NkImagePixelFormat fmt, int32 stride = 0) noexcept;

            /**
             * Tone-mapping d'une image HDR (RGB96F/RGBA128F) vers RGBA32 LDR.
             * Applique : pixel_ldr = pow(clamp(pixel_hdr * exposure), 1/gamma) * 255.
             *
             * @param hdrImage  Image source HDR (doit être IsHDR()==true).
             * @param exposure  Facteur d'exposition (défaut 1.0).
             * @param gamma     Gamma de correction (défaut 2.2, sRGB standard).
             * @return Nouvelle image RGBA32, ou nullptr si hdrImage invalide.
             */
            static NkImage* ConvertToTexture(const NkImage& hdrImage,
                                            float exposure = 1.0f,
                                            float gamma    = 2.2f) noexcept;

        private:
            // ── Données membres ───────────────────────────────────────────────────────

            uint8*             mPixels = nullptr;                        ///< Buffer pixel.
            int32              mWidth  = 0;                              ///< Largeur en pixels.
            int32              mHeight = 0;                              ///< Hauteur en pixels.
            int32              mStride = 0;                              ///< Bytes par ligne (aligné 4).
            NkImagePixelFormat mFormat = NkImagePixelFormat::NK_UNKNOWN; ///< Format pixel.
            NkImageFormat      mSrcFmt = NkImageFormat::NK_UNKNOWN;      ///< Format du fichier source.
            bool               mOwning = true;  ///< Si false, le destructeur ne libère pas mPixels.

            // ── Helpers privés ────────────────────────────────────────────────────────

            /**
             * Détecte le format du fichier image depuis les magic bytes.
             * Inspecte les premiers octets du buffer pour identifier PNG, JPEG, BMP, etc.
             * Retourne NK_UNKNOWN si non reconnu.
             */
            static NkImageFormat DetectFormat(const uint8* data, usize size) noexcept;

            /**
             * Dispatch vers le codec approprié selon le format détecté.
             * Si `desired` > 0 et différent du format natif, convertit après décodage.
             */
            static NkImage* Dispatch(const uint8* data, usize size,
                                    int32 desired, NkImageFormat fmt) noexcept;

            /**
             * Conversion bas niveau de canaux pour des données pixel brutes.
             * Gère toutes les combinaisons (1→4, 4→1, 3→4, etc.) via luminance perceptuelle.
             * Alloue le buffer de destination (aligné 4 octets par ligne).
             *
             * @param src       Buffer source.
             * @param w, h      Dimensions de l'image.
             * @param srcCh     Nombre de canaux sources.
             * @param dstCh     Nombre de canaux cibles.
             * @param srcStride Stride source en octets.
             * @return Buffer destination alloué via nkCalloc, ou nullptr en cas d'échec.
             */
            static uint8* ConvertChannels(const uint8* src, int32 w, int32 h,
                                        int32 srcCh, int32 dstCh,
                                        int32 srcStride) noexcept;

            /**
             * Helper interne partagé par Load() et LoadFromMemory() (API instance).
             * Charge depuis un buffer mémoire déjà lu, stocke le résultat dans *this.
             * Libère l'ancien buffer avant de remplir *this.
             */
            bool LoadFromMemoryImpl(const uint8* data, usize size,
                                    int32 desiredChannels) noexcept;

            /**
             * Noyau interne de BlitRegion : copie une sous-région de src (déjà validée
             * et clippée) dans *this sans redimensionnement.
             * Appelé par BlitRegion quand les dimensions src et dst sont identiques.
             *
             * @param src    Image source.
             * @param sx,sy  Coin supérieur gauche dans src (déjà clippé, >= 0).
             * @param sw,sh  Dimensions à copier (déjà clippées, > 0).
             * @param dx,dy  Coin supérieur gauche dans *this (déjà clippé, >= 0).
             */
            void BlitRegionDirect(const NkImage& src,
                                  int32 sx, int32 sy, int32 sw, int32 sh,
                                  int32 dx, int32 dy) noexcept;

            /**
             * Noyau interne de BlitRegion : copie une sous-région de src dans une
             * sous-région de *this avec redimensionnement bilinéaire.
             * Appelé par BlitRegion quand les dimensions src et dst diffèrent.
             *
             * @param src         Image source.
             * @param sx,sy,sw,sh Région source (clippée, > 0).
             * @param dx,dy,dw,dh Région de destination (clippée, > 0).
             */
            void BlitRegionScaled(const NkImage& src,
                                  int32 sx, int32 sy, int32 sw, int32 sh,
                                  int32 dx, int32 dy, int32 dw, int32 dh) noexcept;

            // ── Accès ami pour les codecs ─────────────────────────────────────────────
            // Les codecs accèdent aux membres privés (mPixels, mFormat, etc.)
            // pour construire leur résultat via Alloc() et affecter mSrcFmt.

            friend class NkPNGCodec;
            friend class NkJPEGCodec;
            friend class NkBMPCodec;
            friend class NkTGACodec;
            friend class NkHDRCodec;
            friend class NkPPMCodec;
            friend class NkQOICodec;
            friend class NkGIFCodec;
            friend class NkICOCodec;
            friend class NkEXRCodec;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkImageStream
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @class NkImageStream
     *
     * Buffer de lecture/écriture binaire utilisé par les codecs d'images.
     *
     * MODE LECTURE :
     *   Construit avec (data, size), expose Read*, Skip, Seek.
     *   Les lectures hors bornes positionnent mError=true et retournent 0/zéro.
     *
     * MODE ÉCRITURE :
     *   Construit sans arguments, expose Write*.
     *   Croît dynamiquement via nkRealloc (doublage de capacité).
     *   Appeler TakeBuffer() pour récupérer le buffer final.
     *
     * ENDIANNESS :
     *   - BE (Big Endian)   : PNG, JPEG (réseau)
     *   - LE (Little Endian): BMP, TGA, QOI, EXR
     *
     * MÉMOIRE :
     *   Le buffer d'écriture est alloué via NkAlloc.
     *   Après TakeBuffer(), l'appelant est responsable de le libérer avec NkFree.
     */
    class NKENTSEU_IMAGE_API NkImageStream {
        public:
            /** Constructeur lecture : pointe sur un buffer existant (non owning). */
            NkImageStream(const uint8* data, usize size) noexcept
                : mRdData(data), mRdSize(size) {}

            /** Constructeur écriture : buffer dynamique initialement vide. */
            NkImageStream() noexcept {}

            ~NkImageStream() noexcept {}

            // ── Lecture ───────────────────────────────────────────────────────────────

            uint8  ReadU8()    noexcept; ///< Lit 1 octet non signé.
            uint16 ReadU16BE() noexcept; ///< Lit 2 octets big-endian non signés.
            uint16 ReadU16LE() noexcept; ///< Lit 2 octets little-endian non signés.
            uint32 ReadU32BE() noexcept; ///< Lit 4 octets big-endian non signés.
            uint32 ReadU32LE() noexcept; ///< Lit 4 octets little-endian non signés.
            int16  ReadI16BE() noexcept; ///< Lit 2 octets big-endian signés.
            int32  ReadI32LE() noexcept; ///< Lit 4 octets little-endian signés.

            /** Lit `n` octets dans `dst` (peut être nullptr pour avancer le curseur). */
            usize ReadBytes(uint8* dst, usize n) noexcept;

            /** Avance le curseur de `n` octets. */
            void Skip(usize n) noexcept;

            /** Positionne le curseur à l'offset `pos` depuis le début. */
            void Seek(usize pos) noexcept;

            // ── Accesseurs lecture ────────────────────────────────────────────────────

            usize Tell()           const noexcept { return mRdPos;              }
            usize Size()           const noexcept { return mRdSize;             }
            bool  IsEOF()          const noexcept { return mRdPos >= mRdSize;   }
            bool  HasBytes(usize n) const noexcept { return mRdPos + n <= mRdSize; }
            bool  HasError()       const noexcept { return mError;              }

            /** Pointeur vers la position de lecture courante. */
            const uint8* Ptr() const noexcept { return mRdData + mRdPos; }

            // ── Écriture ──────────────────────────────────────────────────────────────

            bool WriteU8   (uint8  v) noexcept; ///< Écrit 1 octet.
            bool WriteU16BE(uint16 v) noexcept; ///< Écrit 2 octets big-endian.
            bool WriteU16LE(uint16 v) noexcept; ///< Écrit 2 octets little-endian.
            bool WriteU32BE(uint32 v) noexcept; ///< Écrit 4 octets big-endian.
            bool WriteU32LE(uint32 v) noexcept; ///< Écrit 4 octets little-endian.
            bool WriteI32LE(int32  v) noexcept; ///< Écrit 4 octets little-endian signés.
            bool WriteBytes(const uint8* src, usize n) noexcept; ///< Écrit n octets.

            /**
             * Transfère la propriété du buffer d'écriture vers l'appelant.
             * Après l'appel, le stream ne possède plus le buffer (mWrBuf=null).
             * L'appelant DOIT libérer outData avec NkFree.
             *
             * @param outData  Reçoit le pointeur vers les données écrites.
             * @param outSize  Reçoit la taille en octets.
             * @return true si le buffer était non-null.
             */
            bool TakeBuffer(uint8*& outData, usize& outSize) noexcept {
                outData  = mWrBuf;  outSize = mWrSize;
                mWrBuf   = nullptr; mWrSize = 0; mWrCap = 0;
                return outData != nullptr;
            }

            /** Taille actuelle du buffer d'écriture en octets. */
            usize WriteSize() const noexcept { return mWrSize; }

        private:
            // Lecture
            const uint8* mRdData = nullptr; ///< Buffer source (non owning).
            usize        mRdSize = 0;       ///< Taille du buffer source.
            usize        mRdPos  = 0;       ///< Curseur de lecture.
            bool         mError  = false;   ///< true si une lecture hors bornes s'est produite.

            // Écriture
            uint8* mWrBuf  = nullptr; ///< Buffer dynamique (owning, alloué via NkAlloc).
            usize  mWrSize = 0;       ///< Octets effectivement écrits.
            usize  mWrCap  = 0;       ///< Capacité totale du buffer.

            /**
             * Croît le buffer d'écriture pour accueillir `needed` octets supplémentaires.
             * Stratégie : doublage de capacité à partir de 4096 octets.
             * @return true si le realloc a réussi.
             */
            bool Grow(usize needed) noexcept;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkDeflate — inflate / deflate (adapté de stb_image v2.16, public domain)
    // ─────────────────────────────────────────────────────────────────────────────

    /**
     * @class NkDeflate
     *
     * Implémentation inflate (décompression DEFLATE RFC 1951 + zlib RFC 1950)
     * et deflate minimal (compression sans compression = stored blocks) pour
     * permettre l'écriture de PNG valides.
     *
     * ─── CORRECTNESS INFLATE (LSB-first, stb_image exact) ────────────────────
     *
     *  DEFLATE est LSB-first : le premier bit du premier code est dans le bit 0
     *  du premier octet.  L'accumulation des bits se fait de gauche à droite
     *  dans un registre 32 bits :
     *
     *    bits |= byte << nbits    (fill_bits : accumule par le MSB du registre)
     *    v = bits & mask          (receive    : extrait par le LSB du registre)
     *    bits >>= n               (consume    : décale vers le bas)
     *
     *  La fast table Huffman est indexée directement par les `FAST` bits LSB
     *  du registre (qui correspondent à `FAST` bits bit-reversed dans l'espace
     *  des codes Huffman canoniques).
     *
     * ─── FIX CRITIQUE : en-tête zlib (CMF/FLG) ──────────────────────────────
     *
     *  zlib RFC 1950 : CMF = in[0], FLG = in[1].
     *  FDICT = bit 5 de FLG (in[1]), PAS bit 5 de CMF (in[0]).
     *
     *  CMF=0x78 (le plus courant pour les PNG : niveau 6 ou 9) a TOUJOURS
     *  le bit 5 à 1 (CINFO=7 dans le nibble haut).  L'ancienne version qui
     *  testait if(in[0] & 0x20) rejetait donc TOUS les PNG standard :
     *    0x78 0x9C  (compression défaut)
     *    0x78 0xDA  (compression maximum)
     *    0x78 0x5E  (compression rapide)
     *    0x78 0x01  (sans compression)
     *
     *  Correction : if(flg & 0x20) où flg = in[1].
     */
    class NKENTSEU_IMAGE_API NkDeflate {
        public:

            /**
             * Décompresse un flux zlib RFC 1950 (avec en-tête CMF/FLG et checksum Adler-32).
             * Utilisé pour décoder les blocs IDAT des PNG.
             *
             * @param in      Buffer compressé.
             * @param inSz    Taille du buffer compressé.
             * @param out     Buffer de sortie pré-alloué.
             * @param outCap  Capacité du buffer de sortie.
             * @param written Nombre d'octets effectivement écrits dans out.
             * @return true si la décompression a réussi sans erreur.
             */
            static bool Decompress(const uint8* in,  usize inSz,
                                    uint8*       out, usize outCap,
                                    usize& written) noexcept;

            /**
             * Décompresse un flux DEFLATE brut RFC 1951 (sans en-tête zlib ni checksum).
             * Utilisé pour les formats qui embarquent du DEFLATE brut (ex: ZIP, gzip).
             */
            static bool DecompressRaw(const uint8* in,  usize inSz,
                                    uint8*       out, usize outCap,
                                    usize& written) noexcept;

            /**
             * Compresse les données en zlib RFC 1950 avec stored blocks (BTYPE=00).
             * Produit un flux zlib valide lisible par tout décompresseur standard,
             * mais sans compression réelle (ratio 1:1 + overhead ~6 octets/bloc).
             * Suffisant pour écrire des PNG valides avec NkPNGCodec.
             *
             * @param in      Données à "compresser".
             * @param inSz    Taille des données.
             * @param outData Reçoit le buffer zlib alloué (NkAlloc).  L'appelant doit NkFree.
             * @param outSz   Reçoit la taille du buffer de sortie.
             * @param level   Niveau de compression (ignoré pour l'instant, stored uniquement).
             * @return true si l'encodage a réussi.
             */
            static bool Compress(const uint8* in,  usize inSz,
                                uint8*&      out, usize& outSz,
                                int32 level = 6) noexcept;

        private:

            // ── Table Huffman (stb_image stbi__zhuffman) ──────────────────────────────
            /**
             * Structure d'une table de décodage Huffman canonique.
             * Combinaison d'une LUT rapide (FAST=9 bits, O(1) pour 99% des codes)
             * et d'un slow path basé sur un tableau maxcode[] pré-shifté.
             */
            struct ZHuff {
                static constexpr int32 FAST = 9; ///< Taille de la fast table (bits).
                uint16 fast[1 << FAST]; ///< fast[idx] = (longueur<<9)|symbole, 0=non utilisé.
                uint16 firstcode[16];   ///< Premier code canonique pour chaque longueur.
                int32  maxcode[17];     ///< Limite supérieure (pré-shiftée 16-l) pour chaque longueur.
                uint16 firstsym[16];    ///< Premier symbole pour chaque longueur dans sizes[]/values[].
                uint8  sizes[288];      ///< Longueur de code pour chaque symbole (trié par code).
                uint16 values[288];     ///< Symbole correspondant (trié par code).
            };

            // ── Contexte inflate ──────────────────────────────────────────────────────
            /**
             * État complet d'un décodage inflate en cours.
             * Stocke le registre de bits, le buffer de sortie et la position courante.
             */
            struct ZBuf {
                const uint8* data;    ///< Buffer compressé.
                usize        size;    ///< Taille du buffer compressé.
                usize        pos;     ///< Position de lecture courante (octets).
                uint32       bits;    ///< Registre de bits accumulés (LSB-first).
                int32        nbits;   ///< Nombre de bits valides dans `bits`.
                bool         err;     ///< true si une erreur irrécouvrable s'est produite.
                uint8*       out;     ///< Buffer de sortie (pré-alloué par l'appelant).
                usize        outCap;  ///< Capacité du buffer de sortie.
                usize        outPos;  ///< Position d'écriture courante dans le buffer de sortie.
            };

            // ── Primitives inflate ────────────────────────────────────────────────────

            /** Remplit le registre de bits depuis le flux compressé (au moins 25 bits). */
            static void   zFill(ZBuf& z) noexcept;

            /** Extrait et consomme `n` bits depuis le registre (LSB-first). */
            static uint32 zBits(ZBuf& z, int32 n) noexcept;

            /**
             * Construit une table Huffman depuis une liste de longueurs de codes.
             * Gère la fast table (bit-reverse du code pour l'indexation directe)
             * et la slow table (firstcode, maxcode, firstsym, sizes, values).
             *
             * @param h      Table à construire.
             * @param szList Liste des longueurs de codes (une par symbole).
             * @param num    Nombre de symboles.
             * @return false si la liste est invalide (longueurs incohérentes).
             */
            static bool   zBuildH(ZHuff& h, const uint8* szList, int32 num) noexcept;

            /**
             * Décode un symbole Huffman depuis le registre de bits.
             * Fast path O(1) pour les codes <= FAST bits.
             * Slow path O(longueur) pour les codes plus longs.
             * @return Symbole décodé, ou -1 si erreur.
             */
            static int32  zDecode(ZBuf& z, const ZHuff& h) noexcept;

            /** Entrée principale inflate : parse l'en-tête zlib si hdr==true, puis les blocs. */
            static bool   zInflate(ZBuf& z, bool parseHdr) noexcept;

            /** Décode un bloc DEFLATE : lit BFINAL et BTYPE, dispatch vers le type. */
            static bool   zBlock(ZBuf& z, bool& last) noexcept;

            /** Décode un bloc Huffman (types 1 fixed et 2 dynamic). */
            static bool   zHuffBlock(ZBuf& z, const ZHuff& zl, const ZHuff& zd) noexcept;

            /** Décode un bloc non-compressé (BTYPE=00) : copie len octets verbatim. */
            static bool   zStored(ZBuf& z) noexcept;

            /** Décode un bloc à codes fixes (BTYPE=01, tables RFC 1951 hardcodées). */
            static bool   zFixed(ZBuf& z) noexcept;

            /** Décode un bloc à codes dynamiques (BTYPE=10, tables encodées dans le flux). */
            static bool   zDynamic(ZBuf& z) noexcept;

            /**
             * Calcule le checksum Adler-32.
             * @param prev  Checksum initial (1 pour un nouveau calcul).
             * @return Checksum Adler-32 mis à jour.
             */
            static uint32 Adler32(const uint8* data, usize size, uint32 prev = 1) noexcept;

            // ── Tables statiques DEFLATE (RFC 1951) ───────────────────────────────────

            static const uint16 kLenBase[29];  ///< Longueur de base pour les codes 257–285.
            static const uint8  kLenExtra[29]; ///< Bits extra pour les codes de longueur.
            static const uint16 kDistBase[30]; ///< Distance de base pour les codes 0–29.
            static const uint8  kDistExtra[30];///< Bits extra pour les codes de distance.
            static const uint8  kCLOrder[19];  ///< Ordre de lecture des longueurs CL (dynamic).
            static const uint8  kZDefLen[288]; ///< Longueurs des codes litéraux/longueur fixes.
            static const uint8  kZDefDist[32]; ///< Longueurs des codes de distance fixes.
    };

} // namespace nkentseu