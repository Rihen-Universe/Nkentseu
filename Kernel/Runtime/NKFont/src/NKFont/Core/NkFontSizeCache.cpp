// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontSizeCache.cpp
// DESCRIPTION: Implémentation du cache de tailles de fontes.
// -----------------------------------------------------------------------------

// ============================================================
// INCLUDES
// ============================================================

#include "NkFontSizeCache.h"
#include "NKFont/NkFont.h"
#include <string.h>
#include <math.h>

namespace nkentseu {

    // ============================================================
    // Init
    // ============================================================

    void NkFontSizeCache::Init(NkFontAtlas* atlas,
                               const char* fontPath,
                               const nkft_float32* preloadSizes,
                               nkft_int32 preloadCount) {
        mAtlas = atlas;
        if (fontPath) {
            strncpy(mFontPath, fontPath, sizeof(mFontPath) - 1);
            mFontPath[sizeof(mFontPath) - 1] = '\0';
        }

        mCount          = 0;
        mNeedsRebuild   = false;
        mNeedsGpuUpload = false;
        mProfileReady   = false;

        // Analyse le profil (bitmap vs vectoriel)
        if (fontPath) {
            mProfile      = NkFontDetector::AnalyzeFile(fontPath);
            mProfileReady = true;
        }

        // Préchauffe les tailles demandées
        if (preloadSizes && preloadCount > 0 && atlas) {
            for (nkft_int32 i = 0; i < preloadCount && i < MAX_CACHED_SIZES; ++i) {
                GetFont(preloadSizes[i], 0);
            }
            BuildAtlasIfNeeded();
        }
    }

    // ============================================================
    // HasSize
    // ============================================================

    bool NkFontSizeCache::HasSize(nkft_float32 sizePx, nkft_int32* outIdx) const {
        for (nkft_int32 i = 0; i < mCount; ++i) {
            if (mEntries[i].isValid && fabsf(mEntries[i].sizePx - sizePx) < 0.25f) {
                if (outIdx) *outIdx = i;
                return true;
            }
        }
        return false;
    }

    // ============================================================
    // EvictLRU — supprime la moins récemment utilisée
    // ============================================================

    void NkFontSizeCache::EvictLRU() {
        if (mCount == 0) return;

        nkft_int32 oldest = 0;
        for (nkft_int32 i = 1; i < mCount; ++i) {
            if (mEntries[i].lastUsedFrame < mEntries[oldest].lastUsedFrame) {
                oldest = i;
            }
        }

        // Décale les entrées
        for (nkft_int32 i = oldest; i < mCount - 1; ++i) {
            mEntries[i] = mEntries[i + 1];
        }
        mEntries[--mCount] = NkFontSizeEntry{};
        // Note : le rebuild complet sera nécessaire car on ne peut pas
        // retirer un font de l'atlas sans tout reconstruire.
        mNeedsRebuild = true;
    }

    // ============================================================
    // GetFont
    // ============================================================

    NkFont* NkFontSizeCache::GetFont(nkft_float32 sizePx, nkft_uint64 frameIdx) {
        if (!mAtlas || mFontPath[0] == '\0') return nullptr;

        // Snap de taille si police bitmap
        if (mProfileReady) {
            sizePx = NkFontDetector::SnapSize(mProfile, sizePx);
        }

        // Cherche dans le cache
        nkft_int32 idx = -1;
        if (HasSize(sizePx, &idx)) {
            mEntries[idx].lastUsedFrame = frameIdx;
            return mEntries[idx].font;
        }

        // Pas trouvé → ajoute
        if (mCount >= MAX_CACHED_SIZES) {
            EvictLRU();
        }

        // Construit le config optimal
        NkFontConfig cfg;
        if (mProfileReady) {
            NkFontDetector::ApplyOptimalConfig(mProfile, sizePx, &cfg);
        } else {
            cfg.oversampleH = 2;
            cfg.oversampleV = 1;
            cfg.sizePixels  = sizePx;
        }
        cfg.glyphRanges = NkFontAtlas::GetGlyphRangesDefault();

        // Ajoute à l'atlas (ne déclenche pas le rebuild, juste enregistre)
        NkFont* font = mAtlas->AddFontFromFile(mFontPath, sizePx, &cfg);
        if (!font) return nullptr;

        NkFontSizeEntry& entry = mEntries[mCount++];
        strncpy(entry.fontPath, mFontPath, sizeof(entry.fontPath) - 1);
        entry.fontPath[sizeof(entry.fontPath) - 1] = '\0';
        entry.sizePx        = sizePx;
        entry.font          = font;
        entry.profile       = mProfile;
        entry.lastUsedFrame = frameIdx;
        entry.isValid       = true;

        mNeedsRebuild = true;
        return nullptr; // disponible après BuildAtlasIfNeeded()
    }

    // ============================================================
    // BuildAtlasIfNeeded
    // ============================================================

    bool NkFontSizeCache::BuildAtlasIfNeeded() {
        if (!mNeedsRebuild || !mAtlas) return false;

        bool ok = mAtlas->Build();
        mNeedsRebuild   = false;
        mNeedsGpuUpload = ok;

        // Met à jour les pointeurs de fontes (Build() recrée les NkFont)
        // Note : les pointeurs dans mEntries[].font restent valides car
        // NkFontAtlas::fonts[] garde les mêmes pointeurs NkFont*.
        // Seul l'atlas GPU change.

        return ok;
    }

} // namespace nkentseu

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================