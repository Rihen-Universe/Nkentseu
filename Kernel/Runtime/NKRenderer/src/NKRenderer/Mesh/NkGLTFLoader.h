#pragma once
// =============================================================================
// NkGLTFLoader.h  — NKRenderer v4.0  (Mesh/)
//
// Loader glTF 2.0 from-scratch (MVP geometrie) zero-STL.
//
// Supporte :
//   - .gltf (JSON + buffers externes .bin + data URI base64)
//   - .glb  (conteneur binaire : header + chunk JSON + chunk BIN)
//   - Attributs : POSITION (obligatoire), NORMAL (sinon calcule par-face),
//     TANGENT, TEXCOORD_0, TEXCOORD_1, COLOR_0 (sinon blanc).
//   - Indices : u8/u16/u32 -> promus en uint32. Sinon indices sequentiels.
//   - Sortie : NkVector<NkVertex3D> entrelaces (layout Default3D) +
//     NkVector<uint32> + un NkSubMesh par primitive + NkAABB global/par-submesh.
//
// DIFFERE (non implemente — voir ROADMAP) :
//   - Materiaux / textures glTF (NkMaterialSystem est en cours de reecriture).
//     subMesh.material reste invalide ; importMaterials est ignore pour le MVP.
//   - Skinning (JOINTS_0 / WEIGHTS_0), animations, morph targets.
//   - Cameras / lights / KHR extensions / sparse accessors.
//   - Transformations de noeuds (scene graph) : les primitives sont chargees
//     en espace local de leur mesh, sans appliquer les matrices de node.
//
// Auteur : Rihen
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKImage/Core/NkImage.h"

namespace nkentseu {
    namespace renderer {

        // ── Materiau PBR glTF (pbrMetallicRoughness + extensions de base) ─────
        // Extrait depuis la section glTF `materials[]`. Les *Image* sont des
        // index dans NkGLTFMeshData::images (-1 = pas de texture pour ce slot).
        // Les facteurs/couleurs suivent la spec glTF 2.0 (lineaire, sauf
        // baseColor qui est sRGB cote texture).
        struct NkGLTFMaterial {
            NkVec4f baseColorFactor      = {1.f, 1.f, 1.f, 1.f};
            float32 metallicFactor       = 1.f;
            float32 roughnessFactor      = 1.f;
            NkVec3f emissiveFactor       = {0.f, 0.f, 0.f};
            float32 emissiveStrength     = 1.f;   // KHR_materials_emissive_strength
            float32 normalScale          = 1.f;   // normalTexture.scale
            float32 occlusionStrength    = 1.f;   // occlusionTexture.strength
            float32 alphaCutoff          = 0.5f;
            int32   alphaMode            = 0;      // 0=OPAQUE 1=MASK 2=BLEND
            bool    doubleSided          = false;
            int32   baseColorImage         = -1;   // index dans images[]
            int32   metallicRoughnessImage = -1;   // glTF : G=rough, B=metal (ORM-like)
            int32   normalImage            = -1;
            int32   emissiveImage          = -1;
            int32   occlusionImage         = -1;   // R = AO
            NkString name;
        };

        // ── Image decodee referencee par les materiaux glTF ──────────────────
        // Source possible : URI externe (relatif au .gltf), data URI base64, ou
        // bufferView (.glb embarque). `decoded` contient les pixels RGBA8 si le
        // decodage NKImage a reussi (sinon valid()==false : slot a ignorer).
        struct NkGLTFImage {
            NkImage  decoded;     // pixels CPU (RGBA recommande via desiredChannels=4)
            NkString uri;         // pour diagnostic (vide si embarque)
            bool     valid = false;
        };

        // Resultat CPU d'un chargement glTF : geometrie prete a passer a
        // NkMeshSystem::Create via NkMeshDesc. Les buffers sont possedes par
        // cette struct (NkVector) ; la duree de vie doit couvrir l'appel a
        // Create (qui copie les donnees dans des NkBuffer GPU).
        // ── Animation glTF (squelettique) ────────────────────────────────────
        // Echantillonneur d'un canal : keyframes (input=temps) -> valeurs
        // (output : VEC3 translation/scale, VEC4 quaternion rotation).
        enum class NkGLTFPath : uint8 { TRANSLATION, ROTATION, SCALE, WEIGHTS };
        enum class NkGLTFInterp : uint8 { LINEAR, STEP, CUBICSPLINE };

        struct NkGLTFAnimChannel {
            int32        node = -1;            // node cible
            NkGLTFPath   path = NkGLTFPath::TRANSLATION;
            NkGLTFInterp interp = NkGLTFInterp::LINEAR;
            NkVector<float32> times;           // input (n keyframes)
            NkVector<NkVec4f> values;          // output (n) : .xyz (T/S) ou .xyzw (quat)
        };

        struct NkGLTFAnimation {
            NkString name;
            float32  duration = 0.f;
            NkVector<NkGLTFAnimChannel> channels;
        };

        // Transform local d'un node (TRS) + hierarchie. Sert a evaluer la pose
        // et a baker les transforms de scene-graph dans les meshes statiques.
        struct NkGLTFNode {
            NkVec3f  translation = {0,0,0};
            NkVec4f  rotation    = {0,0,0,1};  // quaternion (x,y,z,w)
            NkVec3f  scale       = {1,1,1};
            bool     hasMatrix   = false;
            NkMat4f  matrix      = NkMat4f::Identity();
            int32    mesh        = -1;         // index dans meshes[] (-1 = aucun)
            NkVector<int32> children;
        };

        struct NkGLTFMeshData {
            NkVector<NkVertex3D> vertices;
            NkVector<uint32>     indices;
            NkVector<NkSubMesh>  subMeshes;       // un par primitive
            NkVector<int32>      subMeshMaterial; // index materiau glTF par submesh (-1 = aucun)
            NkVector<NkGLTFMaterial> materials;   // section glTF materials[]
            NkVector<NkGLTFImage>    images;       // images decodees (CPU)
            NkAABB               bounds;            // englobant global
            NkString             debugName;

            // ── Skinning (rempli si JOINTS_0/WEIGHTS_0 + skins[] presents) ────
            bool                    isSkinned = false;
            NkVector<NkVertexSkinned> skinnedVertices; // parallele a `vertices`
            NkVector<int32>           skinJoints;       // skins[0].joints[] = node indices
            NkVector<NkMat4f>         inverseBind;      // inverseBindMatrices[] (par joint)
            int32                     skinRootNode = -1; // skins[0].skeleton (optionnel)

            // ── Scene graph + animations (pour evaluer la pose a un temps t) ──
            NkVector<NkGLTFNode>      nodes;            // tous les nodes (TRS + children)
            NkVector<NkGLTFAnimation> animations;       // animations[]

            // Copie desactivee (NkImage non copiable) — passe par reference.
            NkGLTFMeshData() = default;
            NkGLTFMeshData(const NkGLTFMeshData&)            = delete;
            NkGLTFMeshData& operator=(const NkGLTFMeshData&) = delete;

            bool IsValid() const { return !vertices.Empty(); }
        };

        // ── Evaluation de pose squelettique ──────────────────────────────────
        // Echantillonne l'animation `animIdx` au temps `t` (boucle sur la duree),
        // recompose les transforms locales de chaque node, calcule les transforms
        // globales (hierarchie), puis ecrit dans `outBones` (taille = nb joints)
        // les joint matrices = globalTransform(joint) * inverseBind(joint).
        // Si `animIdx < 0` ou aucune animation : produit la BIND POSE (joints a
        // partir des TRS statiques des nodes).
        // Retourne false si data non skinnee.
        bool EvaluateGLTFPose(const NkGLTFMeshData& data, int32 animIdx,
                              float32 t, NkVector<NkMat4f>& outBones);

        // ── Transforms MONDE par joint (pour l'IK + rendu squelette) ──────────
        // Comme EvaluateGLTFPose mais sort les matrices MONDE de chaque joint
        // (= globalTransform(joint), SANS l'inverseBind) dans `outWorld`, et le
        // parent de chaque joint EN INDICE DE JOINT (ou -1) dans `outParentJoint`.
        // La position monde d'un joint = colonne translation de outWorld[j].
        // Sert a NkIKSystem (chaine sur un membre reel) et au debug-draw squelette.
        // Retourne false si data non skinnee.
        bool EvaluateGLTFWorldJoints(const NkGLTFMeshData& data, int32 animIdx,
                                     float32 t, NkVector<NkMat4f>& outWorld,
                                     NkVector<int32>& outParentJoint);

        // Charge un fichier glTF 2.0 (.gltf ou .glb) dans `out`.
        // path : chemin du fichier (les .bin externes sont resolus relativement
        //        au dossier de `path`).
        // Retourne true si au moins une primitive a ete chargee.
        // En cas d'echec (fichier introuvable, JSON invalide, aucune geometrie),
        // retourne false et log un warning ; `out` reste vide.
        bool LoadGLTF(const NkString& path, NkGLTFMeshData& out);

    } // namespace renderer
} // namespace nkentseu
