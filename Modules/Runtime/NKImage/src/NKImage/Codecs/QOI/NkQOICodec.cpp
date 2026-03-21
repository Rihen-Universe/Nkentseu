/**
 * @File    NkQOICodec.cpp
 * @Brief   Codec QOI (Quite OK Image Format) — lecture et écriture.
 *          Spec: https://qoiformat.org/
 */
#include "NKImage/NkQOICodec.h"
#include <cstring>
namespace nkentseu {

static constexpr uint8  QOI_OP_INDEX = 0x00;
static constexpr uint8  QOI_OP_DIFF  = 0x40;
static constexpr uint8  QOI_OP_LUMA  = 0x80;
static constexpr uint8  QOI_OP_RUN   = 0xC0;
static constexpr uint8  QOI_OP_RGB   = 0xFE;
static constexpr uint8  QOI_OP_RGBA  = 0xFF;
static constexpr uint8  QOI_MASK2    = 0xC0;

static NKIMG_INLINE uint32 QOI_HASH(uint8 r,uint8 g,uint8 b,uint8 a){
    return static_cast<uint32>(r*3+g*5+b*7+a*11)%64u;
}

NkImage* NkQOICodec::Decode(const uint8* data, usize size) noexcept {
    if (size < 14+8) return nullptr;
    NkImageStream s(data, size);
    if (s.ReadU32BE() != 0x716F6966u) return nullptr; // "qoif"
    const uint32 w = s.ReadU32BE(), h = s.ReadU32BE();
    const uint8 channels = s.ReadU8(), colorSpace = s.ReadU8();
    (void)colorSpace;
    if (w==0||h==0||channels<3||channels>4) return nullptr;

    NkPixelFormat fmt = channels==4 ? NkPixelFormat::RGBA32 : NkPixelFormat::RGB24;
    NkImage* img = NkImage::Alloc(w, h, fmt);
    if (!img) return nullptr;

    uint8 index[64][4] = {};
    uint8 px[4] = {0,0,0,255};
    int32 run = 0;

    for (uint32 y = 0; y < h; ++y) {
        uint8* row = img->RowPtr(y);
        for (uint32 x = 0; x < w; ++x) {
            if (run > 0) { --run; }
            else {
                const uint8 b1 = s.ReadU8();
                if (b1 == QOI_OP_RGB) {
                    px[0]=s.ReadU8(); px[1]=s.ReadU8(); px[2]=s.ReadU8();
                } else if (b1 == QOI_OP_RGBA) {
                    px[0]=s.ReadU8(); px[1]=s.ReadU8(); px[2]=s.ReadU8(); px[3]=s.ReadU8();
                } else if ((b1 & QOI_MASK2) == QOI_OP_INDEX) {
                    ::memcpy(px, index[b1&0x3F], 4);
                } else if ((b1 & QOI_MASK2) == QOI_OP_DIFF) {
                    px[0] += ((b1>>4)&3)-2; px[1] += ((b1>>2)&3)-2; px[2] += (b1&3)-2;
                } else if ((b1 & QOI_MASK2) == QOI_OP_LUMA) {
                    const uint8 b2 = s.ReadU8();
                    const int32 dg = (b1&0x3F)-32;
                    px[0] += dg-8+((b2>>4)&0xF); px[1] += dg; px[2] += dg-8+(b2&0xF);
                } else { // RUN
                    run = (b1&0x3F);
                }
                ::memcpy(index[QOI_HASH(px[0],px[1],px[2],px[3])], px, 4);
            }
            uint8* dst = row + x * channels;
            dst[0]=px[0]; dst[1]=px[1]; dst[2]=px[2];
            if (channels==4) dst[3]=px[3];
        }
    }
    return img;
}

bool NkQOICodec::Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept {
    if (!img.IsValid()) return false;
    const NkImage* src = &img;
    NkImage* conv = nullptr;
    if (img.Channels()!=3&&img.Channels()!=4) {
        conv=img.Convert(NkPixelFormat::RGB24);
        if(!conv) return false; src=conv;
    }
    const int32 w=src->Width(),h=src->Height(),ch=src->Channels();

    NkImageStream s;
    s.WriteU32BE(0x716F6966u);
    s.WriteU32BE(static_cast<uint32>(w));
    s.WriteU32BE(static_cast<uint32>(h));
    s.WriteU8(static_cast<uint8>(ch));
    s.WriteU8(1); // sRGB+linear alpha

    uint8 index[64][4] = {};
    uint8 prev[4]={0,0,0,255};
    int32 run=0;

    auto flush = [&](){ if(run>0){s.WriteU8(static_cast<uint8>(QOI_OP_RUN|(run-1)));run=0;} };

    for (int32 y = 0; y < h; ++y) {
        const uint8* row = src->RowPtr(y);
        for (int32 x = 0; x < w; ++x) {
            const uint8* p = row + x*ch;
            uint8 px[4]={p[0],p[1],p[2],ch==4?p[3]:uint8(255)};

            if (::memcmp(px,prev,4)==0) {
                ++run;
                if (run==62||( x==w-1&&y==h-1)) flush();
            } else {
                flush();
                const uint32 hi = QOI_HASH(px[0],px[1],px[2],px[3]);
                if (::memcmp(index[hi],px,4)==0) {
                    s.WriteU8(static_cast<uint8>(QOI_OP_INDEX|hi));
                } else {
                    ::memcpy(index[hi],px,4);
                    if (px[3]==prev[3]) {
                        const int32 dr=px[0]-prev[0],dg=px[1]-prev[1],db=px[2]-prev[2];
                        const int32 drdg=dr-dg, dbdg=db-dg;
                        if (dr>=-2&&dr<=1&&dg>=-2&&dg<=1&&db>=-2&&db<=1) {
                            s.WriteU8(static_cast<uint8>(QOI_OP_DIFF|((dr+2)<<4)|((dg+2)<<2)|(db+2)));
                        } else if (dg>=-32&&dg<=31&&drdg>=-8&&drdg<=7&&dbdg>=-8&&dbdg<=7) {
                            s.WriteU8(static_cast<uint8>(QOI_OP_LUMA|(dg+32)));
                            s.WriteU8(static_cast<uint8>(((drdg+8)<<4)|(dbdg+8)));
                        } else {
                            s.WriteU8(QOI_OP_RGB);
                            s.WriteU8(px[0]);s.WriteU8(px[1]);s.WriteU8(px[2]);
                        }
                    } else {
                        s.WriteU8(QOI_OP_RGBA);
                        s.WriteU8(px[0]);s.WriteU8(px[1]);s.WriteU8(px[2]);s.WriteU8(px[3]);
                    }
                }
                ::memcpy(prev,px,4);
            }
        }
    }
    flush();
    // End marker
    const uint8 end[8]={0,0,0,0,0,0,0,1};
    s.WriteBytes(end,8);
    if (conv) conv->Free();
    return s.TakeBuffer(out,outSize);
}
} // namespace nkentseu
