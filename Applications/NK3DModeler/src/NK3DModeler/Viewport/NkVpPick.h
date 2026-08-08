#pragma once
// -----------------------------------------------------------------------------
// @File    NkVpPick.h
// @Brief   Selection sous le curseur : sommet / arete / face d'un NkEditMesh.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// PORTE DE Applications/Sandbox/src/Demo/Demo3D.cpp (l. 849-899 et 5188-5360),
// ou ce code tourne depuis des mois. Il est recopie DELIBEREMENT ligne a ligne,
// commentaires compris : chacun de ses commentaires documente un bug corrige,
// et les reecrire de memoire reviendrait a les reintroduire un par un.
//
// Ce fichier appartient a l'ilot NKRenderer de NK3DModeler : il n'est inclus
// que par NkViewport3D.cpp, jamais par Shell/. Il ne depend que de NkEditMesh
// et de la geometrie -- donc le jour ou il sera stable, il remontera tel quel
// dans NKRenderer/Mesh/ et Sandbox pourra cesser d'en heberger sa copie.
// -----------------------------------------------------------------------------

#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Mesh/NkEditMesh.h"
#include "NKMath/NkMat.h"
#include <cmath>

namespace nkentseu {
	namespace nk3d {

		using namespace nkentseu::renderer;
		using math::NkMat4f;
		using math::NkVec3f;

		// Tout ce que le picking a besoin de savoir de la scene. Un contexte plutot
		// qu'un `Demo3DState *` : la fonction devient utilisable par n'importe quel
		// appelant, et c'est la condition pour la remonter un jour dans le noyau.
		struct NkPickCtx {
				const NkEditMesh *mesh = nullptr;
				const NkVector<NkVertex3D> *rest = nullptr; ///< sommets, espace objet
				const NkVector<uint32> *tris = nullptr;		///< triangulation de rendu
				const NkVector<uint32> *edges = nullptr;	///< paires, cf. GetUniqueEdges
				NkMat4f anchor = NkMat4f::Identity();		///< objet -> monde
				NkVec3f camPos{};
				NkVec3f fwd{}, rgt{}, upv{};
				float32 thX = 1.f, thY = 1.f;
				float32 vpW = 1.f, vpH = 1.f;
				uint32 selMask = 1u; ///< 1 sommet, 2 arete, 4 face (combinables)
				bool xray = false;

				NkVec3f WorldV(int32 i) const {
					return anchor * (*rest)[(uint32)i].pos;
				}

				bool Project(NkVec3f P, float32 &px, float32 &py) const {
					const NkVec3f v = P - camPos;
					const float32 zc = v.Dot(fwd);
					if (zc <= 1e-3f)
						return false;
					const float32 nx = v.Dot(rgt) / (zc * thX), ny = v.Dot(upv) / (zc * thY);
					px = (nx * 0.5f + 0.5f) * vpW;
					py = (0.5f - ny * 0.5f) * vpH;
					return true;
				}
		};

		struct NkPickResult {
				int32 vert = -1;			 ///< sommet elu, -1 si aucun
				int32 edgeA = -1, edgeB = -1; ///< arete elue
				int32 tri = -1;				 ///< premier indice du triangle touche
				float32 bestScreenDist = 1e30f; ///< pour arbitrer avec le gizmo
		};

		// Le point `w` est-il cache par une face ? `skipA`/`skipB` sont les indices a
		// IGNORER : les triangles qui touchent le candidat lui-meme. Sans cela un
		// sommet de silhouette serait occulte par sa propre face.
		inline bool NkPointOccluded(const NkPickCtx &c, const NkVec3f &w, int32 skipA, int32 skipB) {
			if (c.xray)
				return false;
			NkVec3f dv = w - c.camPos;
			const float32 dist = dv.Len();
			if (dist < 1e-5f)
				return false;
			dv = dv * (1.f / dist);
			const float32 eps = (1e-4f > 3e-3f * dist) ? 1e-4f : 3e-3f * dist;
			const uint32 rc = (uint32)c.rest->Size();
			const NkVector<uint32> &idx = *c.tris;
			for (uint32 t = 0; t + 2 < (uint32)idx.Size(); t += 3) {
				const int32 i0 = (int32)idx[t], i1 = (int32)idx[t + 1], i2 = (int32)idx[t + 2];
				if (i0 == skipA || i1 == skipA || i2 == skipA || i0 == skipB || i1 == skipB || i2 == skipB)
					continue;
				if ((uint32)i0 >= rc || (uint32)i1 >= rc || (uint32)i2 >= rc)
					continue;
				const NkVec3f v0 = c.WorldV(i0), v1 = c.WorldV(i1), v2 = c.WorldV(i2);
				NkVec3f e1 = v1 - v0, e2 = v2 - v0, h = dv.Cross(e2);
				float32 aa = e1.Dot(h);
				if (fabsf(aa) < 1e-7f)
					continue;
				const float32 f = 1.f / aa;
				NkVec3f sv = c.camPos - v0;
				const float32 u = f * sv.Dot(h);
				if (u < 0.f || u > 1.f)
					continue;
				NkVec3f q = sv.Cross(e1);
				const float32 vv = f * dv.Dot(q);
				if (vv < 0.f || u + vv > 1.f)
					continue;
				const float32 tt = f * e2.Dot(q);
				if (tt > 1e-4f && tt < dist - eps)
					return true;
			}
			return false;
		}

		// Rayon curseur -> triangle le plus proche. Sert au pick de FACE, et de base
		// au pick d'objet.
		inline int32 NkPickTriangle(const NkPickCtx &c, float32 mx, float32 my, float32 &tNearOut) {
			const float32 rNdcX = mx / c.vpW * 2.f - 1.f, rNdcY = 1.f - my / c.vpH * 2.f;
			NkVec3f rDir = c.fwd + c.rgt * (rNdcX * c.thX) + c.upv * (rNdcY * c.thY);
			const float32 l = rDir.Len();
			if (l > 1e-6f)
				rDir = rDir * (1.f / l);
			float32 tNear = 1e30f;
			int32 nearest = -1;
			const NkVector<uint32> &idx = *c.tris;
			for (uint32 t = 0; t + 2 < (uint32)idx.Size(); t += 3) {
				const NkVec3f v0 = c.WorldV((int32)idx[t]), v1 = c.WorldV((int32)idx[t + 1]),
							  v2 = c.WorldV((int32)idx[t + 2]);
				NkVec3f e1 = v1 - v0, e2 = v2 - v0, h = rDir.Cross(e2);
				const float32 aa = e1.Dot(h);
				if (fabsf(aa) < 1e-7f)
					continue;
				const float32 f = 1.f / aa;
				NkVec3f sv = c.camPos - v0;
				const float32 u = f * sv.Dot(h);
				if (u < 0.f || u > 1.f)
					continue;
				NkVec3f q = sv.Cross(e1);
				const float32 vv = f * rDir.Dot(q);
				if (vv < 0.f || u + vv > 1.f)
					continue;
				const float32 tt = f * e2.Dot(q);
				if (tt > 1e-4f && tt < tNear) {
					tNear = tt;
					nearest = (int32)t;
				}
			}
			tNearOut = tNear;
			return nearest;
		}

		// LE MORCEAU CRITIQUE. Ordre de decision, dans cet ordre precis :
		//   1. distance ECRAN au curseur (c'est ce que fait Blender),
		//   2. la PROFONDEUR ne departage que les quasi-ex aequo (fenetre de 1,5 px).
		// La version d'avant retenait n'importe quel element dans le rayon pourvu
		// qu'il soit le plus proche de la camera : un sommet a 13 px battait celui
		// pile sous le curseur.
		//
		// Deux heuristiques ont ete ESSAYEES PUIS ABANDONNEES, et il ne faut pas les
		// reintroduire :
		//  - le filtre « moitie near » (ne garder que ce qui est devant le plan
		//    median entre l'entree et la sortie du rayon) n'est vrai que si la
		//    surface est a peu pres perpendiculaire a la vue. Sur une face rasante,
		//    des sommets parfaitement visibles tombaient derriere le plan et etaient
		//    rejetes ;
		//  - le filtre « dos-camera » par NORMALE : un sommet de silhouette a une
		//    normale quasi perpendiculaire a la vue, donc un produit scalaire proche
		//    de zero, et il basculait au hasard du bruit numerique.
		// La visibilite est donc testee par un VRAI lancer de rayon camera ->
		// candidat, et seulement sur les quelques candidats sous le curseur.
		inline NkPickResult NkPickEditElement(const NkPickCtx &c, float32 mx, float32 my) {
			NkPickResult out;
			if (!c.mesh || !c.rest || !c.tris || !c.edges)
				return out;
			const int32 nv = (int32)c.rest->Size();

			float32 tNear = 1e30f;
			const int32 nearestTri = NkPickTriangle(c, mx, my, tNear);

			static const int32 kMaxCand = 32;
			const float32 kTiePx = 1.5f;
			int32 cI[kMaxCand], cJ[kMaxCand];
			float32 cD[kMaxCand], cZ[kMaxCand];
			NkVec3f cP[kMaxCand];
			int32 nC = 0;

			// Insertion TRIEE par distance ecran croissante, tableau borne : on garde
			// les kMaxCand meilleurs. Aucune allocation.
			auto pushCand = [&](int32 ia, int32 ib, float32 d, float32 z, NkVec3f pw) {
				if (nC == kMaxCand && d >= cD[kMaxCand - 1])
					return;
				int32 pos = (nC < kMaxCand) ? nC : (kMaxCand - 1);
				if (nC < kMaxCand)
					nC++;
				while (pos > 0 && cD[pos - 1] > d) {
					cI[pos] = cI[pos - 1];
					cJ[pos] = cJ[pos - 1];
					cD[pos] = cD[pos - 1];
					cZ[pos] = cZ[pos - 1];
					cP[pos] = cP[pos - 1];
					pos--;
				}
				cI[pos] = ia;
				cJ[pos] = ib;
				cD[pos] = d;
				cZ[pos] = z;
				cP[pos] = pw;
			};
			// Parcourt les candidats tries et retient le PREMIER non occulte, puis,
			// dans la fenetre d'egalite, celui qui est le plus proche de la camera.
			auto electCand = [&](int32 &oa, int32 &ob, float32 &od) {
				oa = -1;
				ob = -1;
				od = 1e30f;
				float32 winD = 1e30f, winZ = 1e30f;
				for (int32 k = 0; k < nC; k++) {
					if (oa >= 0 && cD[k] > winD + kTiePx)
						break;
					if (NkPointOccluded(c, cP[k], cI[k], cJ[k]))
						continue;
					if (oa < 0 || cZ[k] < winZ) {
						if (oa < 0)
							winD = cD[k];
						winZ = cZ[k];
						oa = cI[k];
						ob = cJ[k];
						od = cD[k];
					}
				}
			};

			// SOMMET : rayon de pick en PIXELS ECRAN, donc constant quel que soit le
			// zoom. Un rayon en unites monde changerait de taille apparente.
			const float32 kVertPx = 14.f;
			float32 dV = 1e30f, dE = 1e30f;
			int32 dummy = -1;
			if (c.selMask & 1u) {
				nC = 0;
				for (int32 i = 0; i < nv; i++) {
					const NkVec3f w = c.WorldV(i);
					float32 px, py;
					if (!c.Project(w, px, py))
						continue;
					const float32 d = sqrtf((px - mx) * (px - mx) + (py - my) * (py - my));
					if (d < kVertPx)
						pushCand(i, i, d, (w - c.camPos).Len(), w);
				}
				electCand(out.vert, dummy, dV);
			}

			// ARETE : distance ecran au SEGMENT. Le point teste pour l'occlusion est
			// le point de l'arete le plus proche du curseur, et non son milieu : sur
			// une arete longue et rasante, le milieu peut etre cache alors que le
			// bout cliquable est parfaitement visible.
			const float32 kEdgePx = 12.f;
			if (c.selMask & 2u) {
				nC = 0;
				const NkVector<uint32> &ed = *c.edges;
				for (uint32 e = 0; e + 1 < (uint32)ed.Size(); e += 2) {
					const int32 ia = (int32)ed[e], ib = (int32)ed[e + 1];
					if (ia >= nv || ib >= nv)
						continue;
					const NkVec3f wa = c.WorldV(ia), wb = c.WorldV(ib);
					float32 ax, ay, bx, by;
					if (!c.Project(wa, ax, ay) || !c.Project(wb, bx, by))
						continue;
					const float32 dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
					float32 tt = (l2 > 1e-6f) ? ((mx - ax) * dx + (my - ay) * dy) / l2 : 0.f;
					tt = tt < 0.f ? 0.f : (tt > 1.f ? 1.f : tt);
					const float32 qx = ax + tt * dx, qy = ay + tt * dy;
					const float32 d = sqrtf((mx - qx) * (mx - qx) + (my - qy) * (my - qy));
					if (d >= kEdgePx)
						continue;
					const NkVec3f pw = wa + (wb - wa) * tt;
					pushCand(ia, ib, d, (pw - c.camPos).Len(), pw);
				}
				electCand(out.edgeA, out.edgeB, dE);
			}

			// FACE : le triangle le plus proche touche par le rayon curseur.
			out.tri = (c.selMask & 4u) ? nearestTri : -1;
			out.bestScreenDist = (dV < dE) ? dV : dE;
			return out;
		}

	} // namespace nk3d
} // namespace nkentseu
