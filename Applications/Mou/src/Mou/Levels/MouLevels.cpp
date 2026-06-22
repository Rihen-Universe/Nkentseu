// =============================================================================
// Levels/MouLevels.cpp
// =============================================================================
#include "Levels/MouLevels.h"
#include "Core/MouConfig.h"
#include "NKFileSystem/NkFile.h"
#include "NKSerialization/JSON/NkJSONReader.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringView.h"
#include <cstdio>
#include <cstring>

namespace mou {

    using namespace nkentseu;

    int32 MouLevels::Clamp(int32 i) const noexcept {
        const int32 n = (int32)mRecs.Size();
        if (n <= 0) return 0;
        if (i < 0) return 0;
        if (i >= n) return n - 1;
        return i;
    }

    int32 MouLevels::GetInt(int32 level, const char* key, int32 def) const noexcept {
        if (mRecs.Size() == 0 || !key) return def;
        nk_int64 v = 0;
        if (mRecs[(usize)Clamp(level)].GetInt64(key, v)) return (int32)v;
        return def;
    }

    bool MouLevels::GetStr(int32 level, const char* key, char* out, usize cap) const noexcept {
        if (mRecs.Size() == 0 || !key || !out || cap == 0) return false;
        NkString s;
        if (mRecs[(usize)Clamp(level)].GetString(key, s)) {
            std::strncpy(out, s.CStr(), cap - 1);
            out[cap - 1] = 0;
            return true;
        }
        return false;
    }

    int32 MouLevels::Load(const char* name) noexcept {
        mRecs.Clear();
        if (!name) return 0;

        char full[512];
#if defined(__ANDROID__) || defined(NKENTSEU_PLATFORM_ANDROID)
        std::snprintf(full, sizeof(full), "levels/%s", name);
#else
        std::snprintf(full, sizeof(full), "assets/levels/%s", name);
#endif

        NkFile file;
        if (!file.Open(full, NkFileMode::NK_READ)) {
            MOU_LOG_WARNF("[MouLevels] introuvable: %s (fallback code en dur)", full);
            return 0;
        }
        const usize sz = (usize)file.Size();
        char buf[16384];
        const usize toRead = (sz < sizeof(buf) - 1) ? sz : (sizeof(buf) - 1);
        const usize rd = file.Read(buf, toRead);
        file.Close();
        buf[rd] = 0;

        NkArchive root;
        NkString err;
        // IMPORTANT : NkStringView(buf, rd) -> longueur RÉELLE (pas le ctor tableau
        // qui prendrait sizeof(buf)-1 et ferait croire à du "trailing content").
        if (!NkJSONReader::ReadArchive(NkStringView(buf, rd), root, &err)) {
            MOU_LOG_WARNF("[MouLevels] %s : JSON invalide (%s)", full, err.CStr());
            return 0;
        }
        if (!root.GetObjectArray("levels", mRecs) || mRecs.Size() == 0) {
            MOU_LOG_WARNF("[MouLevels] %s : tableau \"levels\" absent ou vide", full);
            mRecs.Clear();
            return 0;
        }

        MOU_LOG_INFOF("[MouLevels] %s : %d niveaux charges", full, (int32)mRecs.Size());
        return (int32)mRecs.Size();
    }

}  // namespace mou
