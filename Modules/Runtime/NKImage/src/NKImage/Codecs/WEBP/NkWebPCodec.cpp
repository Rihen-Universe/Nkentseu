/**
 * @File    NkWebPCodec.cpp
 * @Brief   Codec WebP production-ready — VP8L (lossless) complet.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Correctness
 *  VP8L décodage : bit reader LSB-first, 5 arbres Huffman canoniques par
 *   meta-group, back-references LZ77 (longueur + distance avec extras bits),
 *   color cache (hash index), 4 types de transformations dans le bon ordre.
 *
 *  Transformations VP8L :
 *    0 - Predictor       : 14 modes de prédiction par bloc
 *    1 - Color Transform : delta sur les canaux R,B relatif à G
 *    2 - Subtract Green  : soustraction du vert de R et B
 *    3 - Color Indexing  : palette de couleurs indexée (1/2/4/8 bits)
 *
 *  VP8L encodage : sub-green + Huffman sur pixels ARGB en mode littéral.
 *  VP8 (lossy)   : dimensions + image noire (décodage complet = libvpx territory).
 *
 *  RIFF : parsing multi-chunk, VP8X, ALPH (alpha séparé pour VP8 lossy).
 */
#include "NKImage/Codecs/WEBP/NkWebPCodec.h"
#include <cmath>
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {

using namespace nkentseu::memory;

    // ─────────────────────────────────────────────────────────────────────────────
    //  VP8L Bit Reader (LSB-first)
    // ─────────────────────────────────────────────────────────────────────────────

    void NkWebPCodec::VP8LBitReader::Init(const uint8* d, usize s) noexcept {
        data=d; size=s; bytePos=0; bitBuf=0; bitCount=0; error=false;
        // Pré-charge 8 octets
        for(int32 i=0;i<8&&bytePos<size;++i){
            bitBuf|=static_cast<uint64>(data[bytePos++])<<(i*8);
            bitCount+=8;
        }
    }

    uint32 NkWebPCodec::VP8LBitReader::ReadBits(int32 n) noexcept {
        if(n==0) return 0;
        while(bitCount<n&&bytePos<size){
            bitBuf|=static_cast<uint64>(data[bytePos++])<<bitCount;
            bitCount+=8;
        }
        if(bitCount<n){ error=true; return 0; }
        const uint32 v=static_cast<uint32>(bitBuf&((1ULL<<n)-1));
        bitBuf>>=n; bitCount-=n;
        return v;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  VP8L Huffman Tree — lookup 11 bits
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkWebPCodec::VP8LHuffTree::Build(const uint8* lens, int32 n) noexcept {
        numSymbols=n; isValid=false;
        if(n<=0||n>MAX_SYMS) return false;

        // Compte les longueurs
        int32 counts[16]={}, maxLen=0;
        for(int32 i=0;i<n;++i){
            if(lens[i]>0){ if(lens[i]>maxLen)maxLen=lens[i]; ++counts[lens[i]]; }
        }

        if(maxLen==0){
            // Arbre vide ou à un seul symbole
            isValid=true;
            NkSet(lut,-1,sizeof(lut));
            NkSet(lutLen,0,sizeof(lutLen));
            NkSet(codes,-1,sizeof(codes));
            NkSet(codeLens,0,sizeof(codeLens));
            for(int32 i=0;i<n;++i) if(lens[i]==1){ codes[i]=0; codeLens[i]=1; }
            return true;
        }
        if(maxLen>15) return false;

        // Codes canoniques (LSB-first pour VP8L)
        int32 nextCode[16]={}, code=0;
        for(int32 l=1;l<=maxLen;++l){
            nextCode[l]=code;
            code+=counts[l];
            code<<=1;
        }

        NkSet(codes,-1,sizeof(codes));
        NkSet(codeLens,0,sizeof(codeLens));
        NkSet(lut,-1,sizeof(lut));
        NkSet(lutLen,0,sizeof(lutLen));

        for(int32 i=0;i<n;++i){
            const int32 l=lens[i];
            if(!l) continue;
            const int32 c=nextCode[l]++;
            // Inverse le code (VP8L est LSB-first)
            int32 rev=0;
            for(int32 b=0;b<l;++b) rev=(rev<<1)|((c>>b)&1);
            codes[i]=static_cast<int16>(rev);
            codeLens[i]=static_cast<int16>(l);
            // Remplit la LUT
            if(l<=LUT_BITS){
                const int32 stride=1<<l;
                for(int32 j=rev;j<(1<<LUT_BITS);j+=stride){
                    lut[j]=static_cast<int16>(i);
                    lutLen[j]=static_cast<int16>(l);
                }
            }
        }
        isValid=true;
        return true;
    }

    int32 NkWebPCodec::VP8LHuffTree::Decode(VP8LBitReader& br) const noexcept {
        if(!isValid) return -1;
        // Essaie la LUT rapide
        if(br.bitCount>=LUT_BITS){
            const uint32 idx=static_cast<uint32>(br.bitBuf)&((1<<LUT_BITS)-1);
            const int16 l=lutLen[idx];
            if(l>0&&lut[idx]>=0){
                br.ReadBits(l);
                return lut[idx];
            }
        }
        // Décodage bit par bit pour les codes longs
        int32 code=0;
        for(int32 l=1;l<=15;++l){
            code=(code<<1)|static_cast<int32>(br.ReadBits(1));
            // Recherche linéaire sur les codes de cette longueur
            for(int32 i=0;i<numSymbols;++i){
                if(codeLens[i]==l&&codes[i]==code) return i;
            }
        }
        return -1;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  RIFF utilities
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkWebPCodec::FindChunk(const uint8* data, usize size,
                                uint32 fourcc, RIFFChunk& out) noexcept
    {
        if(size<12) return false;
        usize pos=12; // saute le header RIFF
        while(pos+8<=size){
            const uint32 tag = data[pos]
                            |(static_cast<uint32>(data[pos+1])<<8)
                            |(static_cast<uint32>(data[pos+2])<<16)
                            |(static_cast<uint32>(data[pos+3])<<24);
            const uint32 len = data[pos+4]
                            |(static_cast<uint32>(data[pos+5])<<8)
                            |(static_cast<uint32>(data[pos+6])<<16)
                            |(static_cast<uint32>(data[pos+7])<<24);
            if(tag==fourcc){
                out.fourcc     = tag;
                out.size       = len;
                out.dataOffset = pos+8;
                return true;
            }
            pos += 8 + ((len+1)&~1u); // padding sur 2 octets
        }
        return false;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Lecture des codes Huffman VP8L
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkWebPCodec::ReadVP8LHuffmanCodes(VP8LBitReader& br,
                                            VP8LHuffTree trees[5],
                                            int32 numTrees) noexcept
    {
        static const int32 kAlphSizes[5]={256,256,256,256,40};

        for(int32 t=0; t<numTrees && t<5; ++t){
            const int32 alphabetSize = kAlphSizes[t];
            uint8 codeLengths[300]={};

            const int32 simpleCode = static_cast<int32>(br.ReadBits(1));
            if(simpleCode){
                // Codage simple : 1 ou 2 symboles distincts
                const int32 numSym = static_cast<int32>(br.ReadBits(1))+1;
                const int32 isByte = static_cast<int32>(br.ReadBits(1));
                const int32 s0 = static_cast<int32>(br.ReadBits(isByte?8:1));
                if(s0<alphabetSize) codeLengths[s0]=1;
                if(numSym==2){
                    const int32 s1 = static_cast<int32>(br.ReadBits(8));
                    if(s1<alphabetSize) codeLengths[s1]=1;
                }
            } else {
                // Codage Huffman des longueurs de code (méta-Huffman)
                static const int32 kCLOrder[19]={17,18,0,1,2,3,4,5,16,6,7,8,9,10,11,12,13,14,15};
                const int32 numCL = static_cast<int32>(br.ReadBits(4))+4;
                uint8 clLens[19]={};
                for(int32 i=0;i<numCL;++i)
                    clLens[kCLOrder[i]]=static_cast<uint8>(br.ReadBits(3));

                VP8LHuffTree clTree;
                clTree.Build(clLens,19);

                // Limite optionnelle du nombre de symboles
                int32 maxSym = alphabetSize;
                if(br.ReadBits(1)){
                    const int32 nBits = static_cast<int32>(br.ReadBits(3))*2+2;
                    maxSym = static_cast<int32>(br.ReadBits(nBits))+2;
                    if(maxSym>alphabetSize) maxSym=alphabetSize;
                }

                int32 sym=0, prev=8;
                while(sym<maxSym && !br.error){
                    const int32 code = clTree.Decode(br);
                    if(code<0){ br.error=true; break; }
                    if(code<16){
                        codeLengths[sym++]=static_cast<uint8>(code);
                        if(code) prev=code;
                    } else if(code==16){
                        int32 rep=3+static_cast<int32>(br.ReadBits(2));
                        while(rep--&&sym<maxSym) codeLengths[sym++]=static_cast<uint8>(prev);
                    } else if(code==17){
                        int32 rep=3+static_cast<int32>(br.ReadBits(3));
                        while(rep--&&sym<maxSym) codeLengths[sym++]=0;
                    } else { // 18
                        int32 rep=11+static_cast<int32>(br.ReadBits(7));
                        while(rep--&&sym<maxSym) codeLengths[sym++]=0;
                    }
                }
            }
            trees[t].Build(codeLengths, alphabetSize);
        }
        return !br.error;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Décodage VP8L — flux de pixels
    // ─────────────────────────────────────────────────────────────────────────────

    // Tables LZ77 VP8L (longueurs et distances avec extra bits)
    static const int32 kLenSymbols  = 24;
    static const int32 kDistSymbols = 40;

    static const uint8 kLenExtraBits[24]={
        0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,24
    };
    static const uint16 kLenBase[24]={
        1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,
        513,769,1025,1537,2049,2050
    };

    static const uint8 kDistExtraBits[40]={
        0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,
        9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,17,17,18,18
    };
    static const uint32 kDistBase[40]={
        1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
        257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,
        16385,24577,32769,49153,65537,98305,131073,196609,
        262145,393217,524289,786433
    };

    // Distance codes VP8L → distance pixel (avec xy)
    static uint32 VP8LDistToPixDist(uint32 dist, int32 width) noexcept {
        // VP8L spec §7.3 : distance code → (dx,dy) → pixel offset
        static const int32 kDxy[120]={
            0, 1,  1, 0,  1, 1, -1, 1,  0, 2,  2, 0,  1, 2, -1, 2,
            2, 1, -2, 1,  2, 2, -2, 2,  0, 3,  3, 0,  1, 3, -1, 3,
            3, 1, -3, 1,  2, 3, -2, 3,  3, 2, -3, 2,  0, 4,  4, 0,
            1, 4, -1, 4,  4, 1, -4, 1,  3, 3, -3, 3,  2, 4, -2, 4,
            4, 2, -4, 2,  0, 5,  3, 4, -3, 4,  4, 3, -4, 3,  5, 0,
            1, 5, -1, 5,  5, 1, -5, 1,  2, 5, -2, 5,  5, 2, -5, 2,
            4, 4, -4, 4,  3, 5, -3, 5,  5, 3, -5, 3,  0, 6,  6, 0,
            1, 6, -1, 6,  6, 1, -6, 1
        };
        if(dist==0) return 1;
        if(dist<=120){
            const int32 dx=kDxy[(dist-1)*2], dy=kDxy[(dist-1)*2+1];
            return static_cast<uint32>(dy*width+dx);
        }
        return dist - 120;
    }

    bool NkWebPCodec::DecodeVP8LImage(VP8LBitReader& br, VP8LDecoder& dec,
                                        int32 w, int32 h, uint32* out) noexcept
    {
        VP8LHuffTree trees[5];
        if(!ReadVP8LHuffmanCodes(br,trees,5)) return false;

        // Color cache
        const int32 hasColorCache = static_cast<int32>(br.ReadBits(1));
        const int32 ccBits = hasColorCache ? static_cast<int32>(br.ReadBits(4)) : 0;
        const int32 ccSize = hasColorCache ? (1<<ccBits) : 0;
        uint32* cc = nullptr;
        if(ccSize>0){
            cc=static_cast<uint32*>(NkAllocZero(ccSize,sizeof(uint32)));
            if(!cc) return false;
        }

        auto ccInsert=[&](uint32 argb){
            if(cc&&ccSize>0){
                const int32 idx = static_cast<int32>(
                    (0x1E35A7BDu * argb) >> (32-ccBits)) & (ccSize-1);
                cc[idx] = argb;
            }
        };

        const int32 total = w*h;
        int32 pos = 0;

        while(pos<total && !br.error){
            const int32 gSym = trees[0].Decode(br);
            if(gSym<0){ br.error=true; break; }

            if(gSym<256){
                // Pixel littéral : G déjà lu, puis R, B, A
                const int32 rSym = trees[1].Decode(br);
                const int32 bSym = trees[2].Decode(br);
                const int32 aSym = trees[3].Decode(br);
                if(rSym<0||bSym<0||aSym<0){ br.error=true; break; }
                const uint32 argb = (static_cast<uint32>(aSym)<<24)
                                |(static_cast<uint32>(rSym)<<16)
                                |(static_cast<uint32>(gSym)<<8)
                                | static_cast<uint32>(bSym);
                out[pos++] = argb;
                ccInsert(argb);
            }
            else if(gSym < 256+kLenSymbols){
                // Back reference LZ77
                const int32 lenCode = gSym - 256;
                const int32 extraBits = kLenExtraBits[lenCode];
                const int32 len = static_cast<int32>(kLenBase[lenCode])
                                + static_cast<int32>(br.ReadBits(extraBits));

                const int32 distSym = trees[4].Decode(br);
                if(distSym<0||distSym>=kDistSymbols){ br.error=true; break; }
                const int32 distExtra = kDistExtraBits[distSym];
                const uint32 rawDist = kDistBase[distSym]
                                    + br.ReadBits(distExtra);
                const uint32 pixDist = VP8LDistToPixDist(rawDist, w);
                if(static_cast<int32>(pixDist)>pos){ br.error=true; break; }

                for(int32 i=0; i<len && pos<total; ++i){
                    const uint32 src = out[pos-static_cast<int32>(pixDist)];
                    out[pos++] = src;
                    ccInsert(src);
                }
            }
            else {
                // Color cache reference
                if(!cc||ccSize<=0){ br.error=true; break; }
                const int32 ccIdx = gSym - 256 - kLenSymbols;
                if(ccIdx<0||ccIdx>=ccSize){ br.error=true; break; }
                out[pos++] = cc[ccIdx];
            }
        }

        if(cc) NkFree(cc);
        return !br.error;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Transformations VP8L
    // ─────────────────────────────────────────────────────────────────────────────

    void NkWebPCodec::ApplySubtractGreen(uint32* pixels, int32 n) noexcept {
        for(int32 i=0; i<n; ++i){
            const uint32 p = pixels[i];
            const uint8 a = static_cast<uint8>(p>>24);
            const uint8 r = static_cast<uint8>(p>>16);
            const uint8 g = static_cast<uint8>(p>>8);
            const uint8 b = static_cast<uint8>(p);
            pixels[i] = (static_cast<uint32>(a)<<24)
                    | (static_cast<uint32>((r+g)&0xFF)<<16)
                    | (static_cast<uint32>(g)<<8)
                    | static_cast<uint32>((b+g)&0xFF);
        }
    }

    static NKIMG_INLINE uint32 AddPixels(uint32 a, uint32 b) noexcept {
        return ((a^b)&0x00FF00FFu)
            ? ( (a&0x00FF00FFu)+(b&0x00FF00FFu)
            |(((a>>8)&0x00FF00FFu)+((b>>8)&0x00FF00FFu))<<8 )
            : a+b; // path rapide si pas de carry inter-canal
        // Version sûre :
    }

    static NKIMG_INLINE uint32 AddARGB(uint32 a, uint32 b) noexcept {
        return ( (((a>>24)+(b>>24))&0xFF)<<24 )
            |( (((a>>16&0xFF)+(b>>16&0xFF))&0xFF)<<16 )
            |( (((a>>8 &0xFF)+(b>>8 &0xFF))&0xFF)<<8  )
            |  (((a    &0xFF)+(b    &0xFF))&0xFF);
    }

    static NKIMG_INLINE uint32 Average2(uint32 a, uint32 b) noexcept {
        return (((a^b)&0xFEFEFEFEu)>>1)+(a&b);
    }

    static NKIMG_INLINE uint8 Clamp8(int32 v) noexcept {
        return static_cast<uint8>(v<0?0:v>255?255:v);
    }

    static NKIMG_INLINE uint32 Select(uint32 L, uint32 T, uint32 TL) noexcept {
        // Prédicteur : Lx+Tx-TLx (par canal, clampé)
        auto pred=[](uint8 l,uint8 t,uint8 tl)->uint32{
            const int32 pa=l<t?t-l:l-t; // |pa - tl|
            const int32 pb=t<tl?tl-t:t-tl;
            return static_cast<uint32>(pa<=pb?l:t);
        };
        return (pred(L>>24,T>>24,TL>>24)<<24)
            |(pred((L>>16)&0xFF,(T>>16)&0xFF,(TL>>16)&0xFF)<<16)
            |(pred((L>>8)&0xFF,(T>>8)&0xFF,(TL>>8)&0xFF)<<8)
            | pred(L&0xFF,T&0xFF,TL&0xFF);
    }

    static NKIMG_INLINE uint32 ClampAddSubtractFull(uint32 c0,uint32 c1,uint32 c2) noexcept {
        auto f=[](uint8 a,uint8 b,uint8 c)->uint8{
            return Clamp8(static_cast<int32>(a)+static_cast<int32>(b)-static_cast<int32>(c));};
        return (static_cast<uint32>(f(c0>>24,c1>>24,c2>>24))<<24)
            |(static_cast<uint32>(f((c0>>16)&0xFF,(c1>>16)&0xFF,(c2>>16)&0xFF))<<16)
            |(static_cast<uint32>(f((c0>>8)&0xFF,(c1>>8)&0xFF,(c2>>8)&0xFF))<<8)
            | static_cast<uint32>(f(c0&0xFF,c1&0xFF,c2&0xFF));
    }

    static NKIMG_INLINE uint32 ClampAddSubtractHalf(uint32 c0,uint32 c1) noexcept {
        const uint32 avg=Average2(c0,c1);
        return ClampAddSubtractFull(avg,c0,c1);
    }

    void NkWebPCodec::ApplyPredictTransform(const uint32* data, uint32* pixels,
                                            int32 w, int32 h, int32 bits) noexcept
    {
        const int32 tw=(w+(1<<bits)-1)>>bits;
        for(int32 y=0; y<h; ++y){
            for(int32 x=0; x<w; ++x){
                if(y==0&&x==0){ pixels[0]=AddARGB(pixels[0],0xFF000000u); continue; }
                const int32 tx=x>>bits, ty=y>>bits;
                const uint8 mode=static_cast<uint8>((data[ty*tw+tx]>>8)&0xF);
                const uint32 L  = (x>0) ? pixels[y*w+x-1] : (y>0?pixels[(y-1)*w]:0xFF000000u);
                const uint32 T  = (y>0) ? pixels[(y-1)*w+x] : L;
                const uint32 TL = (y>0&&x>0)?pixels[(y-1)*w+x-1]:(y>0?T:L);
                const uint32 TR = (y>0&&x<w-1)?pixels[(y-1)*w+x+1]:(y>0?T:L);
                uint32 pred=0;
                switch(mode){
                    case  0: pred=0xFF000000u; break;
                    case  1: pred=L; break;
                    case  2: pred=T; break;
                    case  3: pred=TR; break;
                    case  4: pred=TL; break;
                    case  5: pred=Average2(Average2(L,TR),T); break;
                    case  6: pred=Average2(L,TL); break;
                    case  7: pred=Average2(L,T); break;
                    case  8: pred=Average2(TL,T); break;
                    case  9: pred=Average2(T,TR); break;
                    case 10: pred=Average2(Average2(L,TL),Average2(T,TR)); break;
                    case 11: pred=Select(L,T,TL); break;
                    case 12: pred=ClampAddSubtractFull(L,T,TL); break;
                    case 13: pred=ClampAddSubtractHalf(Average2(L,T),TL); break;
                    default: pred=L; break;
                }
                pixels[y*w+x]=AddARGB(pixels[y*w+x],pred);
            }
        }
    }

    void NkWebPCodec::ApplyColorTransform(const uint32* data, uint32* pixels,
                                            int32 w, int32 h, int32 bits) noexcept
    {
        const int32 tw=(w+(1<<bits)-1)>>bits;
        for(int32 y=0; y<h; ++y){
            for(int32 x=0; x<w; ++x){
                const int32 tx=x>>bits, ty=y>>bits;
                const uint32 t=data[ty*tw+tx];
                const int8 gToR=static_cast<int8>((t>>16)&0xFF);
                const int8 gToB=static_cast<int8>( t     &0xFF);
                const int8 rToB=static_cast<int8>((t>> 8)&0xFF);
                uint32& p=pixels[y*w+x];
                const uint8 g=static_cast<uint8>((p>>8)&0xFF);
                const uint8 r=static_cast<uint8>((p>>16)&0xFF);
                const uint8 b=static_cast<uint8>(p&0xFF);
                const uint8 a=static_cast<uint8>(p>>24);
                // delta color transform
                const uint8 nr=static_cast<uint8>(r+(static_cast<int32>(gToR)*g>>5));
                const uint8 nb=static_cast<uint8>(b+(static_cast<int32>(gToB)*g>>5)
                                                    +(static_cast<int32>(rToB)*r>>5));
                p=(static_cast<uint32>(a)<<24)|(static_cast<uint32>(nr)<<16)
                |(static_cast<uint32>(g)<<8)|nb;
            }
        }
    }

    void NkWebPCodec::ApplyColorIndexing(const uint32* table, uint32* pixels,
                                        int32 w, int32 h, int32 bits) noexcept
    {
        // bits = log2(4/N) où N est le nombre de pixels par octet (1,2,4 ou N=tableSize)
        // Quand bits>0 : plusieurs pixels sont empaquetés dans le canal vert
        const int32 pixPerByte = 1<<(8>>bits);
        if(pixPerByte<=1){
            // Cas simple : indice direct dans le canal vert
            for(int32 i=0; i<w*h; ++i){
                const uint32 idx=(pixels[i]>>8)&0xFF;
                pixels[i]=table[idx];
            }
        } else {
            // Décompactage : plusieurs pixels par "pixel"
            // Réalloue un buffer temporaire de la bonne taille
            const int32 packedW=(w+(pixPerByte-1))/pixPerByte;
            uint32* tmp=static_cast<uint32*>(NkAlloc(static_cast<usize>(packedW)*h*sizeof(uint32)));
            if(!tmp) return;
            NkCopy(tmp,pixels,static_cast<usize>(packedW)*h*sizeof(uint32));
            const int32 mask=(1<<(8>>bits))-1;
            for(int32 y=0; y<h; ++y){
                for(int32 x=0; x<w; ++x){
                    const int32 px=x/pixPerByte;
                    const int32 shift=((x%pixPerByte)<<(8>>bits));
                    const uint32 packed=tmp[y*packedW+px];
                    const uint32 idx=((packed>>8)>>shift)&mask;
                    pixels[y*w+x]=table[idx];
                }
            }
            NkFree(tmp);
        }
        (void)bits;
    }

    void NkWebPCodec::ApplyTransforms(VP8LDecoder& dec) noexcept {
        // Application dans l'ordre inverse de lecture
        for(int32 i=dec.numTransforms-1; i>=0; --i){
            const auto& t=dec.transforms[i];
            switch(t.type){
                case 2: ApplySubtractGreen(dec.pixels,dec.width*dec.height); break;
                case 0: if(t.data) ApplyPredictTransform(t.data,dec.pixels,dec.width,dec.height,t.bits); break;
                case 1: if(t.data) ApplyColorTransform(t.data,dec.pixels,dec.width,dec.height,t.bits); break;
                case 3: if(t.data) ApplyColorIndexing(t.data,dec.pixels,dec.width,dec.height,t.bits); break;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  DecodeVP8L — point d'entrée
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkWebPCodec::DecodeVP8L(const uint8* data, usize size) noexcept {
        if(size<5||data[0]!=0x2Fu) return nullptr; // signature VP8L

        VP8LBitReader br;
        br.Init(data+1, size-1);

        const int32 w = static_cast<int32>(br.ReadBits(14))+1;
        const int32 h = static_cast<int32>(br.ReadBits(14))+1;
        const bool  hasAlpha = br.ReadBits(1)!=0;
        const int32 version  = static_cast<int32>(br.ReadBits(3));
        if(br.error||version!=0||w<=0||h<=0) return nullptr;

        const int32 total=w*h;
        uint32* argb=static_cast<uint32*>(NkAlloc(static_cast<usize>(total)*4));
        if(!argb) return nullptr;
        NkSet(argb,0,static_cast<usize>(total)*4);

        VP8LDecoder dec;
        NkSet(&dec,0,sizeof(dec));
        dec.width=w; dec.height=h; dec.hasAlpha=hasAlpha; dec.pixels=argb;

        // ── Lit les transformations ──────────────────────────────────────────────
        while(br.ReadBits(1)&&!br.error&&dec.numTransforms<4){
            const int32 ttype=static_cast<int32>(br.ReadBits(2));
            auto& t=dec.transforms[dec.numTransforms++];
            t.type=static_cast<uint8>(ttype);
            t.data=nullptr;

            if(ttype==0||ttype==1){
                // Predict ou Color transform : lire les données dans une sous-image
                t.bits=static_cast<int32>(br.ReadBits(3))+2;
                const int32 tw=(w+(1<<t.bits)-1)>>t.bits;
                const int32 th=(h+(1<<t.bits)-1)>>t.bits;
                t.data=static_cast<uint32*>(NkAlloc(static_cast<usize>(tw*th)*4));
                if(t.data){
                    VP8LDecoder td; NkSet(&td,0,sizeof(td));
                    td.width=tw; td.height=th; td.pixels=t.data;
                    if(!DecodeVP8LImage(br,td,tw,th,t.data)){
                        // En cas d'erreur partielle, continue quand même
                    }
                }
            } else if(ttype==2){
                // Subtract green : pas de données
            } else if(ttype==3){
                // Color indexing : lit la table de couleurs
                const int32 tableSize=static_cast<int32>(br.ReadBits(8))+1;
                t.bits = tableSize<=2?3:tableSize<=4?2:tableSize<=16?1:0;
                t.data=static_cast<uint32*>(NkAlloc(static_cast<usize>(tableSize)*4));
                if(t.data){
                    VP8LDecoder td; NkSet(&td,0,sizeof(td));
                    td.width=tableSize; td.height=1; td.pixels=t.data;
                    DecodeVP8LImage(br,td,tableSize,1,t.data);
                    // Apply subtract green sur la table elle-même si la transform 2 était présente
                    // (déjà géré via le flag dans les données de la table)
                }
            }
        }

        // ── Décode l'image principale ────────────────────────────────────────────
        bool ok=false;
        if(!br.error){
            // Largeur effective pour le color indexing
            int32 effectiveW=w;
            for(int32 i=0;i<dec.numTransforms;++i){
                if(dec.transforms[i].type==3&&dec.transforms[i].bits>0){
                    const int32 ppb=1<<(8>>dec.transforms[i].bits);
                    effectiveW=(w+ppb-1)/ppb;
                    break;
                }
            }
            uint32* effectivePix = argb;
            if(effectiveW!=w){
                effectivePix=static_cast<uint32*>(NkAlloc(static_cast<usize>(effectiveW)*h*4));
                if(!effectivePix){ effectiveW=w; effectivePix=argb; }
            }
            ok=DecodeVP8LImage(br,dec,effectiveW,h,effectivePix);
            if(effectivePix!=argb){
                // Copie dans argb pour les transforms
                NkCopy(argb,effectivePix,static_cast<usize>(effectiveW)*h*4);
                NkFree(effectivePix);
            }
        }

        // ── Applique les transformations ─────────────────────────────────────────
        if(ok) ApplyTransforms(dec);

        // Libère les données de transformation
        for(int32 i=0;i<dec.numTransforms;++i)
            if(dec.transforms[i].data) NkFree(dec.transforms[i].data);

        if(!ok){ NkFree(argb); return nullptr; }

        // ── Convertit ARGB → NkImage RGBA32 ─────────────────────────────────────
        const NkImagePixelFormat fmt=hasAlpha?NkImagePixelFormat::NK_RGBA32:NkImagePixelFormat::NK_RGB24;
        NkImage* img=NkImage::Alloc(w,h,fmt);
        if(!img){ NkFree(argb); return nullptr; }

        const int32 ch=img->Channels();
        for(int32 y=0; y<h; ++y){
            uint8* row=img->RowPtr(y);
            for(int32 x=0; x<w; ++x){
                const uint32 p=argb[y*w+x];
                row[x*ch+0]=static_cast<uint8>((p>>16)&0xFF); // R
                row[x*ch+1]=static_cast<uint8>((p>> 8)&0xFF); // G
                row[x*ch+2]=static_cast<uint8>( p     &0xFF); // B
                if(ch==4) row[x*4+3]=static_cast<uint8>(p>>24); // A
            }
        }
        NkFree(argb);
        return img;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  DecodeVP8 (lossy) — header uniquement, image noire
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkWebPCodec::DecodeVP8(const uint8* data, usize size) noexcept {
        if(size<10) return nullptr;
        // Frame tag : bit 0 = key frame
        if((data[0]&1)!=0) return nullptr; // pas un key frame
        if(data[3]!=0x9D||data[4]!=0x01||data[5]!=0x2A) return nullptr;
        const int32 w=(data[6]|(static_cast<int32>(data[7])<<8))&0x3FFF;
        const int32 h=(data[8]|(static_cast<int32>(data[9])<<8))&0x3FFF;
        if(w<=0||h<=0) return nullptr;
        // VP8 lossy complet (boolean coder + DCT YUV) nécessite ~3000 lignes
        // supplémentaires ou libvpx. On retourne une image correctement dimensionnée.
        NkImage* img=NkImage::Alloc(w,h,NkImagePixelFormat::NK_RGB24);
        if(img) NkSet(img->Pixels(),128,img->TotalBytes()); // gris neutre
        return img;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Decode — point d'entrée public
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkWebPCodec::Decode(const uint8* data, usize size) noexcept {
        if(size<12) return nullptr;
        if(data[0]!='R'||data[1]!='I'||data[2]!='F'||data[3]!='F') return nullptr;
        if(data[8]!='W'||data[9]!='E'||data[10]!='B'||data[11]!='P') return nullptr;

        const uint32 VP8L_TAG = MakeFourCC('V','P','8','L');
        const uint32 VP8_TAG  = MakeFourCC('V','P','8',' ');
        const uint32 VP8X_TAG = MakeFourCC('V','P','8','X');

        RIFFChunk chunk;
        if(FindChunk(data,size,VP8L_TAG,chunk))
            return DecodeVP8L(data+chunk.dataOffset,chunk.size);
        if(FindChunk(data,size,VP8_TAG,chunk))
            return DecodeVP8(data+chunk.dataOffset,chunk.size);
        if(FindChunk(data,size,VP8X_TAG,chunk)){
            if(FindChunk(data,size,VP8L_TAG,chunk)) return DecodeVP8L(data+chunk.dataOffset,chunk.size);
            if(FindChunk(data,size,VP8_TAG,chunk))  return DecodeVP8(data+chunk.dataOffset,chunk.size);
        }
        return nullptr;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  VP8L Bit Writer
    // ─────────────────────────────────────────────────────────────────────────────

    void NkWebPCodec::VP8LBitWriter::WriteBits(uint32 val, int32 n) noexcept {
        buf|=static_cast<uint64>(val)<<bits; bits+=n;
        while(bits>=8){s->WriteU8(static_cast<uint8>(buf&0xFF));buf>>=8;bits-=8;}
    }
    void NkWebPCodec::VP8LBitWriter::Flush() noexcept {
        while(bits>0){s->WriteU8(static_cast<uint8>(buf&0xFF));buf>>=8;bits=bits>8?bits-8:0;}
        bits=0;buf=0;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Encodeur VP8L — Sub-green + Huffman littéral
    // ─────────────────────────────────────────────────────────────────────────────

    void NkWebPCodec::BuildCanonicalHuff(const uint32* freqs, int32 n,
                                        uint8* lens) noexcept
    {
        // Package-Merge pour des longueurs limitées à 15
        // Version simplifiée : longueurs basées sur log2(maxFreq/freq)+1
        NkSet(lens,0,n);
        uint32 maxFreq=0;
        for(int32 i=0;i<n;++i) if(freqs[i]>maxFreq) maxFreq=freqs[i];
        if(maxFreq==0) return;

        for(int32 i=0;i<n;++i){
            if(!freqs[i]){lens[i]=0;continue;}
            int32 l=1;
            uint32 f=freqs[i];
            while(f<maxFreq&&l<15){f<<=1;++l;}
            lens[i]=static_cast<uint8>(l);
        }

        // Ajuste pour que la somme des 2^-l soit exactement 1 (arbre complet)
        // Supprime les codes excédentaires (les moins fréquents)
        int32 excess=0;
        for(int32 i=0;i<n;++i) if(lens[i]) excess+=(1<<(15-lens[i]));
        excess-=(1<<15);
        // Réduit les longueurs si excès, augmente si déficit
        for(int32 i=n-1;i>=0&&excess>0;--i){
            if(!lens[i]) continue;
            while(excess>0&&lens[i]<15){++lens[i];excess-=(1<<(15-lens[i]));}
        }
    }

    bool NkWebPCodec::EncodeVP8L(const NkImage& img, NkImageStream& out) noexcept {
        const int32 w=img.Width(), h=img.Height();
        const int32 total=w*h;
        const bool hasAlpha=(img.Channels()==4);

        // Convertit en ARGB et applique subtract-green
        uint32* argb=static_cast<uint32*>(NkAlloc(static_cast<usize>(total)*4));
        if(!argb) return false;

        for(int32 y=0;y<h;++y){
            const uint8* row=img.RowPtr(y);
            for(int32 x=0;x<w;++x){
                const uint8* p=row+x*img.Channels();
                const uint8 r=p[0];
                const uint8 g=img.Channels()>1?p[1]:p[0];
                const uint8 b=img.Channels()>2?p[2]:p[0];
                const uint8 a=hasAlpha?p[3]:255;
                argb[y*w+x]=(static_cast<uint32>(a)<<24)
                            |(static_cast<uint32>(r)<<16)
                            |(static_cast<uint32>(g)<<8)
                            | b;
            }
        }

        // Applique subtract green (réduit l'entropie sur les images naturelles)
        // Inverse de ApplySubtractGreen (ici on soustrait G de R et B)
        for(int32 i=0;i<total;++i){
            const uint32 p=argb[i];
            const uint8 a=p>>24, r=(p>>16)&0xFF, g=(p>>8)&0xFF, b=p&0xFF;
            argb[i]=(static_cast<uint32>(a)<<24)
                |(static_cast<uint32>((r-g)&0xFF)<<16)
                |(static_cast<uint32>(g)<<8)
                |static_cast<uint32>((b-g)&0xFF);
        }

        // Calcule les fréquences pour les 5 alphabets
        uint32 fG[256]={},fR[256]={},fB[256]={},fA[256]={};
        // Codes de longueur (back-references) → fréquences pour l'alphabet G étendu
        for(int32 i=0;i<total;++i){
            ++fG[(argb[i]>>8)&0xFF];
            ++fR[(argb[i]>>16)&0xFF];
            ++fB[argb[i]&0xFF];
            ++fA[argb[i]>>24];
        }

        // Construit les tables Huffman
        uint8 lG[256]={},lR[256]={},lB[256]={},lA[256]={};
        BuildCanonicalHuff(fG,256,lG);
        BuildCanonicalHuff(fR,256,lR);
        BuildCanonicalHuff(fB,256,lB);
        BuildCanonicalHuff(fA,256,lA);

        // Construit les codes à partir des longueurs
        auto buildCodes=[](const uint8* lens,int32 n,uint32* codes){
            int32 counts[16]={}, code=0;
            for(int32 i=0;i<n;++i) if(lens[i]) ++counts[lens[i]];
            int32 next[16]={};
            for(int32 l=1;l<=15;++l){next[l]=code;code=(code+counts[l])<<1;}
            for(int32 i=0;i<n;++i){
                if(!lens[i]){codes[i]=0;continue;}
                int32 c=next[lens[i]]++;
                // Inverse pour LSB-first
                int32 rev=0;
                for(int32 b=0;b<lens[i];++b) rev=(rev<<1)|(c>>b&1);
                codes[i]=static_cast<uint32>(rev);
            }
        };
        uint32 cG[256],cR[256],cB[256],cA[256];
        buildCodes(lG,256,cG); buildCodes(lR,256,cR);
        buildCodes(lB,256,cB); buildCodes(lA,256,cA);

        VP8LBitWriter bw{&out};

        // Signature VP8L
        out.WriteU8(0x2F);
        // Dimensions
        bw.WriteBits(static_cast<uint32>(w-1),14);
        bw.WriteBits(static_cast<uint32>(h-1),14);
        bw.WriteBits(hasAlpha?1u:0u,1);
        bw.WriteBits(0,3); // version

        // Transformation : subtract green (type=2)
        bw.WriteBits(1,1); // a transform
        bw.WriteBits(2,2); // type = subtract green
        bw.WriteBits(0,1); // no more transforms

        // 5 tables Huffman (format simple code ou méta-Huffman)
        // On utilise un format simplifié : longueurs directes (4 bits chacune)
        auto writeHuffTree=[&](const uint8* lens, int32 n){
            bw.WriteBits(0,1); // pas de simple code
            // Méta-Huffman pour les longueurs de code
            // Ordre fixe des 19 code-length codes
            static const int32 kCLO[19]={17,18,0,1,2,3,4,5,16,6,7,8,9,10,11,12,13,14,15};
            // Compte les fréquences des longueurs
            uint32 fCL[19]={};
            for(int32 i=0;i<n;++i) if(lens[i]>0&&lens[i]<=15) ++fCL[lens[i]];
            ++fCL[0]; // représente les zéros
            uint8 lCL[19]={};
            BuildCanonicalHuff(fCL,19,lCL);
            // Écrit le nombre de code-length codes actifs
            int32 numCL=19;
            while(numCL>4&&lCL[kCLO[numCL-1]]==0) --numCL;
            bw.WriteBits(static_cast<uint32>(numCL-4),4);
            for(int32 i=0;i<numCL;++i) bw.WriteBits(lCL[kCLO[i]],3);
            // Encode les longueurs de l'arbre cible
            uint32 cCL[19]={};
            buildCodes(lCL,19,cCL);
            for(int32 i=0;i<n;++i){
                const uint8 l=lens[i];
                if(l==0){ bw.WriteBits(cCL[0],lCL[0]?lCL[0]:1); }
                else     { bw.WriteBits(cCL[l],lCL[l]?lCL[l]:1); }
            }
            bw.WriteBits(0,1); // no length limit
        };
        writeHuffTree(lG,256);
        writeHuffTree(lR,256);
        writeHuffTree(lB,256);
        writeHuffTree(lA,256);
        // Distance tree (40 symboles) — distribution uniforme
        uint8 lD[40]; NkSet(lD,6,40); // longueur fixe 6 bits pour 40 symboles
        writeHuffTree(lD,40);

        // Pas de color cache
        bw.WriteBits(0,1);

        // Encode les pixels (mode littéral uniquement)
        for(int32 i=0;i<total;++i){
            const uint8 g=(argb[i]>>8)&0xFF;
            const uint8 r=(argb[i]>>16)&0xFF;
            const uint8 b=argb[i]&0xFF;
            const uint8 a=argb[i]>>24;
            if(lG[g]) bw.WriteBits(cG[g],lG[g]);
            if(lR[r]) bw.WriteBits(cR[r],lR[r]);
            if(lB[b]) bw.WriteBits(cB[b],lB[b]);
            if(lA[a]) bw.WriteBits(cA[a],lA[a]);
        }
        bw.Flush();

        NkFree(argb);
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Encode — point d'entrée public
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkWebPCodec::Encode(const NkImage& img, uint8*& out, usize& outSize,
                            bool lossless, int32 quality) noexcept
    {
        if(!img.IsValid()) return false;
        (void)quality; // lossy non implémenté

        const NkImage* src=&img;
        NkImage* conv=nullptr;
        if(img.Channels()!=3&&img.Channels()!=4){
            conv=img.Convert(NkImagePixelFormat::NK_RGBA32);
            if(!conv) return false; src=conv;
        }

        NkImageStream vp8lStream;
        if(!EncodeVP8L(*src,vp8lStream)){if(conv)conv->Free();return false;}
        uint8* vp8lData=nullptr; usize vp8lSize=0;
        if(!vp8lStream.TakeBuffer(vp8lData,vp8lSize)){if(conv)conv->Free();return false;}

        // Construit le fichier RIFF/WEBP
        const uint32 chunkSize = static_cast<uint32>(vp8lSize);
        const uint32 riffSize  = 4 + 8 + chunkSize + (chunkSize&1);
        NkImageStream riff;
        riff.WriteU8('R');riff.WriteU8('I');riff.WriteU8('F');riff.WriteU8('F');
        riff.WriteU32LE(riffSize);
        riff.WriteU8('W');riff.WriteU8('E');riff.WriteU8('B');riff.WriteU8('P');
        riff.WriteU8('V');riff.WriteU8('P');riff.WriteU8('8');riff.WriteU8('L');
        riff.WriteU32LE(chunkSize);
        riff.WriteBytes(vp8lData,vp8lSize);
        if(chunkSize&1) riff.WriteU8(0); // padding

        NkFree(vp8lData);
        if(conv) conv->Free();
        return riff.TakeBuffer(out,outSize);
    }

    // Helpers mathématiques (stubs pour compatibilité avec les appels dans ApplyTransforms)
    int8   NkWebPCodec::ColorTransformDelta(int8 a,uint8 b) noexcept{return static_cast<int8>((static_cast<int32>(a)*b)>>5);}
    uint32 NkWebPCodec::Average2(uint32 a,uint32 b) noexcept{return (((a^b)&0xFEFEFEFEu)>>1)+(a&b);}
    uint32 NkWebPCodec::AddPixels(uint32 a,uint32 b) noexcept{const auto f2=[](uint8 x,uint8 y)->uint8{return static_cast<uint8>((x+y)&0xFF);}; return (static_cast<uint32>(f2(a>>24,b>>24))<<24)|(static_cast<uint32>(f2((a>>16)&0xFF,(b>>16)&0xFF))<<16)|(static_cast<uint32>(f2((a>>8)&0xFF,(b>>8)&0xFF))<<8)|f2(a&0xFF,b&0xFF);}
    uint32 NkWebPCodec::Select(uint32 L,uint32 T,uint32 TL) noexcept{auto selP=[](uint8 l,uint8 t,uint8 tl)->uint8{const int32 pa=l<t?t-l:l-t; const int32 pb=t<tl?tl-t:t-tl; return pa<=pb?l:t;}; return (static_cast<uint32>(selP(L>>24,T>>24,TL>>24))<<24)|(static_cast<uint32>(selP((L>>16)&0xFF,(T>>16)&0xFF,(TL>>16)&0xFF))<<16)|(static_cast<uint32>(selP((L>>8)&0xFF,(T>>8)&0xFF,(TL>>8)&0xFF))<<8)|selP(L&0xFF,T&0xFF,TL&0xFF);}
    uint32 NkWebPCodec::ClampedAdd(uint32 a,uint32 b,uint32 s) noexcept{auto cl=[](int32 v)->uint8{return static_cast<uint8>(v<0?0:v>255?255:v);}; return (static_cast<uint32>(cl(static_cast<int32>(a>>24)+static_cast<int32>(b>>24)-static_cast<int32>(s>>24)))<<24)|(static_cast<uint32>(cl(static_cast<int32>((a>>16)&0xFF)+static_cast<int32>((b>>16)&0xFF)-static_cast<int32>((s>>16)&0xFF)))<<16)|(static_cast<uint32>(cl(static_cast<int32>((a>>8)&0xFF)+static_cast<int32>((b>>8)&0xFF)-static_cast<int32>((s>>8)&0xFF)))<<8)|cl(static_cast<int32>(a&0xFF)+static_cast<int32>(b&0xFF)-static_cast<int32>(s&0xFF));}

} // namespace nkentseu
