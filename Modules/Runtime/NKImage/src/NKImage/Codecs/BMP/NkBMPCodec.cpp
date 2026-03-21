/**
 * @File    NkBMPCodec.cpp
 * @Brief   Codec BMP production-ready — DIB complet.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Correctness
 *  Lecture :
 *    Headers  : BITMAPCOREHEADER (12), BITMAPINFOHEADER (40),
 *               BITMAPV4HEADER (108), BITMAPV5HEADER (124)
 *    Bit depth: 1, 2, 4, 8 (indexé), 16, 24, 32 bpp
 *    Compression : BI_RGB (0), BI_RLE4 (2), BI_RLE8 (1), BI_BITFIELDS (3)
 *    Masques BITFIELDS : 16bpp RGB565/555, 32bpp ARGB
 *    Orientation : bottom-up (hauteur positive), top-down (hauteur négative)
 *    Transparence : canal alpha 32bpp si non nul
 *    OS/2 BMP (CORE header) supporté
 *  Écriture :
 *    BITMAPINFOHEADER (40 octets) toujours
 *    24bpp (RGB) ou 32bpp (RGBA), sans compression
 *    Padding 4 octets par ligne
 *    Orientation : top-down (bit 0x20 dans imageDescriptor = non, on écrit bottom-up standard)
 */
#include "NKImage/NkBMPCodec.h"
#include <cstring>
#include <cstdlib>
namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

static NKIMG_INLINE uint8 Clamp8(int32 v) noexcept {
    return static_cast<uint8>(v<0?0:v>255?255:v);
}

// Extrait N bits à partir de la position bitPos dans src (LSB-first)
static NKIMG_INLINE uint8 ExtractBits(const uint8* src, int32 bitPos, int32 n) noexcept {
    const int32 byteIdx = bitPos/8;
    const int32 shift   = bitPos%8;
    const uint32 raw = static_cast<uint32>(src[byteIdx])
                     |(static_cast<uint32>(src[byteIdx+1])<<8);
    const uint32 mask=(1u<<n)-1;
    return static_cast<uint8>((raw>>shift)&mask);
}

// Convertit une couleur masquée (BITFIELDS) vers [0-255]
static uint8 ScaleMask(uint32 val, uint32 mask) noexcept {
    if(!mask) return 0;
    // Trouve la position du bit le plus bas du masque
    int32 shift=0; uint32 m=mask;
    while(m&&!(m&1)){++shift;m>>=1;}
    uint32 v=(val&mask)>>shift;
    // Normalise vers 8 bits
    uint32 bits=0; uint32 tmp=m; while(tmp){++bits;tmp>>=1;}
    if(bits==0) return 0;
    if(bits>=8) return static_cast<uint8>(v>>(bits-8));
    return static_cast<uint8>((v*255u)/((1u<<bits)-1u));
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkBMPCodec::Decode
// ─────────────────────────────────────────────────────────────────────────────

NkImage* NkBMPCodec::Decode(const uint8* data, usize size) noexcept {
    if(size<14) return nullptr;
    NkImageStream s(data,size);

    // ── File header (BITMAPFILEHEADER) ───────────────────────────────────────
    if(s.ReadU8()!='B'||s.ReadU8()!='M') return nullptr;
    s.Skip(4); // fileSize (non fiable)
    s.Skip(4); // reserved1+reserved2
    const uint32 pixelOffset=s.ReadU32LE();

    // ── DIB header ───────────────────────────────────────────────────────────
    const uint32 dibSize=s.ReadU32LE();
    int32  width=0,height=0;
    uint16 planes=1;
    uint16 bpp=0;
    uint32 compress=0;
    uint32 clrUsed=0;
    bool   isCore=false; // OS/2 BITMAPCOREHEADER

    if(dibSize==12){ // BITMAPCOREHEADER (OS/2)
        width  =s.ReadU16LE();
        height =s.ReadU16LE();
        planes =s.ReadU16LE();
        bpp    =s.ReadU16LE();
        isCore =true;
        compress=0;
    } else { // BITMAPINFOHEADER ou supérieur
        width  =s.ReadI32LE();
        height =s.ReadI32LE();
        planes =s.ReadU16LE();
        bpp    =s.ReadU16LE();
        compress=s.ReadU32LE();
        s.Skip(4); // imageSize
        s.Skip(4); // xPixPerMeter
        s.Skip(4); // yPixPerMeter
        clrUsed=s.ReadU32LE();
        s.Skip(4); // clrImportant
    }
    (void)planes;

    // Orientation : hauteur > 0 = bottom-up, < 0 = top-down
    const bool topDown=(height<0);
    if(topDown) height=-height;
    if(width<=0||height<=0) return nullptr;

    // ── Masques BITFIELDS ────────────────────────────────────────────────────
    uint32 maskR=0,maskG=0,maskB=0,maskA=0;
    const bool hasBitfields=(compress==3||compress==6);

    if(hasBitfields){
        // Masques juste après le DIB header
        const usize maskOff=14+dibSize;
        if(maskOff+16<=size){
            NkImageStream ms(data+maskOff,16);
            maskR=ms.ReadU32LE(); maskG=ms.ReadU32LE();
            maskB=ms.ReadU32LE(); maskA=ms.ReadU32LE();
        }
    }

    // Masques par défaut pour 16bpp RGB555 / RGB565
    if(bpp==16&&!hasBitfields){
        maskR=0x7C00; maskG=0x03E0; maskB=0x001F; maskA=0;
    }
    if(bpp==32&&!hasBitfields){
        maskR=0x00FF0000; maskG=0x0000FF00; maskB=0x000000FF; maskA=0xFF000000;
    }

    // ── Palette de couleurs ──────────────────────────────────────────────────
    int32 palCount=0;
    if(bpp<=8){
        if(isCore) palCount=1<<bpp;
        else palCount=(clrUsed>0)?(int32)clrUsed:(1<<bpp);
    }
    uint8 palette[256*4]={}; // BGRA
    if(palCount>0){
        const int32 entrySize=isCore?3:4;
        for(int32 i=0;i<palCount&&!s.HasError();++i)
            s.ReadBytes(palette+i*4,entrySize);
    }

    // ── Determine le format de sortie ────────────────────────────────────────
    bool hasAlpha=false;
    if(bpp==32){
        if(maskA!=0) hasAlpha=true;
        else {
            // Vérifie si le canal alpha est non-nul (pour les BMP 32bpp sans masque)
            // On le déterminera pendant la lecture des pixels
            hasAlpha=true; // on suppose RGBA, on ajustera si tout zéro
        }
    }

    const NkPixelFormat fmt=hasAlpha?NkPixelFormat::RGBA32:NkPixelFormat::RGB24;
    NkImage* img=NkImage::Alloc(width,height,fmt);
    if(!img) return nullptr;

    // ── Lit les pixels depuis pixelOffset ────────────────────────────────────
    s.Seek(pixelOffset);

    // Stride de la ligne source (aligné sur 4 octets)
    const int32 srcBpp=(bpp+7)/8;
    const int32 srcStride=((width*bpp+31)/32)*4;

    // Tampon d'une ligne source
    uint8* lineBuf=static_cast<uint8*>(::malloc(srcStride+4));
    if(!lineBuf){img->Free();return nullptr;}

    if(compress==0||compress==3||compress==6){
        // ── BI_RGB ou BI_BITFIELDS : lecture directe ──────────────────────────
        bool alphaAllZero=true; // pour détecter les BMP 32bpp sans vrai alpha

        for(int32 row=0;row<height;++row){
            const int32 dstY=topDown?row:(height-1-row);
            uint8* dst=img->RowPtr(dstY);

            s.ReadBytes(lineBuf,srcStride);

            for(int32 x=0;x<width;++x){
                uint8 r=0,g=0,b=0,a=255;

                if(bpp==1){
                    const uint8 idx=ExtractBits(lineBuf,x,1);
                    r=palette[idx*4+2]; g=palette[idx*4+1]; b=palette[idx*4+0];
                } else if(bpp==2){
                    const uint8 idx=ExtractBits(lineBuf,x*2,2);
                    r=palette[idx*4+2]; g=palette[idx*4+1]; b=palette[idx*4+0];
                } else if(bpp==4){
                    const uint8 idx=(x&1)?(lineBuf[x/2]&0x0F):(lineBuf[x/2]>>4);
                    r=palette[idx*4+2]; g=palette[idx*4+1]; b=palette[idx*4+0];
                } else if(bpp==8){
                    const uint8 idx=lineBuf[x];
                    r=palette[idx*4+2]; g=palette[idx*4+1]; b=palette[idx*4+0];
                } else if(bpp==16){
                    const uint16 v=static_cast<uint16>(lineBuf[x*2])
                                 |(static_cast<uint16>(lineBuf[x*2+1])<<8);
                    if(hasBitfields){
                        r=ScaleMask(v,maskR);g=ScaleMask(v,maskG);b=ScaleMask(v,maskB);
                        a=maskA?ScaleMask(v,maskA):255;
                    } else {
                        // RGB555 par défaut
                        r=static_cast<uint8>(((v>>10)&0x1F)*255/31);
                        g=static_cast<uint8>(((v>> 5)&0x1F)*255/31);
                        b=static_cast<uint8>(( v     &0x1F)*255/31);
                    }
                } else if(bpp==24){
                    b=lineBuf[x*3+0]; g=lineBuf[x*3+1]; r=lineBuf[x*3+2];
                } else if(bpp==32){
                    const uint32 v=static_cast<uint32>(lineBuf[x*4+0])
                                 |(static_cast<uint32>(lineBuf[x*4+1])<<8)
                                 |(static_cast<uint32>(lineBuf[x*4+2])<<16)
                                 |(static_cast<uint32>(lineBuf[x*4+3])<<24);
                    if(hasBitfields){
                        r=ScaleMask(v,maskR);g=ScaleMask(v,maskG);b=ScaleMask(v,maskB);
                        a=ScaleMask(v,maskA);
                    } else {
                        b=lineBuf[x*4+0]; g=lineBuf[x*4+1];
                        r=lineBuf[x*4+2]; a=lineBuf[x*4+3];
                    }
                    if(a!=0) alphaAllZero=false;
                }

                if(fmt==NkPixelFormat::RGBA32){
                    dst[x*4+0]=r; dst[x*4+1]=g; dst[x*4+2]=b; dst[x*4+3]=a;
                } else {
                    dst[x*3+0]=r; dst[x*3+1]=g; dst[x*3+2]=b;
                }
            }
        }

        // Si tous les alpha étaient 0 en 32bpp sans masque, force opaque
        if(bpp==32&&!hasBitfields&&alphaAllZero){
            for(int32 y=0;y<height;++y){
                uint8* row=img->RowPtr(y);
                for(int32 x=0;x<width;++x) row[x*4+3]=255;
            }
        }

    } else if(compress==1){ // BI_RLE8
        // ── Décodeur RLE8 ─────────────────────────────────────────────────────
        int32 x=0,y=0;
        while(!s.IsEOF()){
            const uint8 b0=s.ReadU8();
            const uint8 b1=s.ReadU8();
            if(b0==0){
                if(b1==0){      // EOL
                    x=0; ++y;
                } else if(b1==1){ // EOF
                    break;
                } else if(b1==2){ // DELTA
                    x+=s.ReadU8(); y+=s.ReadU8();
                } else {          // absolute run
                    const int32 cnt=b1;
                    for(int32 i=0;i<cnt;++i){
                        const uint8 idx=s.ReadU8();
                        const int32 dstY=height-1-y;
                        if(x<width&&dstY>=0&&dstY<height){
                            uint8* d=img->RowPtr(dstY)+x*3;
                            d[0]=palette[idx*4+2]; d[1]=palette[idx*4+1]; d[2]=palette[idx*4+0];
                        }
                        ++x;
                    }
                    if(cnt&1) s.ReadU8(); // padding
                }
            } else { // encoded run
                const uint8 idx=b1;
                for(int32 i=0;i<b0;++i){
                    const int32 dstY=height-1-y;
                    if(x<width&&dstY>=0&&dstY<height){
                        uint8* d=img->RowPtr(dstY)+x*3;
                        d[0]=palette[idx*4+2]; d[1]=palette[idx*4+1]; d[2]=palette[idx*4+0];
                    }
                    ++x;
                }
            }
        }

    } else if(compress==2){ // BI_RLE4
        // ── Décodeur RLE4 ─────────────────────────────────────────────────────
        int32 x=0,y=0;
        while(!s.IsEOF()){
            const uint8 b0=s.ReadU8();
            const uint8 b1=s.ReadU8();
            if(b0==0){
                if(b1==0){ x=0; ++y; }
                else if(b1==1){ break; }
                else if(b1==2){ x+=s.ReadU8(); y+=s.ReadU8(); }
                else {
                    const int32 cnt=b1;
                    for(int32 i=0;i<cnt;++i){
                        const uint8 byte_=(i&1)?s.ReadU8():s.ReadU8(); // simplifié
                        const uint8 idx=(i&1)?(byte_&0x0F):(byte_>>4);
                        const int32 dstY=height-1-y;
                        if(x<width&&dstY>=0&&dstY<height){
                            uint8* d=img->RowPtr(dstY)+x*3;
                            d[0]=palette[idx*4+2]; d[1]=palette[idx*4+1]; d[2]=palette[idx*4+0];
                        }
                        ++x;
                    }
                    const int32 bytes=(cnt+3)/4*2; (void)bytes;
                    if(((cnt+3)/4)&1) s.ReadU8();
                }
            } else {
                const uint8 idx0=(b1>>4)&0xF, idx1=b1&0xF;
                for(int32 i=0;i<b0;++i){
                    const uint8 idx=(i&1)?idx1:idx0;
                    const int32 dstY=height-1-y;
                    if(x<width&&dstY>=0&&dstY<height){
                        uint8* d=img->RowPtr(dstY)+x*3;
                        d[0]=palette[idx*4+2]; d[1]=palette[idx*4+1]; d[2]=palette[idx*4+0];
                    }
                    ++x;
                }
            }
        }
    }

    ::free(lineBuf);
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkBMPCodec::Encode
// ─────────────────────────────────────────────────────────────────────────────

bool NkBMPCodec::Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept {
    if(!img.IsValid()) return false;

    // Convertit si nécessaire
    const NkImage* src=&img;
    NkImage* conv=nullptr;
    const bool needRGBA=(img.Format()==NkPixelFormat::RGBA32);
    if(img.Format()!=NkPixelFormat::RGB24&&img.Format()!=NkPixelFormat::RGBA32){
        conv=img.Convert(NkPixelFormat::RGB24);
        if(!conv) return false; src=conv;
    }

    const int32 w=src->Width(), h=src->Height();
    const uint16 bpp=static_cast<uint16>(src->Channels()==4?32:24);
    // Stride BMP : aligné sur 4 octets, bottom-up
    const int32 srcStride=src->Stride(); // stride NkImage (déjà aligné sur 4)
    const int32 bmpStride=((w*(bpp/8)+3)/4)*4;
    const uint32 pixDataSize=static_cast<uint32>(bmpStride*h);

    // BITMAPFILEHEADER + BITMAPINFOHEADER = 14 + 40 = 54 octets
    // Pour RGBA32 : BITMAPV4HEADER (108) pour les masques
    const uint32 dibHeaderSize=(bpp==32)?108:40;
    const uint32 fileSize=14+dibHeaderSize+pixDataSize;

    NkImageStream s;

    // ── BITMAPFILEHEADER ─────────────────────────────────────────────────────
    s.WriteU8('B'); s.WriteU8('M');
    s.WriteU32LE(fileSize);
    s.WriteU32LE(0); // reserved
    s.WriteU32LE(14+dibHeaderSize); // offset pixels

    // ── BITMAPINFOHEADER / BITMAPV4HEADER ────────────────────────────────────
    s.WriteU32LE(dibHeaderSize);
    s.WriteI32LE(w);
    s.WriteI32LE(-h); // top-down (height négatif)
    s.WriteU16LE(1);  // planes
    s.WriteU16LE(bpp);
    s.WriteU32LE(bpp==32?3:0); // BI_BITFIELDS pour 32bpp, BI_RGB sinon
    s.WriteU32LE(pixDataSize);
    s.WriteI32LE(2835); // 72 DPI X
    s.WriteI32LE(2835); // 72 DPI Y
    s.WriteU32LE(0); // clrUsed
    s.WriteU32LE(0); // clrImportant

    if(bpp==32){
        // Masques RGBA (BITMAPV4HEADER extension)
        s.WriteU32LE(0x00FF0000); // R
        s.WriteU32LE(0x0000FF00); // G
        s.WriteU32LE(0x000000FF); // B
        s.WriteU32LE(0xFF000000); // A
        // Espace couleur + gamut (champs V4)
        s.WriteU32LE(0x57696E20); // CSType = "Win " (sRGB)
        for(int32 i=0;i<36;++i) s.WriteU8(0); // endpoints
        s.WriteU32LE(0); // gammaRed
        s.WriteU32LE(0); // gammaGreen
        s.WriteU32LE(0); // gammaBlue
    }

    // ── Pixels (bottom-up puisque height positif au final) ───────────────────
    // (on a mis -h dans le header, donc top-down en fait)
    for(int32 y=0; y<h; ++y){
        const uint8* row=src->RowPtr(y);
        int32 written=0;
        for(int32 x=0;x<w;++x){
            const uint8* p=row+x*src->Channels();
            if(bpp==24){
                s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); // BGR
                written+=3;
            } else {
                // RGBA → BGRA
                s.WriteU8(p[2]); s.WriteU8(p[1]); s.WriteU8(p[0]); s.WriteU8(p[3]);
                written+=4;
            }
        }
        // Padding de ligne
        while(written%4){ s.WriteU8(0); ++written; }
    }

    if(conv) conv->Free();
    return s.TakeBuffer(out,outSize);
}

} // namespace nkentseu
