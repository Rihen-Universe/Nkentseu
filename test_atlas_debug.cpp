#include <cstdio>
#include <GL/glad.h>

class TestAtlas {
public:
    static constexpr int MAX_ATLAS_PAGES = 8;
    GLuint mAtlasTex[MAX_ATLAS_PAGES] = {};
    int mAtlasPageCount = 0;
    
    void testCallback() {
        FILE* initlog = fopen("C:\\Users\\Rihen\\Desktop\\initlog_test.txt", "w");
        fprintf(initlog, "TEST START\n");
        fflush(initlog);
        
        // Simulate what happens after UploadAtlas
        printf("After UploadAtlas: mAtlasTex[0]=%u, mAtlasPageCount=%d\n", mAtlasTex[0], mAtlasPageCount);
        fprintf(initlog, "After UploadAtlas: mAtlasTex[0]=%u, mAtlasPageCount=%d\n", mAtlasTex[0], mAtlasPageCount);
        fflush(initlog);
        
        fprintf(initlog, "TEST END\n");
        fflush(initlog);
        fclose(initlog);
        
        printf("Log file written\n");
    }
};

int main() {
    TestAtlas ta;
    ta.testCallback();
    printf("Done\n");
    return 0;
}
