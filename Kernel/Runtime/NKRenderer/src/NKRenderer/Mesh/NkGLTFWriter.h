#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// FICHIER: NKRenderer/Mesh/NkGLTFWriter.h
// DESCRIPTION: L'ECRIVAIN glTF 2.0 BINAIRE (.glb) -- le pendant de NkGLTFLoader.
//
//   Jusqu'au 2026-10-02 le depot LISAIT le glTF et ne l'ECRIVAIT pas (Noge le
//   notait : « aucun ecrivain glTF/GLB n'existe »). Le rig 3D de NkAnimaEditor
//   en a besoin : un maillage rigge, ses poids et ses FORMES (morph targets)
//   doivent repartir vers Blender, Unreal, Unity ou le jeu.
//
//   Ce qu'il ecrit : UN maillage (positions, normales, UV facultatives,
//   triangles), un materiau (couleur de base), la PEAU (joints, poids, matrices
//   de liaison inverses, la hierarchie des joints en matrices locales), et les
//   CIBLES DE MORPH (deltas de position, et de normale si fournis) avec leurs
//   noms dans `mesh.extras.targetNames` (la convention de Blender, relue par
//   NkGLTFLoader) et leurs poids par defaut.
//
//   Ce qu'il n'ecrit PAS (encore) : les animations, les textures, plusieurs
//   maillages. Un fichier ecrit ici se relit par LoadGLTF -- c'est le banc.
// -----------------------------------------------------------------------------

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKMath/NKMath.h"

namespace nkentseu {
	namespace renderer {

		struct NkGLTFWriteMorph {
				NkString name;
				const math::NkVec3f *dPos = nullptr;	///< un delta par sommet (obligatoire)
				const math::NkVec3f *dNormal = nullptr; ///< facultatif
				float32 defaultWeight = 0.f;
		};

		struct NkGLTFWriteDesc {
				NkString name = NkString("Maillage");
				const math::NkVec3f *positions = nullptr;
				const math::NkVec3f *normals = nullptr; ///< facultatif
				const math::NkVec2f *uvs = nullptr;		///< facultatif
				uint32 vertexCount = 0;
				const uint32 *indices = nullptr;
				uint32 indexCount = 0;
				float32 baseColor[4] = {0.8f, 0.8f, 0.8f, 1.f};
				float32 roughness = 0.6f;

				// ── La peau (facultative : jointCount = 0 -> maillage statique) ──
				uint32 jointCount = 0;
				const math::NkMat4f *jointWorld = nullptr;	 ///< repos, monde (les noeuds en derivent leur local)
				const int32 *jointParent = nullptr;			 ///< -1 = racine
				const NkString *jointNames = nullptr;		 ///< facultatif
				const math::NkMat4f *inverseBind = nullptr;	 ///< facultatif : sinon inverse(jointWorld)
				const float32 *joints4 = nullptr;			 ///< 4 indices par sommet (flottants, comme NkVertexSkinned)
				const float32 *weights4 = nullptr;			 ///< 4 poids par sommet

				// ── Les formes ───────────────────────────────────────────────────
				NkVector<NkGLTFWriteMorph> morphs;
		};

		/// Ecrit un .glb. Rend faux et dit pourquoi (`why`) si rien n'est ecrit.
		bool NkWriteGLB(const NkString &path, const NkGLTFWriteDesc &d, NkString *why = nullptr);

	} // namespace renderer
} // namespace nkentseu
