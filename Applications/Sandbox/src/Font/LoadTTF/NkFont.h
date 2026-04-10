#pragma once

#include "NKMath/NKMath.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {

    #define NK_READ_BE16(mem) ((((uint8*)(mem))[0] << 8) | (((uint8*)(mem))[1]))
    #define NK_READ_BE32(mem) ((((uint8*)(mem))[0] << 24) | (((uint8*)(mem))[1] << 16) | (((uint8*)(mem))[2] << 8) | (((uint8*)(mem))[3]))
    #define NK_P_MOVE(mem, a) ((mem) += (a))
    #define NK_READ_BE16_MOVE(mem) (NK_READ_BE16((mem))); (NK_P_MOVE((mem), 2))
    #define NK_READ_BE32_MOVE(mem) (NK_READ_BE32((mem))); (NK_P_MOVE((mem), 4))

    struct NkOffsetSubTable {
        uint32 scalerType;
        uint16 numTables;
        uint16 searchRange;
        uint16 entrySelector;
        uint16 rangeShift;
    };

    struct NkDirectoryTable {
        union {
            nk_char tagC[4];
            uint32  balise;
        };
        uint32 checkSum;
        uint32 decalage;
        uint32 length;
    };

    struct NkFontDirectory {
        NkOffsetSubTable  offSub;
        NkDirectoryTable* tblDir;
    };

    struct NkCmapEncodingSubtab {
        uint16 platformID;
        uint16 platformSpecificID;
        uint32 decalage;
    };

    struct NkCmap {
        uint16 version;
        uint16 numSubtab;
        NkCmapEncodingSubtab subTab;
    };

    // struct {
    //     uint16 format;
    //     uint16 length;
    //     uint16 language;
    // };

    nk_char* NkReadFile(const nk_char* filename, int32* filesize);
    void NkReadFontDyrectory(nk_char** mem, NkFontDirectory* ft);
    void NkReadOffsetSubtable(nk_char** mem, NkOffsetSubTable* offSub);
    void NkReadTableDirectory(nk_char** mem, NkDirectoryTable** tblDir, int32 tblSize);
    void NkPrintTableDirectory(NkDirectoryTable* tblDir, int32 tblSize);
    void NkReadCmap(char* mem, NkCmap* cmap);
}