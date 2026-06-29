#pragma once
// =============================================================================
// NkOBJLoader.h — NKRenderer (Mesh/)
//
// Loader Wavefront OBJ (+ .mtl) from-scratch, zero-STL / NKMemory.
//
// Supporte :
//   - v / vt / vn (positions / texcoords / normales), indices 1-based ET
//     negatifs (relatifs). Faces triangles ET polygones (triangules en fan).
//   - Formes de corner : v, v/vt, v//vn, v/vt/vn.
//   - usemtl -> un sous-mesh par materiau ; mtllib -> parse le .mtl associe.
//   - .mtl : Kd (baseColor), d/Tr (alpha), Ns (-> roughness), Ke (emissive),
//     map_Kd (baseColor), map_Bump/bump/norm (normal), map_Ke (emissive).
//     Textures chargees via NKImage (relatif au dossier du .obj/.mtl).
//   - Deduplication des vertices (vi/ti/ni) via NkHashMap -> mesh indexe.
//   - Normales lissees calculees si aucune vn dans le fichier.
//
// Sortie : NkGLTFMeshData (format CPU commun). Champs skinning/nodes vides.
// V flippe (1-v) : OBJ a une origine UV bas-gauche, nos textures haut-gauche.
//
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    namespace renderer {

        // Charge un fichier .obj dans `out`. Retourne true si >=1 triangle charge.
        bool LoadOBJ(const NkString& path, NkGLTFMeshData& out);

    } // namespace renderer
} // namespace nkentseu
