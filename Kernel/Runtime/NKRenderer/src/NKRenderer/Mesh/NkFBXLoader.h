#pragma once
// =============================================================================
// NkFBXLoader.h — NKRenderer (Mesh/)
//
// Loader FBX (Kaydara) from-scratch, zero-STL. BINAIRE + ASCII. Geometrie.
//   - ASCII : tokenizer texte -> meme arbre FbxNode (Name: props { enfants },
//     arrays *N { a: ... }).
//   - Header "Kaydara FBX Binary", versions <7500 (offsets 32-bit) et >=7500
//     (offsets 64-bit). Arbre de noeuds + proprietes typees (Y/C/I/F/D/L/S/R +
//     arrays f/d/l/i/b). Arrays compresses (encoding=1) -> inflate NkDeflate.
//   - Geometrie : Objects/Geometry -> Vertices (control points),
//     PolygonVertexIndex (polygones, fin = index negatif, triangulation fan),
//     LayerElementNormal/Normals, LayerElementUV/UV (+UVIndex).
//   - 1 sous-mesh par Geometry. Normales recalculees si absentes.
//
// NON supporte : FBX ASCII, materiaux/textures, skinning/anim, transforms de
// noeuds Model (les Geometry sont en espace local -> orientation = celle bakee
// dans les sommets). UpAxis lu depuis GlobalSettings (Z-up -> Y-up auto). Les
// exports 3ds Max ont parfois une geometrie Z-up mal etiquetee Y-up : variable
// d'env NK_FBX_ZUP pour forcer la conversion. Sortie : NkGLTFMeshData.
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {
        bool LoadFBX(const NkString& path, NkGLTFMeshData& out);
    }
}
