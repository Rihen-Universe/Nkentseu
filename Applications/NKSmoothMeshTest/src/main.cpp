// =============================================================================
// NKSmoothMeshTest — surface LISSE (Surface Nets) vs proxy cubique.
//   Champ de métaballs 40x40x40 (forme organique fusionnée) -> maillage lisse
//   quad-dominant (gen::SurfaceNets), exporté en OBJ. On compare aussi au maillage
//   cubique (VoxelsToMesh) pour montrer le gain. Base propre pour la retopologie.
// =============================================================================
#include "NKGen/NkGen.h"
#include "NKContainers/Sequential/NkVector.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::ai;

int main() {
	printf("=== NKSmoothMeshTest : surface lisse (Surface Nets) ===\n\n");

	// Champ scalaire : 3 métaballs fusionnées sur une grille 40^3, domaine [-2,2].
	const uint32 N = 40;
	NkVector<float> field;
	field.Resize(N * N * N);
	const float centers[3][3] = {{-0.6f, 0.f, 0.f}, {0.6f, 0.f, 0.f}, {0.f, 0.7f, 0.2f}};
	for (uint32 z = 0; z < N; ++z)
		for (uint32 y = 0; y < N; ++y)
			for (uint32 x = 0; x < N; ++x) {
				float px = -2.f + 4.f * (float)x / (float)(N - 1);
				float py = -2.f + 4.f * (float)y / (float)(N - 1);
				float pz = -2.f + 4.f * (float)z / (float)(N - 1);
				float s = 0.f;
				for (int b = 0; b < 3; ++b) {
					float dx = px - centers[b][0], dy = py - centers[b][1], dz = pz - centers[b][2];
					s += 1.0f / (dx * dx + dy * dy + dz * dz + 0.05f);
				}
				field[x + N * (y + N * z)] = s;
			}
	const float iso = 3.0f;

	// Maillage LISSE (surface nets).
	gen::NkMesh smooth = gen::SurfaceNets(field.Data(), N, N, N, iso, /*cell*/ 1.0f);
	printf("  Surface Nets (lisse)  : %u sommets, %u triangles\n", smooth.VertexCount(), smooth.TriangleCount());

	// Maillage CUBIQUE (proxy) sur la même occupation, pour comparaison.
	// (VoxelsToMesh attend une grille cubique G^3 ; on réutilise N.)
	gen::NkMesh blocky = gen::VoxelsToMesh(field.Data(), N, iso, 1.0f);
	printf("  VoxelsToMesh (proxy)  : %u sommets, %u triangles\n", blocky.VertexCount(), blocky.TriangleCount());

	gen::ComputeNormals(smooth); // normales par sommet -> OBJ prêt-moteur (vn + v//vn)
	bool savedSmooth = gen::SaveMeshObj("Build/nkgen_smooth.obj", smooth);
	printf("  export OBJ lisse : %s -> Build/nkgen_smooth.obj (normales: %s)\n", savedSmooth ? "OK" : "KO",
		   smooth.HasNormals() ? "oui" : "non");

	// RETOPOLOGIE (passe 1) : décimation par clustering -> maillage allégé.
	printf("\n-- Retopologie (passe 1 : décimation par clustering) --\n");
	gen::NkMesh retopo = gen::DecimateClustering(smooth, /*grille*/ 14);
	printf("  décimé : %u sommets, %u triangles  (réduction : %.0f%% des triangles)\n", retopo.VertexCount(),
		   retopo.TriangleCount(),
		   smooth.TriangleCount() ? 100.0 * (1.0 - (double)retopo.TriangleCount() / (double)smooth.TriangleCount())
								  : 0.0);
	bool savedRetopo = gen::SaveMeshObj("Build/nkgen_retopo.obj", retopo);
	printf("  export OBJ retopo : %s -> Build/nkgen_retopo.obj\n", savedRetopo ? "OK" : "KO");

	// RETOPOLOGIE (sortie QUAD) : maillage quad-dominant natif (Surface Nets quads).
	printf("\n-- Retopologie (maillage QUAD natif) --\n");
	gen::NkMesh quadMesh = gen::SurfaceNetsQuads(field.Data(), N, N, N, iso, 1.0f);
	printf("  quads : %u sommets, %u faces quad\n", quadMesh.VertexCount(), quadMesh.QuadCount());
	bool savedQuads = gen::SaveMeshObj("Build/nkgen_quads.obj", quadMesh);
	printf("  export OBJ quad : %s -> Build/nkgen_quads.obj\n", savedQuads ? "OK" : "KO");

	// Validation.
	int pass = 0, fail = 0;
	auto check = [&](bool ok, const char *n) {
		(ok ? pass : fail)++;
		printf("  [ %s ] %s\n", ok ? "OK" : "KO", n);
	};
	auto idxValid = [](const gen::NkMesh &m) {
		for (uint32 i = 0; i < m.triangles.Size(); ++i)
			if (m.triangles[i] >= m.VertexCount())
				return false;
		return true;
	};

	check(smooth.VertexCount() > 500, "surface lisse non triviale (>500 sommets)");
	check(idxValid(smooth), "surface lisse : indices valides");
	check(savedSmooth, "OBJ lisse écrit");
	check(retopo.TriangleCount() > 0 && retopo.TriangleCount() < smooth.TriangleCount(),
		  "décimation : maillage allégé et non vide");
	check(idxValid(retopo), "maillage décimé : indices valides");
	check(savedRetopo, "OBJ retopo écrit (prêt pour UV/rig)");

	bool quadIdxOk = true;
	for (uint32 i = 0; i < quadMesh.quads.Size(); ++i)
		if (quadMesh.quads[i] >= quadMesh.VertexCount()) {
			quadIdxOk = false;
			break;
		}
	check(quadMesh.QuadCount() > 0 && quadIdxOk, "maillage QUAD natif valide (faces à 4 côtés)");
	check(savedQuads, "OBJ quad écrit");

	printf("\n=== Résultat : %d OK, %d échec(s) ===\n", pass, fail);
	return fail == 0 ? 0 : 1;
}
