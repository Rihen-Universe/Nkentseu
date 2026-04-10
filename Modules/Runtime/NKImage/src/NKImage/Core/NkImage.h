#pragma once
/**
 * @File    NkImage.h
 * @Brief   NKImage — chargement/sauvegarde d'images, sans dépendance externe.
 *          Algorithmes adaptés de stb_image v2.16 (public domain, Sean Barrett).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NkImageExport.h"
#include <cstdio>

namespace nkentseu {

    // ─────────────────────────────────────────────────────────────────────────────
    //  Formats pixel
    // ─────────────────────────────────────────────────────────────────────────────

    enum class NkImagePixelFormat : uint8 {
        NK_UNKNOWN  = 0,
        NK_GRAY8    = 1,
        NK_GRAY_A16 = 2,
        NK_RGB24    = 3,
        NK_RGBA32   = 4,
        NK_RGBA128F = 5,
        NK_RGB96F   = 6,
    };

    NKIMG_INLINE constexpr int32 ChannelsOf(NkImagePixelFormat f) noexcept {
        switch(f){
            case NkImagePixelFormat::NK_GRAY8:    return 1;
            case NkImagePixelFormat::NK_GRAY_A16: return 2;
            case NkImagePixelFormat::NK_RGB24:    return 3;
            case NkImagePixelFormat::NK_RGBA32:   return 4;
            case NkImagePixelFormat::NK_RGBA128F: return 4;
            case NkImagePixelFormat::NK_RGB96F:   return 3;
            default: return 0;
        }
    }
    NKIMG_INLINE constexpr int32 BytesPerPixelOf(NkImagePixelFormat f) noexcept {
        switch(f){
            case NkImagePixelFormat::NK_GRAY8:    return 1;
            case NkImagePixelFormat::NK_GRAY_A16: return 2;
            case NkImagePixelFormat::NK_RGB24:    return 3;
            case NkImagePixelFormat::NK_RGBA32:   return 4;
            case NkImagePixelFormat::NK_RGBA128F: return 16;
            case NkImagePixelFormat::NK_RGB96F:   return 12;
            default: return 0;
        }
    }

    enum class NkImageFormat : uint8 {
        NK_UNKNOWN=0, NK_PNG, NK_JPEG, NK_BMP, NK_TGA, NK_HDR,
        NK_PPM, NK_PGM, NK_PBM, NK_QOI, NK_GIF, NK_ICO,
    };

    enum class NkResizeFilter : uint8 {
        NK_NEAREST, NK_BILINEAR, NK_BICUBIC, NK_LANCZOS3,
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkImage
    // ─────────────────────────────────────────────────────────────────────────────

    class NKENTSEU_IMAGE_API NkImage {
    public:
        NkImage()  noexcept = default;
        ~NkImage() noexcept;
        NkImage(const NkImage&)            = delete;
        NkImage& operator=(const NkImage&) = delete;

        // Chargement
        static NkImage* Load(const char* path, int32 desiredChannels=0) noexcept;
        static NkImage* LoadFromMemory(const uint8* data, usize size, int32 desiredChannels=0) noexcept;

        // Sauvegarde
        bool Save    (const char* path, int32 quality=90) const noexcept;
        bool SavePNG (const char* path)                   const noexcept;
        bool SaveJPEG(const char* path, int32 quality=90) const noexcept;
        bool SaveBMP (const char* path)                   const noexcept;
        bool SaveTGA (const char* path)                   const noexcept;
        bool SavePPM (const char* path)                   const noexcept;
        bool SaveHDR (const char* path)                   const noexcept;
        bool SaveQOI (const char* path)                   const noexcept;
        bool SaveGIF (const char* path)                   const noexcept;
        bool SaveWebP(const char* path, bool lossless=true, int32 quality=90) const noexcept;
        bool SaveSVG (const char* path)                   const noexcept;

        // Encodage mémoire (buffer alloué avec malloc — appelant libère avec free)
        bool EncodePNG (uint8*& out, usize& size)                     const noexcept;
        bool EncodeJPEG(uint8*& out, usize& size, int32 quality=90)   const noexcept;
        bool EncodeBMP (uint8*& out, usize& size)                     const noexcept;
        bool EncodeTGA (uint8*& out, usize& size)                     const noexcept;
        bool EncodeQOI (uint8*& out, usize& size)                     const noexcept;

        // Manipulation
        void     FlipVertical()   noexcept;
        void     FlipHorizontal() noexcept;
        void     PremultiplyAlpha() noexcept;
        NkImage* Convert(NkImagePixelFormat newFmt)                           const noexcept;
        NkImage* Resize (int32 nw, int32 nh, NkResizeFilter f=NkResizeFilter::NK_BILINEAR) const noexcept;
        void     Blit   (const NkImage& src, int32 dstX, int32 dstY)               noexcept;
        NkImage* Crop   (int32 x, int32 y, int32 w, int32 h)                 const noexcept;

        // Accès
        NKIMG_INLINE uint8*       Pixels()   noexcept       { return mPixels; }
        NKIMG_INLINE const uint8* Pixels()   const noexcept { return mPixels; }
        NKIMG_INLINE int32        Width()    const noexcept { return mWidth;  }
        NKIMG_INLINE int32        Height()   const noexcept { return mHeight; }
        NKIMG_INLINE int32        Channels() const noexcept { return ChannelsOf(mFormat); }
        NKIMG_INLINE int32        BytesPP()  const noexcept { return BytesPerPixelOf(mFormat); }
        NKIMG_INLINE int32        Stride()   const noexcept { return mStride; }
        NKIMG_INLINE NkImagePixelFormat Format() const noexcept { return mFormat; }
        NKIMG_INLINE NkImageFormat SourceFormat() const noexcept { return mSrcFmt; }
        NKIMG_INLINE bool IsValid() const noexcept { return mPixels&&mWidth>0&&mHeight>0; }
        NKIMG_INLINE bool IsHDR()   const noexcept {
            return mFormat==NkImagePixelFormat::NK_RGBA128F||mFormat==NkImagePixelFormat::NK_RGB96F;
        }
        NKIMG_INLINE usize TotalBytes() const noexcept { return usize(mStride)*mHeight; }

        NKIMG_INLINE uint8* RowPtr(int32 y) noexcept {
            return mPixels + usize(y)*mStride;
        }
        NKIMG_INLINE const uint8* RowPtr(int32 y) const noexcept {
            return mPixels + usize(y)*mStride;
        }

        // Gestion mémoire
        void Free() noexcept;

        // Fabriques (usage codecs)
        static NkImage* Alloc(int32 w, int32 h, NkImagePixelFormat fmt) noexcept;
        static NkImage* Wrap (uint8* pixels, int32 w, int32 h, NkImagePixelFormat fmt, int32 stride=0) noexcept;

        static NkImage* ConvertToTexture(const NkImage& hdrImage, float exposure = 1.0f, float gamma = 2.2f) noexcept;
        // static bool ConvertToTextureData(const NkImage& hdrImage, NkTextureData& outData, float exposure = 1.0f, float gamma = 2.2f) noexcept;

    private:
        uint8*             mPixels = nullptr;
        int32              mWidth  = 0;
        int32              mHeight = 0;
        int32              mStride = 0;
        NkImagePixelFormat mFormat = NkImagePixelFormat::NK_UNKNOWN;
        NkImageFormat      mSrcFmt = NkImageFormat::NK_UNKNOWN;
        bool               mOwning = true;

        static NkImageFormat DetectFormat(const uint8* data, usize size) noexcept;
        static NkImage*      Dispatch    (const uint8* data, usize size,
                                        int32 desired, NkImageFormat fmt) noexcept;
        static uint8* ConvertChannels(const uint8* src, int32 w, int32 h,
                                    int32 srcCh, int32 dstCh, int32 srcStride) noexcept;

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

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkImageStream
    // ─────────────────────────────────────────────────────────────────────────────

    class NKENTSEU_IMAGE_API NkImageStream {
    public:
        NkImageStream(const uint8* data, usize size) noexcept
            : mRdData(data), mRdSize(size) {}
        NkImageStream() noexcept {}
        ~NkImageStream() noexcept {}

        // Lecture
        uint8  ReadU8()    noexcept;
        uint16 ReadU16BE() noexcept;
        uint16 ReadU16LE() noexcept;
        uint32 ReadU32BE() noexcept;
        uint32 ReadU32LE() noexcept;
        int16  ReadI16BE() noexcept;
        int32  ReadI32LE() noexcept;
        usize  ReadBytes(uint8* dst, usize n) noexcept;
        void   Skip(usize n) noexcept;
        void   Seek(usize pos) noexcept;

        usize Tell()       const noexcept { return mRdPos; }
        usize Size()       const noexcept { return mRdSize; }
        bool  IsEOF()      const noexcept { return mRdPos>=mRdSize; }
        bool  HasBytes(usize n) const noexcept { return mRdPos+n<=mRdSize; }
        bool  HasError()   const noexcept { return mError; }
        const uint8* Ptr() const noexcept { return mRdData+mRdPos; }

        // Écriture
        bool WriteU8   (uint8  v) noexcept;
        bool WriteU16BE(uint16 v) noexcept;
        bool WriteU16LE(uint16 v) noexcept;
        bool WriteU32BE(uint32 v) noexcept;
        bool WriteU32LE(uint32 v) noexcept;
        bool WriteI32LE(int32  v) noexcept;
        bool WriteBytes(const uint8* src, usize n) noexcept;

        bool TakeBuffer(uint8*& outData, usize& outSize) noexcept {
            outData=mWrBuf; outSize=mWrSize;
            mWrBuf=nullptr; mWrSize=0; mWrCap=0;
            return outData!=nullptr;
        }
        usize WriteSize() const noexcept { return mWrSize; }

    private:
        const uint8* mRdData = nullptr;
        usize        mRdSize = 0;
        usize        mRdPos  = 0;
        bool         mError  = false;
        uint8*       mWrBuf  = nullptr;
        usize        mWrSize = 0;
        usize        mWrCap  = 0;
        bool Grow(usize needed) noexcept;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkDeflate — inflate/deflate (stb_image inflate exact)
    // ─────────────────────────────────────────────────────────────────────────────

    class NKENTSEU_IMAGE_API NkDeflate {
    public:
        /**
         * Décompresse un flux zlib RFC 1950.
         * FIX: FDICT lu depuis in[1] (FLG), PAS in[0] (CMF).
         * CMF=0x78 a bit5=1 (CINFO) — ancienne version rejetait tous les PNG.
         */
        static bool Decompress  (const uint8* in,  usize inSz,
                                uint8*       out, usize outCap, usize& written) noexcept;
        static bool DecompressRaw(const uint8* in,  usize inSz,
                                uint8*       out, usize outCap, usize& written) noexcept;
        static bool Compress    (const uint8* in,  usize inSz,
                                uint8*&      out, usize& outSz, int32 level=6) noexcept;

    private:
        // Table Huffman (stb_image stbi__zhuffman — LUT FAST=9 bits, LSB-first)
        struct ZHuff {
            static constexpr int32 FAST = 9;
            uint16 fast[1<<FAST]; // 0 = unused
            uint16 firstcode[16];
            int32  maxcode[17];   // pré-shifté (16-l)
            uint16 firstsym[16];
            uint8  sizes[288];
            uint16 values[288];
        };

        // Contexte inflate
        struct ZBuf {
            const uint8* data; usize size; usize pos;
            uint32 bits; int32 nbits; bool err;
            uint8* out; usize outCap; usize outPos;
        };

        static void  zFill(ZBuf& z) noexcept;
        static uint32 zBits(ZBuf& z, int32 n) noexcept;
        static bool   zBuildH(ZHuff& h, const uint8* szList, int32 num) noexcept;
        static int32  zDecode(ZBuf& z, const ZHuff& h) noexcept;
        static bool   zInflate(ZBuf& z, bool parseHdr) noexcept;
        static bool   zBlock(ZBuf& z, bool& last) noexcept;
        static bool   zHuffBlock(ZBuf& z, const ZHuff& zl, const ZHuff& zd) noexcept;
        static bool   zStored(ZBuf& z) noexcept;
        static bool   zFixed(ZBuf& z) noexcept;
        static bool   zDynamic(ZBuf& z) noexcept;
        static uint32 Adler32(const uint8* data, usize size, uint32 prev=1) noexcept;

        static const uint16 kLenBase[29];
        static const uint8  kLenExtra[29];
        static const uint16 kDistBase[30];
        static const uint8  kDistExtra[30];
        static const uint8  kCLOrder[19];
        static const uint8  kZDefLen[288];
        static const uint8  kZDefDist[32];
    };

} // namespace nkentseu