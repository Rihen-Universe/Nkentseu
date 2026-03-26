#pragma once
/**
 * @File    NkImage.h
 * @Brief   API publique NKImage — chargement et sauvegarde d'images.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NKImage est une bibliothèque self-contained de chargement/sauvegarde
 *  d'images, équivalente à stb_image, sans dépendance externe.
 *
 *  Usage minimal :
 * @code
 *   NkImage* img = NkImage::Load("photo.png");
 *   if (!img) { ... }
 *   // img->Pixels() : uint8* RGBA
 *   // img->Width(), img->Height(), img->Channels()
 *   img->Save("sortie.jpg", 90);
 *   img->Free();
 * @endcode
 *
 *  Formats supportés :
 *    Lecture  : PNG, JPEG, BMP, TGA, HDR, PPM/PGM/PBM, QOI, GIF, ICO
 *    Écriture : PNG, JPEG, BMP, TGA, HDR, PPM/PGM, QOI
 *
 *  Pixels en sortie : toujours entrelacés (packed), ligne par ligne, Y vers le bas.
 *  Formats de pixels : Gray8, GrayAlpha16, RGB24, RGBA32, RGBA128F (HDR).
 *
 *  Gestion mémoire :
 *    - NkImage::Load alloue avec malloc.
 *    - NkImage::Free libère avec free.
 *    - NkImage::LoadInto : alloue dans un buffer fourni par l'appelant.
 *    - Pas de STL, pas de new/delete.
 */

#include "NkImageExport.h"

namespace nkentseu {

    // =========================================================================
    //  NkImagePixelFormat
    // =========================================================================

    enum class NkImagePixelFormat : uint8 {
        NK_UNKNOW   = 0,
        NK_GRAY8     = 1,   ///< 1 octet/pixel — luminance
        NK_GRAY_A16   = 2,   ///< 2 octets/pixel — luminance + alpha
        NK_RGB24     = 3,   ///< 3 octets/pixel — rouge vert bleu
        NK_RGBA32    = 4,   ///< 4 octets/pixel — rouge vert bleu alpha
        NK_RGBA128F  = 5,   ///< 4 × float32/pixel — HDR
        NK_RGB96F    = 6,   ///< 3 × float32/pixel — HDR sans alpha
    };

    NKIMG_INLINE constexpr int32 ChannelsOf(NkImagePixelFormat fmt) noexcept {
        switch (fmt) {
            case NkImagePixelFormat::NK_GRAY8:       return 1;
            case NkImagePixelFormat::NK_GRAY_A16:    return 2;
            case NkImagePixelFormat::NK_RGB24:      return 3;
            case NkImagePixelFormat::NK_RGBA32:      return 4;
            case NkImagePixelFormat::NK_RGBA128F:    return 4;
            case NkImagePixelFormat::NK_RGB96F:      return 3;
            default: return 0;
        }
    }

    NKIMG_INLINE constexpr int32 BytesPerPixel(NkImagePixelFormat fmt) noexcept {
        switch (fmt) {
            case NkImagePixelFormat::NK_GRAY8:    return 1;
            case NkImagePixelFormat::NK_GRAY_A16:  return 2;
            case NkImagePixelFormat::NK_RGB24:    return 3;
            case NkImagePixelFormat::NK_RGBA32:   return 4;
            case NkImagePixelFormat::NK_RGBA128F: return 16;
            case NkImagePixelFormat::NK_RGB96F:   return 12;
            default: return 0;
        }
    }

    // =========================================================================
    //  NkResizeFilter
    // =========================================================================

    enum class NkResizeFilter : uint8 {
        NK_NEAREST,    ///< Pixel le plus proche — rapide
        NK_BILINEAR,   ///< Interpolation bilinéaire — doux
        NK_BICUBIC,    ///< Interpolation bicubique — net
        NK_LANCZOS3,   ///< Filtre Lanczos radius 3 — haute qualité
    };

    // =========================================================================
    //  NkImageFormat — format de fichier détecté
    // =========================================================================

    enum class NkImageFormat : uint8 {
        NK_UNKNOW = 0,
        NK_PNG, NK_JPEG, NK_BMP, NK_TGA, NK_HDR,
        NK_PPM, NK_PGM, NK_PBM,
        NK_QOI, NK_GIF, NK_ICO,
    };

    // =========================================================================
    //  NkImage
    // =========================================================================

    class NKENTSEU_IMAGE_API NkImage {
        public:

            NkImage()  noexcept = default;
            ~NkImage() noexcept;

            NkImage(const NkImage&)            = delete;
            NkImage& operator=(const NkImage&) = delete;

            // ── Chargement ───────────────────────────────────────────────────────

            /**
             * @Brief Charge une image depuis un fichier.
             * @param path            Chemin vers le fichier.
             * @param desiredChannels 0 = natif, 1-4 = forcer le nombre de canaux.
             * @return NkImage alloué (Free() requis), nullptr si échec.
             */
            NKIMG_NODISCARD static NkImage* Load(
                const char* path,
                int32 desiredChannels = 0
            ) noexcept;

            /**
             * @Brief Charge une image depuis un buffer en mémoire.
             * @param data            Données du fichier.
             * @param size            Taille en octets.
             * @param desiredChannels 0 = natif, 1-4 = forcer le nombre de canaux.
             * @return NkImage alloué (Free() requis), nullptr si échec.
             */
            NKIMG_NODISCARD static NkImage* LoadFromMemory(
                const uint8* data,
                usize        size,
                int32        desiredChannels = 0
            ) noexcept;

            /**
             * @Brief Interroge les dimensions sans charger les pixels.
             * @return true si succès.
             */
            NKIMG_NODISCARD static bool QueryInfo(
                const char* path,
                int32& outWidth, int32& outHeight,
                int32& outChannels, NkImageFormat& outFormat
            ) noexcept;

            // ── Backend STB (alternatif) ─────────────────────────────────────────

            /** @Brief Charge via stb_image (alternatif aux codecs natifs). */
            NKIMG_NODISCARD static NkImage* LoadSTB(
                const char* path, int32 desiredChannels = 0
            ) noexcept;

            NKIMG_NODISCARD static NkImage* LoadFromMemorySTB(
                const uint8* data, usize size, int32 desiredChannels = 0
            ) noexcept;

            /** @Brief Sauvegarde via stb_image_write (.png .jpg .bmp .tga .hdr). */
            bool SaveSTB(const char* path, int32 quality = 90) const noexcept;

            // ── Sauvegarde ───────────────────────────────────────────────────────

            /**
             * @Brief Sauvegarde l'image dans un fichier.
             *        Le format est déduit de l'extension : .png, .jpg, .bmp, .tga, .hdr, .ppm, .qoi
             * @param quality JPEG uniquement [1-100] (défaut 90).
             */
            bool Save    (const char* path, int32 quality = 90) const noexcept;
            bool SavePNG (const char* path)                     const noexcept;
            bool SaveJPEG(const char* path, int32 quality = 90) const noexcept;
            bool SaveBMP (const char* path)                     const noexcept;
            bool SaveTGA (const char* path)                     const noexcept;
            bool SaveHDR (const char* path)                     const noexcept;
            bool SavePPM (const char* path)                     const noexcept;
            bool SaveQOI (const char* path)                     const noexcept;
            bool SaveGIF (const char* path)                     const noexcept;
            bool SaveWebP(const char* path, bool lossless=true, int32 quality=90) const noexcept;
            bool SaveSVG (const char* path)                     const noexcept;

            /**
             * @Brief Encode en mémoire. outData est alloué avec malloc (appelant libère avec free).
             */
            bool EncodePNG (uint8*& outData, usize& outSize)                     const noexcept;
            bool EncodeJPEG(uint8*& outData, usize& outSize, int32 quality = 90) const noexcept;
            bool EncodeBMP (uint8*& outData, usize& outSize)                     const noexcept;
            bool EncodeTGA (uint8*& outData, usize& outSize)                     const noexcept;
            bool EncodeQOI (uint8*& outData, usize& outSize)                     const noexcept;

            // ── Manipulation ─────────────────────────────────────────────────────

            /// Flip vertical (Y vers le haut/bas). In-place.
            void FlipVertical()   noexcept;
            /// Flip horizontal (miroir gauche/droite). In-place.
            void FlipHorizontal() noexcept;

            /**
             * @Brief Prémultiplie l'alpha (RGB *= A/255). Irréversible.
             *        Nécessaire pour le blending correct en rendu GPU.
             */
            void PremultiplyAlpha() noexcept;

            /**
             * @Brief Convertit vers un autre format de pixels.
             * @return Nouvelle NkImage allouée, nullptr si échec.
             */
            NKIMG_NODISCARD NkImage* Convert(NkImagePixelFormat newFmt) const noexcept;

            /**
             * @Brief Redimensionne l'image.
             * @return Nouvelle NkImage allouée, nullptr si échec.
             */
            NKIMG_NODISCARD NkImage* Resize(
                int32 newWidth, int32 newHeight,
                NkResizeFilter filter = NkResizeFilter::NK_BILINEAR
            ) const noexcept;

            /**
             * @Brief Copie une région source dans this à la position (dstX, dstY).
             *        Clippe automatiquement. Les deux images doivent avoir le même format.
             */
            void Blit(const NkImage& src, int32 dstX, int32 dstY) noexcept;

            /**
             * @Brief Extrait une sous-région. Retourne une nouvelle NkImage allouée.
             */
            NKIMG_NODISCARD NkImage* Crop(
                int32 x, int32 y, int32 w, int32 h
            ) const noexcept;

            // ── Accès ────────────────────────────────────────────────────────────

            NKIMG_NODISCARD NKIMG_INLINE uint8*  Pixels()   noexcept       { return mPixels; }
            NKIMG_NODISCARD NKIMG_INLINE const uint8* Pixels() const noexcept { return mPixels; }
            NKIMG_NODISCARD NKIMG_INLINE float32* PixelsF() noexcept       { return reinterpret_cast<float32*>(mPixels); }
            NKIMG_NODISCARD NKIMG_INLINE const float32* PixelsF() const noexcept { return reinterpret_cast<const float32*>(mPixels); }
            NKIMG_NODISCARD NKIMG_INLINE int32   Width()    const noexcept { return mWidth;  }
            NKIMG_NODISCARD NKIMG_INLINE int32   Height()   const noexcept { return mHeight; }
            NKIMG_NODISCARD NKIMG_INLINE int32   Channels() const noexcept { return ChannelsOf(mFormat); }
            NKIMG_NODISCARD NKIMG_INLINE int32   BytesPP()  const noexcept { return BytesPerPixel(mFormat); }
            NKIMG_NODISCARD NKIMG_INLINE int32   Stride()   const noexcept { return mStride; }
            NKIMG_NODISCARD NKIMG_INLINE NkImagePixelFormat  Format() const noexcept { return mFormat; }
            NKIMG_NODISCARD NKIMG_INLINE NkImageFormat  SourceFormat() const noexcept { return mSourceFormat; }
            NKIMG_NODISCARD NKIMG_INLINE bool    IsValid()  const noexcept { return mPixels && mWidth > 0 && mHeight > 0; }
            NKIMG_NODISCARD NKIMG_INLINE bool    IsHDR()    const noexcept { return mFormat == NkImagePixelFormat::NK_RGBA128F || mFormat == NkImagePixelFormat::NK_RGB96F; }
            NKIMG_NODISCARD NKIMG_INLINE usize   TotalBytes() const noexcept { return static_cast<usize>(mStride) * mHeight; }

            /// Pointeur sur le début d'une ligne.
            NKIMG_NODISCARD NKIMG_INLINE uint8* RowPtr(int32 y) noexcept {
                return mPixels + static_cast<usize>(y) * mStride;
            }
            NKIMG_NODISCARD NKIMG_INLINE const uint8* RowPtr(int32 y) const noexcept {
                return mPixels + static_cast<usize>(y) * mStride;
            }

            /// Accès pixel individuel (RGBA32 uniquement).
            NKIMG_NODISCARD NKIMG_INLINE uint32 GetPixelRGBA(int32 x, int32 y) const noexcept {
                const uint8* p = RowPtr(y) + x * 4;
                return (static_cast<uint32>(p[0]) << 24)
                    | (static_cast<uint32>(p[1]) << 16)
                    | (static_cast<uint32>(p[2]) <<  8)
                    |  static_cast<uint32>(p[3]);
            }
            NKIMG_INLINE void SetPixelRGBA(int32 x, int32 y, uint8 r, uint8 g, uint8 b, uint8 a) noexcept {
                uint8* p = RowPtr(y) + x * 4;
                p[0]=r; p[1]=g; p[2]=b; p[3]=a;
            }

            // ── Gestion mémoire ───────────────────────────────────────────────────

            /// Libère les pixels et l'objet lui-même (alloué par Load).
            void Free() noexcept;

            // ── Fabriques internes (pour les codecs) ──────────────────────────────

            /**
             * @Brief Alloue une NkImage avec un buffer de pixels initialisé à zéro.
             * @param owning true = l'objet libère les pixels dans Free(). false = buffer externe.
             */
            NKIMG_NODISCARD static NkImage* Alloc(
                int32 width, int32 height,
                NkImagePixelFormat format,
                bool owning = true
            ) noexcept;

            /**
             * @Brief Crée une NkImage vue sur un buffer externe (pas de ownership).
             */
            NKIMG_NODISCARD static NkImage* Wrap(
                uint8* pixels, int32 width, int32 height,
                NkImagePixelFormat format, int32 stride = 0
            ) noexcept;

        private:
            uint8*        mPixels       = nullptr;
            int32         mWidth        = 0;
            int32         mHeight       = 0;
            int32         mStride       = 0;
            NkImagePixelFormat mFormat       = NkImagePixelFormat::NK_UNKNOW;
            NkImageFormat mSourceFormat = NkImageFormat::NK_UNKNOW;
            bool          mOwning       = true;

            // Détection et dispatch
            static NkImageFormat DetectFormat(const uint8* data, usize size) noexcept;
            static NkImage*      Dispatch    (const uint8* data, usize size,
                                            int32 desired, NkImageFormat fmt) noexcept;

            // Conversion de canaux (interne)
            static uint8* ConvertChannels(
                const uint8* src, int32 w, int32 h,
                int32 srcCh, int32 dstCh
            ) noexcept;

            // Resize interne par filtre (basé sur stb_image_resize2)
            static NkImage* ResizeNearest (const NkImage& src, int32 w, int32 h) noexcept;
            static NkImage* ResizeBilinear(const NkImage& src, int32 w, int32 h) noexcept;
            static NkImage* ResizeBicubic (const NkImage& src, int32 w, int32 h) noexcept;
            static NkImage* ResizeLanczos3(const NkImage& src, int32 w, int32 h) noexcept;

            friend class NkPNGCodec;
            friend class NkJPEGCodec;
            friend class NkBMPCodec;
            friend class NkTGACodec;
            friend class NkHDRCodec;
            friend class NkPPMCodec;
            friend class NkQOICodec;
            friend class NkGIFCodec;
            friend class NkICOCodec;
    };

    // =========================================================================
    //  NkImageStream — lecteur/écriveur de flux binaire pour les codecs
    // =========================================================================

    class NKENTSEU_IMAGE_API NkImageStream {
        public:
            // ── Lecture depuis mémoire ────────────────────────────────────────────
            NkImageStream(const uint8* data, usize size) noexcept
                : mData(data), mSize(size), mPos(0), mWriteBuf(nullptr) {}

            // ── Écriture vers buffer dynamique (malloc) ───────────────────────────
            NkImageStream() noexcept
                : mData(nullptr), mSize(0), mPos(0), mWriteBuf(nullptr),
                mWriteSize(0), mWriteCapacity(0) {}

            ~NkImageStream() noexcept {
                // mWriteBuf est rendu à l'appelant via TakeBuffer() — pas de free ici
            }

            // Lecture
            NKIMG_NODISCARD uint8  ReadU8()  noexcept;
            NKIMG_NODISCARD uint16 ReadU16BE() noexcept;
            NKIMG_NODISCARD uint16 ReadU16LE() noexcept;
            NKIMG_NODISCARD uint32 ReadU32BE() noexcept;
            NKIMG_NODISCARD uint32 ReadU32LE() noexcept;
            NKIMG_NODISCARD int16  ReadI16BE() noexcept;
            NKIMG_NODISCARD int32  ReadI32LE() noexcept;
            usize ReadBytes(uint8* dst, usize count) noexcept;
            void  Skip(usize count) noexcept;
            void  Seek(usize pos)   noexcept;
            NKIMG_NODISCARD usize Tell()      const noexcept { return mPos; }
            NKIMG_NODISCARD usize Size()      const noexcept { return mSize; }
            NKIMG_NODISCARD bool  IsEOF()     const noexcept { return mPos >= mSize; }
            NKIMG_NODISCARD bool  HasBytes(usize n) const noexcept { return mPos + n <= mSize; }
            NKIMG_NODISCARD bool  HasError()  const noexcept { return mError; }
            NKIMG_NODISCARD const uint8* CurrentPtr() const noexcept { return mData + mPos; }

            // Écriture (buffer dynamique)
            bool WriteU8  (uint8  v)  noexcept;
            bool WriteU16BE(uint16 v) noexcept;
            bool WriteU16LE(uint16 v) noexcept;
            bool WriteU32BE(uint32 v) noexcept;
            bool WriteU32LE(uint32 v) noexcept;
            bool WriteI32LE(int32  v) noexcept;
            bool WriteBytes(const uint8* src, usize count) noexcept;
            bool WriteByte (uint8 b)  noexcept { return WriteU8(b); }

            // Prend le buffer de sortie (transfert de propriété — appelant libère avec free)
            bool TakeBuffer(uint8*& outData, usize& outSize) noexcept {
                outData  = mWriteBuf;  outSize  = mWriteSize;
                mWriteBuf = nullptr;  mWriteSize = mWriteCapacity = 0;
                return outData != nullptr;
            }

            NKIMG_NODISCARD usize WriteSize() const noexcept { return mWriteSize; }

            // Écrit aussi dans un FILE* (pour Save)
            void SetFile(FILE* f) noexcept { mFile = f; }

        private:
            const uint8* mData      = nullptr;
            usize        mSize      = 0;
            usize        mPos       = 0;
            bool         mError     = false;

            uint8*  mWriteBuf      = nullptr;
            usize   mWriteSize     = 0;
            usize   mWriteCapacity = 0;
            FILE*   mFile          = nullptr;

            bool Grow(usize needed) noexcept;
    };

    // =========================================================================
    //  NkDeflate — inflate/deflate partagé (PNG + futur usage)
    // =========================================================================

    class NKENTSEU_IMAGE_API NkDeflate {
        public:
            /// Décompresse un flux zlib (RFC 1950).
            static bool Decompress(
                const uint8* in,  usize inSize,
                uint8*       out, usize outSize,
                usize&       written
            ) noexcept;

            /// Compresse vers zlib (niveau 0-9, 6 = défaut).
            static bool Compress(
                const uint8* in,  usize inSize,
                uint8*&      out, usize& outSize,
                int32 level = 6
            ) noexcept;

            /// Décompresse deflate brut (sans header zlib).
            static bool DecompressRaw(
                const uint8* in,  usize inSize,
                uint8*       out, usize outSize,
                usize&       written
            ) noexcept;

        private:
            // Inflate (RFC 1951)
            struct Context {
                const uint8* in; usize inSize; usize inPos;
                uint8* out; usize outSize; usize outPos;
                uint32 bitBuf; int32 bitCount; bool error;
            };
            struct HuffTree {
                static constexpr int32 MAX_BITS = 16;
                static constexpr int32 MAX_SYMS = 288;
                int16 counts[MAX_BITS+1]; int16 symbols[MAX_SYMS]; int32 numSymbols;
            };
            static uint32 ReadBits  (Context& c, int32 n) noexcept;
            static bool   BuildHuff (HuffTree& t, const uint8* lens, int32 n) noexcept;
            static int32  Decode    (Context& c, const HuffTree& t) noexcept;
            static bool   Block     (Context& c, bool& last) noexcept;
            static bool   Stored    (Context& c) noexcept;
            static bool   Fixed     (Context& c) noexcept;
            static bool   Dynamic   (Context& c) noexcept;
            static bool   Huffman   (Context& c, const HuffTree& lt, const HuffTree& dt) noexcept;
            static const uint16 kLenBase[29];
            static const uint8  kLenExtra[29];
            static const uint16 kDistBase[30];
            static const uint8  kDistExtra[30];
            static const uint8  kCLOrder[19];

            // Deflate (RFC 1951) — compression
            struct DeflateCtx {
                NkImageStream* out;
                const uint8* in; usize inSize;
                uint32 adler;
            };
            static bool CompressLevel0(DeflateCtx& ctx) noexcept;
            static bool CompressLevel1(DeflateCtx& ctx) noexcept;
            static uint32 Adler32(const uint8* data, usize size, uint32 prev=1) noexcept;
    };

} // namespace nkentseu
