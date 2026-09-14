// =============================================================================
// NkTerrainSplat.cpp — NKRenderer / Mesh
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// =============================================================================
#include "NkTerrainSplat.h"

#include "NKImage/Core/NkImage.h"

#include <math.h>

namespace nkentseu {
	namespace renderer {

		NkTerrainPoids NkTerrainPoidsDepuisPente(float32 angleDeg, float32 hauteur, float32 hMin,
												 float32 hMax, const NkTerrainSplatParams &p) {
			NkTerrainPoids r;

			const float32 s = NkTerrainLisser(angleDeg, p.penteRoche0, p.penteRoche1);
			const float32 reste = 1.f - s;

			// ── LE CAS DÉGÉNÉRÉ EST UNE RÈGLE POSÉE, PAS UN ACCIDENT ────────
			// Étendue nulle = tout le terrain à une seule altitude. Les deux
			// transitions s'effondrent, et « lisser » y répondrait par un seuil
			// dur dont le côté dépendrait d'une comparaison flottante. On
			// tranche explicitement : pas de neige, tout en terre.
			const float32 etendue = hMax - hMin;
			float32 fNeige = 0.f, fTerre = 1.f;
			if (etendue > 1e-6f) {
				const float32 neige0 = hMin + p.fracNeige0 * etendue;
				const float32 neige1 = hMin + p.fracNeige1 * etendue;
				const float32 terre0 = hMin + p.fracTerre0 * etendue;
				const float32 terre1 = hMin + p.fracTerre1 * etendue;
				fNeige = NkTerrainLisser(hauteur, neige0, neige1);
				fTerre = 1.f - NkTerrainLisser(hauteur, terre0, terre1);
			}

			const float32 wRoche = s;
			const float32 wNeige = reste * fNeige;
			const float32 reste2 = reste - wNeige;
			const float32 wTerre = reste2 * fTerre;
			const float32 wHerbe = reste2 - wTerre;

			r.w[NK_TERRAIN_HERBE] = wHerbe;
			r.w[NK_TERRAIN_TERRE] = wTerre;
			r.w[NK_TERRAIN_ROCHE] = wRoche;
			r.w[NK_TERRAIN_NEIGE] = wNeige;
			return r;
		}

		NkTerrainStatut NkTerrainPoidsDuMaillage(const NkEditMesh &mesh, const NkTerrainSplatParams &p,
												 NkVector<NkTerrainPoids> &out) {
			const uint32 nv = (uint32)mesh.verts.Size();
			out.Clear();
			if (nv == 0u)
				return NkTerrainStatut::NK_IMAGE_INVALIDE;

			// ── L'ÉTENDUE EST MESURÉE SUR LE MAILLAGE QU'ELLE JUGE ──────────
			// Jamais recopiée d'un paramètre : un attendu en dur se périme sans
			// prévenir et se met à crier rouge sur un montage correct.
			float32 hMin = mesh.verts[0].pos.y, hMax = hMin;
			for (uint32 k = 1; k < nv; ++k) {
				const float32 h = mesh.verts[k].pos.y;
				if (h < hMin)
					hMin = h;
				if (h > hMax)
					hMax = h;
			}

			out.Resize(nv);
			for (uint32 k = 0; k < nv; ++k) {
				float32 ny = mesh.verts[k].normal.y;
				// La normale est censée être unitaire ; on ne le suppose pas.
				// Un `acos` hors de [-1,1] rend NaN, et un NaN se propage dans
				// les quatre poids sans qu'aucune comparaison ne le signale.
				if (ny > 1.f)
					ny = 1.f;
				if (ny < -1.f)
					ny = -1.f;
				const float32 angleDeg = (float32)(acos((double)ny) * 57.29577951308232);
				out[k] = NkTerrainPoidsDepuisPente(angleDeg, mesh.verts[k].pos.y, hMin, hMax, p);
			}
			return NkTerrainStatut::NK_OK;
		}

		NkTerrainStatut NkTerrainSplatmapDepuisPoids(const NkTerrainPoids *poids, uint32 N, uint32 M,
													 NkImage &out, uint32 *outSomme255Max) {
			if (poids == nullptr || N == 0u || M == 0u)
				return NkTerrainStatut::NK_IMAGE_INVALIDE;
			out = NkImage::Create(N, M, NkImagePixelFormat::NK_RGBA32, 0u);
			if (!out.IsValid())
				return NkTerrainStatut::NK_IMAGE_INVALIDE;

			uint32 pireEcart = 0;
			for (uint32 j = 0; j < M; ++j) {
				for (uint32 i = 0; i < N; ++i) {
					const NkTerrainPoids &w = poids[j * N + i];
					// R = herbe, G = terre, B = roche, A = neige : l'ordre de
					// `splat.r/g/b/a` dans terrain.frag.gl.glsl l.23-26.
					uint8 c[4];
					uint32 somme = 0;
					for (uint32 k = 0; k < 4u; ++k) {
						float32 v = w.w[k] * 255.f + 0.5f;
						if (v < 0.f)
							v = 0.f;
						if (v > 255.f)
							v = 255.f;
						c[k] = (uint8)v;
						somme += c[k];
					}
					const uint32 e = somme > 255u ? somme - 255u : 255u - somme;
					if (e > pireEcart)
						pireEcart = e;
					out.SetPixel((int32)i, (int32)j, math::NkColor(c[0], c[1], c[2], c[3]));
				}
			}
			if (outSomme255Max)
				*outSomme255Max = pireEcart;
			return NkTerrainStatut::NK_OK;
		}

	} // namespace renderer
} // namespace nkentseu
