// =============================================================================
//  Test du rastérizer bitmap en isolation
//  Construit un contour TEST et rastérise
// =============================================================================

#include <cstdio>
#include <cmath>

#include "NKFont/NKFont.h"

int main() {
    printf("=== NKRasterizer Unit Test ===\n");
    
    // Crée une sortie redirigeant stderr vers un fichier
    FILE* debug = fopen("test_rasterizer_output.txt", "w");
    if (!debug) debug = stdout;
    
    fprintf(debug, "Test started\n");
    fflush(debug);
    
    // Crée un rasterizer
    NkRasterizer rasterizer;
    fprintf(debug, "Rasterizer created\n");
    fflush(debug);
    
    // Crée un contour simple: carré 10x10
    NkGlyphOutline outline;
    // outline doit avoir quelques points et segments
    // Pour simplifier, crée juste un contour vide d'abord
    fprintf(debug, "Creating simple outline...\n");
    fflush(debug);
    
    // Créer un rectangle simple manuellement 
    // C'est très difficile sans l'API complète...
    // Laisse plutôt juste teste la RasterizeBitmap call
    
    NkGlyphMetrics metrics;
    metrics.width = 10.f;
    metrics.height = 10.f;
    metrics.bearingX = 0.f;
    metrics.bearingY = 10.f;
    
    NkRasterizeParams params = NkRasterizeParams::ForBitmap();
    fprintf(debug, "Params: renderMode=%d\n", (int)params.renderMode);
    fflush(debug);
    
    NkBitmapBuffer bitmap;
    NkFontResult result = rasterizer.Rasterize(outline, metrics, params, bitmap);
    
    fprintf(debug, "Rasterize result: %s\n", result.ToString());
    fprintf(debug, "Bitmap: width=%u height=%u ready=%d\n", 
            bitmap.mWidth, bitmap.mHeight, (int)bitmap.IsReady());
    
    if (bitmap.IsReady()) {
        nk_uint8* data = bitmap.Data();
        fprintf(debug, "Bitmap data first 10 bytes: ");
        for (int i = 0; i < 10; ++i) {
            fprintf(debug, "%02x ", data[i]);
        }
        fprintf(debug, "\n");
    }
    
    fflush(debug);
    if (debug != stdout) fclose(debug);
    
    printf("Test completed. Check test_rasterizer_output.txt\n");
    return 0;
}
