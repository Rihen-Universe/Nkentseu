#pragma once
// =============================================================================
// Nkentseu/Modeling/NkEditableMesh.h
// =============================================================================
// Représentation CPU d'un mesh éditable pour la modélisation polygonale.
//
// [RÉVISION 2026-07-23] : NkEditableMesh est désormais une fine COUCHE
// D'ADAPTATION au-dessus de renderer::NkEditMesh (Kernel/Runtime/NKRenderer/
// src/NKRenderer/Mesh/NkEditMesh.h), et non plus une réimplémentation
// indépendante de la topologie. Raison : NkEditMesh existe DÉJÀ en
// production (utilisé par les commandes d'édition .nkmec, ExtrudeSelectedFaces,
// MergeSelectedVerts, SubdivideSelectedFaces, LoopCutFromSelectedEdge,
// BisectByPlane, Quadify, l'historique undo/redo NkEditHistory et la pile de
// modificateurs NkModifierStack Mirror/Array/Subsurf) et supporte nativement
// les N-GONS (faces à nombre de côtés quelconque) via une structure demi-arête
// complète (Vert/Hedge/Face). La première version de ce fichier réimplémentait
// sa PROPRE demi-arête avec des faces à TAILLE FIXE (NkEditFace::kMaxVerts=4)
// — incapable de représenter un n-gon à 5+ côtés — ce qui dupliquait (en pire,
// en plus limité) une structure déjà résolue côté NKRenderer.
//
// DESIGN ACTUEL :
//   • Stockage unique : `renderer::NkEditMesh mEdit` (demi-arête, n-gons).
//   • AddVertex/AddTri/AddQuad/AddPolygon reconstruisent mEdit via le
//     round-trip public ToPolygons()/BuildFromPolygons() (coût O(taille du
//     mesh) par appel — acceptable pour un outil d'édition interactif, PAS
//     une boucle chaude runtime). Pas d'état de "staging" séparé à
//     resynchroniser : mEdit est TOUJOURS la source de vérité unique.
//   • Extrude/LoopCut/Subdivide DÉLÈGUENT aux commandes d'édition natives de
//     NkEditMesh (ExtrudeSelectedFaces/LoopCutFromSelectedEdge/
//     SubdivideSelectedFaces) via la sélection par sommet (Vert::sel).
//   • FlipNormals/Triangulate/MergeByDistance n'ont PAS d'équivalent direct
//     dans la couche de commandes de NkEditMesh : ils sont implémentés comme
//     manipulations pures du CSR polygones (ToPolygons -> transforme la liste
//     de faces -> BuildFromPolygons), ce qui délègue TOUJOURS le rechaînage
//     demi-arête (twins, next, hedge) à NkEditMesh — Noge ne touche jamais
//     directement les champs Hedge::twin/next.
//   • ToMeshDesc délègue entièrement à `mEdit.Triangulate(...)`.
//   • BevelEdges, SmartUVProject, CubeProject, CylindricalProject,
//     GrowSelection, ShrinkSelection : AUCUN équivalent dans NkEditMesh —
//     laissés en TODO honnête (voir NkEditableMesh.cpp), pas d'invention.
//
// TOPOLOGIE (déléguée à renderer::NkEditMesh) :
//   Vertex → une demi-arête sortante (Vert::hedge)
//   Hedge  → origin/twin/next/face (demi-arête classique)
//   Face   → une demi-arête de bord + boucle via next (N sommets quelconque)
//
// USAGE TYPIQUE :
//   NkEditableMesh em;
//   uint32 v0 = em.AddVertex({-1,0,0}, {0,0});
//   uint32 v1 = em.AddVertex({ 1,0,0}, {1,0});
//   uint32 v2 = em.AddVertex({ 0,1,0}, {0.5f,1});
//   em.AddTri(v0, v1, v2);
//   em.RecalcNormals();
//   renderer::NkMeshDesc gpuDesc;
//   em.ToMeshDesc(gpuDesc);
//   NkMeshHandle h = resources.CreateMesh(gpuDesc);
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Views/NkSpan.h"
// NOTE [fix 2026-07-23] : "NKRenderer/src/Resources/NkResourceDescs.h" n'existe pas dans
// l'arborescence NKRenderer actuelle (ce fichier n'a donc jamais été compilé — aucun .cpp
// de Noge ne l'incluait avant NkEditableMesh.cpp). NkMeshDesc + NkAABB vivent réellement
// dans NKRenderer/Mesh/NkMeshSystem.h. NkEditMesh.h (demi-arête n-gon) inclut lui-même
// NkMeshSystem.h.
#include "NKRenderer/Mesh/NkEditMesh.h"

namespace nkentseu {

	using namespace math;

	// =========================================================================
	// Alias de continuité d'API — les noms publics NkEditVertex/NkEditEdge/
	// NkEditFace sont conservés (code appelant existant, docs), mais pointent
	// maintenant directement vers les types demi-arête n-gon de NKRenderer au
	// lieu d'une struct à taille fixe dupliquée.
	// =========================================================================
	using NkEditVertex = renderer::NkEditMesh::Vert; ///< pos, normal, uv, hedge (sortante), sel
	using NkEditEdge = renderer::NkEditMesh::Hedge;  ///< origin, twin, next, face, alive
	using NkEditFace = renderer::NkEditMesh::Face;	 ///< hedge (bord), normal, sel, alive — N sommets

	static constexpr uint32 kNkInvalidIdx = renderer::NK_EM_INVALID;

	// =========================================================================
	// NkEditableMesh
	// =========================================================================
	class NkEditableMesh {
		public:
			NkEditableMesh() noexcept = default;
			~NkEditableMesh() noexcept = default;

			// Pas de copie directe — utiliser Clone()
			NkEditableMesh(const NkEditableMesh &) = delete;
			NkEditableMesh &operator=(const NkEditableMesh &) = delete;

			// Déplacement autorisé
			NkEditableMesh(NkEditableMesh &&) noexcept = default;
			NkEditableMesh &operator=(NkEditableMesh &&) noexcept = default;

			// ── Création de topologie ─────────────────────────────────────

			/**
			 * @brief Ajoute un vertex ISOLÉ (sans face) et retourne son index.
			 * @note Reconstruit mEdit via ToPolygons()/BuildFromPolygons() —
			 *       O(taille du mesh). NkEditMesh ne stocke pas la couleur, le
			 *       canal UV2 ni la tangente par sommet (struct Vert = pos +
			 *       normal + uv uniquement) : ces paramètres sont acceptés
			 *       pour compatibilité d'API mais ne sont PAS conservés.
			 */
			uint32 AddVertex(const NkVec3f &pos, const NkVec2f &uv = {}, const NkVec3f &normal = {0, 1, 0},
							 const NkVec4f &color = {1, 1, 1, 1}) noexcept;

			/**
			 * @brief Ajoute une face à N sommets quelconque (n-gon), dans l'ordre CCW.
			 * @return Index de la face créée.
			 */
			uint32 AddPolygon(NkSpan<const uint32> vertIds) noexcept;

			/**
			 * @brief Ajoute un triangle (v0, v1, v2 dans l'ordre CCW).
			 * @return Index de la face créée.
			 */
			uint32 AddTri(uint32 v0, uint32 v1, uint32 v2) noexcept;

			/**
			 * @brief Ajoute un quad (v0, v1, v2, v3 dans l'ordre CCW).
			 * @return Index de la face créée.
			 */
			uint32 AddQuad(uint32 v0, uint32 v1, uint32 v2, uint32 v3) noexcept;

			// ── Opérations de modélisation ────────────────────────────────

			/**
			 * @brief Extrude les faces données le long de leur normale.
			 * @param faceIds  Faces à extruder (vide = toutes les faces).
			 * @param direction Utilisé uniquement pour sa NORME (distance
			 *        d'extrusion) : délègue à NkEditMesh::ExtrudeSelectedFaces,
			 *        qui n'extrude qu'un scalaire le long de la normale de
			 *        chaque face — pas un vecteur de direction arbitraire.
			 */
			void ExtrudeFaces(NkSpan<const uint32> faceIds, const NkVec3f &direction) noexcept;

			/**
			 * @brief NON IMPLÉMENTÉ — aucun équivalent dans renderer::NkEditMesh
			 * (pas de commande "bevel edge"). Voir NkEditableMesh.cpp.
			 */
			void BevelEdges(NkSpan<const uint32> edgeIds, float32 width, uint32 segments = 1) noexcept;

			/**
			 * @brief Coupe une edge loop passant par l'arête `edgeId`.
			 * @param edgeId Index de demi-arête (Hedge) de référence.
			 * @param factor NON SUPPORTÉ par NkEditMesh::LoopCutFromSelectedEdge
			 *        (coupe toujours au milieu, 0.5) — ignoré si != 0.5 (log).
			 */
			void LoopCut(uint32 edgeId, float32 factor = 0.5f) noexcept;

			/**
			 * @brief Fusionne les vertices trop proches (soudure par proximité).
			 * @note Pas d'équivalent direct : NkEditMesh::MergeSelectedVerts
			 *       fusionne toute la sélection en UN point (Center/First/Last),
			 *       ce n'est pas une fusion par PAIRES sous un seuil de distance.
			 *       Implémenté ici via ToPolygons() -> soudure géométrique pure
			 *       (aucune logique demi-arête dupliquée) -> BuildFromPolygons().
			 * @param threshold Distance maximale entre vertices à fusionner.
			 * @return Nombre de vertices fusionnés.
			 */
			uint32 MergeByDistance(float32 threshold = 0.0001f) noexcept;

			/**
			 * @brief Inverse la direction des normales des faces (et leur winding).
			 * @note Pas de commande dédiée côté NkEditMesh : implémenté via
			 *       ToPolygons() -> inversion de l'ordre des sommets par face ->
			 *       BuildFromPolygons() (le rechaînage demi-arête reste délégué).
			 * @param faceIds Faces à retourner (vide = toutes).
			 */
			void FlipNormals(NkSpan<const uint32> faceIds = {}) noexcept;

			/**
			 * @brief Triangule TOUTES les faces à plus de 3 côtés (fan), en
			 *        mutant la topologie de mEdit elle-même (contrairement à
			 *        NkEditMesh::Triangulate() qui ne produit qu'un mesh de
			 *        rendu temporaire sans modifier les n-gons stockés).
			 * @param method 0=Fan (implémenté). 1=Beauty : NON IMPLÉMENTÉ,
			 *        retombe sur Fan (log d'avertissement).
			 */
			void Triangulate(uint32 method = 1) noexcept;

			/**
			 * @brief Subdivise (délègue à NkEditMesh::SubdivideSelectedFaces,
			 *        "rien sélectionné" = tout le mesh, cf. doc NkEditMesh.h).
			 * @param levels Nombre de passes de subdivision.
			 */
			void Subdivide(uint32 levels = 1) noexcept;

			// ── Normales ──────────────────────────────────────────────────

			/**
			 * @brief Recalcule les normales (délègue à NkEditMesh::RecomputeNormals).
			 * @param smooth NkEditMesh ne calcule QUE des normales moyennées
			 *        par sommet (pas de mode "flat") — smooth=false log un
			 *        avertissement et retombe sur le comportement smooth.
			 */
			void RecalcNormals(bool smooth = true) noexcept;

			/**
			 * @brief Recalcule les normales — NkEditMesh::RecomputeNormals()
			 *        est TOUJOURS global (pas de recalcul partiel par face) :
			 *        `faceIds` non vide déclenche un avertissement puis un
			 *        recalcul complet quand même.
			 */
			void RecalcFaceNormals(NkSpan<const uint32> faceIds = {}) noexcept;

			// ── Sélection ─────────────────────────────────────────────────
			// (déléguée à Vert::sel ; une face/arête est "sélectionnée" si
			// TOUS ses sommets le sont, convention NkEditMesh::PolyFaceSelected)

			void SelectAll() noexcept;
			void DeselectAll() noexcept;
			void InvertSelection() noexcept;
			void SelectVertex(uint32 id, bool v) noexcept;
			void SelectFace(uint32 id, bool v) noexcept;
			void SelectEdge(uint32 id, bool v) noexcept;

			/**
			 * @brief NON IMPLÉMENTÉ — aucun équivalent dans renderer::NkEditMesh.
			 */
			void GrowSelection() noexcept;
			void ShrinkSelection() noexcept;

			/**
			 * @brief Retourne les indices des vertices/faces/edges sélectionnés.
			 */
			void GetSelectedVertices(NkVector<uint32> &out) const noexcept;
			void GetSelectedFaces(NkVector<uint32> &out) const noexcept;
			void GetSelectedEdges(NkVector<uint32> &out) const noexcept;

			// ── UVs ──────────────────────────────────────────────────────
			// NON IMPLÉMENTÉES — aucun équivalent dans renderer::NkEditMesh
			// (pas de dépliage UV). Voir NkEditableMesh.cpp.

			void SmartUVProject(float32 angleLimit = 66.f, float32 islandMargin = 0.02f) noexcept;
			void CubeProject(float32 scale = 1.f) noexcept;
			void CylindricalProject(float32 scale = 1.f) noexcept;

			// ── AABB ──────────────────────────────────────────────────────

			[[nodiscard]] const renderer::NkAABB &GetBounds() const noexcept;
			void RecomputeBounds() noexcept;

			// ── Accès aux données ─────────────────────────────────────────
			// Accès direct à la demi-arête sous-jacente (NkEditMesh) — permet
			// d'utiliser toute l'API NKRenderer (GetFaceVerts, FaceSize,
			// GetUniqueEdges, Quadify...) sans que Noge n'ait à la redupliquer.

			[[nodiscard]] renderer::NkEditMesh &Edit() noexcept {
				return mEdit;
			}

			[[nodiscard]] const renderer::NkEditMesh &Edit() const noexcept {
				return mEdit;
			}

			[[nodiscard]] NkVector<NkEditVertex> &Vertices() noexcept {
				return mEdit.verts;
			}

			[[nodiscard]] NkVector<NkEditEdge> &Edges() noexcept {
				return mEdit.hedges;
			}

			[[nodiscard]] NkVector<NkEditFace> &Faces() noexcept {
				return mEdit.faces;
			}

			[[nodiscard]] const NkVector<NkEditVertex> &Vertices() const noexcept {
				return mEdit.verts;
			}

			[[nodiscard]] const NkVector<NkEditEdge> &Edges() const noexcept {
				return mEdit.hedges;
			}

			[[nodiscard]] const NkVector<NkEditFace> &Faces() const noexcept {
				return mEdit.faces;
			}

			[[nodiscard]] uint32 VertexCount() const noexcept {
				return mEdit.VertCount();
			}

			[[nodiscard]] uint32 FaceCount() const noexcept {
				return mEdit.FaceCount();
			}

			[[nodiscard]] uint32 EdgeCount() const noexcept {
				return static_cast<uint32>(mEdit.hedges.Size());
			}

			// ── Conversion GPU ────────────────────────────────────────────

			/**
			 * @brief Convertit vers NkMeshDesc pour upload GPU.
			 * @note Délègue entièrement à NkEditMesh::Triangulate() (fan) — la
			 *       triangulation d'export existait déjà côté NKRenderer,
			 *       Noge ne la redéfinit pas.
			 */
			void ToMeshDesc(renderer::NkMeshDesc &out) const noexcept;

			/**
			 * @brief Clone le mesh (deep copy).
			 */
			NkEditableMesh Clone() const noexcept;

			/**
			 * @brief Vide complètement le mesh.
			 */
			void Clear() noexcept {
				mEdit.Clear();
				mBoundsDirty = true;
			}

			/**
			 * @brief Génère un mesh depuis un NkMeshDesc (indexé, triangles).
			 * @note Suppose que desc.layout correspond à NkVertex3D (position/
			 *       normal/tangent/uv/uv2/color) — c'est le layout produit par
			 *       NkVertexLayout::Default3D(), le cas d'usage standard. Un
			 *       layout custom n'est pas réinterprété (limitation connue).
			 */
			static NkEditableMesh FromMeshDesc(const renderer::NkMeshDesc &desc) noexcept;

		private:
			// ── Données ───────────────────────────────────────────────────
			// Source de vérité UNIQUE : la demi-arête n-gon de NKRenderer.
			// Plus de tableaux dupliqués côté Noge (voir note de révision en
			// tête de fichier).
			renderer::NkEditMesh mEdit;

			mutable renderer::NkAABB mBounds;
			mutable bool mBoundsDirty = true;

			// ── Cache GPU (ToMeshDesc) ──────────────────────────────────────
			// NkMeshDesc ne POSSEDE pas ses buffers (vertices/indices sont des
			// pointeurs bruts, cf. NkMeshSystem.h) : ils doivent rester valides
			// tant que le NkMeshDesc est utilisé. mEdit.Triangulate() écrit ici
			// plutôt que dans une variable locale de ToMeshDesc() qui
			// deviendrait pendante dès le retour de la fonction.
			mutable NkVector<renderer::NkVertex3D> mGpuVertexCache;
			mutable NkVector<uint32> mGpuIndexCache;
			mutable NkVector<renderer::NkEmId> mGpuTriFaceCache;
	};

} // namespace nkentseu
