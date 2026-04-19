
#pragma once
// =============================================================================
// Nkentseu/IO/NkOBJIO.h — Import/Export OBJ + MTL
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "Nkentseu/Modeling/NkEditableMesh.h"

namespace nkentseu {

    struct NkOBJImportOptions {
        bool   triangulate  = false;
        float32 scaleFactor = 1.f;
        bool   flipY        = false;
        bool   generateNormals = true;
    };

    struct NkOBJExportOptions {
        bool   exportNormals = true;
        bool   exportUVs     = true;
        bool   exportMTL     = true;
        float32 precision    = 6.f;
    };

    class NkOBJIO {
    public:
        [[nodiscard]] static NkEditableMesh Import(const char* path,
            const NkOBJImportOptions& opts = {}) noexcept;
        [[nodiscard]] static bool Export(const NkEditableMesh& mesh, const char* path,
            const NkOBJExportOptions& opts = {}) noexcept;
        [[nodiscard]] static const NkString& GetLastError() noexcept;
    };
}
