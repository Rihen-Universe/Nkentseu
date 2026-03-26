/**
 * @File    NkImage.cpp
 * @Brief   Implémentation NkImage, NkImageStream, NkDeflate.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 */
#include "NKImage/Core/NkImage.h"
#include "NKImage/Codecs/STB/NkSTBCodec.h"
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKImage/Codecs/JPEG/NkJPEGCodec.h"
#include "NKImage/Codecs/BMP/NkBMPCodec.h"
#include "NKImage/Codecs/TGA/NkTGACodec.h"
#include "NKImage/Codecs/HDR/NkHDRCodec.h"
#include "NKImage/Codecs/PPM/NkPPMCodec.h"
#include "NKImage/Codecs/QOI/NkQOICodec.h"
#include "NKImage/Codecs/GIF/NkGIFCodec.h"
#include "NKImage/Codecs/ICO/NkICOCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {

    using namespace nkentseu::memory;
    using namespace nkentseu::math;

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkImageStream
    // ─────────────────────────────────────────────────────────────────────────────

    uint8 NkImageStream::ReadU8() noexcept {
        if (NKIMG_UNLIKELY(mPos >= mSize)) { mError = true; return 0; }
        return mData[mPos++];
    }
    uint16 NkImageStream::ReadU16BE() noexcept {
        if (NKIMG_UNLIKELY(!HasBytes(2))) { mError=true; return 0; }
        uint16 v = (static_cast<uint16>(mData[mPos])<<8)|mData[mPos+1]; mPos+=2; return v;
    }
    uint16 NkImageStream::ReadU16LE() noexcept {
        if (NKIMG_UNLIKELY(!HasBytes(2))) { mError=true; return 0; }
        uint16 v = mData[mPos]|(static_cast<uint16>(mData[mPos+1])<<8); mPos+=2; return v;
    }
    uint32 NkImageStream::ReadU32BE() noexcept {
        if (NKIMG_UNLIKELY(!HasBytes(4))) { mError=true; return 0; }
        uint32 v = (static_cast<uint32>(mData[mPos])<<24)|(static_cast<uint32>(mData[mPos+1])<<16)
                |(static_cast<uint32>(mData[mPos+2])<<8)|mData[mPos+3]; mPos+=4; return v;
    }
    uint32 NkImageStream::ReadU32LE() noexcept {
        if (NKIMG_UNLIKELY(!HasBytes(4))) { mError=true; return 0; }
        uint32 v = mData[mPos]|(static_cast<uint32>(mData[mPos+1])<<8)
                |(static_cast<uint32>(mData[mPos+2])<<16)|(static_cast<uint32>(mData[mPos+3])<<24);
        mPos+=4; return v;
    }
    int16  NkImageStream::ReadI16BE() noexcept { return static_cast<int16>(ReadU16BE()); }
    int32  NkImageStream::ReadI32LE() noexcept { return static_cast<int32>(ReadU32LE()); }
    usize  NkImageStream::ReadBytes(uint8* dst, usize count) noexcept {
        if (mPos + count > mSize) { mError=true; count=mSize-mPos; }
        if (dst) NkCopy(dst, mData+mPos, count);
        mPos+=count; return count;
    }
    void NkImageStream::Skip(usize count) noexcept {
        if (mPos+count>mSize) { mError=true; mPos=mSize; } else mPos+=count;
    }
    void NkImageStream::Seek(usize pos) noexcept {
        if (pos>mSize) { mError=true; mPos=mSize; } else mPos=pos;
    }

    bool NkImageStream::Grow(usize needed) noexcept {
        if (mWriteSize + needed <= mWriteCapacity) return true;
        usize newCap = mWriteCapacity == 0 ? 4096 : mWriteCapacity * 2;
        while (newCap < mWriteSize + needed) newCap *= 2;
        uint8* nb = static_cast<uint8*>(NkRealloc(mWriteBuf, mWriteCapacity, newCap));
        if (!nb) return false;
        mWriteBuf = nb; mWriteCapacity = newCap;
        return true;
    }
    bool NkImageStream::WriteU8(uint8 v) noexcept {
        if (!Grow(1)) return false;
        mWriteBuf[mWriteSize++] = v;
        if (mFile) ::fwrite(&v, 1, 1, mFile);
        return true;
    }
    bool NkImageStream::WriteU16BE(uint16 v) noexcept {
        uint8 b[2] = {static_cast<uint8>(v>>8), static_cast<uint8>(v)};
        return WriteBytes(b, 2);
    }
    bool NkImageStream::WriteU16LE(uint16 v) noexcept {
        uint8 b[2] = {static_cast<uint8>(v), static_cast<uint8>(v>>8)};
        return WriteBytes(b, 2);
    }
    bool NkImageStream::WriteU32BE(uint32 v) noexcept {
        uint8 b[4] = {static_cast<uint8>(v>>24),static_cast<uint8>(v>>16),
                    static_cast<uint8>(v>>8), static_cast<uint8>(v)};
        return WriteBytes(b, 4);
    }
    bool NkImageStream::WriteU32LE(uint32 v) noexcept {
        uint8 b[4] = {static_cast<uint8>(v),static_cast<uint8>(v>>8),
                    static_cast<uint8>(v>>16),static_cast<uint8>(v>>24)};
        return WriteBytes(b, 4);
    }
    bool NkImageStream::WriteI32LE(int32 v) noexcept { return WriteU32LE(static_cast<uint32>(v)); }
    bool NkImageStream::WriteBytes(const uint8* src, usize count) noexcept {
        if (!Grow(count)) return false;
        NkCopy(mWriteBuf + mWriteSize, src, count);
        mWriteSize += count;
        if (mFile) ::fwrite(src, 1, count, mFile);
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkDeflate — tables
    // ─────────────────────────────────────────────────────────────────────────────

    const uint16 NkDeflate::kLenBase[29]  = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    const uint8  NkDeflate::kLenExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    const uint16 NkDeflate::kDistBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    const uint8  NkDeflate::kDistExtra[30]= {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    const uint8  NkDeflate::kCLOrder[19]  = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

    uint32 NkDeflate::ReadBits(Context& c, int32 n) noexcept {
        while (c.bitCount < n) {
            if (c.inPos >= c.inSize) { c.error=true; return 0; }
            c.bitBuf |= static_cast<uint32>(c.in[c.inPos++]) << c.bitCount;
            c.bitCount += 8;
        }
        uint32 v = c.bitBuf & ((1u<<n)-1); c.bitBuf>>=n; c.bitCount-=n; return v;
    }
    bool NkDeflate::BuildHuff(HuffTree& t, const uint8* lens, int32 n) noexcept {
        NkSet(t.counts,0,sizeof(t.counts)); t.numSymbols=n;
        for(int32 i=0;i<n;++i) if(lens[i]>0&&lens[i]<=HuffTree::MAX_BITS) ++t.counts[lens[i]];
        int16 nc[HuffTree::MAX_BITS+1]={}, code=0;
        for(int32 b=1;b<=HuffTree::MAX_BITS;++b){code=static_cast<int16>((code+t.counts[b-1])<<1);nc[b]=code;}
        NkSet(t.symbols,-1,sizeof(t.symbols));
        for(int32 i=0;i<n;++i){
            int32 l=lens[i]; if(!l) continue;
            int16 c2=nc[l]++; int16 rev=0;
            for(int32 b=0;b<l;++b) rev=static_cast<int16>((rev<<1)|((c2>>b)&1));
            if(rev<HuffTree::MAX_SYMS) t.symbols[rev]=static_cast<int16>(i);
        }
        return true;
    }
    int32 NkDeflate::Decode(Context& c, const HuffTree& t) noexcept {
        int32 code=0,first=0,index=0;
        for(int32 l=1;l<=HuffTree::MAX_BITS;++l){
            code=(code<<1)|static_cast<int32>(ReadBits(c,1));
            first=(first<<1)+t.counts[l]; index+=t.counts[l];
            if(code-t.counts[l]<first){int32 s=index+(code-first);return(s>=0&&s<t.numSymbols)?s:-1;}
        }
        return -1;
    }
    bool NkDeflate::Huffman(Context& c, const HuffTree& lt, const HuffTree& dt) noexcept {
        for(;;){
            if(c.error) return false;
            int32 sym=Decode(c,lt); if(sym<0) return false;
            if(sym<256){if(c.outPos>=c.outSize){c.error=true;return false;}c.out[c.outPos++]=static_cast<uint8>(sym);}
            else if(sym==256) break;
            else {
                int32 li=sym-257; if(li<0||li>=29) return false;
                uint32 len=kLenBase[li]+ReadBits(c,kLenExtra[li]);
                int32 ds=Decode(c,dt); if(ds<0||ds>=30) return false;
                uint32 dist=kDistBase[ds]+ReadBits(c,kDistExtra[ds]);
                if(c.outPos<dist) return false;
                usize base=c.outPos-dist;
                for(uint32 i=0;i<len;++i){if(c.outPos>=c.outSize){c.error=true;return false;}c.out[c.outPos++]=c.out[base+(i%dist)];}
            }
        }
        return !c.error;
    }
    bool NkDeflate::Stored(Context& c) noexcept {
        c.bitBuf=0;c.bitCount=0;
        if(c.inPos+4>c.inSize) return false;
        uint16 len =static_cast<uint16>(c.in[c.inPos])|(static_cast<uint16>(c.in[c.inPos+1])<<8);
        uint16 nlen=static_cast<uint16>(c.in[c.inPos+2])|(static_cast<uint16>(c.in[c.inPos+3])<<8);
        c.inPos+=4; if((len^nlen)!=0xFFFF) return false;
        if(c.inPos+len>c.inSize||c.outPos+len>c.outSize) return false;
        NkCopy(c.out+c.outPos,c.in+c.inPos,len); c.inPos+=len; c.outPos+=len; return true;
    }
    bool NkDeflate::Fixed(Context& c) noexcept {
        uint8 ll[288],dl[30];
        for(int i=0;i<=143;++i) ll[i]=8; for(int i=144;i<=255;++i) ll[i]=9;
        for(int i=256;i<=279;++i) ll[i]=7; for(int i=280;i<=287;++i) ll[i]=8;
        for(int i=0;i<30;++i) dl[i]=5;
        HuffTree lt,dt; BuildHuff(lt,ll,288); BuildHuff(dt,dl,30);
        return Huffman(c,lt,dt);
    }
    bool NkDeflate::Dynamic(Context& c) noexcept {
        int32 hlit=static_cast<int32>(ReadBits(c,5))+257;
        int32 hdist=static_cast<int32>(ReadBits(c,5))+1;
        int32 hclen=static_cast<int32>(ReadBits(c,4))+4;
        uint8 cl[19]={};
        for(int32 i=0;i<hclen;++i) cl[kCLOrder[i]]=static_cast<uint8>(ReadBits(c,3));
        HuffTree ct; BuildHuff(ct,cl,19);
        uint8 lens[320]={}; int32 total=hlit+hdist,i=0;
        while(i<total){
            int32 s=Decode(c,ct);
            if(s<16){lens[i++]=static_cast<uint8>(s);}
            else if(s==16){uint8 p=i>0?lens[i-1]:0;int32 n=static_cast<int32>(ReadBits(c,2))+3;while(n--&&i<total)lens[i++]=p;}
            else if(s==17){int32 n=static_cast<int32>(ReadBits(c,3))+3;while(n--&&i<total)lens[i++]=0;}
            else{int32 n=static_cast<int32>(ReadBits(c,7))+11;while(n--&&i<total)lens[i++]=0;}
        }
        HuffTree lt,dt; BuildHuff(lt,lens,hlit); BuildHuff(dt,lens+hlit,hdist);
        return Huffman(c,lt,dt);
    }
    bool NkDeflate::Block(Context& c, bool& last) noexcept {
        last=(ReadBits(c,1)!=0); uint32 bt=ReadBits(c,2);
        if(c.error) return false;
        switch(bt){case 0:return Stored(c);case 1:return Fixed(c);case 2:return Dynamic(c);}
        return false;
    }
    bool NkDeflate::DecompressRaw(const uint8* in,usize inSz,uint8* out,usize outSz,usize& w) noexcept {
        w=0; Context c{}; c.in=in;c.inSize=inSz;c.out=out;c.outSize=outSz;
        bool last=false;
        while(!last&&!c.error) if(!Block(c,last)) return false;
        w=c.outPos; return !c.error;
    }
    bool NkDeflate::Decompress(const uint8* in,usize inSz,uint8* out,usize outSz,usize& w) noexcept {
        w=0; if(inSz<2) return false;
        if(((static_cast<uint16>(in[0])<<8)|in[1])%31!=0) return false;
        if((in[0]&0x0F)!=8) return false;
        if(in[0]&0x20) return false; // preset dict
        return DecompressRaw(in+2,inSz-2,out,outSz,w);
    }

    uint32 NkDeflate::Adler32(const uint8* data,usize size,uint32 prev) noexcept {
        uint32 s1=prev&0xFFFF, s2=(prev>>16)&0xFFFF;
        for(usize i=0;i<size;++i){s1=(s1+data[i])%65521;s2=(s2+s1)%65521;}
        return (s2<<16)|s1;
    }

    bool NkDeflate::CompressLevel0(DeflateCtx& ctx) noexcept {
        // Stored blocks (no compression) — level 0
        const uint8* src=ctx.in; usize remain=ctx.inSize;
        while(remain>0) {
            const uint16 blen=static_cast<uint16>(remain>65535?65535:remain);
            const uint16 nlen=static_cast<uint16>(~blen);
            const bool last=(blen==remain);
            ctx.out->WriteU8(last?0x01:0x00); // BFINAL|BTYPE=00
            ctx.out->WriteU16LE(blen);
            ctx.out->WriteU16LE(nlen);
            ctx.out->WriteBytes(src,blen);
            src+=blen; remain-=blen;
        }
        ctx.adler=Adler32(ctx.in,ctx.inSize,1);
        return true;
    }

    bool NkDeflate::Compress(const uint8* in,usize inSz,uint8*& outData,usize& outSz,int32 level) noexcept {
        NkImageStream s;
        // zlib header : CMF=0x78 (deflate,window=32k), FLG
        uint8 cmf=0x78, flg=static_cast<uint8>((31-((static_cast<uint16>(cmf)<<8))%31)%31);
        s.WriteU8(cmf); s.WriteU8(flg);
        DeflateCtx ctx{&s,in,inSz,1};
        bool ok=(level==0)?CompressLevel0(ctx):CompressLevel0(ctx); // level>0 → still level0 for now
        if(!ok) return false;
        // Adler32 checksum (big-endian)
        uint32 a=Adler32(in,inSz,1);
        s.WriteU32BE(a);
        return s.TakeBuffer(outData,outSz);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkImage — fabrique
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkImage::Alloc(int32 w,int32 h,NkImagePixelFormat fmt,bool owning) noexcept {
        NkImage* img=static_cast<NkImage*>(NkAlloc(sizeof(NkImage)));
        if(!img) return nullptr;
        new(img) NkImage();
        const int32 bpp=BytesPerPixel(fmt);
        img->mStride=(w*bpp+3)&~3;
        const usize bytes=static_cast<usize>(img->mStride)*h;
        img->mPixels=static_cast<uint8*>(NkAllocZero(bytes,1));
        if(!img->mPixels){NkFree(img);return nullptr;}
        img->mWidth=w; img->mHeight=h; img->mFormat=fmt; img->mOwning=owning;
        return img;
    }

    NkImage* NkImage::Wrap(uint8* pixels,int32 w,int32 h,NkImagePixelFormat fmt,int32 stride) noexcept {
        NkImage* img=static_cast<NkImage*>(NkAlloc(sizeof(NkImage)));
        if(!img) return nullptr;
        new(img) NkImage();
        img->mPixels=pixels; img->mWidth=w; img->mHeight=h; img->mFormat=fmt;
        img->mStride=stride>0?stride:w*BytesPerPixel(fmt);
        img->mOwning=false;
        return img;
    }

    NkImage::~NkImage() noexcept {
        if(mOwning&&mPixels) NkFree(mPixels);
    }

    void NkImage::Free() noexcept {
        if(mOwning&&mPixels) NkFree(mPixels);
        mPixels=nullptr;
        NkFree(this);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Détection de format
    // ─────────────────────────────────────────────────────────────────────────────

    NkImageFormat NkImage::DetectFormat(const uint8* d,usize sz) noexcept {
        if(sz<4) return NkImageFormat::NK_UNKNOW;
        if(sz>=8&&d[0]==0x89&&d[1]=='P'&&d[2]=='N'&&d[3]=='G') return NkImageFormat::NK_PNG;
        if(sz>=3&&d[0]==0xFF&&d[1]==0xD8&&d[2]==0xFF)           return NkImageFormat::NK_JPEG;
        if(sz>=2&&d[0]=='B'&&d[1]=='M')                         return NkImageFormat::NK_BMP;
        if(sz>=4&&d[0]=='q'&&d[1]=='o'&&d[2]=='i'&&d[3]=='f')  return NkImageFormat::NK_QOI;
        if(sz>=4&&d[0]=='G'&&d[1]=='I'&&d[2]=='F'&&d[3]=='8')  return NkImageFormat::NK_GIF;
        if(sz>=4&&d[0]==0x00&&d[1]==0x00&&(d[2]==0x01||d[2]==0x02)&&d[3]==0x00) return NkImageFormat::NK_ICO;
        if(sz>=10&&d[0]=='#'&&d[1]=='?'&&d[2]=='R')             return NkImageFormat::NK_HDR;
        if(sz>=2&&(d[0]=='P')&&d[1]>='1'&&d[1]<='6')           {
            if(d[1]=='1'||d[1]=='4') return NkImageFormat::NK_PBM;
            if(d[1]=='2'||d[1]=='5') return NkImageFormat::NK_PGM;
            return NkImageFormat::NK_PPM;
        }
        // TGA : pas de magic byte fiable — détection heuristique
        if(sz>=18) {
            const uint8 imgType=d[2];
            if(imgType==0||imgType==1||imgType==2||imgType==3||
            imgType==9||imgType==10||imgType==11)
                return NkImageFormat::NK_TGA;
        }
        return NkImageFormat::NK_UNKNOW;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Dispatch vers les codecs
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkImage::Dispatch(const uint8* d,usize sz,int32 ch,NkImageFormat fmt) noexcept {
        NkImage* img=nullptr;
        switch(fmt) {
            case NkImageFormat::NK_PNG:  img=NkPNGCodec ::Decode(d,sz); break;
            case NkImageFormat::NK_JPEG: img=NkJPEGCodec::Decode(d,sz); break;
            case NkImageFormat::NK_BMP:  img=NkBMPCodec ::Decode(d,sz); break;
            case NkImageFormat::NK_TGA:  img=NkTGACodec ::Decode(d,sz); break;
            case NkImageFormat::NK_HDR:  img=NkHDRCodec ::Decode(d,sz); break;
            case NkImageFormat::NK_PPM:
            case NkImageFormat::NK_PGM:
            case NkImageFormat::NK_PBM:  img=NkPPMCodec ::Decode(d,sz); break;
            case NkImageFormat::NK_QOI:  img=NkQOICodec ::Decode(d,sz); break;
            case NkImageFormat::NK_GIF:  img=NkGIFCodec ::Decode(d,sz); break;
            case NkImageFormat::NK_ICO:  img=NkICOCodec ::Decode(d,sz); break;
            default: return nullptr;
        }
        if(!img) return nullptr;
        // Conversion de canaux si demandée
        if(ch>0 && ch!=img->Channels()) {
            const NkImagePixelFormat tgt=(ch==1)?NkImagePixelFormat::NK_GRAY8:
                                    (ch==2)?NkImagePixelFormat::NK_GRAY_A16:
                                    (ch==3)?NkImagePixelFormat::NK_RGB24:NkImagePixelFormat::NK_RGBA32;
            NkImage* conv=img->Convert(tgt);
            img->Free();
            return conv;
        }
        return img;
    }

    NkImage* NkImage::LoadFromMemory(const uint8* data,usize size,int32 ch) noexcept {
        if(!data||size<4) return nullptr;
        const NkImageFormat fmt=DetectFormat(data,size);
        if(fmt==NkImageFormat::NK_UNKNOW) return nullptr;
        NkImage* img=Dispatch(data,size,ch,fmt);
        if(img) img->mSourceFormat=fmt;
        return img;
    }

    NkImage* NkImage::Load(const char* path,int32 ch) noexcept {
        if(!path) return nullptr;
        FILE* f=::fopen(path,"rb");
        if(!f) return nullptr;
        ::fseek(f,0,SEEK_END);
        const long sz=::ftell(f);
        ::fseek(f,0,SEEK_SET);
        if(sz<=0){::fclose(f);return nullptr;}
        uint8* buf=static_cast<uint8*>(NkAlloc(static_cast<usize>(sz)));
        if(!buf){::fclose(f);return nullptr;}
        const usize read=::fread(buf,1,static_cast<usize>(sz),f);
        ::fclose(f);
        NkImage* img=nullptr;
        if(read==static_cast<usize>(sz))
            img=LoadFromMemory(buf,read,ch);
        NkFree(buf);
        return img;
    }

    bool NkImage::QueryInfo(const char* path,int32& ow,int32& oh,int32& oc,NkImageFormat& of_) noexcept {
        NkImage* img=Load(path,0);
        if(!img) return false;
        ow=img->mWidth; oh=img->mHeight; oc=img->Channels(); of_=img->mSourceFormat;
        img->Free(); return true;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Sauvegarde
    // ─────────────────────────────────────────────────────────────────────────────

    static const char* FileExtension(const char* path) noexcept {
        const char* dot=nullptr;
        for(const char* p=path;*p;++p) if(*p=='.') dot=p;
        return dot?dot+1:"";
    }

    bool NkImage::Save(const char* path,int32 quality) const noexcept {
        const char* ext=FileExtension(path);
        auto eq=[](const char* a,const char* b){
            while(*a&&*b){if((*a|32)!=(*b|32))return false;++a;++b;}return *a==*b;};
        if(eq(ext,"png"))  return SavePNG(path);
        if(eq(ext,"jpg")||eq(ext,"jpeg")) return SaveJPEG(path,quality);
        if(eq(ext,"bmp"))  return SaveBMP(path);
        if(eq(ext,"tga"))  return SaveTGA(path);
        if(eq(ext,"hdr"))  return SaveHDR(path);
        if(eq(ext,"ppm")||eq(ext,"pgm")) return SavePPM(path);
        if(eq(ext,"qoi"))  return SaveQOI(path);
        return false;
    }

    static bool WriteFile(const char* path,const uint8* data,usize size) noexcept {
        FILE* f=::fopen(path,"wb");
        if(!f) return false;
        const bool ok=(::fwrite(data,1,size,f)==size);
        ::fclose(f);
        return ok;
    }

    bool NkImage::SavePNG(const char* path)  const noexcept {
        uint8* d=nullptr; usize sz=0;
        if(!EncodePNG(d,sz)) return false;
        bool ok=WriteFile(path,d,sz); NkFree(d); return ok;
    }
    bool NkImage::SaveJPEG(const char* path,int32 q) const noexcept {
        uint8* d=nullptr; usize sz=0;
        if(!EncodeJPEG(d,sz,q)) return false;
        bool ok=WriteFile(path,d,sz); NkFree(d); return ok;
    }
    bool NkImage::SaveBMP(const char* path)  const noexcept {
        uint8* d=nullptr; usize sz=0;
        if(!EncodeBMP(d,sz)) return false;
        bool ok=WriteFile(path,d,sz); NkFree(d); return ok;
    }
    bool NkImage::SaveTGA(const char* path)  const noexcept {
        uint8* d=nullptr; usize sz=0;
        if(!EncodeTGA(d,sz)) return false;
        bool ok=WriteFile(path,d,sz); NkFree(d); return ok;
    }
    bool NkImage::SaveHDR(const char* path)  const noexcept { return NkHDRCodec::Encode(*this,path); }
    bool NkImage::SavePPM(const char* path)  const noexcept { return NkPPMCodec::Encode(*this,path); }
    bool NkImage::SaveQOI(const char* path)  const noexcept {
        uint8* d=nullptr; usize sz=0;
        if(!EncodeQOI(d,sz)) return false;
        bool ok=WriteFile(path,d,sz); NkFree(d); return ok;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Backend STB
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkImage::LoadSTB(const char* path, int32 ch) noexcept {
        return NkSTBCodec::Load(path, ch);
    }

    NkImage* NkImage::LoadFromMemorySTB(const uint8* data, usize size, int32 ch) noexcept {
        return NkSTBCodec::LoadFromMemory(data, size, ch);
    }

    bool NkImage::SaveSTB(const char* path, int32 quality) const noexcept {
        return NkSTBCodec::Save(*this, path, quality);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Encode
    // ─────────────────────────────────────────────────────────────────────────────

    bool NkImage::EncodePNG (uint8*& d,usize& s)             const noexcept { return NkPNGCodec ::Encode(*this,d,s); }
    bool NkImage::EncodeJPEG(uint8*& d,usize& s,int32 q)     const noexcept { return NkJPEGCodec::Encode(*this,d,s,q); }
    bool NkImage::EncodeBMP (uint8*& d,usize& s)             const noexcept { return NkBMPCodec ::Encode(*this,d,s); }
    bool NkImage::EncodeTGA (uint8*& d,usize& s)             const noexcept { return NkTGACodec ::Encode(*this,d,s); }
    bool NkImage::EncodeQOI (uint8*& d,usize& s)             const noexcept { return NkQOICodec ::Encode(*this,d,s); }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Manipulation
    // ─────────────────────────────────────────────────────────────────────────────

    void NkImage::FlipVertical() noexcept {
        if(!IsValid()) return;
        uint8* tmp=static_cast<uint8*>(NkAlloc(mStride));
        if(!tmp) return;
        for(int32 y=0;y<mHeight/2;++y){
            NkCopy(tmp, RowPtr(y), mStride);
            NkCopy(RowPtr(y), RowPtr(mHeight-1-y), mStride);
            NkCopy(RowPtr(mHeight-1-y), tmp, mStride);
        }
        NkFree(tmp);
    }

    void NkImage::FlipHorizontal() noexcept {
        if(!IsValid()) return;
        const int32 bpp=BytesPP();
        for(int32 y=0;y<mHeight;++y){
            uint8* row=RowPtr(y);
            for(int32 x=0;x<mWidth/2;++x){
                uint8* a=row+x*bpp, *b=row+(mWidth-1-x)*bpp;
                for(int32 i=0;i<bpp;++i){ uint8 t=a[i];a[i]=b[i];b[i]=t; }
            }
        }
    }

    void NkImage::PremultiplyAlpha() noexcept {
        if(!IsValid()||mFormat!=NkImagePixelFormat::NK_RGBA32) return;
        for(int32 y=0;y<mHeight;++y){
            uint8* p=RowPtr(y);
            for(int32 x=0;x<mWidth;++x,p+=4){
                const uint32 a=p[3];
                p[0]=static_cast<uint8>((p[0]*a+127)/255);
                p[1]=static_cast<uint8>((p[1]*a+127)/255);
                p[2]=static_cast<uint8>((p[2]*a+127)/255);
            }
        }
    }

    void NkImage::Blit(const NkImage& src,int32 dstX,int32 dstY) noexcept {
        if(!IsValid()||!src.IsValid()||mFormat!=src.mFormat) return;
        int32 sx=0,sy=0,dx=dstX,dy=dstY,w=src.mWidth,h=src.mHeight;
        if(dx<0){sx-=dx;w+=dx;dx=0;} if(dy<0){sy-=dy;h+=dy;dy=0;}
        if(dx+w>mWidth)  w=mWidth-dx;
        if(dy+h>mHeight) h=mHeight-dy;
        if(w<=0||h<=0) return;
        const int32 bpp=BytesPP(), rowBytes=w*bpp;
        for(int32 row=0;row<h;++row)
            NkCopy(RowPtr(dy+row)+dx*bpp, src.RowPtr(sy+row)+sx*bpp, rowBytes);
    }

    NkImage* NkImage::Crop(int32 x,int32 y,int32 w,int32 h) const noexcept {
        if(!IsValid()||x<0||y<0||x+w>mWidth||y+h>mHeight||w<=0||h<=0) return nullptr;
        NkImage* dst=Alloc(w,h,mFormat);
        if(!dst) return nullptr;
        for(int32 row=0;row<h;++row)
            NkCopy(dst->RowPtr(row), RowPtr(y+row)+x*BytesPP(), w*BytesPP());
        return dst;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Conversion de canaux
    // ─────────────────────────────────────────────────────────────────────────────

    uint8* NkImage::ConvertChannels(const uint8* src,int32 w,int32 h,int32 sc,int32 dc) noexcept {
        const usize dstStride=static_cast<usize>((w*dc+3)&~3);
        uint8* dst=static_cast<uint8*>(NkAllocZero(dstStride*h,1));
        if(!dst) return nullptr;
        for(int32 y=0;y<h;++y){
            const uint8* sr=src+static_cast<usize>(y)*((w*sc+3)&~3);
            uint8*       dr=dst+dstStride*y;
            for(int32 x=0;x<w;++x){
                const uint8* sp=sr+x*sc; uint8* dp=dr+x*dc;
                uint8 r=sp[0],g=(sc>1)?sp[1]:r,b=(sc>2)?sp[2]:r,a=(sc>3)?sp[3]:255;
                switch(dc){
                    case 1: dp[0]=static_cast<uint8>((static_cast<uint32>(r)*77+g*150+b*29)>>8); break;
                    case 2: dp[0]=static_cast<uint8>((static_cast<uint32>(r)*77+g*150+b*29)>>8);dp[1]=a; break;
                    case 3: dp[0]=r;dp[1]=g;dp[2]=b; break;
                    case 4: dp[0]=r;dp[1]=g;dp[2]=b;dp[3]=a; break;
                }
            }
        }
        return dst;
    }

    NkImage* NkImage::Convert(NkImagePixelFormat newFmt) const noexcept {
        if(!IsValid()) return nullptr;
        if(newFmt==mFormat){
            NkImage* c=Alloc(mWidth,mHeight,mFormat);
            if(c) NkCopy(c->mPixels,mPixels,TotalBytes());
            return c;
        }
        const int32 dc=ChannelsOf(newFmt);
        if(dc==0) return nullptr;
        uint8* pix=ConvertChannels(mPixels,mWidth,mHeight,Channels(),dc);
        if(!pix) return nullptr;
        NkImage* img=static_cast<NkImage*>(NkAlloc(sizeof(NkImage)));
        if(!img){NkFree(pix);return nullptr;}
        new(img) NkImage();
        img->mPixels=pix; img->mWidth=mWidth; img->mHeight=mHeight;
        img->mFormat=newFmt; img->mStride=(mWidth*BytesPerPixel(newFmt)+3)&~3;
        img->mOwning=true;
        return img;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Resize
    // ─────────────────────────────────────────────────────────────────────────────

    NkImage* NkImage::ResizeNearest(const NkImage& src,int32 nw,int32 nh) noexcept {
        NkImage* dst=Alloc(nw,nh,src.mFormat);
        if(!dst) return nullptr;
        const int32 bpp=src.BytesPP();
        for(int32 y=0;y<nh;++y){
            const int32 sy=(y*src.mHeight)/nh;
            for(int32 x=0;x<nw;++x){
                const int32 sx=(x*src.mWidth)/nw;
                NkCopy(dst->RowPtr(y)+x*bpp, src.RowPtr(sy)+sx*bpp, bpp);
            }
        }
        return dst;
    }

    NkImage* NkImage::ResizeBilinear(const NkImage& src,int32 nw,int32 nh) noexcept {
        NkImage* dst=Alloc(nw,nh,src.mFormat);
        if(!dst) return nullptr;
        const int32 bpp=src.BytesPP();
        const float32 xScale=static_cast<float32>(src.mWidth)/nw;
        const float32 yScale=static_cast<float32>(src.mHeight)/nh;
        for(int32 y=0;y<nh;++y){
            const float32 fy=(y+0.5f)*yScale-0.5f;
            const int32 y0=static_cast<int32>(fy); const float32 yt=fy-y0;
            const int32 y1=y0+1<src.mHeight?y0+1:y0;
            for(int32 x=0;x<nw;++x){
                const float32 fx=(x+0.5f)*xScale-0.5f;
                const int32 x0=static_cast<int32>(fx); const float32 xt=fx-x0;
                const int32 x1=x0+1<src.mWidth?x0+1:x0;
                uint8* dp=dst->RowPtr(y)+x*bpp;
                for(int32 c=0;c<bpp;++c){
                    const float32 p00=src.RowPtr(y0)[x0*bpp+c];
                    const float32 p10=src.RowPtr(y0)[x1*bpp+c];
                    const float32 p01=src.RowPtr(y1)[x0*bpp+c];
                    const float32 p11=src.RowPtr(y1)[x1*bpp+c];
                    dp[c]=static_cast<uint8>(p00*(1-xt)*(1-yt)+p10*xt*(1-yt)+p01*(1-xt)*yt+p11*xt*yt+0.5f);
                }
            }
        }
        return dst;
    }

    NkImage* NkImage::ResizeBicubic(const NkImage& src,int32 nw,int32 nh) noexcept {
        // Bicubic Catmull-Rom
        NkImage* dst=Alloc(nw,nh,src.mFormat);
        if(!dst) return nullptr;
        const int32 bpp=src.BytesPP();
        auto cubic=[](float32 t)->float32 {
            const float32 a=t<0?-t:t;
            if(a<1) return 1.5f*a*a*a-2.5f*a*a+1;
            if(a<2) return -0.5f*a*a*a+2.5f*a*a-4*a+2;
            return 0;
        };
        const float32 xs=static_cast<float32>(src.mWidth)/nw;
        const float32 ys=static_cast<float32>(src.mHeight)/nh;
        for(int32 y=0;y<nh;++y){
            const float32 fy=(y+0.5f)*ys-0.5f;
            const int32 yf=static_cast<int32>(fy);
            for(int32 x=0;x<nw;++x){
                const float32 fx=(x+0.5f)*xs-0.5f;
                const int32 xf=static_cast<int32>(fx);
                uint8* dp=dst->RowPtr(y)+x*bpp;
                for(int32 c=0;c<bpp;++c){
                    float32 sum=0,wsum=0;
                    for(int32 m=-1;m<=2;++m){
                        const int32 sy2=yf+m<0?0:yf+m>=src.mHeight?src.mHeight-1:yf+m;
                        const float32 wy=cubic(fy-static_cast<float32>(yf+m));
                        for(int32 n=-1;n<=2;++n){
                            const int32 sx2=xf+n<0?0:xf+n>=src.mWidth?src.mWidth-1:xf+n;
                            const float32 wx=cubic(fx-static_cast<float32>(xf+n));
                            sum+=src.RowPtr(sy2)[sx2*bpp+c]*wx*wy;
                            wsum+=wx*wy;
                        }
                    }
                    float32 v=wsum!=0?sum/wsum:0;
                    dp[c]=static_cast<uint8>(v<0?0:v>255?255:v+0.5f);
                }
            }
        }
        return dst;
    }

    NkImage* NkImage::ResizeLanczos3(const NkImage& src,int32 nw,int32 nh) noexcept {
        NkImage* dst=Alloc(nw,nh,src.mFormat);
        if(!dst) return nullptr;
        const int32 bpp=src.BytesPP();
        auto sinc=[](float32 x)->float32 {
            if(x==0) return 1;
            x*=math::constants::kPiF;
            return NkSin(x)/x;
        };
        auto lanczos=[&](float32 x)->float32 {
            if(x<0)x=-x;
            if(x<3) return sinc(x)*sinc(x/3);
            return 0;
        };
        const float32 xs=static_cast<float32>(src.mWidth)/nw;
        const float32 ys=static_cast<float32>(src.mHeight)/nh;
        for(int32 y=0;y<nh;++y){
            const float32 fy=(y+0.5f)*ys-0.5f;
            const int32 yc=static_cast<int32>(fy);
            for(int32 x=0;x<nw;++x){
                const float32 fx=(x+0.5f)*xs-0.5f;
                const int32 xc=static_cast<int32>(fx);
                uint8* dp=dst->RowPtr(y)+x*bpp;
                for(int32 c=0;c<bpp;++c){
                    float32 sum=0,wsum=0;
                    for(int32 m=-2;m<=3;++m){
                        const int32 sy2=yc+m<0?0:yc+m>=src.mHeight?src.mHeight-1:yc+m;
                        const float32 wy=lanczos(fy-static_cast<float32>(yc+m));
                        for(int32 n=-2;n<=3;++n){
                            const int32 sx2=xc+n<0?0:xc+n>=src.mWidth?src.mWidth-1:xc+n;
                            const float32 wx=lanczos(fx-static_cast<float32>(xc+n));
                            sum+=src.RowPtr(sy2)[sx2*bpp+c]*wx*wy;
                            wsum+=wx*wy;
                        }
                    }
                    float32 v=wsum!=0?sum/wsum:0;
                    dp[c]=static_cast<uint8>(v<0?0:v>255?255:v+0.5f);
                }
            }
        }
        return dst;
    }

    NkImage* NkImage::Resize(int32 nw,int32 nh,NkResizeFilter f) const noexcept {
        if(!IsValid()||nw<=0||nh<=0) return nullptr;
        switch(f){
            case NkResizeFilter::NK_NEAREST:  return ResizeNearest (*this,nw,nh);
            case NkResizeFilter::NK_BILINEAR: return ResizeBilinear(*this,nw,nh);
            case NkResizeFilter::NK_BICUBIC:  return ResizeBicubic (*this,nw,nh);
            case NkResizeFilter::NK_LANCZOS3: return ResizeLanczos3(*this,nw,nh);
        }
        return ResizeBilinear(*this,nw,nh);
    }

    } // namespace nkentseu

//     // ─────────────────────────────────────────────────────────────────────────────
//     //  Sauvegarde WebP, SVG, GIF (ajouts)
//     // ─────────────────────────────────────────────────────────────────────────────
//     #include "NKImage/Codecs/WEBP/NkWebPCodec.h"
//     #include "NKImage/Codecs/SVG/NkSVGCodec.h"

//     namespace nkentseu {

//     using namespace nkentseu::memory;

//     bool NkImage::SaveGIF (const char* path) const noexcept {
//         return NkGIFCodec::Save(*this, path);
//     }

//     bool NkImage::SaveWebP(const char* path, bool lossless, int32 quality) const noexcept {
//         uint8* d=nullptr; usize sz=0;
//         if(!NkWebPCodec::Encode(*this,d,sz,lossless,quality)) return false;
//         FILE* f=::fopen(path,"wb");
//         if(!f){NkFree(d);return false;}
//         const bool ok=(::fwrite(d,1,sz,f)==sz);
//         ::fclose(f); NkFree(d); return ok;
//     }

//     bool NkImage::SaveSVG(const char* path) const noexcept {
//         return NkSVGCodec::EncodeToFile(*this, path);
//     }

//     // static int32 StbPixelLayoutToChannels(int32 layout) noexcept {
//     //     // Conversion simplifiée du layout stb_image_resize2
//     //     // STBIR_RGBA = 4, STBIR_RGB = 3, etc.
//     //     return layout;
//     // }

//     // NkImage* NkImage::ResizeNearest(const NkImage& src, int32 nw, int32 nh) noexcept {
//     //     if(!src.IsValid() || nw <= 0 || nh <= 0) return nullptr;
        
//     //     NkImage* dst = Alloc(nw, nh, src.mFormat);
//     //     if(!dst) return nullptr;
        
//     //     const int32 bpp = src.BytesPP();
//     //     const int32 sw = src.mWidth;
//     //     const int32 sh = src.mHeight;
        
//     //     for(int32 y = 0; y < nh; ++y) {
//     //         const int32 sy = (y * sh) / nh;
//     //         for(int32 x = 0; x < nw; ++x) {
//     //             const int32 sx = (x * sw) / nw;
//     //             NkCopy(dst->RowPtr(y) + x * bpp, src.RowPtr(sy) + sx * bpp, bpp);
//     //         }
//     //     }
        
//     //     return dst;
//     // }

//     // NkImage* NkImage::ResizeBilinear(const NkImage& src, int32 nw, int32 nh) noexcept {
//     //     if(!src.IsValid() || nw <= 0 || nh <= 0) return nullptr;
        
//     //     NkImage* dst = Alloc(nw, nh, src.mFormat);
//     //     if(!dst) return nullptr;
        
//     //     const int32 bpp = src.BytesPP();
//     //     const int32 sw = src.mWidth;
//     //     const int32 sh = src.mHeight;
//     //     const float32 xScale = static_cast<float32>(sw) / nw;
//     //     const float32 yScale = static_cast<float32>(sh) / nh;
        
//     //     for(int32 y = 0; y < nh; ++y) {
//     //         const float32 fy = (y + 0.5f) * yScale - 0.5f;
//     //         const int32 y0 = static_cast<int32>(fy);
//     //         const float32 yt = fy - y0;
//     //         const int32 y1 = (y0 + 1 < sh) ? y0 + 1 : y0;
            
//     //         for(int32 x = 0; x < nw; ++x) {
//     //             const float32 fx = (x + 0.5f) * xScale - 0.5f;
//     //             const int32 x0 = static_cast<int32>(fx);
//     //             const float32 xt = fx - x0;
//     //             const int32 x1 = (x0 + 1 < sw) ? x0 + 1 : x0;
                
//     //             uint8* dstPixel = dst->RowPtr(y) + x * bpp;
                
//     //             for(int32 c = 0; c < bpp; ++c) {
//     //                 const float32 p00 = src.RowPtr(y0)[x0 * bpp + c];
//     //                 const float32 p10 = src.RowPtr(y0)[x1 * bpp + c];
//     //                 const float32 p01 = src.RowPtr(y1)[x0 * bpp + c];
//     //                 const float32 p11 = src.RowPtr(y1)[x1 * bpp + c];
                    
//     //                 const float32 val = p00 * (1 - xt) * (1 - yt) +
//     //                                     p10 * xt * (1 - yt) +
//     //                                     p01 * (1 - xt) * yt +
//     //                                     p11 * xt * yt;
                    
//     //                 dstPixel[c] = static_cast<uint8>(val < 0 ? 0 : val > 255 ? 255 : val + 0.5f);
//     //             }
//     //         }
//     //     }
        
//     //     return dst;
//     // }

//     // NkImage* NkImage::ResizeBicubic(const NkImage& src, int32 nw, int32 nh) noexcept {
//     //     if(!src.IsValid() || nw <= 0 || nh <= 0) return nullptr;
        
//     //     NkImage* dst = Alloc(nw, nh, src.mFormat);
//     //     if(!dst) return nullptr;
        
//     //     const int32 bpp = src.BytesPP();
//     //     const int32 sw = src.mWidth;
//     //     const int32 sh = src.mHeight;
//     //     const float32 xScale = static_cast<float32>(sw) / nw;
//     //     const float32 yScale = static_cast<float32>(sh) / nh;
        
//     //     auto cubic = [](float32 t) -> float32 {
//     //         const float32 a = (t < 0) ? -t : t;
//     //         if(a < 1) return 1.5f * a * a * a - 2.5f * a * a + 1;
//     //         if(a < 2) return -0.5f * a * a * a + 2.5f * a * a - 4 * a + 2;
//     //         return 0;
//     //     };
        
//     //     for(int32 y = 0; y < nh; ++y) {
//     //         const float32 fy = (y + 0.5f) * yScale - 0.5f;
//     //         const int32 yf = static_cast<int32>(fy);
            
//     //         for(int32 x = 0; x < nw; ++x) {
//     //             const float32 fx = (x + 0.5f) * xScale - 0.5f;
//     //             const int32 xf = static_cast<int32>(fx);
                
//     //             uint8* dstPixel = dst->RowPtr(y) + x * bpp;
                
//     //             for(int32 c = 0; c < bpp; ++c) {
//     //                 float32 sum = 0, wsum = 0;
                    
//     //                 for(int32 m = -1; m <= 2; ++m) {
//     //                     const int32 sy = (yf + m < 0) ? 0 : (yf + m >= sh) ? sh - 1 : yf + m;
//     //                     const float32 wy = cubic(fy - static_cast<float32>(yf + m));
                        
//     //                     for(int32 n = -1; n <= 2; ++n) {
//     //                         const int32 sx = (xf + n < 0) ? 0 : (xf + n >= sw) ? sw - 1 : xf + n;
//     //                         const float32 wx = cubic(fx - static_cast<float32>(xf + n));
                            
//     //                         sum += src.RowPtr(sy)[sx * bpp + c] * wx * wy;
//     //                         wsum += wx * wy;
//     //                     }
//     //                 }
                    
//     //                 float32 val = (wsum != 0) ? sum / wsum : 0;
//     //                 dstPixel[c] = static_cast<uint8>(val < 0 ? 0 : val > 255 ? 255 : val + 0.5f);
//     //             }
//     //         }
//     //     }
        
//     //     return dst;
//     // }

//     // NkImage* NkImage::ResizeLanczos3(const NkImage& src, int32 nw, int32 nh) noexcept {
//     //     if(!src.IsValid() || nw <= 0 || nh <= 0) return nullptr;
        
//     //     NkImage* dst = Alloc(nw, nh, src.mFormat);
//     //     if(!dst) return nullptr;
        
//     //     const int32 bpp = src.BytesPP();
//     //     const int32 sw = src.mWidth;
//     //     const int32 sh = src.mHeight;
//     //     const float32 xScale = static_cast<float32>(sw) / nw;
//     //     const float32 yScale = static_cast<float32>(sh) / nh;
        
//     //     auto sinc = [](float32 x) -> float32 {
//     //         if(x == 0) return 1;
//     //         x *= 3.14159265358979323846f;
//     //         return math::NkSin(x) / x;
//     //     };
        
//     //     auto lanczos = [&](float32 x) -> float32 {
//     //         if(x < 0) x = -x;
//     //         if(x < 3) return sinc(x) * sinc(x / 3);
//     //         return 0;
//     //     };
        
//     //     for(int32 y = 0; y < nh; ++y) {
//     //         const float32 fy = (y + 0.5f) * yScale - 0.5f;
//     //         const int32 yc = static_cast<int32>(fy);
            
//     //         for(int32 x = 0; x < nw; ++x) {
//     //             const float32 fx = (x + 0.5f) * xScale - 0.5f;
//     //             const int32 xc = static_cast<int32>(fx);
                
//     //             uint8* dstPixel = dst->RowPtr(y) + x * bpp;
                
//     //             for(int32 c = 0; c < bpp; ++c) {
//     //                 float32 sum = 0, wsum = 0;
                    
//     //                 for(int32 m = -2; m <= 3; ++m) {
//     //                     const int32 sy = (yc + m < 0) ? 0 : (yc + m >= sh) ? sh - 1 : yc + m;
//     //                     const float32 wy = lanczos(fy - static_cast<float32>(yc + m));
                        
//     //                     for(int32 n = -2; n <= 3; ++n) {
//     //                         const int32 sx = (xc + n < 0) ? 0 : (xc + n >= sw) ? sw - 1 : xc + n;
//     //                         const float32 wx = lanczos(fx - static_cast<float32>(xc + n));
                            
//     //                         sum += src.RowPtr(sy)[sx * bpp + c] * wx * wy;
//     //                         wsum += wx * wy;
//     //                     }
//     //                 }
                    
//     //                 float32 val = (wsum != 0) ? sum / wsum : 0;
//     //                 dstPixel[c] = static_cast<uint8>(val < 0 ? 0 : val > 255 ? 255 : val + 0.5f);
//     //             }
//     //         }
//     //     }
        
//     //     return dst;
//     // }

//     // NkImage* NkImage::Resize(int32 nw, int32 nh, NkResizeFilter filter) const noexcept {
//     //     if(!IsValid() || nw <= 0 || nh <= 0) return nullptr;
        
//     //     switch(filter) {
//     //         case NkResizeFilter::NK_NEAREST:
//     //             return ResizeNearest(*this, nw, nh);
//     //         case NkResizeFilter::NK_BILINEAR:
//     //             return ResizeBilinear(*this, nw, nh);
//     //         case NkResizeFilter::NK_BICUBIC:
//     //             return ResizeBicubic(*this, nw, nh);
//     //         case NkResizeFilter::NK_LANCZOS3:
//     //             return ResizeLanczos3(*this, nw, nh);
//     //         default:
//     //             return ResizeBilinear(*this, nw, nh);
//     //     }
//     // }

// } // namespace nkentseu
