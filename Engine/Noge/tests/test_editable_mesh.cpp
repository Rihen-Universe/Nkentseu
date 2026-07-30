// =============================================================================
// Engine/Noge/tests/test_editable_mesh.cpp
// =============================================================================
// Premier test unitaire réel pour Engine/Noge (aucun test n'existait avant ce
// fichier — voir ROADMAP.md, section "Phase A / Incrément 1"). Couvre
// NkEditableMesh, choisi comme premier incrément CPU-only de la Phase A —
// désormais une fine couche d'adaptation au-dessus de renderer::NkEditMesh
// (demi-arête n-gon, Kernel/Runtime/NKRenderer/src/NKRenderer/Mesh/
// NkEditMesh.h) plutôt qu'une réimplémentation dupliquée à faces de taille
// fixe (cf. note de révision 2026-07-23 en tête de NkEditableMesh.h).
// =============================================================================
#include <Unitest/Unitest.h>
#include <Unitest/TestMacro.h>

#include "Noge/Modeling/NkEditableMesh.h"

using namespace nkentseu;

// -----------------------------------------------------------------------------
// Construit un triangle unique et vérifie topologie + normale + bounds.
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, SingleTriangle) {
	NkEditableMesh mesh;
	const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
	const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
	const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
	const uint32 f0 = mesh.AddTri(v0, v1, v2);

	ASSERT_EQUAL(3, static_cast<int>(mesh.VertexCount()));
	ASSERT_EQUAL(1, static_cast<int>(mesh.FaceCount()));
	ASSERT_EQUAL(3, static_cast<int>(mesh.EdgeCount())); // triangle isolé : 3 demi-arêtes, aucun twin
	ASSERT_EQUAL(3, static_cast<int>(mesh.Edit().FaceSize(f0)));

	// Normale CCW autour de +Z pour un triangle dans le plan XY.
	const auto &face = mesh.Faces()[f0];
	ASSERT_NEAR(0.0f, face.normal.x, 0.0001f);
	ASSERT_NEAR(0.0f, face.normal.y, 0.0001f);
	ASSERT_NEAR(1.0f, face.normal.z, 0.0001f);

	// Bounds = boîte englobant les 3 sommets.
	const auto &bounds = mesh.GetBounds();
	ASSERT_NEAR(0.0f, bounds.min.x, 0.0001f);
	ASSERT_NEAR(0.0f, bounds.min.y, 0.0001f);
	ASSERT_NEAR(1.0f, bounds.max.x, 0.0001f);
	ASSERT_NEAR(1.0f, bounds.max.y, 0.0001f);
}

// -----------------------------------------------------------------------------
// Deux triangles partageant une arête : le twin doit être résolu correctement.
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, SharedEdgeTwin) {
	NkEditableMesh mesh;
	const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
	const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
	const uint32 v2 = mesh.AddVertex({1.f, 1.f, 0.f});
	const uint32 v3 = mesh.AddVertex({0.f, 1.f, 0.f});

	mesh.AddTri(v0, v1, v2);
	mesh.AddTri(v0, v2, v3);

	ASSERT_EQUAL(4, static_cast<int>(mesh.VertexCount()));
	ASSERT_EQUAL(2, static_cast<int>(mesh.FaceCount()));
	// 6 demi-arêtes créées (3 par triangle), dont une paire de twins sur la
	// diagonale partagée (v0-v2).
	ASSERT_EQUAL(6, static_cast<int>(mesh.EdgeCount()));

	bool foundTwinPair = false;
	const auto &edges = mesh.Edges();
	for (uint32 i = 0; i < edges.Size(); ++i) {
		if (edges[i].twin != kNkInvalidIdx) {
			ASSERT_EQUAL(static_cast<int>(i), static_cast<int>(edges[edges[i].twin].twin));
			foundTwinPair = true;
		}
	}
	ASSERT_TRUE(foundTwinPair);
}

// -----------------------------------------------------------------------------
// Quad (n-gon à 4 côtés) : topologie + triangulation Fan.
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, QuadAndTriangulate) {
	NkEditableMesh mesh;
	const uint32 v0 = mesh.AddVertex({-0.5f, -0.5f, 0.f});
	const uint32 v1 = mesh.AddVertex({0.5f, -0.5f, 0.f});
	const uint32 v2 = mesh.AddVertex({0.5f, 0.5f, 0.f});
	const uint32 v3 = mesh.AddVertex({-0.5f, 0.5f, 0.f});
	mesh.AddQuad(v0, v1, v2, v3);

	ASSERT_EQUAL(1, static_cast<int>(mesh.FaceCount()));
	ASSERT_EQUAL(4, static_cast<int>(mesh.Edit().FaceSize(0)));

	mesh.Triangulate(0); // Fan
	ASSERT_EQUAL(2, static_cast<int>(mesh.FaceCount()));
	ASSERT_EQUAL(3, static_cast<int>(mesh.Edit().FaceSize(0)));
	ASSERT_EQUAL(3, static_cast<int>(mesh.Edit().FaceSize(1)));
}

// -----------------------------------------------------------------------------
// AddPolygon : n-gon à 5 côtés (impossible avec l'ancienne implémentation
// NkEditFace::kMaxVerts=4 — c'est précisément la limitation corrigée par la
// révision 2026-07-23 qui délègue à renderer::NkEditMesh).
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, PentagonNgon) {
	NkEditableMesh mesh;
	uint32 ids[5];
	ids[0] = mesh.AddVertex({1.0f, 0.0f, 0.f});
	ids[1] = mesh.AddVertex({0.309f, 0.951f, 0.f});
	ids[2] = mesh.AddVertex({-0.809f, 0.588f, 0.f});
	ids[3] = mesh.AddVertex({-0.809f, -0.588f, 0.f});
	ids[4] = mesh.AddVertex({0.309f, -0.951f, 0.f});

	const uint32 f0 = mesh.AddPolygon(NkSpan<const uint32>(ids, 5));

	ASSERT_EQUAL(5, static_cast<int>(mesh.VertexCount()));
	ASSERT_EQUAL(1, static_cast<int>(mesh.FaceCount()));
	ASSERT_EQUAL(5, static_cast<int>(mesh.Edit().FaceSize(f0)));
	ASSERT_EQUAL(5, static_cast<int>(mesh.EdgeCount()));
}

// -----------------------------------------------------------------------------
// RecalcNormals(smooth) : un vertex partagé par deux faces coplanaires reçoit
// une normale moyenne colinéaire à chaque normale de face.
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, RecalcNormalsSmooth) {
	NkEditableMesh mesh;
	const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
	const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
	const uint32 v2 = mesh.AddVertex({1.f, 1.f, 0.f});
	const uint32 v3 = mesh.AddVertex({0.f, 1.f, 0.f});
	mesh.AddTri(v0, v1, v2);
	mesh.AddTri(v0, v2, v3);

	mesh.RecalcNormals(true);

	// Les deux faces sont coplanaires (plan XY) : la normale moyenne au
	// vertex partagé v0/v2 reste (0,0,1).
	const auto &verts = mesh.Vertices();
	ASSERT_NEAR(0.0f, verts[v0].normal.x, 0.0001f);
	ASSERT_NEAR(0.0f, verts[v0].normal.y, 0.0001f);
	ASSERT_NEAR(1.0f, verts[v0].normal.z, 0.0001f);
	ASSERT_NEAR(1.0f, verts[v2].normal.z, 0.0001f);
}

// -----------------------------------------------------------------------------
// FlipNormals inverse le winding et le signe de la normale.
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, FlipNormals) {
	NkEditableMesh mesh;
	const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
	const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
	const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
	mesh.AddTri(v0, v1, v2);

	ASSERT_NEAR(1.0f, mesh.Faces()[0].normal.z, 0.0001f);

	mesh.FlipNormals();

	ASSERT_NEAR(-1.0f, mesh.Faces()[0].normal.z, 0.0001f);
	// Le premier sommet de la boucle de face reste inchangé (v0).
	NkVector<uint32> fv;
	mesh.Edit().GetFaceVerts(0, fv);
	ASSERT_EQUAL(static_cast<int>(v0), static_cast<int>(fv[0]));
}

// -----------------------------------------------------------------------------
// MergeByDistance fusionne deux vertices dupliqués au même endroit.
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, MergeByDistance) {
	NkEditableMesh mesh;
	const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
	const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
	const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
	// v3 est un doublon (quasi-identique) de v0.
	const uint32 v3 = mesh.AddVertex({0.00001f, 0.f, 0.f});
	mesh.AddTri(v0, v1, v2);
	mesh.AddTri(v3, v2, v1); // utilise le doublon plutôt que v0

	ASSERT_EQUAL(4, static_cast<int>(mesh.VertexCount()));

	const uint32 merged = mesh.MergeByDistance(0.001f);

	ASSERT_EQUAL(1, static_cast<int>(merged));
	ASSERT_EQUAL(3, static_cast<int>(mesh.VertexCount()));
	// Les deux faces référencent maintenant le même vertex fusionné en position (0,0,0).
	NkVector<uint32> fv;
	mesh.Edit().GetFaceVerts(0, fv);
	ASSERT_EQUAL(0, static_cast<int>(fv[0]));
}

// -----------------------------------------------------------------------------
// Clone produit une copie indépendante (deep copy).
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, CloneIsIndependent) {
	NkEditableMesh mesh;
	const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
	const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
	const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
	mesh.AddTri(v0, v1, v2);

	NkEditableMesh clone = mesh.Clone();
	ASSERT_EQUAL(3, static_cast<int>(clone.VertexCount()));
	ASSERT_EQUAL(1, static_cast<int>(clone.FaceCount()));

	// Modifier le clone ne doit pas affecter l'original.
	clone.AddVertex({5.f, 5.f, 5.f});
	ASSERT_EQUAL(4, static_cast<int>(clone.VertexCount()));
	ASSERT_EQUAL(3, static_cast<int>(mesh.VertexCount()));
}

// -----------------------------------------------------------------------------
// ToMeshDesc : export GPU (indices triangulés, vertex count, bounds) —
// délégué à renderer::NkEditMesh::Triangulate().
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, ToMeshDescExportsTriangulatedGeometry) {
	NkEditableMesh mesh;
	const uint32 v0 = mesh.AddVertex({-0.5f, -0.5f, 0.f}, {0.f, 0.f});
	const uint32 v1 = mesh.AddVertex({0.5f, -0.5f, 0.f}, {1.f, 0.f});
	const uint32 v2 = mesh.AddVertex({0.5f, 0.5f, 0.f}, {1.f, 1.f});
	const uint32 v3 = mesh.AddVertex({-0.5f, 0.5f, 0.f}, {0.f, 1.f});
	mesh.AddQuad(v0, v1, v2, v3);
	mesh.RecalcNormals(true);

	renderer::NkMeshDesc desc;
	mesh.ToMeshDesc(desc);

	ASSERT_EQUAL(4, static_cast<int>(desc.vertexCount));
	ASSERT_EQUAL(6, static_cast<int>(desc.indexCount)); // 1 quad -> 2 triangles -> 6 indices
	ASSERT_NOT_NULL(desc.vertices);
	ASSERT_NOT_NULL(desc.indices);
	ASSERT_NEAR(-0.5f, desc.bounds.min.x, 0.0001f);
	ASSERT_NEAR(0.5f, desc.bounds.max.x, 0.0001f);
}

// -----------------------------------------------------------------------------
// Sélection : SelectAll / GetSelectedFaces / DeselectAll / InvertSelection.
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, Selection) {
	NkEditableMesh mesh;
	const uint32 v0 = mesh.AddVertex({0.f, 0.f, 0.f});
	const uint32 v1 = mesh.AddVertex({1.f, 0.f, 0.f});
	const uint32 v2 = mesh.AddVertex({0.f, 1.f, 0.f});
	mesh.AddTri(v0, v1, v2);

	mesh.SelectAll();
	NkVector<uint32> selectedFaces;
	mesh.GetSelectedFaces(selectedFaces);
	ASSERT_EQUAL(1, static_cast<int>(selectedFaces.Size()));

	mesh.DeselectAll();
	selectedFaces.Clear();
	mesh.GetSelectedFaces(selectedFaces);
	ASSERT_EQUAL(0, static_cast<int>(selectedFaces.Size()));

	mesh.InvertSelection();
	selectedFaces.Clear();
	mesh.GetSelectedFaces(selectedFaces);
	ASSERT_EQUAL(1, static_cast<int>(selectedFaces.Size()));
}

// -----------------------------------------------------------------------------
// ExtrudeFaces : délégué à renderer::NkEditMesh::ExtrudeSelectedFaces —
// vérifie que la géométrie change réellement (plus de sommets/faces).
// -----------------------------------------------------------------------------
TEST_CASE(NogeEditableMesh, ExtrudeFacesDelegatesToNkEditMesh) {
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

	// Une extrusion de face crée de nouveaux sommets (le "capuchon" détaché
	// du reste) et de nouvelles faces (parois latérales).
	ASSERT_TRUE(mesh.VertexCount() > vertsBefore);
	ASSERT_TRUE(mesh.FaceCount() > facesBefore);
}
