#include "pch.h"
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/MeshSculpt/NkMeshSculpt.cpp
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   Application d'un trait de brosse aux sommets d'un NkEditMesh.
// -----------------------------------------------------------------------------

#include "NKRenderer/Tools/MeshSculpt/NkMeshSculpt.h"

#include "NKContainers/Sequential/NkVector.h"

#include <cmath>

namespace nkentseu {
	namespace renderer {

		namespace {
			inline float32 Dot3(const NkVec3f &a, const NkVec3f &b) noexcept {
				return a.x * b.x + a.y * b.y + a.z * b.z;
			}
		} // namespace

		float32 NkSculptSignedVolume(const NkEditMesh &mesh) noexcept {
			// Somme des produits mixtes sur un eventail par face. Pour une surface
			// fermee, c'est SIX fois le volume signe ; on divise donc par 6. Pour
			// une surface ouverte la valeur n'est pas un volume, mais elle reste
			// une fonctionnelle CONTINUE et ORIENTEE des positions -- ce qui suffit
			// a dire si un trait a pousse la matiere dehors ou dedans.
			float64 acc = 0.0;
			NkVector<NkEmId> loop;
			for (uint32 f = 0; f < mesh.FaceCount(); ++f) {
				if (!mesh.faces[f].alive)
					continue;
				loop.Clear();
				mesh.GetFaceVerts((NkEmId)f, loop);
				if (loop.Size() < 3)
					continue;
				const NkVec3f &p0 = mesh.verts[loop[0]].pos;
				for (uint32 k = 1; k + 1 < (uint32)loop.Size(); ++k) {
					const NkVec3f &p1 = mesh.verts[loop[k]].pos;
					const NkVec3f &p2 = mesh.verts[loop[k + 1]].pos;
					acc += (float64)Dot3(p0, p1.Cross(p2));
				}
			}
			return (float32)(acc / 6.0);
		}

		NkSculptApply NkSculptApplyStroke(NkEditMesh &mesh, const NkBrushDesc &brush,
										  const NkSculptPoint *points, uint32 count) noexcept {
			NkSculptApply out;

			// ── LE ZERO : SORTIR AVANT D'ECRIRE QUOI QUE CE SOIT ────────────────
			// ⚠️ Y compris les normales. Un `RecomputeNormals()` de precaution
			//    suffirait a faire bouger des octets sur un trait qui ne deforme
			//    rien, et la comparaison au bit deviendrait impossible -- on aurait
			//    perdu le seul critere qu'un bug ne peut pas satisfaire par hasard.
			if (!brush.valid || !points || count == 0)
				return out;
			if (mesh.VertCount() == 0)
				return out;
			const float32 amp = brush.strength * brush.dir;
			if (amp == 0.f)
				return out;

			// ── SOUDURE LOGIQUE ─────────────────────────────────────────────────
			// Deux sommets EXACTEMENT au meme endroit sont un seul coin. Sans ce
			// regroupement, les 3 copies d'un coin de cube partent chacune le long
			// de SA normale et le cube se dechire. (Meme raison que dans la
			// signature du harnais : « une primitive dont les faces dupliquent
			// leurs sommets n'aurait aucune arete partagee ».)
			NkVector<uint32> canon;
			mesh.BuildVertexMerge(canon);
			const uint32 vc = mesh.VertCount();
			if (canon.Size() < vc)
				return out; // instrument incoherent : on refuse plutot que deviner

			// Position et normale MOYENNES par groupe.
			NkVector<NkVec3f> gPos, gNrm;
			NkVector<uint32> gCnt;
			gPos.Resize(vc);
			gNrm.Resize(vc);
			gCnt.Resize(vc);
			for (uint32 i = 0; i < vc; ++i) {
				gPos[i] = NkVec3f{0.f, 0.f, 0.f};
				gNrm[i] = NkVec3f{0.f, 0.f, 0.f};
				gCnt[i] = 0;
			}
			for (uint32 i = 0; i < vc; ++i) {
				const uint32 c = canon[i];
				if (c >= vc)
					continue;
				gPos[c] = gPos[c] + mesh.verts[i].pos;
				gNrm[c] = gNrm[c] + mesh.verts[i].normal;
				gCnt[c]++;
			}
			for (uint32 i = 0; i < vc; ++i)
				if (gCnt[i] > 1) {
					const float32 inv = 1.f / (float32)gCnt[i];
					gPos[i] = gPos[i] * inv;
					gNrm[i] = gNrm[i] * inv;
				}

			// ── DEPLACEMENT PAR GROUPE ──────────────────────────────────────────
			NkVector<NkVec3f> disp;
			disp.Resize(vc);
			for (uint32 i = 0; i < vc; ++i)
				disp[i] = NkVec3f{0.f, 0.f, 0.f};

			uint32 groupsTouched = 0;
			for (uint32 g = 0; g < vc; ++g) {
				if (gCnt[g] == 0)
					continue; // pas un representant de groupe
				bool touched = false;
				NkVec3f acc{0.f, 0.f, 0.f};

				for (uint32 p = 0; p < count; ++p) {
					const NkSculptPoint &pt = points[p];
					const float32 r = (pt.radius > 0.f) ? pt.radius : brush.radius;
					if (r <= 0.f)
						continue;
					const NkVec3f d = gPos[g] - pt.pos;
					const float32 dist = sqrtf(Dot3(d, d));
					// ⚠️ LA ZONE : strictement au-dela du rayon, on ne touche pas.
					if (dist > r)
						continue;
					float32 pr = pt.pressure;
					if (pr < 0.f)
						pr = 0.f;
					if (pr > 1.f)
						pr = 1.f;
					const float32 w = NkBrushFalloff(dist / r, brush.falloff, brush.hardness) * amp * pr;
					if (w == 0.f)
						continue;
					// Deplacement le long de la normale du GROUPE, mis a l'echelle
					// par le rayon : la force reste ainsi sans unite, et une meme
					// brosse se comporte pareil sur un objet de 1 cm et de 10 m.
					acc = acc + gNrm[g] * (w * r);
					touched = true;
				}

				if (touched) {
					disp[g] = acc;
					++groupsTouched;
				}
			}
			out.groupsInRadius = groupsTouched;

			// ── RIEN A FAIRE ? ALORS RIEN N'EST ECRIT ───────────────────────────
			if (groupsTouched == 0)
				return out;

			out.volumeBefore = NkSculptSignedVolume(mesh);

			uint32 moved = 0;
			float32 maxDisp = 0.f;
			for (uint32 i = 0; i < vc; ++i) {
				const uint32 c = canon[i];
				if (c >= vc)
					continue;
				const NkVec3f &d = disp[c];
				if (d.x == 0.f && d.y == 0.f && d.z == 0.f)
					continue;
				mesh.verts[i].pos = mesh.verts[i].pos + d;
				++moved;
				const float32 m = sqrtf(Dot3(d, d));
				if (m > maxDisp)
					maxDisp = m;
			}

			if (moved == 0)
				return out; // volumeBefore lu, mais aucun octet ecrit : cohérent

			mesh.RecomputeNormals();
			out.vertsMoved = moved;
			out.maxDisplacement = maxDisp;
			out.volumeAfter = NkSculptSignedVolume(mesh);
			out.applied = true;
			return out;
		}

	} // namespace renderer
} // namespace nkentseu
