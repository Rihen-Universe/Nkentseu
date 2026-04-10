#pragma once

#include "NKFont/NKFont.h"
#include "NKUI/NkUIFont.h"

namespace nkentseu {
    namespace nkui {

        // Forward declaration
        struct NkUIFontAtlas;

        // Callback upload GPU - backend agnostic
        using NkGPUUploadFunc = bool(*)(void* device, uint32 texId, const uint8* data, int32 w, int32 h);

        class NKUI_API NkUIFontLoader {
            public:
                NkUIFontLoader() = default;
                ~NkUIFontLoader() { Destroy(); }

                bool Init(const NkUIFontConfig& config = NkUIFontConfig(),
                        usize permanentBytes = 8 * 1024 * 1024,
                        usize scratchBytes = 2 * 1024 * 1024,
                        int32 atlasWidth = 512,
                        int32 atlasHeight = 512,
                        void* gpuDevice = nullptr,
                        NkGPUUploadFunc uploadFunc = nullptr) noexcept;
                
                void Destroy() noexcept;
                
                uint32 LoadFontFromFile(const char* filepath, float32 fontSize, bool preloadASCII = true) noexcept;
                uint32 LoadSystemFont(const char* fontName, float32 fontSize, bool preloadASCII = true) noexcept;
                
                NkUIFont* GetFont(uint32 idx) noexcept;
                NkUIFont* DefaultFont() noexcept;
                uint32 GetFontCount() const noexcept { return mNumFonts; }
                
                void UploadAtlas() noexcept;
                NkUIFontAtlas* GetAtlas() noexcept { return mAtlas; }
                NkFontLibrary* GetFontLibrary() noexcept { return &mFontLib; }

                void SetGPUDevice(void* device) noexcept { mGPUDevice = device; }
                void SetGPUUploadFunc(NkGPUUploadFunc func) noexcept { mGPUUploadFunc = func; }

                void* GetGPUDevice() const noexcept { return mGPUDevice; }
                NkGPUUploadFunc GetGPUUploadFunc() const noexcept { return mGPUUploadFunc; }

                uint8* GetRGBA8Atlas() const {
                    if (!mAtlas) return nullptr;
                    
                    const int32 size = NkUIFontAtlas::ATLAS_W * NkUIFontAtlas::ATLAS_H;
                    uint8* rgbaData = new uint8[size * 4];
                    
                    for (int32 i = 0; i < size; ++i) {
                        uint8 alpha = mAtlas->pixels[i];
                        rgbaData[i * 4 + 0] = 255;  // R
                        rgbaData[i * 4 + 1] = 255;  // G
                        rgbaData[i * 4 + 2] = 255;  // B
                        rgbaData[i * 4 + 3] = alpha; // A
                    }
                    
                    return rgbaData;
                }
                
                // Getter pour l'atlas original
                const uint8* GetAtlasPixels() const { return mAtlas ? mAtlas->pixels : nullptr; }
                
                // Indique si l'atlas doit être uploadé en RGBA8
                bool RequiresRGBAConversion() const { return uploadRGBA8; }
            private:
                struct LoadedFont {
                    NkUIFont          uiFont;
                    NkFontFace*       nkFace = nullptr;
                    // NkFTFontFace*     ftFace = nullptr;
                    NkUIFontMetrics   metrics;
                    char              name[64];
                    bool              isValid = false;
                };
                
                static constexpr int32 MAX_LOADED_FONTS = 16;
                
                NkFontLibrary      mFontLib;
                NkUIFontAtlas*     mAtlas = nullptr;
                LoadedFont         mFonts[MAX_LOADED_FONTS];
                uint32             mNumFonts = 0;
                NkUIFontConfig     mConfig;
                bool               mInitialized = false;
                
                // opaque pointer, backend-owned
                void*              mGPUDevice = nullptr;
                NkGPUUploadFunc    mGPUUploadFunc = nullptr;

                bool uploadRGBA8 = true;
                
                bool CreateAtlas(int32 width, int32 height) noexcept;
                
                void ComputeMetricsFromNKFont(NkFontFace* face, NkUIFontMetrics& outMetrics) noexcept;
                void PreloadASCIIFromNKFont(NkFontFace* face) noexcept;
                bool RasterizeGlyphToAtlas(NkFontFace* face, uint32 codepoint, NkUIGlyph& outGlyph) noexcept;
        };

    } // namespace nkui
} // namespace nkentseu
