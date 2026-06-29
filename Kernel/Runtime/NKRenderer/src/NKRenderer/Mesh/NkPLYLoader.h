#pragma once
// =============================================================================
// NkPLYLoader.h — NKRenderer (Mesh/)
//
// Loader PLY (Stanford polygon) from-scratch, zero-STL.
//   - format ascii 1.0 ET binary_little_endian 1.0.
//   - element vertex : x/y/z (pos), nx/ny/nz (normales), red/green/blue/alpha
//     (couleur, uchar 0-255 ou float 0-1), s/t | u/v | texture_u/texture_v (UV).
//   - element face : property list (vertex_indices) -> triangulation en fan.
//   - Normales recalculees si absentes.
//
// Sortie : NkGLTFMeshData (un sous-mesh, materiau -1). big_endian non supporte.
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {
        bool LoadPLY(const NkString& path, NkGLTFMeshData& out);
    }
}
