/**
 * @File    NkQOICodec.cpp
 * @Brief   Codec QOI (Quite OK Image Format) — spec v1.0 exacte.
 *          https://qoiformat.org/qoi-specification.pdf
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Spec
 *  Header : "qoif" + uint32 width + uint32 height + uint8 channels + uint8 colorspace
 *  Chunks : QOI_OP_RGB(0xFE), QOI_OP_RGBA(0xFF), QOI_OP_INDEX(00xxxxxx),
 *           QOI_OP_DIFF(01xxxxxx), QOI_OP_LUMA(10xxxxxx), QOI_OP_RUN(11xxxxxx)
 *  End    : 7 bytes 0x00 + 1 byte 0x01
 */
#include "NKImage/Codecs/QOI/NkQOICodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {
    using namespace nkentseu::memory;

    static inline void* qM(usize n)  noexcept { return NkAlloc(n); }
    static inline void  qF(void* p)  noexcept { if(p) NkFree(p); }
    static inline void  qCp(void* d,const void* s,usize n) noexcept { NkCopy(static_cast<uint8*>(d),static_cast<const uint8*>(s),n); }
    static inline void  qSt(void* d,int v,usize n) noexcept { NkSet(static_cast<uint8*>(d),uint8(v),n); }

    // Hash QOI : (r*3 + g*5 + b*7 + a*11) % 64
    static NKIMG_INLINE int32 qHash(uint8 r,uint8 g,uint8 b,uint8 a) noexcept {
        return (int32(r)*3+int32(g)*5+int32(b)*7+int32(a)*11)%64;
    }

    static const uint8 kEnd[8]={0,0,0,0,0,0,0,1};

    //=============================================================================
    // NkQOICodec::Decode
    //=============================================================================
    NkImage* NkQOICodec::Decode(const uint8* data,usize size) noexcept {
        if(size<14) return nullptr;
        if(data[0]!='q'||data[1]!='o'||data[2]!='i'||data[3]!='f') return nullptr;

        const int32 w = int32((uint32(data[4])<<24)|(uint32(data[5])<<16)|(uint32(data[6])<<8)|data[7]);
        const int32 h = int32((uint32(data[8])<<24)|(uint32(data[9])<<16)|(uint32(data[10])<<8)|data[11]);
        const int32 channels = data[12]; // 3=RGB, 4=RGBA
        // data[13] = colorspace (0=sRGB, 1=linear) — ignored for decoding

        if(w<=0||h<=0||channels<3||channels>4) return nullptr;

        const NkImagePixelFormat fmt=(channels==4)?NkImagePixelFormat::NK_RGBA32:NkImagePixelFormat::NK_RGB24;
        NkImage* img=NkImage::Alloc(w,h,fmt);
        if(!img) return nullptr;

        // Table d'index couleurs (64 entrees RGBA)
        uint8 table[64*4];
        qSt(table,0,sizeof(table));

        uint8 r=0,g=0,b=0,a=255; // pixel courant
        int32 run=0;
        usize pos=14; // apres le header de 14 bytes
        const usize limit=size-8; // exclut les 8 bytes de fin

        for(int32 y=0;y<h;++y){
            uint8* row=img->RowPtr(y);
            for(int32 x=0;x<w;++x){
                if(run>0){
                    --run;
                } else {
                    if(pos>=limit) goto done;
                    const uint8 b0=data[pos++];

                    if(b0==0xFE){ // QOI_OP_RGB
                        if(pos+3>limit) goto done;
                        r=data[pos++];g=data[pos++];b=data[pos++];
                    } else if(b0==0xFF){ // QOI_OP_RGBA
                        if(pos+4>limit) goto done;
                        r=data[pos++];g=data[pos++];b=data[pos++];a=data[pos++];
                    } else {
                        const uint8 tag=b0>>6;
                        if(tag==0){ // QOI_OP_INDEX
                            const int32 idx=(b0&0x3F)*4;
                            r=table[idx];g=table[idx+1];b=table[idx+2];a=table[idx+3];
                        } else if(tag==1){ // QOI_OP_DIFF
                            r=uint8(r+((b0>>4)&3)-2);
                            g=uint8(g+((b0>>2)&3)-2);
                            b=uint8(b+(b0&3)-2);
                        } else if(tag==2){ // QOI_OP_LUMA
                            if(pos>=limit) goto done;
                            const uint8 b1=data[pos++];
                            const int32 dg=(b0&0x3F)-32;
                            r=uint8(r+dg+((b1>>4)&0xF)-8);
                            g=uint8(g+dg);
                            b=uint8(b+dg+(b1&0xF)-8);
                        } else { // QOI_OP_RUN (tag==3)
                            run=(b0&0x3F); // run-1 (0=1 pixel, 61=62 pixels)
                        }
                    }
                    // Met a jour la table d'index
                    const int32 idx=qHash(r,g,b,a)*4;
                    table[idx]=r;table[idx+1]=g;table[idx+2]=b;table[idx+3]=a;
                }
                // Ecrit le pixel
                if(channels==4){uint8* p=row+x*4;p[0]=r;p[1]=g;p[2]=b;p[3]=a;}
                else            {uint8* p=row+x*3;p[0]=r;p[1]=g;p[2]=b;}
            }
        }
        done:
        return img;
    }

    //=============================================================================
    // NkQOICodec::Encode
    //=============================================================================
    bool NkQOICodec::Encode(const NkImage& img,uint8*& out,usize& outSize) noexcept {
        if(!img.IsValid()) return false;

        // Normalise vers RGB24 ou RGBA32
        const NkImage* src=&img; NkImage* conv=nullptr;
        const int32 ch=img.Channels();
        if(ch!=3&&ch!=4){
            const NkImagePixelFormat tgt=(ch==1||ch==2)?NkImagePixelFormat::NK_RGBA32:NkImagePixelFormat::NK_RGB24;
            conv=img.Convert(tgt);if(!conv)return false;src=conv;
        }
        const int32 w=src->Width(),h=src->Height(),sch=src->Channels();
        const bool hasAlpha=(sch==4);

        // Pire cas : chaque pixel prend 5 octets (RGBA) + header(14) + end(8)
        const usize maxSz=14+usize(w)*h*5+8;
        uint8* buf=static_cast<uint8*>(qM(maxSz));
        if(!buf){if(conv)conv->Free();return false;}

        // Header
        buf[0]='q';buf[1]='o';buf[2]='i';buf[3]='f';
        buf[4]=uint8(w>>24);buf[5]=uint8(w>>16);buf[6]=uint8(w>>8);buf[7]=uint8(w);
        buf[8]=uint8(h>>24);buf[9]=uint8(h>>16);buf[10]=uint8(h>>8);buf[11]=uint8(h);
        buf[12]=uint8(sch);  // 3 ou 4
        buf[13]=0;           // colorspace: sRGB
        usize pos=14;

        uint8 table[64*4]; qSt(table,0,sizeof(table));
        uint8 pr=0,pg=0,pb=0,pa=255; // pixel precedent
        int32 run=0;

        auto flushRun=[&](){
            if(run>0){buf[pos++]=uint8(0xC0|(run-1));run=0;}
        };

        for(int32 y=0;y<h;++y){
            const uint8* row=src->RowPtr(y);
            for(int32 x=0;x<w;++x){
                const uint8* p=row+x*sch;
                const uint8 r=p[0],g=p[1],b=p[2],a=(hasAlpha?p[3]:255);

                if(r==pr&&g==pg&&b==pb&&a==pa){
                    // QOI_OP_RUN
                    ++run;
                    if(run==62){flushRun();}
                } else {
                    flushRun();
                    // Essaie QOI_OP_INDEX
                    const int32 hi=qHash(r,g,b,a)*4;
                    if(table[hi]==r&&table[hi+1]==g&&table[hi+2]==b&&table[hi+3]==a){
                        buf[pos++]=uint8(hi/4); // tag 00 + index 6 bits
                    } else {
                        table[hi]=r;table[hi+1]=g;table[hi+2]=b;table[hi+3]=a;
                        const int32 dr=int32(r)-pr,dg=int32(g)-pg,db=int32(b)-pb;
                        if(a==pa){
                            // Essaie QOI_OP_DIFF : dr,dg,db in [-2,1]
                            if(dr>=-2&&dr<=1&&dg>=-2&&dg<=1&&db>=-2&&db<=1){
                                buf[pos++]=uint8(0x40|((dr+2)<<4)|((dg+2)<<2)|(db+2));
                            } else {
                                // Essaie QOI_OP_LUMA : dg in [-32,31], dr-dg in [-8,7], db-dg in [-8,7]
                                const int32 drdg=dr-dg,dbdg=db-dg;
                                if(dg>=-32&&dg<=31&&drdg>=-8&&drdg<=7&&dbdg>=-8&&dbdg<=7){
                                    buf[pos++]=uint8(0x80|(dg+32));
                                    buf[pos++]=uint8(((drdg+8)<<4)|(dbdg+8));
                                } else {
                                    // QOI_OP_RGB
                                    buf[pos++]=0xFE;buf[pos++]=r;buf[pos++]=g;buf[pos++]=b;
                                }
                            }
                        } else {
                            // QOI_OP_RGBA
                            buf[pos++]=0xFF;buf[pos++]=r;buf[pos++]=g;buf[pos++]=b;buf[pos++]=a;
                        }
                    }
                    pr=r;pg=g;pb=b;pa=a;
                }
            }
        }
        flushRun();
        // End marker
        qCp(buf+pos,kEnd,8); pos+=8;

        if(conv)conv->Free();
        out=buf; outSize=pos;
        return true;
    }

} // namespace nkentseu