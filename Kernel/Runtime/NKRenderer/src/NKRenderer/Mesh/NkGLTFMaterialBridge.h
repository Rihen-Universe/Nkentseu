#pragma once
// =============================================================================
// NkGLTFMaterialBridge.h  — NKRenderer v5.0  (Mesh/)
//
// Pont entre les materiaux glTF decodes (NkGLTFMeshData::materials/images) et
// le systeme de materiaux du renderer (NkMaterialSystem). Produit un
// NkMatInstHandle par materiau glTF :
//   1. upload chaque image decodee (CPU RGBA8) en NkTexHandle via NkTextureLibrary
//   2. cree une NkMaterialInstance (template DefaultPBR)
//   3. applique factors PBR (albedo/metallic/roughness/emissive) + maps
//
// Zero-STL : conteneurs NKContainers, allocation via NkMaterialSystem.
// N'altere PAS NkMaterialSystem (API publique uniquement).
//
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Mesh/NkGLTFLoader.h"
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace renderer {

        class NkMaterialSystem;
        class NkTextureLibrary;

        // Resultat du pont : un NkMatInstHandle par materiau glTF (meme ordre
        // que NkGLTFMeshData::materials). Si un materiau echoue, son handle est
        // invalide (IsValid()==false) et l'appelant retombe sur le tint/PBR
        // par-drawcall.
        struct NkGLTFMaterialSet {
            NkVector<NkMatInstHandle> instances;   // 1 par materiau glTF
            NkVector<NkTexHandle>     textures;     // 1 par image glTF (debug/lifetime)

            // Retourne le handle d'instance pour l'index materiau glTF donne
            // (-1 ou hors borne -> handle invalide).
            NkMatInstHandle InstanceForMaterial(int32 materialIndex) const {
                if (materialIndex < 0 || (uint32)materialIndex >= instances.Size())
                    return NkMatInstHandle::Null();
                return instances[(uint32)materialIndex];
            }
        };

        // Construit les NkMaterialInstance + textures depuis les donnees glTF.
        // matSys / texLib doivent etre valides (issus du renderer). `data` reste
        // const : on ne lit que materials/images.
        // Retourne true si au moins un materiau a ete cree (false si aucun
        // materiau dans le glTF — l'appelant utilise alors le PBR par-drawcall).
        bool BuildGLTFMaterials(const NkGLTFMeshData& data,
                                NkMaterialSystem* matSys,
                                NkTextureLibrary* texLib,
                                NkGLTFMaterialSet& out);

    } // namespace renderer
} // namespace nkentseu
