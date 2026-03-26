/**
 * @File    NkICOCodec.cpp
 * @Brief   Codec ICO/CUR — lecture seule, sélectionne la plus grande image.
 */
#include "NKImage/Codecs/ICO/NkICOCodec.h"
#include "NKImage/Codecs/BMP/NkBMPCodec.h"
#include "NKImage/Codecs/PNG/NkPNGCodec.h"
#include "NKMemory/NkAllocator.h"
#include "NKMemory/NkFunction.h"
namespace nkentseu {

using namespace nkentseu::memory;
NkImage* NkICOCodec::Decode(const uint8* data, usize size) noexcept {
    if (size < 6) return nullptr;
    NkImageStream s(data, size);
    (void)s.ReadU16LE(); // reserved
    const uint16 type  = s.ReadU16LE(); // 1=ICO, 2=CUR
    const uint16 count = s.ReadU16LE();
    if ((type!=1&&type!=2)||count==0) return nullptr;

    // Cherche la plus grande image
    int32 bestW=0, bestH=0;
    uint32 bestOffset=0, bestSize=0;
    for (uint16 i = 0; i < count; ++i) {
        uint8 w=s.ReadU8(), h=s.ReadU8();
        s.Skip(6); // colorCount, reserved, planes/hotspot, bitcount/hotspot
        const uint32 imgSize   = s.ReadU32LE();
        const uint32 imgOffset = s.ReadU32LE();
        const int32 iw = w==0?256:w, ih = h==0?256:h;
        if (iw*ih > bestW*bestH) {
            bestW=iw; bestH=ih; bestOffset=imgOffset; bestSize=imgSize;
        }
    }
    if (bestOffset+bestSize > size) return nullptr;
    const uint8* imgData = data + bestOffset;
    // PNG embeds commence par 0x89PNG
    if (bestSize>=8 && imgData[0]==0x89 && imgData[1]=='P')
        return NkPNGCodec::Decode(imgData, bestSize);
    // BMP embed : pas de file header, commence par DIB header
    // On construit un faux BMP header (14+40 octets)
    const usize fakeSz = 14 + bestSize;
    uint8* fakeBMP = static_cast<uint8*>(NkAlloc(fakeSz));
    if (!fakeBMP) return nullptr;
    fakeBMP[0]='B'; fakeBMP[1]='M';
    const uint32 fsize=static_cast<uint32>(fakeSz);
    fakeBMP[2]=fsize&0xFF; fakeBMP[3]=(fsize>>8)&0xFF; fakeBMP[4]=(fsize>>16)&0xFF; fakeBMP[5]=(fsize>>24)&0xFF;
    fakeBMP[6]=fakeBMP[7]=fakeBMP[8]=fakeBMP[9]=0;
    const uint32 poff=14+40; // après DIB header
    fakeBMP[10]=poff&0xFF; fakeBMP[11]=(poff>>8)&0xFF; fakeBMP[12]=(poff>>16)&0xFF; fakeBMP[13]=(poff>>24)&0xFF;
    NkCopy(fakeBMP+14, imgData, bestSize);
    NkImage* result = NkBMPCodec::Decode(fakeBMP, fakeSz);
    NkFree(fakeBMP);
    return result;
}
} // namespace nkentseu
