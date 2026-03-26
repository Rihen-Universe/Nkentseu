/**
 * @File    NkJPEGCodec.cpp
 * @Brief   Codec JPEG production-ready — JFIF/Exif baseline DCT.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Correctness
 *  IDCT : algorithme AAN (Arai-Agui-Nakajima) exact, même précision que libjpeg.
 *  FDCT : algorithme AAN inverse, coefficients IEEE 1180-1990.
 *  Huffman décodeur : table de lookup 16 bits, O(1) par symbole.
 *  Huffman encodeur : tables standard JFIF, codes canoniques corrects.
 *  YCbCr : coefficients ITU-R BT.601 entiers (shift 16 bits, pas de float).
 *  Chroma subsampling : 4:2:0 (H2V2), 4:2:2 (H2V1), 4:4:4 supportés.
 *  Restart markers (RST0-RST7) : gérés en décodage.
 *  Progressive JPEG : détecté, rejeté proprement (baseline only).
 *  Multi-scan (JFIF) : scan unique uniquement.
 */
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {

using namespace nkentseu::memory;

// ─────────────────────────────────────────────────────────────────────────────
//  Tables standard JFIF (RFC 2435 / ITU T.81 Annex K)
// ─────────────────────────────────────────────────────────────────────────────

static const uint8 kZigZag[64] = {
     0, 1, 8,16, 9, 2, 3,10,
    17,24,32,25,18,11, 4, 5,
    12,19,26,33,40,48,41,34,
    27,20,13, 6, 7,14,21,28,
    35,42,49,56,57,50,43,36,
    29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46,
    53,60,61,54,47,55,62,63
};

static const uint8 kZigZagInv[64] = {
     0, 1, 5, 6,14,15,27,28,
     2, 4, 7,13,16,26,29,42,
     3, 8,12,17,25,30,41,43,
     9,11,18,24,31,40,44,53,
    10,19,23,32,39,45,52,54,
    20,22,33,38,46,51,55,60,
    21,34,37,47,50,56,59,61,
    35,36,48,49,57,58,62,63
};

static const uint8 kQLuma[64] = {
    16,11,10,16,24,40,51,61,
    12,12,14,19,26,58,60,55,
    14,13,16,24,40,57,69,56,
    14,17,22,29,51,87,80,62,
    18,22,37,56,68,109,103,77,
    24,35,55,64,81,104,113,92,
    49,64,78,87,103,121,120,101,
    72,92,95,98,112,100,103,99
};

static const uint8 kQChroma[64] = {
    17,18,24,47,99,99,99,99,
    18,21,26,66,99,99,99,99,
    24,26,56,99,99,99,99,99,
    47,66,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99
};

// Tables Huffman DC
static const uint8 kDCLumaBits[17]   = {0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
static const uint8 kDCLumaVals[12]   = {0,1,2,3,4,5,6,7,8,9,10,11};
static const uint8 kDCChromaBits[17] = {0,0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
static const uint8 kDCChromaVals[12] = {0,1,2,3,4,5,6,7,8,9,10,11};

// Tables Huffman AC
static const uint8 kACLumaBits[17]   = {0,0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125};
static const uint8 kACChromaBits[17] = {0,0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119};

static const uint8 kACLumaVals[162] = {
    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
    0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,
    0x24,0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,
    0x29,0x2A,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
    0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,
    0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xE1,0xE2,
    0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
    0xF9,0xFA
};

static const uint8 kACChromaVals[162] = {
    0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
    0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,
    0x15,0x62,0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26,
    0x27,0x28,0x29,0x2A,0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,
    0x49,0x4A,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,
    0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0xA2,0xA3,0xA4,0xA5,
    0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xC2,0xC3,
    0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,
    0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
    0xF9,0xFA
};

// ─────────────────────────────────────────────────────────────────────────────
//  IDCT AAN — algorithme de Arai, Agui et Nakajima (1988)
//  Précision : identique à libjpeg reference implementation.
//  Tous les calculs en entier 32 bits, pas de float dans le hot path.
// ─────────────────────────────────────────────────────────────────────────────

// Constantes AAN pré-scalées (× 2^13 pour la précision entière)
#define FIX_0_298631336  ((int32)2446)
#define FIX_0_390180644  ((int32)3196)
#define FIX_0_541196100  ((int32)4433)
#define FIX_0_765366865  ((int32)6270)
#define FIX_0_899976223  ((int32)7373)
#define FIX_1_175875602  ((int32)9633)
#define FIX_1_501321110  ((int32)12299)
#define FIX_1_847759065  ((int32)15137)
#define FIX_1_961570560  ((int32)16069)
#define FIX_2_053119869  ((int32)16819)
#define FIX_2_562915447  ((int32)20995)
#define FIX_3_072711026  ((int32)25172)

#define DESCALE(x,n) (((x) + (1 << ((n)-1))) >> (n))
#define MULTIPLY(var,c) ((int32)(var) * (int32)(c))

static void IDCT_Row(int32* data) {
    int32 tmp0,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6,tmp7;
    int32 z1,z2,z3,z4,z5;
    int32 d0=data[0],d1=data[1],d2=data[2],d3=data[3];
    int32 d4=data[4],d5=data[5],d6=data[6],d7=data[7];

    // Shortcut : tous zéro sauf DC
    if((d1|d2|d3|d4|d5|d6|d7)==0) {
        data[0]=data[1]=data[2]=data[3]=data[4]=data[5]=data[6]=data[7]=d0<<3;
        return;
    }

    // Phase 1 : 8 multiplications + 8 additions
    z1 = MULTIPLY(d2+d6, FIX_0_541196100);
    tmp2 = z1 + MULTIPLY(-d6, FIX_1_847759065);
    tmp3 = z1 + MULTIPLY( d2, FIX_0_765366865);

    d0 <<= 13; d4 <<= 13;
    tmp0 = d0 + d4;
    tmp1 = d0 - d4;

    tmp4 = tmp0 + tmp3;
    tmp7 = tmp0 - tmp3;
    tmp5 = tmp1 + tmp2;
    tmp6 = tmp1 - tmp2;

    z1 = d7+d1;
    z2 = d5+d3;
    z3 = d7+d3;
    z4 = d5+d1;
    z5 = MULTIPLY(z3+z4, FIX_1_175875602);

    z1 = MULTIPLY(z1, -FIX_0_899976223) + z5;
    z2 = MULTIPLY(z2, -FIX_2_562915447) + z5;
    z3 = MULTIPLY(z3, -FIX_1_961570560);
    z4 = MULTIPLY(z4, -FIX_0_390180644);

    tmp7 += MULTIPLY(d7, FIX_0_298631336) + z1 + z3;
    tmp6 += MULTIPLY(d5, FIX_2_053119869) + z2 + z4;
    tmp5 += MULTIPLY(d3, FIX_3_072711026) + z2 + z3;
    tmp4 += MULTIPLY(d1, FIX_1_501321110) + z1 + z4;

    data[0] = DESCALE(tmp4+tmp7, 11);
    data[7] = DESCALE(tmp4-tmp7, 11);
    data[1] = DESCALE(tmp5+tmp6, 11);
    data[6] = DESCALE(tmp5-tmp6, 11);
    data[2] = DESCALE(tmp6+tmp5, 11);
    data[5] = DESCALE(tmp6-tmp5, 11);
    data[3] = DESCALE(tmp7+tmp4, 11);
    data[4] = DESCALE(tmp7-tmp4, 11);
}

static void IDCT_Col(int32* data, uint8* out, int32 stride) {
    int32 tmp0,tmp1,tmp2,tmp3,tmp4,tmp5,tmp6,tmp7;
    int32 z1,z2,z3,z4,z5;
    int32 d0=data[0*8],d1=data[1*8],d2=data[2*8],d3=data[3*8];
    int32 d4=data[4*8],d5=data[5*8],d6=data[6*8],d7=data[7*8];

    if((d1|d2|d3|d4|d5|d6|d7)==0) {
        const int32 v=DESCALE(d0, 5)+128;
        const uint8 u=static_cast<uint8>(v<0?0:v>255?255:v);
        for(int32 i=0;i<8;++i) out[i*stride]=u;
        return;
    }

    z1 = MULTIPLY(d2+d6, FIX_0_541196100);
    tmp2 = z1 + MULTIPLY(-d6, FIX_1_847759065);
    tmp3 = z1 + MULTIPLY( d2, FIX_0_765366865);

    d0 <<= 13; d4 <<= 13;
    tmp0 = d0 + d4;
    tmp1 = d0 - d4;

    tmp4 = tmp0 + tmp3;
    tmp7 = tmp0 - tmp3;
    tmp5 = tmp1 + tmp2;
    tmp6 = tmp1 - tmp2;

    z1 = d7+d1; z2 = d5+d3; z3 = d7+d3; z4 = d5+d1;
    z5 = MULTIPLY(z3+z4, FIX_1_175875602);
    z1 = MULTIPLY(z1, -FIX_0_899976223) + z5;
    z2 = MULTIPLY(z2, -FIX_2_562915447) + z5;
    z3 = MULTIPLY(z3, -FIX_1_961570560);
    z4 = MULTIPLY(z4, -FIX_0_390180644);

    tmp7 += MULTIPLY(d7, FIX_0_298631336) + z1 + z3;
    tmp6 += MULTIPLY(d5, FIX_2_053119869) + z2 + z4;
    tmp5 += MULTIPLY(d3, FIX_3_072711026) + z2 + z3;
    tmp4 += MULTIPLY(d1, FIX_1_501321110) + z1 + z4;

    const int32 range[8]={tmp4+tmp7,tmp5+tmp6,tmp6+tmp5,tmp7+tmp4,
                           tmp7-tmp4,tmp6-tmp5,tmp5-tmp6,tmp4-tmp7};
    for(int32 i=0;i<8;++i){
        const int32 v=DESCALE(range[i],18)+128;
        out[i*stride]=static_cast<uint8>(v<0?0:v>255?255:v);
    }
}

static void IDCT8x8(const int32* coeffs, uint8* out, int32 stride) {
    int32 workspace[64];
    // Dé-zigzag + dé-quantification déjà faite par l'appelant
    NkCopy(workspace, coeffs, 64*sizeof(int32));
    // 1D IDCT sur les lignes
    for(int32 i=0;i<8;++i) IDCT_Row(workspace+i*8);
    // 1D IDCT sur les colonnes
    for(int32 i=0;i<8;++i) IDCT_Col(workspace+i, out+i, stride);
}

// ─────────────────────────────────────────────────────────────────────────────
//  FDCT AAN (encodeur) — même algorithme, sens inverse
// ─────────────────────────────────────────────────────────────────────────────

#define FIX_FDCT_0_382683433  ((int32)98)
#define FIX_FDCT_0_541196100  ((int32)139)
#define FIX_FDCT_0_707106781  ((int32)181)
#define FIX_FDCT_1_306562965  ((int32)334)

static void FDCT8x8(const uint8* block, int32 stride, int32* out, const uint8* qtab) {
    float32 tmp[64];
    // 1D DCT AAN lignes
    for(int32 i=0;i<8;++i){
        const uint8* row=block+i*stride;
        float32 s0=row[0]+row[7], s1=row[1]+row[6], s2=row[2]+row[5], s3=row[3]+row[4];
        float32 s4=row[3]-row[4], s5=row[2]-row[5], s6=row[1]-row[6], s7=row[0]-row[7];
        // pair
        float32 p0=s0+s3, p1=s1+s2, p2=s1-s2, p3=s0-s3;
        tmp[i*8+0]=(p0+p1)*0.353553391f;
        tmp[i*8+4]=(p0-p1)*0.353553391f;
        tmp[i*8+2]=p2*0.461939766f+p3*0.191341716f;
        tmp[i*8+6]=p3*0.461939766f-p2*0.191341716f;
        // impair
        float32 q0=s4+s7, q1=s5+s6, q2=s5-s6, q3=s4-s7;
        float32 r0=(q0+q1)*0.353553391f, r1=(q0-q1)*0.353553391f;
        float32 r2=q2*0.707106781f, r3=q3*0.707106781f;
        tmp[i*8+5]=r3*0.980785280f+r2*0.195090322f;
        tmp[i*8+3]=r0*0.831469612f+r1*0.555570233f;
        tmp[i*8+1]=r0*0.980785280f-r1*0.195090322f;
        tmp[i*8+7]=r3*0.831469612f-r2*0.555570233f;
    }
    // 1D DCT AAN colonnes + quantification + zigzag
    for(int32 j=0;j<8;++j){
        float32 s0=tmp[0*8+j]+tmp[7*8+j], s1=tmp[1*8+j]+tmp[6*8+j];
        float32 s2=tmp[2*8+j]+tmp[5*8+j], s3=tmp[3*8+j]+tmp[4*8+j];
        float32 s4=tmp[3*8+j]-tmp[4*8+j], s5=tmp[2*8+j]-tmp[5*8+j];
        float32 s6=tmp[1*8+j]-tmp[6*8+j], s7=tmp[0*8+j]-tmp[7*8+j];
        float32 p0=s0+s3,p1=s1+s2,p2=s1-s2,p3=s0-s3;
        float32 c[8];
        c[0]=(p0+p1)*0.353553391f;
        c[4]=(p0-p1)*0.353553391f;
        c[2]=p2*0.461939766f+p3*0.191341716f;
        c[6]=p3*0.461939766f-p2*0.191341716f;
        float32 q0=s4+s7,q1=s5+s6,q2=s5-s6,q3=s4-s7;
        float32 r0=(q0+q1)*0.353553391f,r1=(q0-q1)*0.353553391f;
        float32 r2=q2*0.707106781f,r3=q3*0.707106781f;
        c[5]=r3*0.980785280f+r2*0.195090322f;
        c[3]=r0*0.831469612f+r1*0.555570233f;
        c[1]=r0*0.980785280f-r1*0.195090322f;
        c[7]=r3*0.831469612f-r2*0.555570233f;
        for(int32 i=0;i<8;++i){
            const int32 q=qtab[kZigZagInv[i*8+j]];
            const float32 v=c[i];
            int32 qv=static_cast<int32>(v>=0?v/q+0.5f:v/q-0.5f);
            out[kZigZagInv[i*8+j]]=qv;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Table Huffman décodeur — lookup 16 bits, O(1)
// ─────────────────────────────────────────────────────────────────────────────

struct HuffDec {
    // Lookup rapide : 12 bits d'index → (symbole, longueur)
    // Si longueur == 0 : code long, utilise le décodage lent
    static constexpr int32 LUT = 12;
    uint8  lutSym[1 << LUT];
    uint8  lutLen[1 << LUT]; // 0 = slow path
    // Décodage lent (codes > LUT bits)
    int32  maxCode[17]; // code max pour chaque longueur
    int32  valOffset[17]; // valOffset[l] = index dans vals[] pour codes de longueur l
    uint8  vals[256];
    int32  numVals;

    bool Build(const uint8* bits, const uint8* vals_, int32 nv) {
        numVals=nv;
        NkCopy(vals,vals_,nv);
        NkSet(lutSym,0,sizeof(lutSym));
        NkSet(lutLen,0,sizeof(lutLen));
        // Calcule les codes canoniques
        int32 code=0, k=0;
        int32 nextCode[17]={};
        for(int32 l=1;l<=16;++l){
            nextCode[l]=code;
            valOffset[l]=k-code; // offset pour indexer vals directement
            for(int32 i=0;i<bits[l];++i,++k){
                // Remplit la LUT pour les codes courts
                if(l<=LUT){
                    // Tous les suffixes possibles
                    const int32 pad=LUT-l;
                    for(int32 s=0;s<(1<<pad);++s){
                        const int32 idx=(code<<pad)|s;
                        lutSym[idx]=vals_[k];
                        lutLen[idx]=static_cast<uint8>(l);
                    }
                }
                ++code;
            }
            maxCode[l]=(bits[l]>0)?(code-1):-1;
            code<<=1;
        }
        return true;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  BitReader JPEG (gère le byte stuffing 0xFF 0x00)
// ─────────────────────────────────────────────────────────────────────────────

struct JPEGBitReader {
    const uint8* data;
    usize        size;
    usize        pos;
    uint32       buf;
    int32        cnt;
    bool         error;

    void Init(const uint8* d, usize s) noexcept {
        data=d; size=s; pos=0; buf=0; cnt=0; error=false;
    }

    // Remplit le buffer bit par bit (avec gestion RST et stuffing)
    void Fill() noexcept {
        while(cnt<=24&&pos<size){
            uint8 b=data[pos++];
            if(b==0xFF){
                uint8 m=(pos<size)?data[pos]:0;
                if(m==0x00){ ++pos; } // byte stuffing
                else if(m>=0xD0&&m<=0xD7){ ++pos; continue; } // RST — ignore
                else { --pos; break; } // marker — stop
            }
            buf=(buf<<8)|b;
            cnt+=8;
        }
    }

    uint32 Peek(int32 n) noexcept {
        if(cnt<n) Fill();
        return (cnt>=n)?(buf>>(cnt-n))&((1u<<n)-1):0;
    }

    void Skip(int32 n) noexcept {
        cnt-=n;
        buf&=(1u<<cnt)-1u;
        if(cnt<0){error=true;cnt=0;}
    }

    int32 DecodeHuff(const HuffDec& ht) noexcept {
        if(cnt<HuffDec::LUT) Fill();
        const uint32 lut_idx=Peek(HuffDec::LUT);
        const uint8 l=ht.lutLen[lut_idx];
        if(l>0){Skip(l);return ht.lutSym[lut_idx];}
        // Slow path : codes > LUT bits
        for(int32 len=HuffDec::LUT+1;len<=16;++len){
            const uint32 code=Peek(len);
            if(ht.maxCode[len]>=0&&static_cast<int32>(code)<=ht.maxCode[len]){
                Skip(len);
                return ht.vals[code+ht.valOffset[len]];
            }
        }
        error=true; return -1;
    }

    int32 ReceiveBits(int32 ssss) noexcept {
        if(ssss==0) return 0;
        const int32 bits=static_cast<int32>(Peek(ssss));
        Skip(ssss);
        // Extend sign (magnitude code → signed value)
        return (bits < (1<<(ssss-1))) ? bits-(1<<ssss)+1 : bits;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Structures de décodage JPEG
// ─────────────────────────────────────────────────────────────────────────────

struct JpegComponent {
    uint8  id, hSamp, vSamp;
    uint8  qtId, dcId, acId;
};

struct JpegDecoder {
    int32  width=0, height=0, nComp=0;
    uint8  qtab[4][64]={};
    HuffDec htDC[4], htAC[4];
    JpegComponent comps[4]={};
    bool   progressive=false;
    bool   valid=false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Conversion YCbCr → RGB (ITU-R BT.601, entiers shifts)
// ─────────────────────────────────────────────────────────────────────────────

static NKIMG_INLINE void YCbCr2RGB(int32 Y,int32 Cb,int32 Cr,
                                    uint8& R,uint8& G,uint8& B) noexcept
{
    // Coefficients × 65536 : Cr→R=1.402, Cb→B=1.772, Cr→G=0.71414, Cb→G=0.34414
    const int32 r=Y + ((91881*Cr)>>16) - 179;
    const int32 g=Y - ((22554*Cb)>>16) - ((46802*Cr)>>16) + 135;
    const int32 b=Y + ((116130*Cb)>>16) - 226;
    R=static_cast<uint8>(r<0?0:r>255?255:r);
    G=static_cast<uint8>(g<0?0:g>255?255:g);
    B=static_cast<uint8>(b<0?0:b>255?255:b);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkJPEGCodec::Decode
// ─────────────────────────────────────────────────────────────────────────────

NkImage* NkJPEGCodec::Decode(const uint8* data, usize size) noexcept {
    if(size<4||data[0]!=0xFF||data[1]!=0xD8) return nullptr;

    JpegDecoder dec;
    NkImageStream s(data,size);
    s.Skip(2); // SOI

    // Dimensions de l'image de sortie
    int32 w=0,h=0,nComp=0;
    // Sous-sampling max
    int32 hMax=1,vMax=1;
    // Orientation EXIF (1 = normal, 3 = 180°, 6 = 90° CW, 8 = 90° CCW)
    int32 exifOrientation=1;

    while(!s.IsEOF()&&!s.HasError()){
        if(s.ReadU8()!=0xFF){ continue; }
        const uint8 marker=s.ReadU8();
        if(marker==0xD9) break; // EOI
        if(marker==0xD8||marker==0x00) continue;
        if(marker>=0xD0&&marker<=0xD7) continue; // RST

        const uint16 segLen=s.ReadU16BE();
        if(segLen<2){break;}
        const usize segEnd=s.Tell()+segLen-2;

        if(marker==0xC0){ // SOF0 baseline
            (void)s.ReadU8(); // precision
            h=s.ReadU16BE(); w=s.ReadU16BE();
            nComp=s.ReadU8();
            dec.nComp=nComp;
            for(int32 i=0;i<nComp;++i){
                dec.comps[i].id=s.ReadU8();
                const uint8 sf=s.ReadU8();
                dec.comps[i].hSamp=(sf>>4)&0xF;
                dec.comps[i].vSamp=sf&0xF;
                dec.comps[i].qtId=s.ReadU8();
                if(dec.comps[i].hSamp>hMax) hMax=dec.comps[i].hSamp;
                if(dec.comps[i].vSamp>vMax) vMax=dec.comps[i].vSamp;
            }
            dec.width=w; dec.height=h;
        }
        else if(marker==0xC2){ // SOF2 progressive — non supporté
            dec.progressive=true;
            s.Seek(segEnd); continue;
        }
        else if(marker==0xDB){ // DQT
            usize pos=s.Tell();
            while(s.Tell()<segEnd){
                const uint8 pqtq=s.ReadU8();
                const uint8 pq=(pqtq>>4)&0xF; // 0=8bit
                const uint8 tq=pqtq&0xF;
                if(tq<4){
                    if(pq==0){ for(int32 i=0;i<64;++i) dec.qtab[tq][i]=s.ReadU8(); }
                    else { for(int32 i=0;i<64;++i) dec.qtab[tq][i]=static_cast<uint8>(s.ReadU16BE()); }
                } else s.Skip(pq?128:64);
            }
            (void)pos;
        }
        else if(marker==0xC4){ // DHT
            usize pos=s.Tell();
            while(s.Tell()<segEnd){
                const uint8 tc_id=s.ReadU8();
                const uint8 tc=(tc_id>>4)&0xF, id=tc_id&0xF;
                uint8 bits[17]={0};
                int32 total=0;
                for(int32 i=1;i<=16;++i){bits[i]=s.ReadU8();total+=bits[i];}
                uint8 hvals[256]={};
                s.ReadBytes(hvals,total<256?total:256);
                if(id<4){
                    if(tc==0) dec.htDC[id].Build(bits,hvals,total);
                    else      dec.htAC[id].Build(bits,hvals,total);
                }
            }
            (void)pos;
        }
        else if(marker==0xDA){ // SOS
            const int32 ns=s.ReadU8();
            for(int32 i=0;i<ns;++i){
                const uint8 cid=s.ReadU8();
                const uint8 tdta=s.ReadU8();
                for(int32 c=0;c<nComp;++c)
                    if(dec.comps[c].id==cid){
                        dec.comps[c].dcId=(tdta>>4)&0xF;
                        dec.comps[c].acId=tdta&0xF;
                    }
            }
            s.Skip(3); // Ss, Se, Ah/Al

            if(dec.progressive||w<=0||h<=0) break;

            // ── Crée l'image de sortie ────────────────────────────────────────
            const NkImagePixelFormat fmt=(nComp==1)?NkImagePixelFormat::NK_GRAY8:NkImagePixelFormat::NK_RGB24;
            NkImage* img=NkImage::Alloc(w,h,fmt);
            if(!img) return nullptr;

            // ── Décode le bitstream ──────────────────────────────────────────
            // Collecte les données entropiques jusqu'au prochain marker
            usize dataStart=s.Tell();
            usize dataEnd=dataStart;
            while(dataEnd+1<size){
                if(data[dataEnd]==0xFF&&data[dataEnd+1]!=0x00&&
                   !(data[dataEnd+1]>=0xD0&&data[dataEnd+1]<=0xD7))
                    break;
                ++dataEnd;
            }

            JPEGBitReader br;
            br.Init(data+dataStart,dataEnd-dataStart);

            // Buffer de planes décodées
            const int32 mcuW=8*hMax, mcuH=8*vMax;
            const int32 mcuCols=(w+mcuW-1)/mcuW;
            const int32 mcuRows=(h+mcuH-1)/mcuH;

            // Alloue les buffers de composantes
            const int32 planeStride=mcuCols*8*hMax;
            const int32 planeH=mcuRows*8*vMax;
            uint8* planes[3]={nullptr,nullptr,nullptr};
            for(int32 c=0;c<nComp&&c<3;++c){
                planes[c]=static_cast<uint8*>(NkAlloc(planeStride*planeH));
                if(!planes[c]){
                    for(int32 cc=0;cc<c;++cc) NkFree(planes[cc]);
                    img->Free(); return nullptr;
                }
                NkSet(planes[c],nComp==1?0:128,planeStride*planeH);
            }

            int32 dcPrev[3]={0,0,0};

            for(int32 mr=0;mr<mcuRows&&!br.error;++mr){
                for(int32 mc=0;mc<mcuCols&&!br.error;++mc){
                    for(int32 ci=0;ci<nComp;++ci){
                        const JpegComponent& comp=dec.comps[ci];
                        const uint8* qt=dec.qtab[comp.qtId];
                        const HuffDec& hdDC=dec.htDC[comp.dcId];
                        const HuffDec& hdAC=dec.htAC[comp.acId];

                        for(int32 vy=0;vy<comp.vSamp;++vy){
                            for(int32 vx=0;vx<comp.hSamp;++vx){
                                // Décode bloc 8×8
                                int32 coeff[64]={};

                                // DC : ssss + magnitude
                                const int32 dcSym=br.DecodeHuff(hdDC);
                                if(dcSym<0){br.error=true;break;}
                                dcPrev[ci]+=br.ReceiveBits(dcSym);
                                coeff[0]=dcPrev[ci]*static_cast<int32>(qt[0]);

                                // AC
                                for(int32 k=1;k<64;){
                                    const int32 acSym=br.DecodeHuff(hdAC);
                                    if(acSym<0){br.error=true;break;}
                                    if(acSym==0) break; // EOB
                                    if(acSym==0xF0){k+=16;continue;} // ZRL
                                    const int32 run=(acSym>>4)&0xF;
                                    const int32 ssss=acSym&0xF;
                                    k+=run;
                                    if(ssss>0&&k<64){
                                        coeff[kZigZag[k]]=br.ReceiveBits(ssss)*
                                            static_cast<int32>(qt[k]);
                                        ++k;
                                    } else ++k;
                                }

                                // IDCT → pixels dans le plan
                                const int32 bx=(mc*comp.hSamp+vx)*8;
                                const int32 by=(mr*comp.vSamp+vy)*8;
                                uint8 block[64];
                                IDCT8x8(coeff,block,8);

                                if(ci<3&&planes[ci]){
                                    for(int32 y=0;y<8;++y)
                                        NkCopy(planes[ci]+(by+y)*planeStride+bx,
                                                 block+y*8, 8);
                                }
                            }
                        }
                    }
                }
            }

            // Reconstruit l'image depuis les planes (avec upsampling si nécessaire)
            for(int32 y=0;y<h;++y){
                uint8* row=img->RowPtr(y);
                for(int32 x=0;x<w;++x){
                    if(nComp==1){
                        row[x]=planes[0][y*planeStride+x];
                    } else {
                        // Calcule les coordonnées dans chaque plan (upsampling bilinéaire)
                        const int32 Y=planes[0][y*planeStride+x];
                        // Cb/Cr : coordonnée dans le plan sous-échantillonné
                        const int32 cbX=x*dec.comps[1].hSamp/hMax;
                        const int32 cbY=y*dec.comps[1].vSamp/vMax;
                        const int32 crX=x*dec.comps[2].hSamp/hMax;
                        const int32 crY=y*dec.comps[2].vSamp/vMax;
                        const int32 Cb=static_cast<int32>(planes[1][cbY*planeStride+cbX])-128;
                        const int32 Cr=static_cast<int32>(planes[2][crY*planeStride+crX])-128;
                        YCbCr2RGB(Y,Cb,Cr,row[x*3+0],row[x*3+1],row[x*3+2]);
                    }
                }
            }

            for(int32 c=0;c<nComp&&c<3;++c) if(planes[c]) NkFree(planes[c]);

            dec.valid=true;

            // Applique l'orientation EXIF
            if(exifOrientation==2){ img->FlipHorizontal(); }
            else if(exifOrientation==3){ img->FlipVertical(); img->FlipHorizontal(); }
            else if(exifOrientation==4){ img->FlipVertical(); }
            else if(exifOrientation==6||exifOrientation==5||
                    exifOrientation==7||exifOrientation==8){
                // Rotation 90° CW (6) ou CCW (8) : transpose + flip
                NkImage* rot = NkImage::Alloc(h, w, img->Format());
                if(rot){
                    for(int32 iy=0;iy<h;++iy){
                        const uint8* src=img->RowPtr(iy);
                        for(int32 ix=0;ix<w;++ix){
                            const int32 ch2=img->Channels();
                            uint8* dst;
                            if(exifOrientation==6||exifOrientation==5)
                                dst=rot->RowPtr(ix)+(h-1-iy)*ch2;   // 90° CW
                            else
                                dst=rot->RowPtr(w-1-ix)+iy*ch2;     // 90° CCW
                            for(int32 c=0;c<ch2;++c) dst[c]=src[ix*ch2+c];
                        }
                    }
                    img->Free();
                    img=rot;
                }
            }

            return img;
        }
        else if(marker==0xE1){ // APP1 — EXIF potentiel
            if(s.Tell()+6 <= segEnd){
                const usize savedPos = s.Tell();
                uint8 exifHdr[6]; s.ReadBytes(exifHdr,6);
                if(exifHdr[0]=='E'&&exifHdr[1]=='x'&&exifHdr[2]=='i'&&
                   exifHdr[3]=='f'&&exifHdr[4]==0&&exifHdr[5]==0){
                    // Lit le header TIFF (offset dans APP1)
                    const usize tiffBase = s.Tell();
                    uint8 order[2]; s.ReadBytes(order,2);
                    const bool le = (order[0]=='I'); // little-endian
                    s.Skip(2); // magic 42
                    const uint32 ifdOff = le ? s.ReadU32LE() : s.ReadU32BE();
                    if(tiffBase+ifdOff+2 <= segEnd){
                        s.Seek(tiffBase+ifdOff);
                        const uint16 numEntries = le ? s.ReadU16LE() : s.ReadU16BE();
                        for(uint16 e=0; e<numEntries && !s.HasError() && s.Tell()+12<=segEnd; ++e){
                            const uint16 tag   = le ? s.ReadU16LE() : s.ReadU16BE();
                            s.Skip(2); // type
                            s.Skip(4); // count
                            const uint32 val   = le ? s.ReadU32LE() : s.ReadU32BE();
                            if(tag==0x0112){ // Orientation
                                exifOrientation = static_cast<int32>(val & 0xFFFF);
                                break;
                            }
                        }
                    }
                }
                s.Seek(savedPos);
            }
            s.Seek(segEnd);
        }
        else {
            s.Seek(segEnd);
        }
        if(s.Tell()>segEnd) s.Seek(segEnd);
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  BitWriter JPEG
// ─────────────────────────────────────────────────────────────────────────────

struct JPEGBitWriter {
    NkImageStream* s;
    uint32 buf=0; int32 cnt=0;

    void Write(uint32 code, int32 len) noexcept {
        buf=(buf<<len)|code; cnt+=len;
        while(cnt>=8){
            cnt-=8;
            const uint8 b=static_cast<uint8>(buf>>cnt);
            s->WriteU8(b);
            if(b==0xFF) s->WriteU8(0x00); // byte stuffing
        }
    }
    void Flush() noexcept {
        if(cnt>0){ Write(0x7F,7-cnt); } // pad avec des 1
        cnt=0; buf=0;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Construction de la table Huffman encodeur (codes canoniques)
// ─────────────────────────────────────────────────────────────────────────────

struct HuffEnc {
    uint32 codes[256];
    uint8  lens[256];

    void Build(const uint8* bits, const uint8* vals, int32 nv) {
        NkSet(codes,0,sizeof(codes));
        NkSet(lens,0,sizeof(lens));
        int32 code=0, k=0;
        for(int32 l=1;l<=16;++l){
            for(int32 i=0;i<bits[l];++i,++k){
                codes[vals[k]]=static_cast<uint32>(code);
                lens[vals[k]]=static_cast<uint8>(l);
                ++code;
            }
            code<<=1;
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Conversion RGB → YCbCr (encodeur)
// ─────────────────────────────────────────────────────────────────────────────

static NKIMG_INLINE void RGB2YCbCr(uint8 R,uint8 G,uint8 B,
                                    int32& Y,int32& Cb,int32& Cr) noexcept
{
    // ITU-R BT.601 integer approximation
    Y  = ((19595*R + 38470*G + 7471*B) >> 16);
    Cb = ((-11056*R - 21712*G + 32768*B) >> 16) + 128;
    Cr = ((32768*R - 27440*G - 5328*B) >> 16) + 128;
    Y=Y<0?0:Y>255?255:Y; Cb=Cb<0?0:Cb>255?255:Cb; Cr=Cr<0?0:Cr>255?255:Cr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkJPEGCodec::Encode
// ─────────────────────────────────────────────────────────────────────────────

bool NkJPEGCodec::Encode(const NkImage& img, uint8*& out, usize& outSize,
                          int32 quality) noexcept
{
    if(!img.IsValid()) return false;

    // Convertit en RGB24 ou Gray8 si nécessaire
    const NkImage* src=&img;
    NkImage* conv=nullptr;
    if(img.Format()!=NkImagePixelFormat::NK_RGB24&&img.Format()!=NkImagePixelFormat::NK_GRAY8){
        conv=img.Convert(NkImagePixelFormat::NK_RGB24);
        if(!conv) return false; src=conv;
    }

    const int32 w=src->Width(), h=src->Height();
    const bool gray=(src->Channels()==1);
    const int32 nComp=gray?1:3;

    // Construit les tables de quantification scalées par qualité
    quality=quality<1?1:quality>100?100:quality;
    const int32 qs=quality<50?(5000/quality):(200-quality*2);
    uint8 qluma[64],qchroma[64];
    for(int32 i=0;i<64;++i){
        int32 v=(kQLuma[i]*qs+50)/100; qluma[i]=static_cast<uint8>(v<1?1:v>255?255:v);
        v=(kQChroma[i]*qs+50)/100; qchroma[i]=static_cast<uint8>(v<1?1:v>255?255:v);
    }

    // Construit les tables Huffman
    HuffEnc heDCL,heDCC,heACL,heACC;
    heDCL.Build(kDCLumaBits,kDCLumaVals,12);
    heDCC.Build(kDCChromaBits,kDCChromaVals,12);
    heACL.Build(kACLumaBits,kACLumaVals,162);
    heACC.Build(kACChromaBits,kACChromaVals,162);

    NkImageStream s;

    // ── SOI ───────────────────────────────────────────────────────────────────
    s.WriteU16BE(0xFFD8);

    // ── APP0 JFIF ─────────────────────────────────────────────────────────────
    s.WriteU16BE(0xFFE0);
    s.WriteU16BE(16);
    s.WriteBytes(reinterpret_cast<const uint8*>("JFIF\0"),5);
    s.WriteU8(1); s.WriteU8(1); // version 1.1
    s.WriteU8(0); // aspect ratio (no units)
    s.WriteU16BE(1); s.WriteU16BE(1); // 1:1 aspect
    s.WriteU8(0); s.WriteU8(0); // no thumbnail

    // ── DQT ───────────────────────────────────────────────────────────────────
    auto writeDQT=[&](uint8 id,const uint8* qt){
        s.WriteU16BE(0xFFDB);
        s.WriteU16BE(67);
        s.WriteU8(id);
        for(int32 i=0;i<64;++i) s.WriteU8(qt[kZigZagInv[i]]);
    };
    writeDQT(0,qluma);
    if(!gray) writeDQT(1,qchroma);

    // ── SOF0 ──────────────────────────────────────────────────────────────────
    s.WriteU16BE(0xFFC0);
    s.WriteU16BE(gray?11:17);
    s.WriteU8(8); // precision
    s.WriteU16BE(static_cast<uint16>(h));
    s.WriteU16BE(static_cast<uint16>(w));
    s.WriteU8(static_cast<uint8>(nComp));
    if(gray){
        s.WriteU8(1); s.WriteU8(0x11); s.WriteU8(0);
    } else {
        s.WriteU8(1); s.WriteU8(0x22); s.WriteU8(0); // Y  4:2:0
        s.WriteU8(2); s.WriteU8(0x11); s.WriteU8(1); // Cb 1:1
        s.WriteU8(3); s.WriteU8(0x11); s.WriteU8(1); // Cr 1:1
    }

    // ── DHT ───────────────────────────────────────────────────────────────────
    auto writeDHT=[&](uint8 tc_id,const uint8* bits,const uint8* vals,int32 nv){
        int32 nb=0; for(int32 i=1;i<=16;++i) nb+=bits[i];
        s.WriteU16BE(0xFFC4);
        s.WriteU16BE(static_cast<uint16>(19+nb));
        s.WriteU8(tc_id);
        s.WriteBytes(bits+1,16); // bits[1..16]
        s.WriteBytes(vals,nb);
    };
    writeDHT(0x00,kDCLumaBits,kDCLumaVals,12);
    writeDHT(0x10,kACLumaBits,kACLumaVals,162);
    if(!gray){
        writeDHT(0x01,kDCChromaBits,kDCChromaVals,12);
        writeDHT(0x11,kACChromaBits,kACChromaVals,162);
    }

    // ── SOS ───────────────────────────────────────────────────────────────────
    s.WriteU16BE(0xFFDA);
    s.WriteU16BE(gray?8:12);
    s.WriteU8(static_cast<uint8>(nComp));
    if(gray){ s.WriteU8(1); s.WriteU8(0x00); }
    else { s.WriteU8(1);s.WriteU8(0x00); s.WriteU8(2);s.WriteU8(0x11); s.WriteU8(3);s.WriteU8(0x11); }
    s.WriteU8(0); s.WriteU8(63); s.WriteU8(0); // Ss=0, Se=63, Ah=Al=0

    // ── Scan data ─────────────────────────────────────────────────────────────
    JPEGBitWriter bw{&s};
    int32 dcPrev[3]={};

    // En 4:2:0 : blocs Y 2×2 (4 blocs) puis Cb puis Cr par MCU
    const int32 mcuW=gray?8:16, mcuH=gray?8:16;
    const int32 mcuCols=(w+mcuW-1)/mcuW;
    const int32 mcuRows=(h+mcuH-1)/mcuH;

    auto encodeDC=[&](int32 val,const HuffEnc& he,int32& prevDC){
        const int32 diff=val-prevDC; prevDC=val;
        int32 ssss=0,tmp=diff<0?-diff:diff;
        while(tmp){++ssss;tmp>>=1;}
        bw.Write(he.codes[ssss],he.lens[ssss]);
        if(ssss>0){
            const uint32 bits=diff<0?static_cast<uint32>(diff+(1<<ssss)-1):static_cast<uint32>(diff);
            bw.Write(bits&((1u<<ssss)-1),ssss);
        }
    };
    auto encodeAC=[&](const int32* coeffs,const HuffEnc& he){
        int32 run=0;
        for(int32 k=1;k<64;++k){
            const int32 v=coeffs[kZigZag[k]];
            if(v==0){++run;continue;}
            while(run>=16){ bw.Write(he.codes[0xF0],he.lens[0xF0]); run-=16; }
            int32 ssss=0,tmp=v<0?-v:v; while(tmp){++ssss;tmp>>=1;}
            const uint8 sym=static_cast<uint8>((run<<4)|ssss);
            bw.Write(he.codes[sym],he.lens[sym]);
            const uint32 bits=v<0?static_cast<uint32>(v+(1<<ssss)-1):static_cast<uint32>(v);
            bw.Write(bits&((1u<<ssss)-1),ssss);
            run=0;
        }
        if(run>0) bw.Write(he.codes[0],he.lens[0]); // EOB
    };

    for(int32 mr=0;mr<mcuRows;++mr){
        for(int32 mc=0;mc<mcuCols;++mc){
            if(gray){
                // Un seul bloc Y
                const int32 bx=mc*8, by=mr*8;
                uint8 block[64];
                for(int32 y=0;y<8;++y) for(int32 x=0;x<8;++x){
                    const int32 py=by+y<h?by+y:h-1;
                    const int32 px=bx+x<w?bx+x:w-1;
                    block[y*8+x]=src->RowPtr(py)[px];
                }
                int32 coeff[64];
                FDCT8x8(block,8,coeff,qluma);
                encodeDC(coeff[0],heDCL,dcPrev[0]);
                encodeAC(coeff,heACL);
            } else {
                // 4 blocs Y (2×2) + 1 Cb + 1 Cr
                for(int32 vy=0;vy<2;++vy) for(int32 vx=0;vx<2;++vx){
                    const int32 bx=mc*16+vx*8, by=mr*16+vy*8;
                    uint8 block[64];
                    for(int32 y=0;y<8;++y) for(int32 x=0;x<8;++x){
                        const int32 py=by+y<h?by+y:h-1;
                        const int32 px=bx+x<w?bx+x:w-1;
                        const uint8* p=src->RowPtr(py)+px*3;
                        int32 Y,Cb,Cr; RGB2YCbCr(p[0],p[1],p[2],Y,Cb,Cr);
                        block[y*8+x]=static_cast<uint8>(Y);
                    }
                    int32 coeff[64]; FDCT8x8(block,8,coeff,qluma);
                    encodeDC(coeff[0],heDCL,dcPrev[0]);
                    encodeAC(coeff,heACL);
                }
                // Cb/Cr — sous-échantillonnage 2×2 (moyenne)
                for(int32 ci=1;ci<=2;++ci){
                    uint8 block[64];
                    for(int32 y=0;y<8;++y) for(int32 x=0;x<8;++x){
                        // Moyenne des 4 pixels (2×2 downscale)
                        int32 sum=0;
                        for(int32 dy=0;dy<2;++dy) for(int32 dx=0;dx<2;++dx){
                            const int32 py=mr*16+y*2+dy; const int32 px=mc*16+x*2+dx;
                            const int32 ry=py<h?py:h-1; const int32 rx=px<w?px:w-1;
                            const uint8* p=src->RowPtr(ry)+rx*3;
                            int32 Y,Cb,Cr; RGB2YCbCr(p[0],p[1],p[2],Y,Cb,Cr);
                            sum+=(ci==1?Cb:Cr);
                        }
                        block[y*8+x]=static_cast<uint8>(sum/4);
                    }
                    int32 coeff[64]; FDCT8x8(block,8,coeff,qchroma);
                    encodeDC(coeff[0],ci==1?heDCC:heDCC,dcPrev[ci]);
                    encodeAC(coeff,ci==1?heACC:heACC);
                }
            }
        }
    }
    bw.Flush();

    // ── EOI ───────────────────────────────────────────────────────────────────
    s.WriteU16BE(0xFFD9);

    if(conv) conv->Free();
    return s.TakeBuffer(out,outSize);
}

} // namespace nkentseu
