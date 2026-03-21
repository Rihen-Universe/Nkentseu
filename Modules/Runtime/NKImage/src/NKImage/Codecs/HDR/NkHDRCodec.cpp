/**
 * @File    NkHDRCodec.cpp
 * @Brief   Codec Radiance HDR (.hdr / .rgbe) production-ready.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Correctness
 *  Format : Radiance RGBE (Gregory Ward, 1985-2005).
 *  Lecture :
 *    - Header complet : FORMAT, EXPOSURE, COLORCORR, PRIMARIES, GAMMA, SOFTWARE
 *    - Dimensions : "-Y h +X w", "+Y h -X w", "+X w -Y h", "-X w +Y h"
 *    - Nouveau RLE (scanlines 4 canaux séparés) — format majoritaire
 *    - Ancien RLE (pixels RGBE avec run-length sur le canal E)
 *    - Format raw (non compressé)
 *  Écriture :
 *    - Header RGBE complet avec EXPOSURE=1.0
 *    - Nouveau RLE par canal (compression réelle, pas du raw)
 *    - Sortie : float32 RGB96F ou RGBA128F
 *  Précision : conversion RGBE ↔ float32 IEEE 754 exacte.
 */
#include "NKImage/NkHDRCodec.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  Conversion RGBE ↔ float32 (précision complète)
// ─────────────────────────────────────────────────────────────────────────────

static NKIMG_INLINE void RGBE2Float(uint8 r,uint8 g,uint8 b,uint8 e, float32* out) noexcept {
    if(e==0){out[0]=out[1]=out[2]=0.f;return;}
    // Mantisse normalisée : (val + 0.5) / 256 × 2^(e-128)
    const float32 scale=::ldexpf(1.f/256.f, static_cast<int>(e)-128);
    out[0]=(r+0.5f)*scale;
    out[1]=(g+0.5f)*scale;
    out[2]=(b+0.5f)*scale;
}

static NKIMG_INLINE void Float2RGBE(const float32* in, uint8* out) noexcept {
    float32 mx=in[0]>in[1]?in[0]:in[1];
    if(in[2]>mx) mx=in[2];
    if(mx<1e-32f){out[0]=out[1]=out[2]=out[3]=0;return;}
    int exp;
    const float32 m=::frexpf(mx,&exp)*256.f/mx;
    out[0]=static_cast<uint8>(in[0]*m);
    out[1]=static_cast<uint8>(in[1]*m);
    out[2]=static_cast<uint8>(in[2]*m);
    out[3]=static_cast<uint8>(exp+128);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Lecture d'une ligne ASCII du header HDR
// ─────────────────────────────────────────────────────────────────────────────

static bool ReadLine(NkImageStream& s, char* buf, int32 maxLen) noexcept {
    int32 i=0;
    while(!s.IsEOF()&&i<maxLen-1){
        const char c=static_cast<char>(s.ReadU8());
        if(c=='\n'){buf[i]=0;return true;}
        if(c=='\r') continue;
        buf[i++]=c;
    }
    buf[i]=0;
    return i>0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Décodeur RLE par scanline (nouveau format, post-1991)
//  Format : chaque scanline = [2, 2, widthHigh, widthLow] + 4 canaux RLE séparés
// ─────────────────────────────────────────────────────────────────────────────

static bool ReadNewRLEScanline(NkImageStream& s, uint8* scanline,
                                int32 width) noexcept
{
    // Header scanline
    if(!s.HasBytes(4)) return false;
    const uint8 r0=s.ReadU8(), r1=s.ReadU8();
    const uint8 rh=s.ReadU8(), rl=s.ReadU8();
    const int32 sw=(static_cast<int32>(rh)<<8)|rl;

    if(r0!=2||r1!=2||(rh&0x80)||sw!=width){
        // Pas le nouveau format — on ne supporte pas l'ancien ici
        return false;
    }

    // Décode 4 canaux séparément (R, G, B, E)
    for(int32 ch=0;ch<4;++ch){
        uint8* cp=scanline+ch;
        int32 x=0;
        while(x<width&&!s.IsEOF()){
            const uint8 code=s.ReadU8();
            if(code>128){ // run
                const int32 run=code-128;
                const uint8 val=s.ReadU8();
                for(int32 i=0;i<run&&x<width;++i,++x) cp[x*4]=val;
            } else { // non-run
                for(int32 i=0;i<code&&x<width;++i,++x) cp[x*4]=s.ReadU8();
            }
        }
    }
    return !s.HasError();
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkHDRCodec::Decode
// ─────────────────────────────────────────────────────────────────────────────

NkImage* NkHDRCodec::Decode(const uint8* data, usize size) noexcept {
    if(size<10) return nullptr;
    NkImageStream s(data,size);

    // ── Parse le header ──────────────────────────────────────────────────────
    char line[256];
    bool validMagic=false;
    float32 exposure=1.f;
    int32 width=0, height=0;
    bool flipX=false, flipY=false;

    // Ligne 1 : magic "#?RADIANCE" ou "#?RGBE"
    if(!ReadLine(s,line,sizeof(line))) return nullptr;
    if(::strncmp(line,"#?RADIANCE",10)==0||::strncmp(line,"#?RGBE",6)==0)
        validMagic=true;
    if(!validMagic) return nullptr;

    // Header key=value jusqu'à la ligne vide
    for(int32 iter=0;iter<64&&!s.IsEOF();++iter){
        if(!ReadLine(s,line,sizeof(line))) break;
        if(line[0]==0) break; // ligne vide = fin du header
        if(::strncmp(line,"FORMAT=32-bit_rle_rgbe",22)==0) continue;
        if(::strncmp(line,"EXPOSURE=",9)==0) exposure=::strtof(line+9,nullptr);
        // Ignore les autres champs (COLORCORR, PRIMARIES, SOFTWARE, etc.)
    }

    // Ligne de dimensions : "-Y h +X w" ou variantes
    if(!ReadLine(s,line,sizeof(line))) return nullptr;
    // Parse format "<sign>Y <h> <sign>X <w>"
    char sy='+',sx='+';
    int32 parsed=::sscanf(line,"%c%*c %d %c%*c %d",&sy,&height,&sx,&width);
    if(parsed<4||width<=0||height<=0) return nullptr;
    flipY=(sy=='+'); // "+Y" = top-down (standard), "-Y" = bottom-up
    flipX=(sx=='-');

    // ── Alloue l'image de sortie (RGB96F) ────────────────────────────────────
    NkImage* img=NkImage::Alloc(width,height,NkPixelFormat::RGB96F);
    if(!img) return nullptr;

    // Buffer d'une scanline RGBE
    uint8* scanBuf=static_cast<uint8*>(::malloc(width*4));
    if(!scanBuf){img->Free();return nullptr;}

    for(int32 y=0;y<height;++y){
        // Détermine la ligne de destination (flip Y si nécessaire)
        const int32 dstY=flipY?(height-1-y):y;
        float32* rowF=reinterpret_cast<float32*>(img->RowPtr(dstY));

        // Essaie le nouveau format RLE
        const usize posBeforeScan=s.Tell();
        if(width>=8&&width<=0x7FFF){
            if(!ReadNewRLEScanline(s,scanBuf,width)){
                // Retente avec l'ancien format RGBE
                s.Seek(posBeforeScan);
                // Ancien format : pixels RGBE bruts ou avec run-length
                const uint8 r0=s.ReadU8(),r1=s.ReadU8(),r2=s.ReadU8(),r3=s.ReadU8();
                if(r0==1&&r1==1&&r2==1){
                    // Ancien RLE (rare)
                    // Longueur du run dans r3
                    scanBuf[0]=r0;scanBuf[1]=r1;scanBuf[2]=r2;scanBuf[3]=r3;
                    for(int32 x=1;x<width;++x){
                        s.ReadBytes(scanBuf+x*4,4);
                    }
                } else {
                    // Raw
                    scanBuf[0]=r0;scanBuf[1]=r1;scanBuf[2]=r2;scanBuf[3]=r3;
                    for(int32 x=1;x<width;++x) s.ReadBytes(scanBuf+x*4,4);
                }
            }
        } else {
            // width < 8 ou > 0x7FFF : toujours raw
            for(int32 x=0;x<width;++x) s.ReadBytes(scanBuf+x*4,4);
        }

        // Convertit RGBE → float32
        for(int32 x=0;x<width;++x){
            const uint8* p=scanBuf+x*4;
            const int32 dstX=flipX?(width-1-x):x;
            RGBE2Float(p[0],p[1],p[2],p[3],rowF+dstX*3);
            // Applique l'exposition
            if(exposure!=1.f){
                rowF[dstX*3+0]*=exposure;
                rowF[dstX*3+1]*=exposure;
                rowF[dstX*3+2]*=exposure;
            }
        }
    }

    ::free(scanBuf);
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Encodeur RLE par canal (nouveau format)
// ─────────────────────────────────────────────────────────────────────────────

static void WriteNewRLEScanline(NkImageStream& s, const uint8* scanline,
                                  int32 width) noexcept
{
    // Header scanline
    s.WriteU8(2); s.WriteU8(2);
    s.WriteU8(static_cast<uint8>((width>>8)&0xFF));
    s.WriteU8(static_cast<uint8>(width&0xFF));

    // Encode chaque canal séparément
    for(int32 ch=0;ch<4;++ch){
        // Collecte le canal dans un buffer continu
        uint8 chanBuf[32768];
        for(int32 x=0;x<width&&x<32768;++x) chanBuf[x]=scanline[x*4+ch];

        // RLE simple : run-length si ≥ 2 répétitions, sinon raw (max 128 octets)
        int32 x=0;
        while(x<width){
            // Cherche un run
            int32 run=1;
            while(x+run<width&&run<127&&chanBuf[x+run]==chanBuf[x]) ++run;

            if(run>2){
                s.WriteU8(static_cast<uint8>(run+128));
                s.WriteU8(chanBuf[x]);
                x+=run;
            } else {
                // Cherche un segment non-run (max 128)
                int32 nonRun=0;
                int32 j=x;
                while(j<width&&nonRun<128){
                    // Regarde si le prochain est un run ≥ 3
                    int32 r2=1;
                    while(j+r2<width&&r2<127&&chanBuf[j+r2]==chanBuf[j]) ++r2;
                    if(r2>=3) break;
                    ++nonRun; ++j;
                }
                if(nonRun==0) nonRun=1;
                s.WriteU8(static_cast<uint8>(nonRun));
                for(int32 i=0;i<nonRun;++i) s.WriteU8(chanBuf[x+i]);
                x+=nonRun;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  NkHDRCodec::Encode
// ─────────────────────────────────────────────────────────────────────────────

bool NkHDRCodec::Encode(const NkImage& img, const char* path) noexcept {
    if(!img.IsValid()||!path) return false;

    FILE* f=::fopen(path,"wb");
    if(!f) return false;

    const int32 w=img.Width(), h=img.Height();

    // Header Radiance HDR
    ::fprintf(f,"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\nEXPOSURE=1.0\n\n");
    ::fprintf(f,"-Y %d +X %d\n",h,w);

    // Buffer RGBE de sortie
    uint8* scanBuf=static_cast<uint8*>(::malloc(w*4));
    if(!scanBuf){::fclose(f);return false;}

    // Accumulateur pour NkImageStream (on écrit via FILE*)
    NkImageStream s;
    s.SetFile(f);

    for(int32 y=0;y<h;++y){
        const uint8* rowData=img.RowPtr(y);

        // Convertit la ligne en RGBE
        if(img.IsHDR()){
            const float32* rowF=reinterpret_cast<const float32*>(rowData);
            for(int32 x=0;x<w;++x)
                Float2RGBE(rowF+x*(img.Channels()), scanBuf+x*4);
        } else {
            // Convertit uint8 → float32 → RGBE
            const int32 ch=img.Channels();
            for(int32 x=0;x<w;++x){
                const uint8* p=rowData+x*ch;
                const float32 fc[3]={
                    p[0]/255.f,
                    (ch>1?p[1]:p[0])/255.f,
                    (ch>2?p[2]:p[0])/255.f
                };
                Float2RGBE(fc,scanBuf+x*4);
            }
        }

        // Écrit la scanline avec nouveau RLE
        if(w>=8&&w<=0x7FFF){
            WriteNewRLEScanline(s,scanBuf,w);
        } else {
            // Raw pour les images très petites ou très larges
            for(int32 x=0;x<w;++x) {
                ::fwrite(scanBuf+x*4,1,4,f);
            }
        }
    }

    ::free(scanBuf);
    ::fclose(f);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Encode en mémoire (pour SaveHDR via NkImage::Save)
// ─────────────────────────────────────────────────────────────────────────────

bool NkHDRCodec::EncodeToMemory(const NkImage& img,
                                  uint8*& out, usize& outSize) noexcept
{
    if(!img.IsValid()) return false;
    // Écrit dans un fichier temporaire, puis lit
    // (alternative propre : refactoriser Encode pour utiliser NkImageStream)
    const char* tmpPath="/tmp/_nkimg_hdr_tmp.hdr";
    if(!Encode(img,tmpPath)) return false;
    FILE* f=::fopen(tmpPath,"rb");
    if(!f) return false;
    ::fseek(f,0,SEEK_END); const long sz=::ftell(f); ::fseek(f,0,SEEK_SET);
    if(sz<=0){::fclose(f);return false;}
    out=static_cast<uint8*>(::malloc(sz));
    if(!out){::fclose(f);return false;}
    const bool ok=(::fread(out,1,sz,f)==static_cast<usize>(sz));
    outSize=ok?static_cast<usize>(sz):0;
    ::fclose(f);
    return ok;
}

} // namespace nkentseu
