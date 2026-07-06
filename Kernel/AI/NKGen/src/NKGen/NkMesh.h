// =============================================================================
// NkMesh.h — maillage à partir de voxels + export OBJ (NKAI, Phase 6).
//
// Convertit une grille de voxels (champ scalaire d'occupation, ex. sortie d'un
// générateur 3D) en MAILLAGE de triangles : chaque voxel occupé émet ses faces,
// les faces internes (entre deux voxels occupés) étant supprimées (culling) pour
// ne garder que la surface. Résultat exportable en .OBJ, chargeable dans le
// moteur / tout visualiseur 3D. Namespace : nkentseu::ai::gen.
//
// (Maillage « cubique » façon voxel ; l'upgrade lissé = marching cubes, cf ROADMAP.)
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace ai {
        namespace gen {

            struct NkMesh {
                NkVector<float>  positions;   // 3 floats (x,y,z) par sommet
                NkVector<float>  normals;     // 3 floats par sommet (vide tant que non calculé)
                NkVector<uint32> triangles;   // 3 indices par triangle
                NkVector<uint32> quads;       // 4 indices par quad (maillage quad-dominant)

                uint32 VertexCount()   const { return positions.Size() / 3; }
                uint32 TriangleCount() const { return triangles.Size() / 3; }
                uint32 QuadCount()     const { return quads.Size() / 4; }
                bool   HasNormals()    const { return normals.Size() == positions.Size(); }
            };

            // Calcule des normales LISSES par sommet (somme pondérée par l'aire des
            // normales de faces adjacentes, puis normalisation). Indispensable pour
            // l'éclairage GPU. Traite triangles ET quads. Remplit `mesh.normals`.
            void ComputeNormals(NkMesh& mesh);

            // Remplit un buffer entrelacé [px,py,pz, nx,ny,nz] par sommet (layout
            // attendu par un renderer : positions + normales). Calcule les normales
            // si absentes. Pratique pour NkMeshSystem::Create / NkMeshDesc::Simple.
            void BuildInterleavedPN(const NkMesh& mesh, NkVector<float>& outVertices,
                                    NkVector<uint32>& outIndices);

            // Maillage surfacique CUBIQUE (proxy) d'une grille G×G×G. `iso` = seuil
            // d'occupation ; `cell` = taille d'un voxel dans l'espace monde.
            NkMesh VoxelsToMesh(const float* voxels, uint32 G,
                                float iso = 0.5f, float cell = 1.0f);

            // Surface LISSE (Naive Surface Nets) d'un champ scalaire échantillonné sur
            // une grille nx×ny×nz : place un sommet par cellule traversant l'iso-surface
            // (moyenne des intersections d'arêtes) et relie les cellules voisines en
            // quads (triangulés). « Inside » = champ > iso. Sortie quad-dominante,
            // adaptée comme base de retopologie. `cell` = pas de grille en unités monde.
            NkMesh SurfaceNets(const float* field, uint32 nx, uint32 ny, uint32 nz,
                               float iso = 0.5f, float cell = 1.0f);

            // Variante QUAD native de Surface Nets : émet des faces QUAD (remesh
            // quad-dominant uniforme) au lieu de triangles — sortie de retopologie
            // plus propre (dans `mesh.quads`). L'edge-flow feature-aligned reste l'étape
            // suivante (cf ROADMAP).
            NkMesh SurfaceNetsQuads(const float* field, uint32 nx, uint32 ny, uint32 nz,
                                    float iso = 0.5f, float cell = 1.0f);

            // Décimation par CLUSTERING de sommets (Rossignac-Borrel) : regroupe les
            // sommets par cellule d'une grille `gridResolution³` (représentant = moyenne),
            // remappe les triangles, supprime les dégénérés. Première passe de
            // retopologie (allègement + soudure). Plus grossier que QEM/quad field-aligned
            // (cf ROADMAP), mais simple et robuste.
            NkMesh DecimateClustering(const NkMesh& src, uint32 gridResolution);

            // Écrit le maillage au format Wavefront OBJ. true si OK.
            bool SaveMeshObj(const char* path, const NkMesh& mesh);

        } // namespace gen
    } // namespace ai
} // namespace nkentseu
