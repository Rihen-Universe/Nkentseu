/**
 * @File    NkPNGCodec.cpp
 * @Brief   Codec PNG — adaptation exacte de stb_image v2.16 (public domain, Sean Barrett).
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @stb_correspondance
 *  Décodage : stbi__parse_png_file + stbi__create_png_image_raw
 *  - Tous color types : 0(gray), 2(RGB), 3(palette), 4(gray+A), 6(RGBA)
 *  - Bit depths : 1, 2, 4, 8, 16
 *  - Filtres : None(0), Sub(1), Up(2), Average(3), Paeth(4) — identiques stb
 *  - stbi__depth_scale_table : kDepthScale[9] = {0,0xFF,0x55,0,0x11,0,0,0,0x01}
 *  - Décompression : NkDeflate::Decompress (inflate identique stb)
 *
 *  L'inflate dans NkDeflate résout le bug précédent :
 *  FDICT lu sur in[1] (FLG) au lieu de in[0] (CMF) — corrigé dans NkImage.cpp.
 */
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"

namespace nkentseu {
    using namespace nkentseu::memory;

    static inline void* pngMalloc(usize n)       noexcept { return NkAlloc(n); }
    static inline void* pngCalloc(usize n,usize s) noexcept { return NkAllocZero(n,s); }
    static inline void  pngFree  (void* p)        noexcept { if(p) NkFree(p); }
    static inline void  pngCopy  (void* d,const void* s,usize n) noexcept { NkCopy(static_cast<uint8*>(d),static_cast<const uint8*>(s),n); }
    static inline void  pngSet   (void* d,int v,usize n)  noexcept { NkSet(static_cast<uint8*>(d),static_cast<uint8>(v),n); }

    // ─────────────────────────────────────────────────────────────────────────────
    //  CRC32 — identique stb_image
    // ─────────────────────────────────────────────────────────────────────────────
    static uint32 gCRC[256];
    static bool   gCRCOk=false;
    static void crcInit() noexcept {
        if(gCRCOk) return;
        for(uint32 n=0;n<256;++n){uint32 c=n;for(int k=0;k<8;++k)c=(c&1)?(0xEDB88320u^(c>>1)):(c>>1);gCRC[n]=c;}
        gCRCOk=true;
    }
    // CRC32 sur type(4 bytes) + data(len bytes) — pour writeChunk
    static uint32 chunkCRC(const uint8* tb, const uint8* data, usize len) noexcept {
        uint32 c=0xFFFFFFFFu;
        for(int i=0;i<4;++i) c=gCRC[(c^tb[i])&0xFF]^(c>>8);
        if(data) for(usize i=0;i<len;++i) c=gCRC[(c^data[i])&0xFF]^(c>>8);
        return ~c;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Paeth — stbi__paeth (RFC 2083 §6.3)
    // ─────────────────────────────────────────────────────────────────────────────
    static NKIMG_INLINE int32 paeth(int32 a,int32 b,int32 c) noexcept {
        int32 p=a+b-c,pa=p>a?p-a:a-p,pb=p>b?p-b:b-p,pc=p>c?p-c:c-p;
        if(pa<=pb&&pa<=pc) return a; if(pb<=pc) return b; return c;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Dé-filtrage — stbi__create_png_image_raw
    // ─────────────────────────────────────────────────────────────────────────────
    // bpp = bytes per filter unit (≥1), rowB = row bytes total
    static void unfilter(uint8* cur,const uint8* prior,int32 rowB,int32 bpp,uint8 f) noexcept {
        if(bpp<1) bpp=1;
        switch(f){
            case 0: break;
            case 1:
                for(int32 i=bpp;i<rowB;++i) cur[i]=uint8(cur[i]+cur[i-bpp]);
                break;
            case 2:
                for(int32 i=0;i<rowB;++i) cur[i]=uint8(cur[i]+prior[i]);
                break;
            case 3:
                // stb: premier groupe (i < bpp) : seul prior contribue
                for(int32 i=0;i<bpp;++i) cur[i]=uint8(cur[i]+(prior[i]>>1));
                for(int32 i=bpp;i<rowB;++i) cur[i]=uint8(cur[i]+uint8((uint32(cur[i-bpp])+prior[i])>>1));
                break;
            case 4:
                for(int32 i=0;i<bpp;++i) cur[i]=uint8(cur[i]+paeth(0,prior[i],0));
                for(int32 i=bpp;i<rowB;++i) cur[i]=uint8(cur[i]+paeth(cur[i-bpp],prior[i],prior[i-bpp]));
                break;
            default: break;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  stbi__depth_scale_table
    // ─────────────────────────────────────────────────────────────────────────────
    // static const uint8 stbi__depth_scale_table[9] = {0,0xff,0x55,0,0x11,0,0,0,0x01};
    static const uint8 kDS[9]={0,0xFF,0x55,0,0x11,0,0,0,0x01};

    // Expansion scalaire (gray) ou indice palette vers uint8 normalisé
    static void expandSamp(const uint8* src,uint8* dst,int32 n,int32 bd) noexcept {
        const uint8 sc=kDS[bd];
        switch(bd){
            case  8: pngCopy(dst,src,n); return;
            case 16: for(int32 i=0;i<n;++i) dst[i]=src[i*2]; return; // MSB
            case  4: for(int32 i=0;i<n;++i) dst[i]=uint8((i&1?(src[i/2]&0xF):(src[i/2]>>4))*sc); return;
            case  2: for(int32 i=0;i<n;++i) dst[i]=uint8(((src[i/4]>>((3-(i%4))*2))&3)*sc); return;
            case  1: for(int32 i=0;i<n;++i) dst[i]=uint8(((src[i/8]>>(7-(i%8)))&1)*sc); return;
            default: return;
        }
    }
    // Indices palette bruts (sans normalisation)
    static void expandIdx(const uint8* src,uint8* dst,int32 w,int32 bd) noexcept {
        switch(bd){
            case 8: pngCopy(dst,src,w); return;
            case 4: for(int32 i=0;i<w;++i) dst[i]=(i&1?(src[i/2]&0xF):(src[i/2]>>4)); return;
            case 2: for(int32 i=0;i<w;++i) dst[i]=(src[i/4]>>((3-(i%4))*2))&3; return;
            case 1: for(int32 i=0;i<w;++i) dst[i]=(src[i/8]>>(7-(i%8)))&1; return;
            default: return;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkPNGCodec::Decode
    // ─────────────────────────────────────────────────────────────────────────────
    NkImage* NkPNGCodec::Decode(const uint8* data,usize size) noexcept {
        if(size<8) return nullptr;
        crcInit();
        NkImageStream s(data,size);

        // Signature
        uint8 sig[8]; s.ReadBytes(sig,8);
        static const uint8 kSIG[8]={0x89,'P','N','G','\r','\n',0x1A,'\n'};
        if(NkCompare(sig,kSIG,8)!=0) return nullptr;

        int32 imgW=0,imgH=0,bd=0,ct=0,interlace=0;
        bool hasIHDR=false;

        // Palette + tRNS
        uint8 pal[256*3]={};
        uint8 palA[256]; pngSet(palA,255,256);
        int32 palSz=0;
        uint16 tRns[3]={0,0,0}; bool hasTRNS=false;

        // Collecte IDAT
        uint8* idat=static_cast<uint8*>(pngMalloc(size));
        if(!idat) return nullptr;
        usize idatLen=0;

        while(!s.IsEOF()&&!s.HasError()){
            if(!s.HasBytes(12)) break;
            const uint32 cLen=s.ReadU32BE();
            const uint32 cType=s.ReadU32BE();
            const usize cData=s.Tell();
            if(cLen>uint32(size)){s.Seek(s.Size());break;}

            if(cType==0x49484452u){ // IHDR
                if(cLen<13){s.Seek(cData+cLen+4);continue;}
                imgW=int32(s.ReadU32BE()); imgH=int32(s.ReadU32BE());
                bd=s.ReadU8(); ct=s.ReadU8();
                (void)s.ReadU8();(void)s.ReadU8();
                interlace=s.ReadU8(); hasIHDR=true;
            }
            else if(cType==0x504C5445u){ // PLTE
                palSz=int32(cLen/3); if(palSz>256)palSz=256;
                for(int32 i=0;i<palSz;++i){pal[i*3]=s.ReadU8();pal[i*3+1]=s.ReadU8();pal[i*3+2]=s.ReadU8();}
            }
            else if(cType==0x74524E53u){ // tRNS
                hasTRNS=true;
                if(ct==3){int32 n=int32(cLen)<256?int32(cLen):256; s.ReadBytes(palA,n);}
                else if(ct==0&&cLen>=2){tRns[0]=tRns[1]=tRns[2]=s.ReadU16BE();}
                else if(ct==2&&cLen>=6){tRns[0]=s.ReadU16BE();tRns[1]=s.ReadU16BE();tRns[2]=s.ReadU16BE();}
            }
            else if(cType==0x49444154u){ // IDAT
                if(idatLen+cLen<=size){s.ReadBytes(idat+idatLen,cLen);idatLen+=cLen;}
            }
            else if(cType==0x49454E44u){s.Seek(cData+cLen+4);break;}

            s.Seek(cData+usize(cLen)+4); // skip data + CRC
        }

        if(!hasIHDR||imgW<=0||imgH<=0||idatLen==0){pngFree(idat);return nullptr;}
        if(interlace!=0){pngFree(idat);return nullptr;} // Adam7 non supporté

        // Décompression zlib (NkDeflate avec le fix FDICT)
        const int32 spp=(ct==0)?1:(ct==2)?3:(ct==3)?1:(ct==4)?2:4;
        const int32 rowB=(imgW*spp*bd+7)/8;    // bytes par ligne (données filtrées)
        const int32 bppF=(spp*bd+7)/8;          // bytes per filter unit
        const usize rawSz=usize(imgH)*(1+rowB);

        uint8* raw=static_cast<uint8*>(pngMalloc(rawSz));
        if(!raw){pngFree(idat);return nullptr;}

        usize written=0;
        if(!NkDeflate::Decompress(idat,idatLen,raw,rawSz,written)){
            pngFree(idat);pngFree(raw);return nullptr;
        }
        pngFree(idat);

        // Canaux de sortie (stbi__parse_png_file logic)
        int32 outN;
        if(ct==0)      outN=hasTRNS?2:1;
        else if(ct==2) outN=hasTRNS?4:3;
        else if(ct==3){bool at=false;for(int32 i=0;i<palSz&&!at;++i)if(palA[i]<255)at=true;outN=at?4:3;}
        else if(ct==4) outN=2;
        else           outN=4;

        const NkImagePixelFormat fmt=
            outN==1?NkImagePixelFormat::NK_GRAY8:
            outN==2?NkImagePixelFormat::NK_GRAY_A16:
            outN==3?NkImagePixelFormat::NK_RGB24:NkImagePixelFormat::NK_RGBA32;

        NkImage* img=NkImage::Alloc(imgW,imgH,fmt);
        if(!img){pngFree(raw);return nullptr;}

        // Buffer ligne précédente (filtre Up/Avg/Paeth)
        uint8* prev=static_cast<uint8*>(pngCalloc(usize(rowB)+1,1));
        // Buffer temporaire pour expansion (pire cas: 16-bit RGB = w*3*2)
        uint8* tmp =static_cast<uint8*>(pngMalloc(usize(imgW)*spp*2+8));
        if(!prev||!tmp){pngFree(prev);pngFree(tmp);pngFree(raw);img->Free();return nullptr;}

        usize rp=0;
        for(int32 y=0;y<imgH;++y){
            uint8 ft=raw[rp++];
            uint8* frow=raw+rp; rp+=rowB;

            // Dé-filtrage in-place — identique stb
            unfilter(frow,prev,rowB,bppF<1?1:bppF,ft);

            // Destination dans NkImage : RowPtr(y) pointe sur stride bytes,
            // on écrit seulement imgW*outN octets utiles (le reste du stride est du padding)
            uint8* dst=img->RowPtr(y);

            if(ct==3){
                // Palette indexée
                expandIdx(frow,tmp,imgW,bd);
                for(int32 x=0;x<imgW;++x){
                    uint8 idx=tmp[x]; int32 pi=(idx<palSz)?idx:0;
                    if(outN==3){dst[x*3]=pal[pi*3];dst[x*3+1]=pal[pi*3+1];dst[x*3+2]=pal[pi*3+2];}
                    else{dst[x*4]=pal[pi*3];dst[x*4+1]=pal[pi*3+1];dst[x*4+2]=pal[pi*3+2];dst[x*4+3]=palA[idx];}
                }
            }
            else if(bd==16){
                // 16-bit → MSB (stbi__convert_16_to_8 style)
                for(int32 i=0;i<imgW*spp;++i) tmp[i]=frow[i*2];
                if(ct==0){
                    uint8 tv=uint8(tRns[0]>>8);
                    for(int32 x=0;x<imgW;++x){uint8 g=tmp[x];if(outN==1)dst[x]=g;else{dst[x*2]=g;dst[x*2+1]=(hasTRNS&&g==tv)?0:255;}}
                } else if(ct==2){
                    uint8 tr=uint8(tRns[0]>>8),tg=uint8(tRns[1]>>8),tb=uint8(tRns[2]>>8);
                    for(int32 x=0;x<imgW;++x){
                        uint8 r=tmp[x*3],g=tmp[x*3+1],b=tmp[x*3+2];
                        if(outN==3){dst[x*3]=r;dst[x*3+1]=g;dst[x*3+2]=b;}
                        else{dst[x*4]=r;dst[x*4+1]=g;dst[x*4+2]=b;dst[x*4+3]=(hasTRNS&&r==tr&&g==tg&&b==tb)?0:255;}
                    }
                } else pngCopy(dst,tmp,usize(imgW*outN));
            }
            else if(bd<8){
                expandSamp(frow,tmp,imgW*spp,bd);
                if(ct==0){
                    uint8 tv=0;
                    if(hasTRNS) tv=uint8((tRns[0]&((1<<bd)-1))*kDS[bd]);
                    for(int32 x=0;x<imgW;++x){uint8 g=tmp[x];if(outN==1)dst[x]=g;else{dst[x*2]=g;dst[x*2+1]=(hasTRNS&&g==tv)?0:255;}}
                }
            }
            else{ // 8-bit — cas le plus courant
                if(ct==0){
                    uint8 tv=uint8(tRns[0]);
                    for(int32 x=0;x<imgW;++x){uint8 g=frow[x];if(outN==1)dst[x]=g;else{dst[x*2]=g;dst[x*2+1]=(hasTRNS&&g==tv)?0:255;}}
                } else if(ct==2){
                    if(outN==3) pngCopy(dst,frow,usize(imgW*3));
                    else{
                        uint8 tr=uint8(tRns[0]),tg=uint8(tRns[1]),tb=uint8(tRns[2]);
                        for(int32 x=0;x<imgW;++x){
                            uint8 r=frow[x*3],g=frow[x*3+1],b=frow[x*3+2];
                            dst[x*4]=r;dst[x*4+1]=g;dst[x*4+2]=b;
                            dst[x*4+3]=(hasTRNS&&r==tr&&g==tg&&b==tb)?0:255;
                        }
                    }
                } else {
                    // ct==4 (Gray+A) ou ct==6 (RGBA) 8-bit : copie directe
                    pngCopy(dst,frow,usize(imgW*outN));
                }
            }

            pngCopy(prev,frow,usize(rowB));
        }

        pngFree(tmp); pngFree(prev); pngFree(raw);
        return img;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkPNGCodec::Encode
    // ─────────────────────────────────────────────────────────────────────────────

    static uint8 bestFilter(const uint8* row,const uint8* prev,int32 rb,int32 bpp) noexcept {
        int64 s[5]={};
        for(int32 i=0;i<rb;++i){
            int32 x=row[i],a=(i>=bpp)?row[i-bpp]:0,b=prev[i],c=(i>=bpp)?prev[i-bpp]:0;
            auto ab=[](int32 v)->int64{return v<0?-v:v;};
            s[0]+=ab(x);
            s[1]+=ab(int8(uint8(x-a)));
            s[2]+=ab(int8(uint8(x-b)));
            s[3]+=ab(int8(uint8(x-((a+b)>>1))));
            s[4]+=ab(int8(uint8(x-paeth(a,b,c))));
        }
        uint8 best=0;int64 bs=s[0];
        for(int32 i=1;i<5;++i)if(s[i]<bs){bs=s[i];best=uint8(i);}
        return best;
    }

    bool NkPNGCodec::Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept {
        if(!img.IsValid()) return false;
        crcInit();

        const int32 w=img.Width(),h=img.Height(),ch=img.Channels();
        const int32 rb=w*ch; // octets utiles par ligne (sans padding stride)
        const usize rawSz=usize(h)*(1+rb);

        uint8* raw =static_cast<uint8*>(pngMalloc(rawSz));
        uint8* prev=static_cast<uint8*>(pngCalloc(usize(rb)+1,1));
        if(!raw||!prev){pngFree(raw);pngFree(prev);return false;}

        usize rp=0;
        for(int32 y=0;y<h;++y){
            const uint8* row=img.RowPtr(y); // stride-aligned, lit rb octets utiles
            uint8 ft=bestFilter(row,prev,rb,ch);
            raw[rp++]=ft;
            switch(ft){
                case 0: pngCopy(raw+rp,row,rb); break;
                case 1: for(int32 i=0;i<rb;++i) raw[rp+i]=uint8(row[i]-(i>=ch?row[i-ch]:0)); break;
                case 2: for(int32 i=0;i<rb;++i) raw[rp+i]=uint8(row[i]-prev[i]); break;
                case 3: for(int32 i=0;i<rb;++i){int32 a=(i>=ch)?row[i-ch]:0;raw[rp+i]=uint8(row[i]-uint8((a+prev[i])>>1));} break;
                case 4: for(int32 i=0;i<rb;++i){int32 a=(i>=ch)?row[i-ch]:0,b=prev[i],c=(i>=ch)?prev[i-ch]:0;raw[rp+i]=uint8(row[i]-paeth(a,b,c));} break;
            }
            rp+=rb;
            pngCopy(prev,row,rb);
        }
        pngFree(prev);

        uint8* comp=nullptr;usize compSz=0;
        if(!NkDeflate::Compress(raw,rawSz,comp,compSz,6)){pngFree(raw);return false;}
        pngFree(raw);

        NkImageStream st;
        static const uint8 kSIG[8]={0x89,'P','N','G','\r','\n',0x1A,'\n'};
        st.WriteBytes(kSIG,8);

        auto wChunk=[&](uint32 type,const uint8* d,uint32 len){
            st.WriteU32BE(len);
            uint8 tb[4]={uint8(type>>24),uint8(type>>16),uint8(type>>8),uint8(type)};
            st.WriteBytes(tb,4);
            if(d&&len) st.WriteBytes(d,len);
            st.WriteU32BE(chunkCRC(tb,d,len));
        };

        uint8 ihdr[13]={};
        ihdr[0]=uint8(w>>24);ihdr[1]=uint8(w>>16);ihdr[2]=uint8(w>>8);ihdr[3]=uint8(w);
        ihdr[4]=uint8(h>>24);ihdr[5]=uint8(h>>16);ihdr[6]=uint8(h>>8);ihdr[7]=uint8(h);
        ihdr[8]=8; // bit depth 8
        ihdr[9]=(ch==1)?0:(ch==2)?4:(ch==3)?2:6; // color type
        // ihdr[10..12] = 0

        wChunk(0x49484452u,ihdr,13);
        wChunk(0x49444154u,comp,uint32(compSz));
        pngFree(comp);
        wChunk(0x49454E44u,nullptr,0);

        return st.TakeBuffer(out,outSize);
    }

} // namespace nkentseu