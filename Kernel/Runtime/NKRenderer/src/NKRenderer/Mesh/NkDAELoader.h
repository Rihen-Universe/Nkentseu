#pragma once
// =============================================================================
// NkDAELoader.h — NKRenderer (Mesh/)
//
// Loader COLLADA (.dae) from-scratch, zero-STL. XML.
//   - Mini DOM XML interne (elements/attributs/texte/enfants).
//   - library_geometries/geometry/mesh : sources (float_array + accessor stride),
//     vertices (POSITION), triangles ET polylist (vcount -> fan). Inputs
//     VERTEX/NORMAL/TEXCOORD avec offsets interleaves dans <p>.
//   - asset/up_axis : Z_UP -> conversion vers Y_UP.
//   - 1 sous-mesh par <triangles>/<polylist>.
//
// NON supporte : materiaux/textures, skin/anim, library_visual_scenes
// (transforms de noeuds). Sortie : NkGLTFMeshData.
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {
        bool LoadDAE(const NkString& path, NkGLTFMeshData& out);
    }
}
