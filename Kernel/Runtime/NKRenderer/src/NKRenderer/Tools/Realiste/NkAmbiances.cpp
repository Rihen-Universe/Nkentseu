// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Realiste/NkAmbiances.cpp
//          (13/11/2026) DEPLACE depuis Applications/NKCraft/src/NKCraft/Render/NkAmbiances.cpp (espace nk3d -> renderer)
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// Les ambiances prereglees : voir NkAmbiances.h.

#include "NKRenderer/Tools/Realiste/NkAmbiances.h"

namespace nkentseu {
	namespace renderer {

		namespace {
			// Les valeurs, ecrites (pas devinees) :
			//  - JOUR : l'apres-midi clair de la description (azimut 250, 40 deg, soleil 5 600 K presque blanc) ;
			//  - COUCHANT : soleil a 5 deg a l'ouest, 2 200 K (orange), air plus trouble, ciel moins lumineux, lampes
			//    allumees, image un peu plus chaude et saturee ;
			//  - NUIT : pas de soleil, ciel presque noir, lampes allumees, cle basse (la nuit reste sombre), image un
			//    peu froide (les lampes, elles, sont chaudes : le contraste des deux fait la nuit) ;
			//  - CHAUDE : soleil bas d'apres-midi (25 deg, 3 500 K), lampes allumees, chaleur +0,4 ;
			//  - FROIDE : ciel voile (trouble 8), soleil haut et faible (8 000 K), desature, chaleur -0,4.
			const NkAmbiance kAmbiances[5] = {
				{"jour", "Jour", 250.f, 40.f, 6.f, 80000.f, {1.0f, 0.96f, 0.90f}, 3.f, 1.f, "jour", true, 0.18f, 1.f, 1.f, 0.f, 0},
				{"couchant", "Couchant", 285.f, 5.f, 4.f, 6000.f, {1.0f, 0.62f, 0.35f}, 4.5f, 0.5f, "soir", true, 0.16f, 1.05f, 1.08f, 0.3f, 0},
				{"nuit", "Nuit", 285.f, -10.f, 0.f, 0.f, {1.f, 1.f, 1.f}, 3.f, 0.03f, "soir", false, 0.10f, 1.1f, 0.9f, -0.1f, 512},
				{"chaude", "Chaude", 230.f, 25.f, 6.f, 60000.f, {1.0f, 0.85f, 0.65f}, 3.5f, 0.9f, "soir", true, 0.18f, 1.05f, 1.1f, 0.4f, 0},
				// (13/11, banc) froide : trouble 8 -> 5 et chaleur -0,4 -> -0,6 -- au temps reel, un ciel tres trouble
				// est plus GRIS que le ciel clair du jour (moins bleu) : la froide sortait plus chaude que le jour
				{"froide", "Froide", 200.f, 55.f, 3.f, 20000.f, {0.85f, 0.92f, 1.0f}, 5.f, 1.2f, "jour", true, 0.18f, 0.95f, 0.85f, -0.6f, 0},
			};
			bool Egal(const char *a, const char *b) {
				if (!a || !b)
					return false;
				while (*a && *a == *b) {
					++a;
					++b;
				}
				return *a == 0 && *b == 0;
			}
		} // namespace

		int32 NkAmbianceNombre() {
			return 5;
		}
		const NkAmbiance &NkAmbianceDe(int32 k) {
			return kAmbiances[k < 0 ? 0 : (k > 4 ? 4 : k)];
		}
		int32 NkAmbianceIndex(const char *nom) {
			for (int32 k = 0; k < 5; ++k)
				if (Egal(kAmbiances[k].nom, nom))
					return k;
			return -1;
		}

	} // namespace renderer
} // namespace nkentseu
