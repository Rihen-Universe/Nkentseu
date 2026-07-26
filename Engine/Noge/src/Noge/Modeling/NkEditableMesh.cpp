// =============================================================================
// Nkentseu/Modeling/NkEditableMesh.cpp
// =============================================================================
// Implémentation de NkEditableMesh comme fine couche d'adaptation au-dessus de
// renderer::NkEditMesh (voir la note de révision en tête de NkEditableMesh.h
// pour le contexte : la première version de ce fichier réimplémentait sa
// propre demi-arête à faces de taille fixe [4 sommets max, aucun n-gon] —
// dupliquant, en pire, une structure demi-arête n-gon déjà en production côté
// NKRenderer).
//
// Avant ce fichier (aucune version), NkEditableMesh.h n'avait JAMAIS été
// compilé : aucun .cpp de Engine/Noge ne l'incluait, et le header lui-même ne
// compilait pas tel quel (include cassé vers un fichier NKRenderer inexistant,
// NkAABB/NkSpan non résolus).
//
// IMPLÉMENTÉ ET TESTÉ (tests/test_editable_mesh.cpp) :
//   AddVertex, AddPolygon, AddTri, AddQuad, RecalcFaceNormals, RecalcNormals,
//   RecomputeBounds/GetBounds, sélection (SelectAll/DeselectAll/
//   InvertSelection/SelectVertex/SelectFace/SelectEdge/GetSelected*),
//   FlipNormals, Triangulate (méthode Fan), MergeByDistance, ToMeshDesc,
//   Clone, ExtrudeFaces (délégué), Subdivide (délégué), LoopCut (délégué).
//
// NON IMPLÉMENTÉ (aucun équivalent dans renderer::NkEditMesh — corps présent,
// no-op documenté + log d'avertissement, pas de fausse réussite silencieuse) :
//   BevelEdges, SmartUVProject, CubeProject, CylindricalProject,
//   GrowSelection, ShrinkSelection.
// =============================================================================
#include "NkEditableMesh.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

	using namespace math;
	using namespace renderer;

	// =========================================================================
	// Helpers internes (round-trip CSR polygones — jamais de manipulation
	// directe des champs Hedge::twin/next : le rechaînage demi-arête reste
	// TOUJOURS délégué à NkEditMesh::BuildFromPolygons/LinkTwins).
	// =========================================================================
	namespace {

		// Est-ce qu'une face (toute sa boucle de sommets) est sélectionnée ?
		// Convention NkEditMesh (cf. NkEditMesh.h : PolyFaceSelected) : une face
		// est sélectionnée si TOUS ses sommets le sont.
		bool IsFaceFullySelected(const NkEditMesh &m, NkEmId faceId) noexcept {
			NkVector<NkEmId> fv;
			m.GetFaceVerts(faceId, fv);
			if (fv.Size() == 0) {
				return false;
			}
			for (uint32 i = 0; i < fv.Size(); ++i) {
				if (!m.verts[fv[i]].sel) {
					return false;
				}
			}
			return true;
		}

	} // namespace

	// =========================================================================
	// Construction de topologie (round-trip ToPolygons -> mutation CSR ->
	// BuildFromPolygons — NkEditMesh reste seul responsable du rechaînage
	// demi-arête).
	// =========================================================================

	uint32 NkEditableMesh::AddVertex(const NkVec3f &pos, const NkVec2f &uv, const NkVec3f &normal,
									 const NkVec4f & /*color*/) noexcept {
		// NOTE : `color` n'est pas conservé — renderer::NkEditMesh::Vert ne
		// stocke que pos/normal/uv (voir NkEditableMesh.h).
		NkVector<NkVertex3D> polyVerts;
		NkVector<uint32> faceStart;
		NkVector<uint32> faceVerts;
		mEdit.ToPolygons(polyVerts, faceStart, faceVerts);

		NkVertex3D nv;
		nv.pos = pos;
		nv.normal = normal;
		nv.tangent = {1.f, 0.f, 0.f};
		nv.uv = uv;
		nv.uv2 = {0.f, 0.f};
		nv.color = 0xFFFFFFFFu;
		polyVerts.PushBack(nv);
		const uint32 newIdx = static_cast<uint32>(polyVerts.Size()) - 1;

		if (faceStart.Size() == 0) {
			faceStart.PushBack(0);
		}
		const uint32 faceCount = faceStart.Size() - 1;
		mEdit.BuildFromPolygons(polyVerts.Data(), static_cast<uint32>(polyVerts.Size()), faceStart.Data(), faceCount,
								faceVerts.Data());
		mBoundsDirty = true;
		return newIdx;
	}

	uint32 NkEditableMesh::AddPolygon(NkSpan<const uint32> vertIds) noexcept {
		if (vertIds.Size() < 3) {
			logger.Errorf("[NkEditableMesh] AddPolygon: il faut au moins 3 sommets ({} fournis)\n",
						  static_cast<uint32>(vertIds.Size()));
			return kNkInvalidIdx;
		}

		NkVector<NkVertex3D> polyVerts;
		NkVector<uint32> faceStart;
		NkVector<uint32> faceVerts;
		mEdit.ToPolygons(polyVerts, faceStart, faceVerts);
		if (faceStart.Size() == 0) {
			faceStart.PushBack(0);
		}

		for (uint32 id : vertIds) {
			faceVerts.PushBack(id);
		}
		faceStart.PushBack(static_cast<uint32>(faceVerts.Size()));
		const uint32 faceId = faceStart.Size() - 2; // nouvelle face = dernière ajoutée

		const uint32 faceCount = faceStart.Size() - 1;
		mEdit.BuildFromPolygons(polyVerts.Data(), static_cast<uint32>(polyVerts.Size()), faceStart.Data(), faceCount,
								faceVerts.Data());
		mBoundsDirty = true;
		return faceId;
	}

	uint32 NkEditableMesh::AddTri(uint32 v0, uint32 v1, uint32 v2) noexcept {
		const uint32 ids[3] = {v0, v1, v2};
		return AddPolygon(NkSpan<const uint32>(ids, 3));
	}

	uint32 NkEditableMesh::AddQuad(uint32 v0, uint32 v1, uint32 v2, uint32 v3) noexcept {
		const uint32 ids[4] = {v0, v1, v2, v3};
		return AddPolygon(NkSpan<const uint32>(ids, 4));
	}

	// =========================================================================
	// Extrude / LoopCut / Subdivide — délégués aux commandes natives de
	// NkEditMesh via la sélection par sommet.
	// =========================================================================

	void NkEditableMesh::ExtrudeFaces(NkSpan<const uint32> faceIds, const NkVec3f &direction) noexcept {
		mEdit.SelectNone();
		if (faceIds.Size() == 0) {
			mEdit.SelectAll();
		} else {
			for (uint32 fid : faceIds) {
				if (fid >= mEdit.faces.Size()) {
					continue;
				}
				NkVector<NkEmId> fv;
				mEdit.GetFaceVerts(fid, fv);
				for (uint32 i = 0; i < fv.Size(); ++i) {
					mEdit.verts[fv[i]].sel = 1;
				}
			}
		}

		// NkExtrudeParams n'a qu'un OFFSET SCALAIRE le long de la normale de
		// chaque face (pas de vecteur de direction arbitraire) — seule la
		// norme de `direction` est utilisée (voir NkEditableMesh.h).
		const float32 len = NkSqrt(direction.Dot(direction));
		NkExtrudeParams p;
		p.individual = false;
		p.offset = (len > 1e-8f) ? len : -1.f; // -1 = auto (8% bbox), cf. NkEditMesh.h

		if (!mEdit.ExtrudeSelectedFaces(p)) {
			logger.Warnf("[NkEditableMesh] ExtrudeFaces: NkEditMesh::ExtrudeSelectedFaces n'a rien change "
						 "(selection vide ou invalide ?)\n");
		}
		mBoundsDirty = true;
	}

	void NkEditableMesh::BevelEdges(NkSpan<const uint32> /*edgeIds*/, float32 /*width*/, uint32 /*segments*/) noexcept {
		logger.Warnf("[NkEditableMesh] BevelEdges: NON IMPLEMENTE — aucune commande equivalente dans "
					 "renderer::NkEditMesh (TODO)\n");
	}

	void NkEditableMesh::LoopCut(uint32 edgeId, float32 factor) noexcept {
		if (edgeId >= mEdit.hedges.Size()) {
			logger.Errorf("[NkEditableMesh] LoopCut: edgeId {} hors-limites\n", edgeId);
			return;
		}
		if (NkAbs(factor - 0.5f) > 1e-4f) {
			logger.Warnf("[NkEditableMesh] LoopCut: NkEditMesh::LoopCutFromSelectedEdge ne coupe qu'au "
						 "milieu (factor=0.5) — factor={} ignore\n",
						 factor);
		}

		const NkEditMesh::Hedge &he = mEdit.hedges[edgeId];
		mEdit.SelectNone();
		if (he.origin != NK_EM_INVALID) {
			mEdit.verts[he.origin].sel = 1;
		}
		if (he.next != NK_EM_INVALID) {
			const NkEmId otherEnd = mEdit.hedges[he.next].origin;
			if (otherEnd != NK_EM_INVALID) {
				mEdit.verts[otherEnd].sel = 1;
			}
		}

		if (!mEdit.LoopCutFromSelectedEdge()) {
			logger.Warnf("[NkEditableMesh] LoopCut: NkEditMesh::LoopCutFromSelectedEdge a echoue "
						 "(arete de bord, selection invalide...)\n");
		}
		mBoundsDirty = true;
	}

	void NkEditableMesh::Subdivide(uint32 levels) noexcept {
		if (levels == 0) {
			return;
		}
		NkSubdivideParams p;
		p.cuts = 1;
		for (uint32 i = 0; i < levels; ++i) {
			// "Rien de selectionne" = subdivise TOUT le mesh (cf. doc NkEditMesh.h).
			mEdit.SelectNone();
			if (!mEdit.SubdivideSelectedFaces(p)) {
				logger.Warnf("[NkEditableMesh] Subdivide: passe {} sans effet (mesh vide ?)\n", i);
				break;
			}
		}
		mBoundsDirty = true;
	}

	// =========================================================================
	// Normales — déléguées à NkEditMesh::RecomputeNormals (toujours globales,
	// toujours moyennées par sommet — pas de mode "flat" ni de recalcul partiel).
	// =========================================================================

	void NkEditableMesh::RecalcNormals(bool smooth) noexcept {
		if (!smooth) {
			logger.Warnf("[NkEditableMesh] RecalcNormals(smooth=false): NkEditMesh::RecomputeNormals ne "
						 "calcule que des normales moyennees par sommet (pas de mode 'flat') — fallback "
						 "sur smooth=true\n");
		}
		mEdit.RecomputeNormals();
	}

	void NkEditableMesh::RecalcFaceNormals(NkSpan<const uint32> faceIds) noexcept {
		if (faceIds.Size() != 0) {
			logger.Warnf("[NkEditableMesh] RecalcFaceNormals: NkEditMesh::RecomputeNormals est TOUJOURS "
						 "global — {} face(s) demandee(s) en sous-ensemble, recalcul de TOUT le mesh quand meme\n",
						 static_cast<uint32>(faceIds.Size()));
		}
		mEdit.RecomputeNormals();
	}

	// =========================================================================
	// Bounds
	// =========================================================================

	void NkEditableMesh::RecomputeBounds() noexcept {
		NkAABB b = NkAABB::Empty();
		for (uint32 i = 0; i < mEdit.verts.Size(); ++i) {
			b.Expand(mEdit.verts[i].pos);
		}
		mBounds = b;
		mBoundsDirty = false;
	}

	const NkAABB &NkEditableMesh::GetBounds() const noexcept {
		if (mBoundsDirty) {
			const_cast<NkEditableMesh *>(this)->RecomputeBounds();
		}
		return mBounds;
	}

	// =========================================================================
	// Sélection (Vert::sel — une face/arête est "sélectionnée" si tous ses
	// sommets le sont).
	// =========================================================================

	void NkEditableMesh::SelectAll() noexcept {
		mEdit.SelectAll();
	}

	void NkEditableMesh::DeselectAll() noexcept {
		mEdit.SelectNone();
	}

	void NkEditableMesh::InvertSelection() noexcept {
		for (uint32 i = 0; i < mEdit.verts.Size(); ++i) {
			mEdit.verts[i].sel = mEdit.verts[i].sel ? 0u : 1u;
		}
	}

	void NkEditableMesh::SelectVertex(uint32 id, bool v) noexcept {
		if (id < mEdit.verts.Size()) {
			mEdit.verts[id].sel = v ? 1u : 0u;
		}
	}

	void NkEditableMesh::SelectFace(uint32 id, bool v) noexcept {
		if (id >= mEdit.faces.Size()) {
			return;
		}
		NkVector<NkEmId> fv;
		mEdit.GetFaceVerts(id, fv);
		for (uint32 i = 0; i < fv.Size(); ++i) {
			// NOTE : un vertex partagé par plusieurs faces peut être
			// désélectionné ici même s'il appartient encore à une autre face
			// sélectionnée (limitation de la convention "face = tous ses
			// sommets", identique à NkEditMesh::PolyFaceSelected).
			mEdit.verts[fv[i]].sel = v ? 1u : 0u;
		}
	}

	void NkEditableMesh::SelectEdge(uint32 id, bool v) noexcept {
		if (id >= mEdit.hedges.Size()) {
			return;
		}
		const NkEditMesh::Hedge &he = mEdit.hedges[id];
		if (he.origin != NK_EM_INVALID) {
			mEdit.verts[he.origin].sel = v ? 1u : 0u;
		}
		if (he.next != NK_EM_INVALID) {
			const NkEmId otherEnd = mEdit.hedges[he.next].origin;
			if (otherEnd != NK_EM_INVALID) {
				mEdit.verts[otherEnd].sel = v ? 1u : 0u;
			}
		}
	}

	void NkEditableMesh::GetSelectedVertices(NkVector<uint32> &out) const noexcept {
		for (uint32 i = 0; i < mEdit.verts.Size(); ++i) {
			if (mEdit.verts[i].sel) {
				out.PushBack(i);
			}
		}
	}

	void NkEditableMesh::GetSelectedFaces(NkVector<uint32> &out) const noexcept {
		for (uint32 i = 0; i < mEdit.faces.Size(); ++i) {
			if (!mEdit.faces[i].alive) {
				continue;
			}
			if (IsFaceFullySelected(mEdit, i)) {
				out.PushBack(i);
			}
		}
	}

	void NkEditableMesh::GetSelectedEdges(NkVector<uint32> &out) const noexcept {
		// Une entrée par PAIRE demi-arête (i < twin, ou pas de twin = bord)
		// pour ne pas compter deux fois la même arête non-orientée.
		for (uint32 i = 0; i < mEdit.hedges.Size(); ++i) {
			const NkEditMesh::Hedge &he = mEdit.hedges[i];
			if (he.twin != NK_EM_INVALID && he.twin < i) {
				continue;
			}
			const NkEmId otherEnd = (he.next != NK_EM_INVALID) ? mEdit.hedges[he.next].origin : NK_EM_INVALID;
			const bool selected = (he.origin != NK_EM_INVALID && mEdit.verts[he.origin].sel) &&
								  (otherEnd != NK_EM_INVALID && mEdit.verts[otherEnd].sel);
			if (selected) {
				out.PushBack(i);
			}
		}
	}

	// =========================================================================
	// FlipNormals — pas de commande dédiée côté NkEditMesh : implémenté via
	// ToPolygons() -> inversion de l'ordre des sommets par face ->
	// BuildFromPolygons() (le rechaînage demi-arête reste délégué).
	// =========================================================================

	void NkEditableMesh::FlipNormals(NkSpan<const uint32> faceIds) noexcept {
		NkVector<NkVertex3D> polyVerts;
		NkVector<uint32> faceStart;
		NkVector<uint32> faceVerts;
		mEdit.ToPolygons(polyVerts, faceStart, faceVerts);
		if (faceStart.Size() < 2) {
			return; // aucune face
		}
		const uint32 faceCount = faceStart.Size() - 1;

		auto flipFace = [&](uint32 fi) noexcept {
			const uint32 s = faceStart[fi];
			const uint32 e = faceStart[fi + 1];
			if (e <= s + 1) {
				return;
			}
			// Inverse [s+1, e) en gardant le premier sommet fixe (a,b,c,d) -> (a,d,c,b).
			uint32 a = s + 1;
			uint32 b = e - 1;
			while (a < b) {
				const uint32 tmp = faceVerts[a];
				faceVerts[a] = faceVerts[b];
				faceVerts[b] = tmp;
				++a;
				--b;
			}
		};

		if (faceIds.Size() == 0) {
			for (uint32 fi = 0; fi < faceCount; ++fi) {
				flipFace(fi);
			}
		} else {
			for (uint32 id : faceIds) {
				if (id < faceCount) {
					flipFace(id);
				}
			}
		}

		mEdit.BuildFromPolygons(polyVerts.Data(), static_cast<uint32>(polyVerts.Size()), faceStart.Data(), faceCount,
								faceVerts.Data());
		mEdit.RecomputeNormals();
		mBoundsDirty = true;
	}

	// =========================================================================
	// Triangulate (méthode Fan uniquement — "Beauty" pas encore implémentée) —
	// mute la topologie n-gon elle-même (contrairement à
	// NkEditMesh::Triangulate() qui ne fait qu'un export de rendu temporaire).
	// =========================================================================

	void NkEditableMesh::Triangulate(uint32 method) noexcept {
		if (method != 0) {
			logger.Warnf("[NkEditableMesh] Triangulate: methode 'Beauty' (1) non implementee — "
						 "utilisation de 'Fan' (0) a la place\n");
		}

		NkVector<NkVertex3D> polyVerts;
		NkVector<uint32> faceStart;
		NkVector<uint32> faceVerts;
		mEdit.ToPolygons(polyVerts, faceStart, faceVerts);
		if (faceStart.Size() < 2) {
			return;
		}
		const uint32 faceCount = faceStart.Size() - 1;

		bool anyNgon = false;
		for (uint32 fi = 0; fi < faceCount; ++fi) {
			if (faceStart[fi + 1] - faceStart[fi] > 3) {
				anyNgon = true;
				break;
			}
		}
		if (!anyNgon) {
			return;
		}

		NkVector<uint32> newFaceStart;
		NkVector<uint32> newFaceVerts;
		newFaceStart.PushBack(0);
		for (uint32 fi = 0; fi < faceCount; ++fi) {
			const uint32 s = faceStart[fi];
			const uint32 n = faceStart[fi + 1] - s;
			if (n < 3) {
				continue;
			}
			for (uint32 i = 1; i + 1 < n; ++i) {
				newFaceVerts.PushBack(faceVerts[s]);
				newFaceVerts.PushBack(faceVerts[s + i]);
				newFaceVerts.PushBack(faceVerts[s + i + 1]);
				newFaceStart.PushBack(static_cast<uint32>(newFaceVerts.Size()));
			}
		}

		mEdit.BuildFromPolygons(polyVerts.Data(), static_cast<uint32>(polyVerts.Size()), newFaceStart.Data(),
								static_cast<uint32>(newFaceStart.Size()) - 1, newFaceVerts.Data());
		mBoundsDirty = true;
	}

	// =========================================================================
	// MergeByDistance — pas de commande dédiée côté NkEditMesh (qui ne fusionne
	// qu'une sélection entière en un point) : soudure géométrique par paires
	// implémentée ici, rechaînage demi-arête délégué à BuildFromPolygons.
	// =========================================================================

	uint32 NkEditableMesh::MergeByDistance(float32 threshold) noexcept {
		NkVector<NkVertex3D> polyVerts;
		NkVector<uint32> faceStart;
		NkVector<uint32> faceVerts;
		mEdit.ToPolygons(polyVerts, faceStart, faceVerts);

		const uint32 n = static_cast<uint32>(polyVerts.Size());
		if (n == 0) {
			return 0;
		}

		NkVector<uint32> remap;
		remap.Reserve(n);
		for (uint32 i = 0; i < n; ++i) {
			remap.PushBack(i);
		}

		const float32 thr2 = threshold * threshold;
		uint32 mergedCount = 0;
		for (uint32 i = 0; i < n; ++i) {
			if (remap[i] != i) {
				continue; // déjà fusionné dans un vertex précédent
			}
			for (uint32 j = i + 1; j < n; ++j) {
				if (remap[j] != j) {
					continue;
				}
				const NkVec3f d = polyVerts[j].pos - polyVerts[i].pos;
				if (d.Dot(d) <= thr2) {
					remap[j] = i;
					++mergedCount;
				}
			}
		}
		if (mergedCount == 0) {
			return 0;
		}

		NkVector<uint32> oldToNew;
		oldToNew.Resize(n, kNkInvalidIdx);
		NkVector<NkVertex3D> newVerts;
		newVerts.Reserve(n - mergedCount);
		for (uint32 i = 0; i < n; ++i) {
			if (remap[i] == i) {
				oldToNew[i] = static_cast<uint32>(newVerts.Size());
				newVerts.PushBack(polyVerts[i]);
			}
		}

		NkVector<uint32> newFaceVerts;
		newFaceVerts.Reserve(faceVerts.Size());
		for (uint32 i = 0; i < faceVerts.Size(); ++i) {
			const uint32 root = remap[faceVerts[i]];
			newFaceVerts.PushBack(oldToNew[root]);
		}

		mEdit.BuildFromPolygons(newVerts.Data(), static_cast<uint32>(newVerts.Size()), faceStart.Data(),
								static_cast<uint32>(faceStart.Size()) - 1, newFaceVerts.Data());
		mBoundsDirty = true;
		return mergedCount;
	}

	// =========================================================================
	// Export GPU (ToMeshDesc) — délégué à NkEditMesh::Triangulate().
	// =========================================================================

	void NkEditableMesh::ToMeshDesc(renderer::NkMeshDesc &out) const noexcept {
		mGpuVertexCache.Clear();
		mGpuIndexCache.Clear();
		mGpuTriFaceCache.Clear();
		mEdit.Triangulate(mGpuVertexCache, mGpuIndexCache, mGpuTriFaceCache);

		// NkVertexLayout est ambigu ici (nkentseu::NkVertexLayout dans NKRHI vs
		// nkentseu::renderer::NkVertexLayout) — qualification explicite requise.
		out = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(), mGpuVertexCache.Data(),
								 static_cast<uint32>(mGpuVertexCache.Size()), mGpuIndexCache.Data(),
								 static_cast<uint32>(mGpuIndexCache.Size()));
		out.bounds = GetBounds();
		out.keepCPU = true; // édition CPU : le CPU reste l'autorité (cf. NkMeshDesc::keepCPU).
		out.debugName = "NkEditableMesh";
	}

	// =========================================================================
	// Clone — copie profonde de la demi-arête (verts/hedges/faces sont des
	// NkVector publics de renderer::NkEditMesh).
	// =========================================================================

	NkEditableMesh NkEditableMesh::Clone() const noexcept {
		NkEditableMesh copy;
		copy.mEdit.verts.Reserve(mEdit.verts.Size());
		for (uint32 i = 0; i < mEdit.verts.Size(); ++i) {
			copy.mEdit.verts.PushBack(mEdit.verts[i]);
		}
		copy.mEdit.hedges.Reserve(mEdit.hedges.Size());
		for (uint32 i = 0; i < mEdit.hedges.Size(); ++i) {
			copy.mEdit.hedges.PushBack(mEdit.hedges[i]);
		}
		copy.mEdit.faces.Reserve(mEdit.faces.Size());
		for (uint32 i = 0; i < mEdit.faces.Size(); ++i) {
			copy.mEdit.faces.PushBack(mEdit.faces[i]);
		}
		copy.mBounds = mBounds;
		copy.mBoundsDirty = mBoundsDirty;
		return copy;
	}

	// =========================================================================
	// NON IMPLÉMENTÉ — aucun équivalent dans renderer::NkEditMesh. Corps
	// volontairement en no-op documenté (pas de fausse réussite silencieuse).
	// =========================================================================

	void NkEditableMesh::SmartUVProject(float32 /*angleLimit*/, float32 /*islandMargin*/) noexcept {
		logger.Warnf("[NkEditableMesh] SmartUVProject: NON IMPLEMENTE — aucun equivalent dans "
					 "renderer::NkEditMesh (TODO)\n");
	}

	void NkEditableMesh::CubeProject(float32 /*scale*/) noexcept {
		logger.Warnf("[NkEditableMesh] CubeProject: NON IMPLEMENTE — aucun equivalent dans "
					 "renderer::NkEditMesh (TODO)\n");
	}

	void NkEditableMesh::CylindricalProject(float32 /*scale*/) noexcept {
		logger.Warnf("[NkEditableMesh] CylindricalProject: NON IMPLEMENTE — aucun equivalent dans "
					 "renderer::NkEditMesh (TODO)\n");
	}

	void NkEditableMesh::GrowSelection() noexcept {
		logger.Warnf("[NkEditableMesh] GrowSelection: NON IMPLEMENTE — aucun equivalent dans "
					 "renderer::NkEditMesh (TODO)\n");
	}

	void NkEditableMesh::ShrinkSelection() noexcept {
		logger.Warnf("[NkEditableMesh] ShrinkSelection: NON IMPLEMENTE — aucun equivalent dans "
					 "renderer::NkEditMesh (TODO)\n");
	}

	// =========================================================================
	// FromMeshDesc — implémenté pour le cas standard (layout NkVertex3D, cf.
	// NkVertexLayout::Default3D()) via NkEditMesh::BuildFromIndexed().
	// =========================================================================

	NkEditableMesh NkEditableMesh::FromMeshDesc(const renderer::NkMeshDesc &desc) noexcept {
		NkEditableMesh mesh;
		if (!desc.vertices || !desc.indices || desc.vertexCount == 0 || desc.indexCount == 0) {
			return mesh;
		}
		// Suppose un layout NkVertex3D standard (voir NkEditableMesh.h) — pas
		// de réinterprétation générique d'un NkVertexLayout custom.
		const auto *verts = static_cast<const NkVertex3D *>(desc.vertices);
		mesh.mEdit.BuildFromIndexed(verts, desc.vertexCount, desc.indices, desc.indexCount, /*quadify=*/false);
		mesh.mBoundsDirty = true;
		return mesh;
	}

} // namespace nkentseu
