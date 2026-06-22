// =============================================================================
// Assets/MouAssets.cpp
// =============================================================================
#include "Assets/MouAssets.h"
#include "Core/MouConfig.h"
#include "NKImage/Codecs/SVG/NkSVGCodec.h"
#include "NKImage/Core/NkImage.h"
#include "NKCanvas/UI/NkUICanvasBackend.h"
#include <cstdio>
#include <cstring>

namespace mou {

    using namespace nkentseu;

    bool MouAssets::Init(renderer::NkUICanvasBackend* backend) noexcept {
        mBackend = backend;
        mNextTexId = 10000;
        return mBackend != nullptr;
    }

    uint32 MouAssets::LoadSvg(const char* svgName, int32 w, int32 h) noexcept {
        if (!svgName) return 0;
        char rel[256];
        std::snprintf(rel, sizeof(rel), "svg/%s", svgName);
        return LoadAsset(rel, w, h);
    }

    uint32 MouAssets::LoadAsset(const char* relPath, int32 w, int32 h) noexcept {
        if (!mBackend || !relPath) return 0;

        // Racine assets : "" sur Android (AAssetManager) / "assets/" sur desktop.
        char full[512];
#if defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_ANDROID)
        std::snprintf(full, sizeof(full), "%s", relPath);
#else
        std::snprintf(full, sizeof(full), "assets/%s", relPath);
#endif

        const char* dot = std::strrchr(relPath, '.');
        const bool isSvg = dot && (std::strcmp(dot, ".svg") == 0 || std::strcmp(dot, ".SVG") == 0);

        NkImage* img = nullptr;
        if (isSvg) {
            img = NkSVGCodec::DecodeFromFile(full, w, h);
        } else {
            img = NkImage::Alloc(1, 1, NkImagePixelFormat::NK_RGBA32);
            if (img && (!img->Load(full, 4) || !img->IsValid())) { img->Free(); img = nullptr; }
        }
        if (!img) {
            MOU_LOG_WARNF("[MouAssets] Asset introuvable ou invalide: %s", full);
            return 0;
        }

        const int32 iw = img->Width(), ih = img->Height();
        mLastW = iw; mLastH = ih;
        const uint32 id = mNextTexId++;
        const bool ok = mBackend->UploadTextureRGBA8(id, img->Pixels(), iw, ih);
        img->Free();
        if (!ok) {
            MOU_LOG_WARNF("[MouAssets] Upload texture echoue: %s", full);
            return 0;
        }
        MOU_LOG_INFOF("[MouAssets] %s -> texId %u (%dx%d)", full, id, iw, ih);
        return id;
    }

}  // namespace mou
