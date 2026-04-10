#include "NkFont.h"

#include "NKLogger/NkLog.h"

namespace nkentseu
{
    nk_char* NkReadFile(const nk_char* filename, int32* filesize) {
        if (strlen(filename) > 0) {
            FILE* file = fopen(filename, "rb");

            if (file) {
                fseek(file, 0, SEEK_END);

                int size = ftell(file);
                fseek(file, 0, SEEK_SET);

                if (filesize) {
                    *filesize = size;
                }

                nk_char* buffer = (nk_char*) malloc(size + 1);
                int readAmount = fread(buffer, size, 1, file);

                buffer[size] = '\0';

                if (readAmount) {
                    fclose(file);
                    return buffer;
                }
                free(buffer);
                fclose(file);

                return nullptr;
            }
        }
        return nullptr;
    }
    
    void NkReadFontDyrectory(nk_char** mem, NkFontDirectory* ft) {
        NkReadOffsetSubtable(mem, &ft->offSub);
        NkReadTableDirectory(mem, &ft->tblDir, ft->offSub.numTables);
    }

    void NkReadOffsetSubtable(nk_char** mem, NkOffsetSubTable* offSub) {
        nk_char* m = *mem;

        offSub->scalerType = NK_READ_BE32_MOVE(m);
        offSub->numTables = NK_READ_BE16_MOVE(m);
        offSub->searchRange = NK_READ_BE16_MOVE(m);
        offSub->entrySelector = NK_READ_BE16_MOVE(m);
        offSub->rangeShift = NK_READ_BE16_MOVE(m);

        *mem = m;
    }

    void NkReadTableDirectory(nk_char** mem, NkDirectoryTable** tblDir, int32 tblSize) {
        nk_char* m = *mem;

        *tblDir = (NkDirectoryTable*) calloc(1, sizeof(NkDirectoryTable) * tblSize);

        for (int32 i = 0; i < tblSize; ++i) {
            NkDirectoryTable* t = *tblDir + i;

            t->balise = NK_READ_BE32_MOVE(m);
            t->checkSum = NK_READ_BE16_MOVE(m);
            t->decalage = NK_READ_BE16_MOVE(m);
            t->length = NK_READ_BE16_MOVE(m);
        }

        *mem = m;
    }

    void NkPrintTableDirectory(NkDirectoryTable* tblDir, int32 tblSize) {
        logger.Info("#) \t tag \t\t len \t offset");

        for (int32 i = 0; i < tblSize; ++i) {
            NkDirectoryTable* t = tblDir + i;

            // logger.Info("{0} \t {1}.{2}.{3}.{4} \t\t {5} \t {6}", i + 1, (char)t->tagC[3], (char)t->tagC[2], (char)t->tagC[1], (char)t->tagC[0], t->length, t->decalage);
            logger.Infof("%d \t %c%c%c%c \t %d \t %d", i + 1, (char)t->tagC[3], (char)t->tagC[2], (char)t->tagC[1], (char)t->tagC[0], t->length, t->decalage);
        }
    }

    void NkReadCmap(char* mem, NkCmap* cmap) {

    }
    
} // namespace nkentseu
