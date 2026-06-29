#pragma once
// =============================================================================
// NkUSDALoader.h — NKRenderer (Mesh/)
//
// Loader USD ASCII (.usda / .usd texte) from-scratch, zero-STL.
//   - def Mesh "..." { points / faceVertexIndices / faceVertexCounts /
//     normals / primvars:st }. Polygones (counts) triangules en fan.
//   - Normales & UV : per-point (count==points) OU faceVarying (count==corners),
//     detecte par la taille. Sinon normales recalculees.
//   - upAxis (metadata header) : "Z" -> conversion vers Y.
//
// NON supporte : USDC/USDZ binaire (crate), references/payloads, transforms de
// Xform parents, materiaux. Sortie : NkGLTFMeshData.
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {
        bool LoadUSDA(const NkString& path, NkGLTFMeshData& out);
    }
}
