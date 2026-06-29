#pragma once
// =============================================================================
// NkSTLLoader.h — NKRenderer (Mesh/)
//
// Loader STL (stereolithography) from-scratch, zero-STL.
//   - Binaire : header 80o + uint32 nbTriangles + N x (normale 3f + 3 sommets
//     3f + attribut u16). Detection par taille (84 + 50*N).
//   - ASCII : solid/facet normal/outer loop/vertex/endloop/endfacet/endsolid.
//   - Pas d'UV ni de materiau (le format n'en a pas). Normale de face fournie ;
//     recalculee si nulle. 3 sommets par triangle (pas de partage), indices
//     sequentiels.
//
// Sortie : NkGLTFMeshData (un seul sous-mesh, materiau -1).
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {
        bool LoadSTL(const NkString& path, NkGLTFMeshData& out);
    }
}
