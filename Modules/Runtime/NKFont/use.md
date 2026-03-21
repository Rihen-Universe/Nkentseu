Deux questions distinctes — on répond aux deux.

---

## 1. Intégration NKFont avec OpenGL / Vulkan / D3D

L'atlas GPU-ready de NKFont produit un `uint8*` Gray8. Le branchement sur chaque API est minimal — NKFont ne connaît pas le GPU, c'est voulu.Voici d'abord le pipeline d'intégration GPU, puis l'architecture de la bibliothèque image.### OpenGL — code complet

```cpp
// NkFontGL.h — couche d'intégration OpenGL (à mettre dans votre moteur)
#include "NKFont/NKFont.h"
#include <glad/glad.h>   // ou glew/glbinding selon votre setup

struct NkFontGL {
    GLuint textureId  = 0;
    GLuint vao = 0, vbo = 0;
    GLuint shader     = 0;
    int    atlasW     = 0;
    int    atlasH     = 0;

    // ── Init ───────────────────────────────────────────────────────────────

    void Init(int atlasWidth, int atlasHeight) {
        atlasW = atlasWidth; atlasH = atlasHeight;

        // Texture Gray8 — un seul canal rouge
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Swizzle : R → RGBA (le shader lit .r comme alpha)
        GLint swizzle[] = {GL_RED, GL_RED, GL_RED, GL_RED};
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

        // Alloue la texture vide (sera remplie par UploadAtlas)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                     atlasW, atlasH, 0,
                     GL_RED, GL_UNSIGNED_BYTE, nullptr);

        // Compile le shader de texte
        shader = CompileTextShader();

        // VAO/VBO pour les quads de glyphes
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        // layout : vec2 pos, vec2 uv  (6 sommets par quad, triangle strip)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
    }

    // ── Upload atlas si modifié ────────────────────────────────────────────

    void UploadAtlas(nkentseu::NkTextureAtlas& atlas) {
        if (!atlas.dirty) return;
        glBindTexture(GL_TEXTURE_2D, textureId);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        atlas.bitmap.width, atlas.bitmap.height,
                        GL_RED, GL_UNSIGNED_BYTE,
                        atlas.bitmap.pixels);
        atlas.dirty = false;
    }

    // ── Rendu d'un NkGlyphRun ──────────────────────────────────────────────

    void DrawRun(
        const nkentseu::NkGlyphRun& run,
        nkentseu::NkFontFace*       face,
        float x, float y,           // position en pixels écran
        float r, float g, float b,  // couleur du texte
        float screenW, float screenH)
    {
        // Construit les quads dans un buffer temporaire
        // 6 sommets × 4 floats × max_glyphs
        std::vector<float> verts;
        verts.reserve(run.numGlyphs * 6 * 4);

        float curX = x;
        for (uint32_t i = 0; i < run.numGlyphs; ++i) {
            const nkentseu::NkGlyphInfo& gi = run[i];
            nkentseu::NkGlyph glyph;
            if (!face->GetGlyphById(gi.glyphId, glyph) || glyph.isEmpty) {
                curX += gi.xAdvance.ToFloat();
                continue;
            }
            if (!glyph.inAtlas) { curX += gi.xAdvance.ToFloat(); continue; }

            const float px = curX + gi.xOffset.ToFloat() + glyph.metrics.bearingX;
            const float py = y    + gi.yOffset.ToFloat() - glyph.metrics.bearingY;
            const float pw = static_cast<float>(glyph.metrics.width);
            const float ph = static_cast<float>(glyph.metrics.height);

            // Coordonnées NDC
            auto toNDC_X = [&](float v){ return  2.f * v / screenW - 1.f; };
            auto toNDC_Y = [&](float v){ return -2.f * v / screenH + 1.f; };

            const float x0 = toNDC_X(px),      y0 = toNDC_Y(py);
            const float x1 = toNDC_X(px + pw),  y1 = toNDC_Y(py + ph);
            const float u0 = glyph.uv.u0, v0 = glyph.uv.v0;
            const float u1 = glyph.uv.u1, v1 = glyph.uv.v1;

            // Triangle 1
            verts.insert(verts.end(), {x0,y0,u0,v0, x1,y0,u1,v0, x0,y1,u0,v1});
            // Triangle 2
            verts.insert(verts.end(), {x1,y0,u1,v0, x1,y1,u1,v1, x0,y1,u0,v1});

            curX += gi.xAdvance.ToFloat();
        }
        if (verts.empty()) return;

        // Upload + draw
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(shader);
        glUniform4f(glGetUniformLocation(shader, "uColor"), r, g, b, 1.f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glUniform1i(glGetUniformLocation(shader, "uAtlas"), 0);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     verts.size() * sizeof(float),
                     verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 4));
    }

private:
    GLuint CompileTextShader() {
        const char* vert = R"(
            #version 330 core
            layout(location=0) in vec2 aPos;
            layout(location=1) in vec2 aUV;
            out vec2 vUV;
            void main() { gl_Position = vec4(aPos, 0, 1); vUV = aUV; }
        )";
        const char* frag = R"(
            #version 330 core
            in vec2 vUV;
            out vec4 fragColor;
            uniform sampler2D uAtlas;
            uniform vec4 uColor;
            void main() {
                // Gray8 swizzlé → .r contient la couverture du glyphe
                float alpha = texture(uAtlas, vUV).r;
                fragColor = vec4(uColor.rgb, uColor.a * alpha);
            }
        )";
        // Compilation standard OpenGL — omise pour la concision
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vert, nullptr); glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &frag, nullptr); glCompileShader(fs);
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vs); glAttachShader(prog, fs);
        glLinkProgram(prog);
        glDeleteShader(vs); glDeleteShader(fs);
        return prog;
    }
};

// ── Usage dans la game loop ──────────────────────────────────────────────────
void GameLoop() {
    nkentseu::NkFontLibrary lib;
    lib.Init(8*1024*1024, 2*1024*1024, 1024, 1024);

    // Charge la police
    auto* face = lib.LoadFont(ttfData, ttfSize, 18);
    face->PreloadASCII();

    NkFontGL gl;
    gl.Init(1024, 1024);
    gl.UploadAtlas(*lib.GetAtlas()); // upload initial

    while (running) {
        lib.BeginFrame(); // reset scratch arena

        // Si de nouveaux glyphes ont été rastérisés ce frame
        gl.UploadAtlas(*lib.GetAtlas());

        nkentseu::NkGlyphRun run;
        face->ShapeText("Bonjour monde!", run);
        gl.DrawRun(run, face, 50, 100, 1,1,1, 1920, 1080);
    }
}
```

### Vulkan — les points clés

```cpp
// NkFontVK.h — points d'intégration Vulkan
struct NkFontVK {

    void CreateAtlasImage(VkDevice device, VmaAllocator allocator,
                          int width, int height)
    {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType   = VK_IMAGE_TYPE_2D;
        imgInfo.format      = VK_FORMAT_R8_UNORM;   // Gray8 natif
        imgInfo.extent      = {(uint32_t)width, (uint32_t)height, 1};
        imgInfo.mipLevels   = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_SAMPLED_BIT;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        vmaCreateImage(allocator, &imgInfo, &allocInfo, &mAtlasImage, &mAlloc, nullptr);

        // View R8 → le shader lit .r
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image            = mAtlasImage;
        viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format           = VK_FORMAT_R8_UNORM;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0,1,0,1};
        vkCreateImageView(device, &viewInfo, nullptr, &mAtlasView);
    }

    void UploadAtlas(VkCommandBuffer cmd, VkDevice device,
                     VmaAllocator allocator,
                     nkentseu::NkTextureAtlas& atlas)
    {
        if (!atlas.dirty) return;
        const size_t size = (size_t)atlas.bitmap.width * atlas.bitmap.height;

        // Staging buffer
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size  = size;
        bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VmaAllocationCreateInfo stagingAlloc{};
        stagingAlloc.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        VkBuffer staging; VmaAllocation stagingAllocation;
        vmaCreateBuffer(allocator, &bufInfo, &stagingAlloc,
                        &staging, &stagingAllocation, nullptr);

        void* mapped;
        vmaMapMemory(allocator, stagingAllocation, &mapped);
        memcpy(mapped, atlas.bitmap.pixels, size);
        vmaUnmapMemory(allocator, stagingAllocation);

        // Transition + copy + transition
        TransitionImage(cmd, mAtlasImage,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        region.imageExtent = {(uint32_t)atlas.bitmap.width,
                              (uint32_t)atlas.bitmap.height, 1};
        vkCmdCopyBufferToImage(cmd, staging, mAtlasImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        TransitionImage(cmd, mAtlasImage,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        atlas.dirty = false;
    }

    // Fragment shader GLSL (VK_FORMAT_R8_UNORM)
    // layout(set=0,binding=0) uniform sampler2D uAtlas;
    // layout(push_constant) uniform PC { vec4 color; } pc;
    // void main() {
    //     float a = texture(uAtlas, inUV).r;
    //     outColor = vec4(pc.color.rgb, pc.color.a * a);
    // }

private:
    VkImage     mAtlasImage = VK_NULL_HANDLE;
    VkImageView mAtlasView  = VK_NULL_HANDLE;
    VmaAllocation mAlloc    = {};
    void TransitionImage(VkCommandBuffer cmd, VkImage img,
                         VkImageLayout from, VkImageLayout to) { /* ... */ }
};
```

### Direct3D 11 — les points clés

```cpp
void NkFontD3D11::CreateAtlasTexture(ID3D11Device* device, int w, int h) {
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width     = w; desc.Height = h;
    desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format    = DXGI_FORMAT_R8_UNORM;   // Gray8 natif
    desc.SampleDesc= {1,0};
    desc.Usage     = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    device->CreateTexture2D(&desc, nullptr, &mAtlasTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format              = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(mAtlasTexture, &srvDesc, &mAtlasSRV);
}

void NkFontD3D11::UploadAtlas(ID3D11DeviceContext* ctx,
                               nkentseu::NkTextureAtlas& atlas) {
    if (!atlas.dirty) return;
    ctx->UpdateSubresource(mAtlasTexture, 0, nullptr,
                           atlas.bitmap.pixels,
                           atlas.bitmap.stride,   // row pitch
                           0);
    atlas.dirty = false;
}

// Pixel shader HLSL :
// Texture2D    gAtlas  : register(t0);
// SamplerState gSampler: register(s0);
// cbuffer CB            : register(b0) { float4 gColor; };
// float4 PS(float2 uv : TEXCOORD) : SV_Target {
//     float a = gAtlas.Sample(gSampler, uv).r;
//     return float4(gColor.rgb, gColor.a * a);
// }
```

---

## 2. Plan de la bibliothèque NKImage (équivalent stb\_image)

Voici l'architecture complète avant de coder :La bibliothèque s'appellera **NKImage**. Voici la liste complète des modules, l'API publique, et on démarre le code immédiatement après confirmation.

**Formats supportés en lecture + écriture :**


| Format              | Lecture | Écriture | Notes                                                       |
| ------------------- | ------- | --------- | ----------------------------------------------------------- |
| PNG                 | oui     | oui       | Deflate from scratch (réutilise NkInflate de NkWOFFParser) |
| JPEG                | oui     | oui       | DCT + Huffman from scratch                                  |
| BMP                 | oui     | oui       | DIB v3/v4/v5, RLE4/RLE8                                     |
| TGA                 | oui     | oui       | Raw + RLE, alpha                                            |
| HDR (Radiance RGBE) | oui     | oui       | Float32 RGBA                                                |
| PPM / PGM / PBM     | oui     | oui       | ASCII et binaire                                            |
| QOI                 | oui     | oui       | Format moderne ultra-simple                                 |
| GIF                 | oui     | non       | LZW, frames multiples                                       |
| ICO/CUR             | oui     | non       | Multi-résolution Windows                                   |

**API publique prévue — identique en esprit à stb\_image :**

```cpp
// Chargement — détection auto du format
NkImage* NkImage::Load(const char* path, int desiredChannels = 0);
NkImage* NkImage::LoadFromMemory(const uint8* data, usize size, int desiredChannels = 0);

// Sauvegarde
bool NkImage::SavePNG (const char* path) const;
bool NkImage::SaveJPEG(const char* path, int quality = 90) const;
bool NkImage::SaveBMP (const char* path) const;
bool NkImage::SaveTGA (const char* path) const;
bool NkImage::SaveHDR (const char* path) const;
bool NkImage::Save    (const char* path) const; // détecte l'extension

// Manipulation
NkImage* NkImage::Resize(int newW, int newH, NkResizeFilter filter) const;
void     NkImage::FlipVertical()   noexcept;
void     NkImage::FlipHorizontal() noexcept;
void     NkImage::Premultiply()    noexcept; // alpha prémultiplié pour GPU
NkImage* NkImage::Convert(NkPixelFormat fmt) const; // RGBA8↔RGB8↔Gray8↔RGBA32F

// Accès
uint8*  NkImage::Pixels()    noexcept;
int     NkImage::Width()     const noexcept;
int     NkImage::Height()    const noexcept;
int     NkImage::Channels()  const noexcept; // 1,2,3,4
usize   NkImage::Stride()    const noexcept;
bool    NkImage::IsHDR()     const noexcept; // float32 vs uint8

void    NkImage::Free() noexcept; // ou RAII via NkImagePtr unique_ptr-like
```

**Dépendances :** zéro. L'inflate de `NkWOFFParser` sera extrait dans `NkDeflate` partagé entre NKFont et NKImage. Le reste est from scratch.

**Structure des fichiers :**

```
NKImage/
  include/NKImage/
    NkImageExport.h
    NkImage.h           ← API publique + NkImageBuffer
    NkPNGCodec.h        ← PNG lecture + écriture
    NkJPEGCodec.h       ← JPEG DCT from scratch
    NkBMPCodec.h        ← BMP DIB
    NkTGACodec.h        ← TGA raw/RLE
    NkHDRCodec.h        ← Radiance RGBE
    NkPPMCodec.h        ← PPM/PGM/PBM
    NkQOICodec.h        ← QOI (le plus simple)
    NkGIFCodec.h        ← GIF lecture seule (LZW)
    NkICOCodec.h        ← ICO/CUR lecture seule
    NkImageResize.h     ← Bilinear, Bicubic, Lanczos
    NkDeflate.h         ← Inflate/Deflate partagé avec NKFont
  src/
    NkImage.cpp
    NkPNGCodec.cpp
    NkJPEGCodec.cpp
    ... (un .cpp par codec)
```

On lance la construction ? On peut aussi paralléliser les deux questions — NKImage est un projet distinct de NKFont, totalement indépendant.
