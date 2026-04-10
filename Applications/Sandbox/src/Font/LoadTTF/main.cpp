#include "NkFont.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;

int main (int argc, char** argv) {
    (void)argc; (void)argv;

    int32 filesize;
    
    nk_char* data = NkReadFile("Resources/Fonts/ProggyClean.ttf", &filesize);
    // nk_char* data = NkReadFile("Resources/Fonts/Geist-Bold.otf", &filesize);

    NkFontDirectory ft;

    NkReadFontDyrectory(&data, &ft);

    
    NkPrintTableDirectory(ft.tblDir, ft.offSub.numTables);

    return (0);
}