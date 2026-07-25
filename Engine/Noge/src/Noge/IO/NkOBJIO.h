
#pragma once
// =============================================================================
// Nkentseu/IO/NkOBJIO.h — Import/Export OBJ + MTL
// =============================================================================
// [RÉVISION 2026-07-23] : NkOBJIO est désormais une fine COUCHE D'ADAPTATION
// au-dessus du loader OBJ from-scratch déjà en production côté NKRenderer
// (Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/NkOBJLoader.cpp,
// `renderer::LoadOBJ`, zéro-STL, dédup vertices, triangulation fan, .mtl),
// et non plus une spec morte (includes cassés, jamais compilée, aucune
// implémentation). Import() délègue TOUJOURS le parsing à `renderer::LoadOBJ`
// — zéro nouvelle logique de lecture OBJ/MTL ici.
//
// Pont de conversion : `renderer::LoadOBJ` produit un `renderer::NkGLTFMeshData`
// (format CPU commun à tous les loaders NKRenderer, cf. NkMeshLoaderUtil.h) ;
// ce fichier construit un `renderer::NkMeshDesc` transitoire à partir de ses
// buffers puis délègue à `NkEditableMesh::FromMeshDesc` (pont déjà existant,
// cf. Noge/Modeling/NkEditableMesh.h) pour obtenir la demi-arête n-gon Noge.
// AUCUN accès GPU : FromMeshDesc lit uniquement les pointeurs CPU du desc, ne
// touche jamais NkMeshSystem::Create/NkIDevice.
//
// OPTIONS RÉELLEMENT HONORÉES (voir NkOBJIO.cpp) :
//   • scaleFactor : facteur d'échelle uniforme appliqué aux positions APRÈS
//     parsing (transformation triviale sur des données déjà chargées, pas une
//     nouvelle logique de parsing).
//   • flipY : inverse la composante Y des positions/normales APRÈS parsing.
// OPTIONS NON HONORÉES (le loader réel n'a pas de commutateur dédié — se
// comporte TOUJOURS de la même façon, log d'avertissement si l'appelant
// demande explicitement le contraire) :
//   • triangulate : `renderer::LoadOBJ` triangule TOUJOURS les polygones
//     (fan) — triangulate=false est ignoré.
//   • generateNormals : `renderer::LoadOBJ` calcule TOUJOURS des normales
//     lissées si le fichier n'en fournit pas — generateNormals=false est
//     ignoré (aucune normale "plate/absente" n'est produite par le loader).
//
// Export() : NON IMPLÉMENTÉ — aucun écrivain OBJ/MTL n'existe côté NKRenderer
// (LoadOBJ est un loader, pas un round-trip). Retourne false + log, pas de
// fausse réussite.
// =============================================================================
#include "NKECS/NkECSDefines.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"
#include "Noge/Modeling/NkEditableMesh.h"

namespace nkentseu {

	struct NkOBJImportOptions {
			bool triangulate = false;
			float32 scaleFactor = 1.f;
			bool flipY = false;
			bool generateNormals = true;
	};

	struct NkOBJExportOptions {
			bool exportNormals = true;
			bool exportUVs = true;
			bool exportMTL = true;
			float32 precision = 6.f;
	};

	class NkOBJIO {
		public:
			/**
			 * @brief Importe un fichier .obj (+.mtl associé) via renderer::LoadOBJ,
			 *        converti en NkEditableMesh (demi-arête n-gon Noge) via
			 *        NkEditableMesh::FromMeshDesc.
			 * @return Mesh vide (VertexCount()==0) si le fichier est introuvable ou
			 *         invalide — voir GetLastError().
			 */
			[[nodiscard]] static NkEditableMesh Import(const char *path, const NkOBJImportOptions &opts = {}) noexcept;

			/**
			 * @brief NON IMPLÉMENTÉ — aucun écrivain OBJ/MTL côté NKRenderer.
			 *        Log un avertissement et retourne false (pas de fausse réussite).
			 */
			[[nodiscard]] static bool Export(const NkEditableMesh &mesh, const char *path,
											 const NkOBJExportOptions &opts = {}) noexcept;

			[[nodiscard]] static const NkString &GetLastError() noexcept;
	};
} // namespace nkentseu
