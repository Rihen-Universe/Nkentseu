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
// Materiaux Phong (Properties70 : DiffuseColor/DiffuseFactor/
// TransparencyFactor/EmissiveColor/EmissiveFactor/ShininessExponent) +
// textures externes connectees (DiffuseColor/NormalMap ou Bump/EmissiveColor,
// resolues via Objects/Connections : Geometry -> Model proprietaire -> 1er
// Material connecte ; RelativeFilename/FileName resolus par rapport au
// dossier du .fbx).
// Scene-graph (2026-08-17, etape (a) du chantier FBX) : out.nodes est rempli
// depuis les Model (nom, TRS local — euler degres + RotationOrder +
// Pre/PostRotation -> quaternion, R = Rpre*Rlcl*Rpost^-1 — hierarchie via
// connexions OO Model->Model). Ces transforms ne sont PAS appliquees a la
// geometrie STATIQUE (sommets en espace local de leur Geometry, comme avant).
// Skinning (etape (b)) : TOUS les Deformer Skin -> UN squelette (skinJoints
// en parcours prefixe des LimbNode, feuilles sans Cluster comprises ;
// inverseBind = TransformLink^-1 ; sommets skinnes ramenes dans l'espace du
// squelette par TransformLink*Transform = monde du mesh au bind).
// Animations (etape (c)) : chaque AnimationStack non vide -> une animation
// (canaux T/R/S echantillonnes sur l'union des cles, rotation complete).
// Temoins : CesiumMan.fbx (export Blender) ET Mixamo natif X Bot / Y Bot
// (banc NkFBXParityDemo, 2026-08-17 soir).
// NON supporte : textures FBX EMBARQUEES (Video binaire), InheritType,
// pivots/offsets de rotation et d'echelle.
// UpAxis lu depuis GlobalSettings (Z-up -> Y-up auto). Les
// exports 3ds Max ont parfois une geometrie Z-up mal etiquetee Y-up : variable
// d'env NK_FBX_ZUP pour forcer la conversion. Sortie : NkGLTFMeshData.
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
	namespace renderer {
		bool LoadFBX(const NkString &path, NkGLTFMeshData &out);
	}
} // namespace nkentseu
