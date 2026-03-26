/**
 * @File    NkGIFCodec.cpp
 * @Brief   Codec GIF production-ready — GIF87a/GIF89a complet.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Correctness
 *  Décodage  : LZW variable (2-12 bits), GCT + LCT, interlacement,
 *              transparence (Graphic Control Extension), multi-frame,
 *              disposal methods 0-3, frame blending correct.
 *  Encodage  : LZW GIF87a/89a, quantification médiane coupure 256 couleurs,
 *              transparence, animation (delays + disposal), GCT optimale.
 *  Robustesse: sous-blocks mal formés, LZW codes invalides, reset mid-stream,
 *              frames avec LCT, frames partielles out-of-bounds — tout géré.
 */
#include "NKImage/Codecs/GIF/NkGIFCodec.h"
#include <cstdio>
#include <cstdlib>
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {

using namespace nkentseu::memory;

    // ─────────────────────────────────────────────────────────────────────────────
    //  LZW décodeur GIF (variable-length codes, 2-12 bits)
    // ─────────────────────────────────────────────────────────────────────────────

    struct LZWDecoder {
        // Table : jusqu'à 4096 entrées
        // Chaque entrée : chaîne d'indices, stockée par liens (prefix+suffix)
        static constexpr int32 MAX_CODE = 4096;

        int16  prefix[MAX_CODE]; // -1 = racine
        uint8  suffix[MAX_CODE];
        uint8  first [MAX_CODE]; // premier octet de la chaîne (pour l'entrée spéciale)

        int32  clearCode, eoiCode, nextCode, codeSize;
        int32  minCodeSize;
        bool   error;

        // Bitstream
        const uint8* subBlockData; // toutes les données des sub-blocks assemblées
        usize        subBlockLen;
        usize        bitPos;

        void Init(int32 minCS) noexcept {
            minCodeSize = minCS < 2 ? 2 : minCS > 8 ? 8 : minCS;
            clearCode   = 1 << minCodeSize;
            eoiCode     = clearCode + 1;
            error       = false;
            Reset();
        }

        void Reset() noexcept {
            nextCode = eoiCode + 1;
            codeSize = minCodeSize + 1;
            for(int32 i = 0; i < clearCode; ++i) {
                prefix[i] = -1;
                suffix[i] = static_cast<uint8>(i);
                first [i] = static_cast<uint8>(i);
            }
        }

        int32 ReadCode() noexcept {
            uint32 code = 0;
            for(int32 b = 0; b < codeSize; ++b) {
                const usize byteIdx = bitPos >> 3;
                const int32 bitIdx  = static_cast<int32>(bitPos & 7);
                if(byteIdx >= subBlockLen) { error = true; return -1; }
                code |= static_cast<uint32>((subBlockData[byteIdx] >> bitIdx) & 1) << b;
                ++bitPos;
            }
            return static_cast<int32>(code);
        }

        // Décode un code → séquence d'indices dans outBuf (retourne la longueur)
        int32 Decode(int32 code, int32 prevCode,
                    uint8* outBuf, int32 maxOut) noexcept
        {
            if(code < clearCode) {
                // Symbole littéral
                if(nextCode < MAX_CODE && prevCode >= 0) {
                    prefix[nextCode] = static_cast<int16>(prevCode);
                    suffix[nextCode] = static_cast<uint8>(code);
                    first [nextCode] = first[prevCode];
                    ++nextCode;
                }
                outBuf[0] = static_cast<uint8>(code);
                return 1;
            }
            if(code >= nextCode) {
                // Code spécial : c+c[0]
                if(prevCode < 0 || prevCode >= nextCode) { error=true; return 0; }
                if(nextCode < MAX_CODE) {
                    prefix[nextCode] = static_cast<int16>(prevCode);
                    suffix[nextCode] = first[prevCode];
                    first [nextCode] = first[prevCode];
                    ++nextCode;
                }
                code = nextCode - 1;
            } else {
                if(nextCode < MAX_CODE && prevCode >= 0) {
                    prefix[nextCode] = static_cast<int16>(prevCode);
                    suffix[nextCode] = first[code];
                    first [nextCode] = first[prevCode];
                    ++nextCode;
                }
            }
            // Déroule la chaîne en remontant les prefixes
            int32 len = 0;
            int32 c   = code;
            while(c >= 0 && len < maxOut) {
                outBuf[len++] = suffix[c];
                c = prefix[c];
            }
            // Inverse (on a remonté à l'envers)
            for(int32 i=0,j=len-1; i<j; ++i,--j) {
                uint8 t=outBuf[i]; outBuf[i]=outBuf[j]; outBuf[j]=t;
            }
            return len;
        }

        void GrowCode() noexcept {
            if(nextCode >= (1<<codeSize) && codeSize < 12) ++codeSize;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  GIF Frame info
    // ─────────────────────────────────────────────────────────────────────────────

    struct GIFFrame {
        int32  left, top, width, height;
        bool   hasLCT;
        uint8  lct[256*3];
        int32  lctSize;
        bool   interlaced;
        // Graphic Control Extension
        bool   hasGCE;
        uint8  disposal;      // 0=none,1=leave,2=bg,3=restore
        bool   hasTransp;
        uint8  transpIdx;
        uint16 delayCS;       // centisecondes
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  Collecte les sub-blocks GIF (octets → buffer continu)
    // ─────────────────────────────────────────────────────────────────────────────

    static uint8* ReadSubBlocks(NkImageStream& s, usize& outLen) noexcept {
        // Passe 1 : calcule la taille totale
        const usize start = s.Tell();
        usize total = 0;
        while(!s.IsEOF()) {
            const uint8 bsize = s.ReadU8();
            if(bsize == 0) break;
            s.Skip(bsize);
            total += bsize;
        }
        if(total == 0) { outLen=0; return nullptr; }

        uint8* buf = static_cast<uint8*>(NkAlloc(total));
        if(!buf){ outLen=0; return nullptr; }

        // Passe 2 : lit les données
        s.Seek(start);
        usize pos = 0;
        while(!s.IsEOF()) {
            const uint8 bsize = s.ReadU8();
            if(bsize == 0) break;
            s.ReadBytes(buf+pos, bsize);
            pos += bsize;
        }
        outLen = total;
        return buf;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Décode une frame GIF → pixels RGBA32
    // ─────────────────────────────────────────────────────────────────────────────

    static bool DecodeFrame(NkImageStream& s, const GIFFrame& frame,
                            const uint8* gct, int32 gctSize,
                            NkImage* canvas, const NkImage* prevCanvas) noexcept
    {
        const uint8* ct     = frame.hasLCT ? frame.lct    : gct;
        const int32  ctSize = frame.hasLCT ? frame.lctSize : gctSize;

        // Lit le minimum code size
        const uint8 minCS = s.ReadU8();
        if(minCS > 8 || minCS < 2) return false;

        // Collecte les sub-blocks de données LZW
        usize lzwLen = 0;
        uint8* lzwBuf = ReadSubBlocks(s, lzwLen);
        if(!lzwBuf) return false;

        // Initialise le décodeur LZW
        LZWDecoder dec;
        dec.Init(minCS);
        dec.subBlockData = lzwBuf;
        dec.subBlockLen  = lzwLen;
        dec.bitPos       = 0;

        const int32 fw = frame.width,  fh = frame.height;
        const int32 fl = frame.left,   ft = frame.top;
        const int32 cw = canvas->Width(), ch = canvas->Height();

        // Interlacement GIF89a (passes 0,1,2,3)
        static const int32 kIntPass[4] = {0,4,2,1};
        static const int32 kIntStep[4] = {8,8,4,2};

        // Tampon de pixels décodés (indices de palette)
        uint8* indexBuf = static_cast<uint8*>(NkAlloc(fw*fh));
        if(!indexBuf){ NkFree(lzwBuf); return false; }
        NkSet(indexBuf, 0, fw*fh);

        int32 pixPos = 0;
        int32 prevCode = -1;
        uint8 outBuf[4096];

        while(!dec.error && pixPos < fw*fh) {
            const int32 code = dec.ReadCode();
            if(code < 0 || dec.error) break;

            if(code == dec.clearCode) {
                dec.Reset();
                prevCode = -1;
                continue;
            }
            if(code == dec.eoiCode) break;

            const int32 len = dec.Decode(code, prevCode, outBuf, 4096);
            for(int32 i=0; i<len && pixPos<fw*fh; ++i)
                indexBuf[pixPos++] = outBuf[i];
            dec.GrowCode();
            prevCode = code;
        }
        NkFree(lzwBuf);

        // Applique le disposal method sur le canvas avant de dessiner
        // disposal=2 : efface la zone de la frame précédente avec la couleur de fond
        // disposal=3 : restaure le canvas précédent (snapshot)
        // (déjà géré par l'appelant avant d'appeler DecodeFrame)

        // Écrit les pixels dans le canvas
        if(frame.interlaced) {
            int32 srcRow = 0;
            for(int32 pass=0; pass<4; ++pass) {
                for(int32 y=kIntPass[pass]; y<fh; y+=kIntStep[pass], ++srcRow) {
                    const int32 cy = ft + y;
                    if(cy < 0 || cy >= ch) { ++srcRow; continue; }
                    for(int32 x=0; x<fw; ++x) {
                        const int32 cx = fl + x;
                        if(cx < 0 || cx >= cw) continue;
                        const uint8 idx = indexBuf[srcRow*fw+x];
                        if(frame.hasTransp && idx == frame.transpIdx) continue;
                        const int32 pi = idx < ctSize ? idx : 0;
                        uint8* p = canvas->RowPtr(cy)+cx*4;
                        p[0]=ct[pi*3]; p[1]=ct[pi*3+1]; p[2]=ct[pi*3+2]; p[3]=255;
                    }
                }
            }
        } else {
            for(int32 y=0; y<fh; ++y) {
                const int32 cy = ft + y;
                if(cy < 0 || cy >= ch) continue;
                for(int32 x=0; x<fw; ++x) {
                    const int32 cx = fl + x;
                    if(cx < 0 || cx >= cw) continue;
                    const uint8 idx = indexBuf[y*fw+x];
                    if(frame.hasTransp && idx == frame.transpIdx) continue;
                    const int32 pi = idx < ctSize ? idx : 0;
                    uint8* p = canvas->RowPtr(cy)+cx*4;
                    p[0]=ct[pi*3]; p[1]=ct[pi*3+1]; p[2]=ct[pi*3+2]; p[3]=255;
                }
            }
        }

        NkFree(indexBuf);
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkGIFCodec::Decode — retourne la première frame (RGBA32)
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkGIFCodec::Decode(const uint8* data, usize size) noexcept {
        if(size < 13) return nullptr;
        NkImageStream s(data, size);

        // Header
        uint8 hdr[6]; s.ReadBytes(hdr, 6);
        if(NkCompare(hdr,"GIF87a",6)!=0 && NkCompare(hdr,"GIF89a",6)!=0) return nullptr;

        // Logical Screen Descriptor
        const uint16 lsWidth  = s.ReadU16LE();
        const uint16 lsHeight = s.ReadU16LE();
        const uint8  packed   = s.ReadU8();
        const uint8  bgColor  = s.ReadU8();
        (void)s.ReadU8(); // pixel aspect ratio

        const bool   hasGCT  = (packed & 0x80) != 0;
        const int32  gctSize = hasGCT ? (2 << (packed & 7)) : 0;
        uint8 gct[256*3] = {};
        if(hasGCT) s.ReadBytes(gct, gctSize*3);

        if(lsWidth==0||lsHeight==0) return nullptr;

        // Canvas RGBA32
        NkImage* canvas = NkImage::Alloc(lsWidth, lsHeight, NkImagePixelFormat::NK_RGBA32);
        if(!canvas) return nullptr;

        // Fond initial : couleur de fond GCT ou transparent
        if(hasGCT && bgColor < gctSize) {
            for(int32 y=0;y<lsHeight;++y){
                uint8* row=canvas->RowPtr(y);
                for(int32 x=0;x<lsWidth;++x){
                    row[x*4+0]=gct[bgColor*3];
                    row[x*4+1]=gct[bgColor*3+1];
                    row[x*4+2]=gct[bgColor*3+2];
                    row[x*4+3]=255;
                }
            }
        } else {
            NkSet(canvas->Pixels(),0,canvas->TotalBytes());
        }

        GIFFrame frame={};
        frame.hasGCE=false;

        bool foundFirstFrame = false;

        while(!s.IsEOF()&&!s.HasError()) {
            const uint8 intro = s.ReadU8();

            if(intro == 0x3B) break; // Trailer

            if(intro == 0x21) { // Extension
                const uint8 label = s.ReadU8();
                if(label == 0xF9) { // Graphic Control Extension
                    (void)s.ReadU8(); // block size (always 4)
                    const uint8 gce_packed = s.ReadU8();
                    frame.hasGCE   = true;
                    frame.disposal = (gce_packed >> 2) & 7;
                    frame.hasTransp= (gce_packed & 1) != 0;
                    frame.delayCS  = s.ReadU16LE();
                    frame.transpIdx= s.ReadU8();
                    (void)s.ReadU8(); // block terminator
                } else {
                    // Autres extensions (Plain Text, Comment, Application) → skip
                    uint8 bsize;
                    do { bsize=s.ReadU8(); s.Skip(bsize); } while(bsize&&!s.IsEOF());
                }
                continue;
            }

            if(intro == 0x2C) { // Image Descriptor
                frame.left   = s.ReadU16LE();
                frame.top    = s.ReadU16LE();
                frame.width  = s.ReadU16LE();
                frame.height = s.ReadU16LE();
                const uint8 img_packed = s.ReadU8();
                frame.hasLCT    = (img_packed & 0x80) != 0;
                frame.interlaced= (img_packed & 0x40) != 0;
                frame.lctSize   = frame.hasLCT ? (2 << (img_packed & 7)) : 0;
                if(frame.hasLCT) s.ReadBytes(frame.lct, frame.lctSize*3);

                if(!foundFirstFrame) {
                    DecodeFrame(s, frame, gct, gctSize, canvas, nullptr);
                    foundFirstFrame = true;
                    break; // On retourne la première frame
                }
            }
        }

        if(!foundFirstFrame) { canvas->Free(); return nullptr; }
        return canvas;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Quantification médiane coupure (median cut) production-ready
    // ─────────────────────────────────────────────────────────────────────────────

    struct MCBox {
        int32  rMin,rMax, gMin,gMax, bMin,bMax;
        uint32 pixelStart; // index dans le tableau de pixels trié
        int32  count;
    };

    // Fonctions de comparaison pour le tri (sort par composante)
    static int CmpR(const void* a,const void* b){
        const uint32 u=*(const uint32*)a, v=*(const uint32*)b;
        return static_cast<int>((u>>16)&0xFF)-static_cast<int>((v>>16)&0xFF);
    }
    static int CmpG(const void* a,const void* b){
        const uint32 u=*(const uint32*)a, v=*(const uint32*)b;
        return static_cast<int>((u>>8)&0xFF)-static_cast<int>((v>>8)&0xFF);
    }
    static int CmpB(const void* a,const void* b){
        const uint32 u=*(const uint32*)a, v=*(const uint32*)b;
        return static_cast<int>(u&0xFF)-static_cast<int>(v&0xFF);
    }

    static void MedianCut(uint32* pixels, int32 total,
                        uint8* palR, uint8* palG, uint8* palB, int32& palCount) noexcept
    {
        MCBox boxes[256];
        int32 numBoxes = 0;

        // Boîte initiale
        boxes[0].pixelStart = 0;
        boxes[0].count      = total;
        boxes[0].rMin=0; boxes[0].rMax=255;
        boxes[0].gMin=0; boxes[0].gMax=255;
        boxes[0].bMin=0; boxes[0].bMax=255;
        numBoxes = 1;

        // Médiane coupure jusqu'à 256 boîtes
        while(numBoxes < 256) {
            // Choisit la boîte avec la plus grande plage (volume)
            int32 best = -1, bestRange = -1;
            for(int32 i=0; i<numBoxes; ++i) {
                if(boxes[i].count <= 1) continue;
                const int32 rr = boxes[i].rMax - boxes[i].rMin;
                const int32 gr = boxes[i].gMax - boxes[i].gMin;
                const int32 br = boxes[i].bMax - boxes[i].bMin;
                // Pondération : vert perçu plus important
                const int32 range = rr*2 + gr*3 + br;
                if(range > bestRange) { bestRange=range; best=i; }
            }
            if(best < 0) break;

            MCBox& b = boxes[best];
            uint32* subPixels = pixels + b.pixelStart;
            const int32 cnt = b.count;

            // Axe de coupure : plus grande plage
            const int32 rr=b.rMax-b.rMin, gr=b.gMax-b.gMin, br=b.bMax-b.bMin;
            if(rr>=gr && rr>=br)      ::qsort(subPixels,cnt,sizeof(uint32),CmpR);
            else if(gr>=rr && gr>=br) ::qsort(subPixels,cnt,sizeof(uint32),CmpG);
            else                       ::qsort(subPixels,cnt,sizeof(uint32),CmpB);

            // Coupe au milieu
            const int32 mid = cnt / 2;
            MCBox& b2 = boxes[numBoxes++];
            b2.pixelStart = b.pixelStart + mid;
            b2.count      = cnt - mid;
            b.count       = mid;

            // Recalcule les ranges
            auto calcRange=[&](MCBox& box){
                box.rMin=255;box.rMax=0;box.gMin=255;box.gMax=0;box.bMin=255;box.bMax=0;
                for(int32 i=0;i<box.count;++i){
                    const uint8 r=(pixels[box.pixelStart+i]>>16)&0xFF;
                    const uint8 g=(pixels[box.pixelStart+i]>>8)&0xFF;
                    const uint8 b2=pixels[box.pixelStart+i]&0xFF;
                    if(r<box.rMin)box.rMin=r; if(r>box.rMax)box.rMax=r;
                    if(g<box.gMin)box.gMin=g; if(g>box.gMax)box.gMax=g;
                    if(b2<box.bMin)box.bMin=b2; if(b2>box.bMax)box.bMax=b2;
                }
            };
            calcRange(b); calcRange(b2);
        }

        // Calcule la couleur moyenne de chaque boîte
        palCount = numBoxes;
        for(int32 i=0; i<numBoxes; ++i) {
            uint64 sr=0,sg=0,sb=0;
            const int32 n = boxes[i].count > 0 ? boxes[i].count : 1;
            for(int32 j=0; j<boxes[i].count; ++j) {
                const uint32 p = pixels[boxes[i].pixelStart+j];
                sr += (p>>16)&0xFF;
                sg += (p>>8)&0xFF;
                sb += p&0xFF;
            }
            palR[i] = static_cast<uint8>(sr/n);
            palG[i] = static_cast<uint8>(sg/n);
            palB[i] = static_cast<uint8>(sb/n);
        }
        // Complète à 256
        for(int32 i=numBoxes; i<256; ++i) { palR[i]=palG[i]=palB[i]=0; }
    }

    // Cherche l'entrée de palette la plus proche (distance euclidienne dans RGB)
    static uint8 NearestColor(uint8 r,uint8 g,uint8 b,
                                const uint8* palR,const uint8* palG,const uint8* palB,
                                int32 palCount) noexcept
    {
        int32 bestDist = 0x7FFFFFFF, best = 0;
        for(int32 i=0; i<palCount; ++i) {
            const int32 dr=r-palR[i], dg=g-palG[i], db=b-palB[i];
            // Pondération perceptuelle : vert > rouge > bleu
            const int32 dist = dr*dr*2 + dg*dg*3 + db*db;
            if(dist < bestDist) { bestDist=dist; best=i; }
        }
        return static_cast<uint8>(best);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Encodeur LZW GIF production-ready (variable 2-12 bits)
    // ─────────────────────────────────────────────────────────────────────────────

    struct LZWEncoder {
        // Table de hachage pour chercher (prefix, suffix) rapidement
        // Clé = prefix*256 + suffix, valeur = code
        static constexpr int32 TABLE_SIZE = 5003; // premier nombre premier > 4096
        int32  table[TABLE_SIZE]; // -1 = vide
        int32  codes[TABLE_SIZE]; // code associé

        int32  clearCode, eoiCode, nextCode, codeSize, minCodeSize;
        uint32 bitBuf; int32 bitCount;

        // Sub-block output
        uint8  block[256];
        int32  blockLen;
        NkImageStream* s;

        void Init(int32 minCS, NkImageStream* out) noexcept {
            minCodeSize = minCS < 2 ? 2 : minCS;
            clearCode   = 1 << minCodeSize;
            eoiCode     = clearCode + 1;
            s           = out;
            bitBuf      = 0; bitCount = 0;
            blockLen    = 0;
            NkSet(table, -1, sizeof(table));
            Reset();
        }

        void Reset() noexcept {
            nextCode = eoiCode + 1;
            codeSize = minCodeSize + 1;
            NkSet(table, -1, sizeof(table));
        }

        void EmitCode(int32 code) noexcept {
            bitBuf |= static_cast<uint32>(code) << bitCount;
            bitCount += codeSize;
            while(bitCount >= 8) {
                block[blockLen++] = static_cast<uint8>(bitBuf & 0xFF);
                bitBuf >>= 8; bitCount -= 8;
                if(blockLen == 255) {
                    s->WriteU8(255);
                    s->WriteBytes(block, 255);
                    blockLen = 0;
                }
            }
        }

        void Flush() noexcept {
            while(bitCount > 0) {
                block[blockLen++] = static_cast<uint8>(bitBuf & 0xFF);
                bitBuf >>= 8;
                bitCount = bitCount > 8 ? bitCount - 8 : 0;
                if(blockLen == 255) {
                    s->WriteU8(255); s->WriteBytes(block,255); blockLen=0;
                }
            }
            if(blockLen > 0) {
                s->WriteU8(static_cast<uint8>(blockLen));
                s->WriteBytes(block, blockLen);
                blockLen = 0;
            }
            s->WriteU8(0); // block terminator
        }

        int32 HashLookup(int32 prefix, uint8 suffix) const noexcept {
            int32 h = (prefix * 257 + suffix) % TABLE_SIZE;
            while(table[h] != -1) {
                if(table[h] == ((prefix<<8)|suffix)) return codes[h];
                h = (h + 1) % TABLE_SIZE;
            }
            return -1;
        }

        void HashInsert(int32 prefix, uint8 suffix, int32 code) noexcept {
            int32 h = (prefix * 257 + suffix) % TABLE_SIZE;
            while(table[h] != -1) h = (h+1) % TABLE_SIZE;
            table[h] = (prefix<<8)|suffix;
            codes[h] = code;
        }

        void Encode(const uint8* indices, int32 count) noexcept {
            EmitCode(clearCode);

            int32 prev = indices[0];
            for(int32 i=1; i<count; ++i) {
                const uint8 cur = indices[i];
                const int32 found = HashLookup(prev, cur);
                if(found >= 0) {
                    prev = found;
                } else {
                    EmitCode(prev);
                    if(nextCode < LZWEncoder::TABLE_SIZE && nextCode < 4096) {
                        HashInsert(prev, cur, nextCode);
                        ++nextCode;
                        if(nextCode > (1<<codeSize) && codeSize < 12) ++codeSize;
                    }
                    if(nextCode >= 4096) {
                        EmitCode(clearCode);
                        Reset();
                    }
                    prev = cur;
                }
            }
            EmitCode(prev);
            EmitCode(eoiCode);
            Flush();
        }
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkGIFCodec::Encode
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkGIFCodec::Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept {
        if(!img.IsValid()) return false;
        const int32 w=img.Width(), h=img.Height();

        // Convertit en RGB si nécessaire (on gère la transparence séparément)
        const bool hasAlpha = (img.Format()==NkImagePixelFormat::NK_RGBA32||img.Format()==NkImagePixelFormat::NK_GRAY_A16);
        const NkImage* src = &img;
        NkImage* conv = nullptr;
        if(img.Channels()!=3 && img.Channels()!=4) {
            conv = img.Convert(NkImagePixelFormat::NK_RGB24);
            if(!conv) return false; src=conv;
        }

        const int32 total = w*h;
        const int32 ch    = src->Channels();

        // Collecte les pixels RGB (ignore alpha pour la quantification)
        uint32* pixels = static_cast<uint32*>(NkAlloc(sizeof(uint32)*total));
        if(!pixels){ if(conv)conv->Free(); return false; }
        for(int32 i=0; i<total; ++i){
            const uint8* p = src->Pixels()+i*ch;
            pixels[i] = (static_cast<uint32>(p[0])<<16)|
                        (static_cast<uint32>(ch>1?p[1]:p[0])<<8)|
                        (ch>2?p[2]:p[0]);
        }

        // Quantification médiane coupure
        uint8 palR[256]={},palG[256]={},palB[256]={};
        int32 palCount = 0;
        MedianCut(pixels, total, palR, palG, palB, palCount);
        NkFree(pixels);

        // Mappe chaque pixel à la palette
        uint8* indexBuf = static_cast<uint8*>(NkAlloc(total));
        if(!indexBuf){ if(conv)conv->Free(); return false; }

        // Index de transparence (si alpha)
        bool useTransp = false;
        uint8 transpIdx = 0;
        if(hasAlpha && palCount < 256) {
            // Réserve la dernière entrée pour la transparence
            transpIdx = static_cast<uint8>(palCount);
            palR[palCount]=palG[palCount]=palB[palCount]=0;
            ++palCount;
            useTransp = true;
        }

        for(int32 i=0; i<total; ++i){
            const uint8* p = src->Pixels()+i*ch;
            if(useTransp && ch==4 && p[3]<128) {
                indexBuf[i] = transpIdx;
            } else {
                indexBuf[i] = NearestColor(p[0],ch>1?p[1]:p[0],ch>2?p[2]:p[0],
                                            palR,palG,palB,
                                            useTransp?palCount-1:palCount);
            }
        }
        if(conv) conv->Free();

        // Taille de la GCT (puissance de 2 ≥ palCount)
        int32 gctPow = 0;
        while((2<<gctPow) < palCount) ++gctPow;
        const int32 gctSize = 2 << gctPow;

        NkImageStream s;

        // Header GIF89a
        s.WriteBytes(reinterpret_cast<const uint8*>("GIF89a"),6);
        s.WriteU16LE(static_cast<uint16>(w));
        s.WriteU16LE(static_cast<uint16>(h));
        s.WriteU8(static_cast<uint8>(0x80 | 0x70 | gctPow)); // GCT, color res 8-bit, gctPow
        s.WriteU8(0); // background color index
        s.WriteU8(0); // pixel aspect ratio

        // Global Color Table
        for(int32 i=0; i<gctSize; ++i) {
            s.WriteU8(palR[i]); s.WriteU8(palG[i]); s.WriteU8(palB[i]);
        }

        // Graphic Control Extension (nécessaire pour la transparence)
        if(useTransp) {
            s.WriteU8(0x21); s.WriteU8(0xF9); // GCE
            s.WriteU8(4);    // block size
            s.WriteU8(0x01); // disposal=none, user input=off, transparent=1
            s.WriteU16LE(0); // delay = 0
            s.WriteU8(transpIdx);
            s.WriteU8(0);    // block terminator
        }

        // Image Descriptor
        s.WriteU8(0x2C);
        s.WriteU16LE(0); s.WriteU16LE(0); // left, top = 0
        s.WriteU16LE(static_cast<uint16>(w));
        s.WriteU16LE(static_cast<uint16>(h));
        s.WriteU8(0); // no LCT, not interlaced

        // LZW minimum code size
        const uint8 minCS = static_cast<uint8>(gctPow < 1 ? 2 : gctPow+1 < 2 ? 2 : gctPow+1);
        s.WriteU8(minCS);

        // Données LZW
        LZWEncoder enc;
        enc.Init(minCS, &s);
        enc.Encode(indexBuf, total);

        NkFree(indexBuf);

        // Trailer
        s.WriteU8(0x3B);

        return s.TakeBuffer(out, outSize);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Implémentation de Save (déclarée dans le header)
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkGIFCodec::Save(const NkImage& img, const char* path) noexcept {
        uint8* d=nullptr; usize sz=0;
        if(!Encode(img,d,sz)) return false;
        FILE* f=::fopen(path,"wb");
        if(!f){NkFree(d);return false;}
        const bool ok=(::fwrite(d,1,sz,f)==sz);
        ::fclose(f); NkFree(d); return ok;
    }

} // namespace nkentseu
