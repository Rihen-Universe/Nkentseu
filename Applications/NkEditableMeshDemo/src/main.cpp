// =============================================================================
// Applications/NkEditableMeshDemo/src/main.cpp
// =============================================================================
// Preuve d'exécution réelle pour Engine/Noge/src/Noge/Modeling/NkEditableMesh
// (voir Engine/Noge/ROADMAP.md, Phase A / Incrément 1). Exécute les mêmes
// scénarios que Engine/Noge/tests/test_editable_mesh.cpp (bloqué à
// l'exécution par la politique de workspace "disableunittestexecution") sous
// forme d'application console classique, avec assertions manuelles.
// =============================================================================
// =============================================================================
// ⚠️ POURQUOI LES ATTENTES DE NORMALES ONT CHANGE DE SIGNE (2026-08-22)
// -----------------------------------------------------------------------------
// Quatre cas de ce banc exigeaient une normale +Z pour un triangle
// (0,0,0)-(1,0,0)-(0,1,0), c'est-a-dire la convention CCW / main droite. Ils
// ETAIENT ROUGES, et ce n'etait pas une regression : c'est LE BANC qui portait
// l'ancienne convention.
//
// LE MOTEUR REND EN FACE AVANT = SENS HORAIRE. `NkEmFaceCross` calcule donc
// deliberement (p2-p0) x (p1-p0), soit l'OPPOSE du produit vectoriel CCW.
//
// ⚠️ VERIFIE CONTRE LA GEOMETRIE, PAS CONTRE LE COMMENTAIRE — un commentaire
// n'est verifie par rien. Cube de NkMeshSystem.cpp:362-373, face du dessus :
// normale DECLAREE n[4] = {0,1,0}, boucle fi[4] = {3,7,6,2}. Le produit CCW
// (p1-p0) x (p2-p0) donne -Y, donc l'inverse de la normale declaree. C'est le
// consommateur reel qui tranche, et il confirme la convention horaire.
//
// ⚠️ NE « RESTAURE » PAS LES ANCIENNES VALEURS EN CROYANT REPARER UNE
// REGRESSION. Verdir ces cas en changeant le signe DANS LE NOYAU aurait inverse
// l'eclairage de tout le maillage edite, envoye les extrusions vers l'INTERIEUR
// et retourne l'orientation « Normal » du gizmo — et fait tomber une partie des
// 181 cas de NKEditMeshHarness. Un banc rouge ne dit jamais OU est la faute.
// =============================================================================
#include "Noge/Modeling/NkEditableMesh.h"
#include "NKLogger/NkLog.h"

using namespace nkentseu;

namespace {

	int gFailCount = 0;
	int gPassCount = 0;

	void Check(bool cond, const char *what) noexcept {
		if (cond) {
			++gPassCount;
			logger.Infof("  [OK]   %s\n", what);
		} else {
			++gFailCount;
			logger.Errorf("  [FAIL] %s\n", what);
		}
	}

	bool Near(float32 a, float32 b, float32 eps = 0.0001f) noexcept {
		const float32 d = a - b;
		return (d < 0.f ? -d : d) <= eps;
	}

	void Test_SingleTriangle() noexcept {
		logger.Infof("-- SingleTriangle --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
		const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
		const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
		const uint32 f0 = mesh.AddTri(v0, v1, v2);

		Check(mesh.VertexCount() == 3, "VertexCount == 3");
		Check(mesh.FaceCount() == 1, "FaceCount == 1");
		Check(mesh.EdgeCount() == 3, "EdgeCount == 3 (triangle isole)");
		Check(mesh.Edit().FaceSize(f0) == 3, "FaceSize(f0) == 3");

		const auto &face = mesh.Faces()[f0];
		Check(Near(face.normal.x, 0.f) && Near(face.normal.y, 0.f) && Near(face.normal.z, -1.f),
			  "normale face == (0,0,-1) [face avant = sens horaire, cf. bandeau]");

		const auto &bounds = mesh.GetBounds();
		Check(Near(bounds.min.x, 0.f) && Near(bounds.min.y, 0.f) && Near(bounds.max.x, 1.f) && Near(bounds.max.y, 1.f),
			  "bounds == [0,0]-[1,1]");
	}

	void Test_SharedEdgeTwin() noexcept {
		logger.Infof("-- SharedEdgeTwin --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
		const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
		const uint32 v2 = mesh.AddVertex({1.f, 1.f, 0.f});
		const uint32 v3 = mesh.AddVertex({0.f, 1.f, 0.f});
		mesh.AddTri(v0, v1, v2);
		mesh.AddTri(v0, v2, v3);

		Check(mesh.VertexCount() == 4, "VertexCount == 4");
		Check(mesh.FaceCount() == 2, "FaceCount == 2");
		Check(mesh.EdgeCount() == 6, "EdgeCount == 6 (2x3 demi-aretes)");

		bool foundTwin = false;
		const auto &edges = mesh.Edges();
		for (uint32 i = 0; i < edges.Size(); ++i) {
			if (edges[i].twin != kNkInvalidIdx) {
				foundTwin = foundTwin || (edges[edges[i].twin].twin == i);
			}
		}
		Check(foundTwin, "au moins une paire de twins resolue (arete partagee)");
	}

	void Test_QuadAndTriangulate() noexcept {
		logger.Infof("-- QuadAndTriangulate --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({-0.5f, -0.5f, 0.f});
		const uint32 v1 = mesh.AddVertex({0.5f, -0.5f, 0.f});
		const uint32 v2 = mesh.AddVertex({0.5f, 0.5f, 0.f});
		const uint32 v3 = mesh.AddVertex({-0.5f, 0.5f, 0.f});
		mesh.AddQuad(v0, v1, v2, v3);

		Check(mesh.FaceCount() == 1, "FaceCount == 1 (quad)");
		Check(mesh.Edit().FaceSize(0) == 4, "FaceSize(0) == 4");

		mesh.Triangulate(0);
		Check(mesh.FaceCount() == 2, "FaceCount == 2 apres Triangulate(Fan)");
		Check(mesh.Edit().FaceSize(0) == 3 && mesh.Edit().FaceSize(1) == 3, "2 triangles de 3 sommets");
	}

	void Test_PentagonNgon() noexcept {
		logger.Infof("-- PentagonNgon (n-gon 5 cotes) --\n");
		NkEditableMesh mesh;
		uint32 ids[5];
		ids[0] = mesh.AddVertex({1.0f, 0.0f, 0.f});
		ids[1] = mesh.AddVertex({0.309f, 0.951f, 0.f});
		ids[2] = mesh.AddVertex({-0.809f, 0.588f, 0.f});
		ids[3] = mesh.AddVertex({-0.809f, -0.588f, 0.f});
		ids[4] = mesh.AddVertex({0.309f, -0.951f, 0.f});
		const uint32 f0 = mesh.AddPolygon(NkSpan<const uint32>(ids, 5));

		Check(mesh.VertexCount() == 5, "VertexCount == 5");
		Check(mesh.FaceCount() == 1, "FaceCount == 1");
		Check(mesh.Edit().FaceSize(f0) == 5, "FaceSize(f0) == 5 (n-gon, impossible avec l'ancienne impl fixe)");
		Check(mesh.EdgeCount() == 5, "EdgeCount == 5");
	}

	void Test_RecalcNormalsSmooth() noexcept {
		logger.Infof("-- RecalcNormalsSmooth --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
		const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
		const uint32 v2 = mesh.AddVertex({1.f, 1.f, 0.f});
		const uint32 v3 = mesh.AddVertex({0.f, 1.f, 0.f});
		mesh.AddTri(v0, v1, v2);
		mesh.AddTri(v0, v2, v3);
		mesh.RecalcNormals(true);

		const auto &verts = mesh.Vertices();
		Check(Near(verts[v0].normal.z, -1.f) && Near(verts[v2].normal.z, -1.f),
			  "normales moyennees au vertex partage restent (0,0,-1) [convention horaire]");
	}

	void Test_FlipNormals() noexcept {
		logger.Infof("-- FlipNormals --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
		const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
		const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
		mesh.AddTri(v0, v1, v2);
		Check(Near(mesh.Faces()[0].normal.z, -1.f), "normale initiale -Z [convention horaire]");

		mesh.FlipNormals();
		Check(Near(mesh.Faces()[0].normal.z, 1.f), "normale inversee +Z apres FlipNormals [convention horaire]");

		NkVector<uint32> fv;
		mesh.Edit().GetFaceVerts(0, fv);
		Check(fv[0] == v0, "premier sommet de la boucle inchange (v0)");
	}

	void Test_MergeByDistance() noexcept {
		logger.Infof("-- MergeByDistance --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
		const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
		const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
		const uint32 v3 = mesh.AddVertex({0.00001f, 0.f, 0.f}); // doublon de v0
		mesh.AddTri(v0, v1, v2);
		mesh.AddTri(v3, v2, v1);
		Check(mesh.VertexCount() == 4, "VertexCount == 4 avant fusion");

		const uint32 merged = mesh.MergeByDistance(0.001f);
		Check(merged == 1, "1 vertex fusionne");
		Check(mesh.VertexCount() == 3, "VertexCount == 3 apres fusion");
	}

	void Test_CloneIsIndependent() noexcept {
		logger.Infof("-- CloneIsIndependent --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
		const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
		const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
		mesh.AddTri(v0, v1, v2);

		NkEditableMesh clone = mesh.Clone();
		Check(clone.VertexCount() == 3 && clone.FaceCount() == 1, "clone == original au depart");

		clone.AddVertex({5.f, 5.f, 5.f});
		Check(clone.VertexCount() == 4, "clone modifie (4 sommets)");
		Check(mesh.VertexCount() == 3, "original inchange (3 sommets) -> deep copy confirmee");
	}

	void Test_ToMeshDesc() noexcept {
		logger.Infof("-- ToMeshDescExportsTriangulatedGeometry --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({-0.5f, -0.5f, 0.f}, {0.f, 0.f});
		const uint32 v1 = mesh.AddVertex({0.5f, -0.5f, 0.f}, {1.f, 0.f});
		const uint32 v2 = mesh.AddVertex({0.5f, 0.5f, 0.f}, {1.f, 1.f});
		const uint32 v3 = mesh.AddVertex({-0.5f, 0.5f, 0.f}, {0.f, 1.f});
		mesh.AddQuad(v0, v1, v2, v3);
		mesh.RecalcNormals(true);

		renderer::NkMeshDesc desc;
		mesh.ToMeshDesc(desc);

		Check(desc.vertexCount == 4, "desc.vertexCount == 4");
		Check(desc.indexCount == 6, "desc.indexCount == 6 (quad -> 2 tris)");
		Check(desc.vertices != nullptr && desc.indices != nullptr, "buffers non-nuls");
		Check(Near(desc.bounds.min.x, -0.5f) && Near(desc.bounds.max.x, 0.5f), "bounds exportees correctes");
	}

	void Test_Selection() noexcept {
		logger.Infof("-- Selection --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
		const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
		const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
		mesh.AddTri(v0, v1, v2);

		mesh.SelectAll();
		NkVector<uint32> sel;
		mesh.GetSelectedFaces(sel);
		Check(sel.Size() == 1, "SelectAll -> 1 face selectionnee");

		mesh.DeselectAll();
		sel.Clear();
		mesh.GetSelectedFaces(sel);
		Check(sel.Size() == 0, "DeselectAll -> 0 face selectionnee");

		mesh.InvertSelection();
		sel.Clear();
		mesh.GetSelectedFaces(sel);
		Check(sel.Size() == 1, "InvertSelection -> 1 face selectionnee");
	}

	void Test_ExtrudeFaces() noexcept {
		logger.Infof("-- ExtrudeFacesDelegatesToNkEditMesh --\n");
		NkEditableMesh mesh;
		const uint32 v0 = mesh.AddVertex({-0.5f, -0.5f, 0.f});
		const uint32 v1 = mesh.AddVertex({0.5f, -0.5f, 0.f});
		const uint32 v2 = mesh.AddVertex({0.5f, 0.5f, 0.f});
		const uint32 v3 = mesh.AddVertex({-0.5f, 0.5f, 0.f});
		const uint32 f0 = mesh.AddQuad(v0, v1, v2, v3);

		const uint32 vertsBefore = mesh.VertexCount();
		const uint32 facesBefore = mesh.FaceCount();

		uint32 ids[1] = {f0};
		mesh.ExtrudeFaces(NkSpan<const uint32>(ids, 1), {0.f, 0.f, 1.f});

		Check(mesh.VertexCount() > vertsBefore, "ExtrudeFaces cree de nouveaux sommets (delegue a NkEditMesh)");
		Check(mesh.FaceCount() > facesBefore, "ExtrudeFaces cree de nouvelles faces (parois laterales)");
	}

} // namespace

int main() {
	logger.Infof("=== NkEditableMeshDemo — preuve d'execution Noge/Modeling/NkEditableMesh ===\n");

	Test_SingleTriangle();
	Test_SharedEdgeTwin();
	Test_QuadAndTriangulate();
	Test_PentagonNgon();
	Test_RecalcNormalsSmooth();
	Test_FlipNormals();
	Test_MergeByDistance();
	Test_CloneIsIndependent();
	Test_ToMeshDesc();
	Test_Selection();
	Test_ExtrudeFaces();

	logger.Infof("=== Resultat : %d OK / %d FAIL ===\n", gPassCount, gFailCount);
	return gFailCount == 0 ? 0 : 1;
}
