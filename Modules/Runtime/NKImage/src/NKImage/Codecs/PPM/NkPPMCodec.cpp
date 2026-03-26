#include "NKImage/Codecs/PPM/NkPPMCodec.h"
#include <cstdio>
namespace nkentseu {

static void skipWhitespaceAndComments(const uint8*& p, const uint8* end) {
    while (p < end) {
        while (p < end && (*p==' '||*p=='\t'||*p=='\n'||*p=='\r')) ++p;
        if (p < end && *p == '#') { while (p < end && *p != '\n') ++p; }
        else break;
    }
}
static uint32 readUint(const uint8*& p, const uint8* end) {
    skipWhitespaceAndComments(p, end);
    uint32 v = 0;
    while (p < end && *p >= '0' && *p <= '9') { v = v*10 + (*p-'0'); ++p; }
    return v;
}

NkImage* NkPPMCodec::Decode(const uint8* data, usize size) noexcept {
    if (size < 3) return nullptr;
    const uint8* p = data, *end = data + size;
    if (*p != 'P') return nullptr;
    ++p;
    const char magic = static_cast<char>(*p++);
    if (magic < '1' || magic > '6') return nullptr;

    const uint32 w = readUint(p, end);
    const uint32 h = readUint(p, end);
    uint32 maxVal = (magic=='1'||magic=='4') ? 1 : readUint(p, end);
    if (w==0||h==0) return nullptr;
    if (p < end && (*p==' '||*p=='\t'||*p=='\n'||*p=='\r')) ++p; // skip one whitespace

    const bool isBinary = (magic >= '4');
    const bool isGray   = (magic=='2'||magic=='5');
    const bool isBit    = (magic=='1'||magic=='4');
    const int32 ch = (isGray || isBit) ? 1 : 3;
    NkImagePixelFormat fmt = ch == 1 ? NkImagePixelFormat::NK_GRAY8 : NkImagePixelFormat::NK_RGB24;
    NkImage* img = NkImage::Alloc(w, h, fmt);
    if (!img) return nullptr;

    for (uint32 y = 0; y < h; ++y) {
        uint8* row = img->RowPtr(y);
        for (uint32 x = 0; x < w * static_cast<uint32>(ch); ++x) {
            if (isBinary) {
                if (isBit) {
                    const uint8 byte = (p < end) ? *p : 0;
                    row[x] = ((byte >> (7 - (x % 8))) & 1) ? 0 : 255;
                    if ((x+1) % 8 == 0 && p < end) ++p;
                } else {
                    row[x] = (p < end) ? *p++ : 0;
                }
            } else {
                row[x] = static_cast<uint8>(readUint(p, end) * 255 / maxVal);
            }
        }
        if (isBit && !isBinary) {} // ascii bit: already handled
    }
    return img;
}

bool NkPPMCodec::Encode(const NkImage& img, const char* path) noexcept {
    if (!img.IsValid()) return false;
    FILE* f = ::fopen(path, "wb");
    if (!f) return false;
    const NkImage* src = &img;
    NkImage* conv = nullptr;
    if (img.Channels() == 1) {
        ::fprintf(f, "P5\n%d %d\n255\n", img.Width(), img.Height());
    } else {
        if (img.Channels() != 3) {
            conv = img.Convert(NkImagePixelFormat::NK_RGB24);
            if (!conv) { ::fclose(f); return false; }
            src = conv;
        }
        ::fprintf(f, "P6\n%d %d\n255\n", src->Width(), src->Height());
    }
    for (int32 y = 0; y < src->Height(); ++y)
        ::fwrite(src->RowPtr(y), 1, src->Width() * src->Channels(), f);
    ::fclose(f);
    if (conv) conv->Free();
    return true;
}
} // namespace nkentseu
