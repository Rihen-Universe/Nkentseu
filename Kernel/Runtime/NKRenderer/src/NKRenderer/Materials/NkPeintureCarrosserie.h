// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NKRenderer/Materials/NkPeintureCarrosserie.h (10/11/2026, rendu-realiste)
//
// LA PEINTURE DE CARROSSERIE : UNE SEULE DEFINITION, TROIS LECTEURS.
//
// Le type NK_CAR_PAINT (gabarit mTmplCarPaint de NkMaterialSystem, « Carrosserie » dans le modeleur)
// est la definition : une base PBR (couleur, metal, rugosite), un VERNIS (clearcoat, clearcoatRough)
// et des PAILLETTES portees par les deux curseurs que ce type reaffectait deja :
//   - sheen      = la QUANTITE de paillettes (0 aucune ; leur normale s'incline de
//                  kNkCpInclinaison x quantite par axe, cube [-1, 1]^3) ;
//   - anisotropy = leur DENSITE, 0 a 1 (cote d'une paillette : 1 / (40 x 50^densite) m, de 25 a 0,5 mm).
// Le vernis : une couche dielectrique (n = 1,5, F0 = 0,04) de normale LISSE (la « Coat Normal » de
// Cycles) ; la base, de normale inclinee par la paillette de sa cellule, est attenuee de l'albedo du
// vernis vu (1 - poids x albedo), comme le Principled de Cycles.
//
// Les lecteurs : le temps reel (CarPaint/NkSL/carpaint.frag.nksl = pbr.frag.nksl avec
// NK_PEINTURE_CARROSSERIE), le traceur de chemins (NkTraceurChemins.cpp et traceur_chemins.comp.nksl :
// meme hachage des cellules, meme inclinaison) et la reference Cycles des bancs
// (Tests/rendu_realiste/outils/reference_cycles2.py). Qui change une formule ici change les trois.
#pragma once

#include "NKCore/NkTypes.h"
#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace renderer {

		/// inclinaison des paillettes par unite de quantite (par axe)
		constexpr float32 kNkCpInclinaison = 0.6f;
		/// paillettes par metre a densite 0, et le rapport de densite 0 -> 1
		constexpr float32 kNkCpDensiteMin = 40.f;
		constexpr float32 kNkCpDensiteRapport = 50.f;

		/// le cote d'une paillette (m) pour une densite (anisotropy du materiau), bornee a [0, 1]
		inline float32 NkCpTaillePaillette(float32 densite) {
			const float32 d = densite < 0.f ? 0.f : (densite > 1.f ? 1.f : densite);
			return 1.f / (kNkCpDensiteMin * (float32)math::NkPow((float64)kNkCpDensiteRapport, (float64)d));
		}
		/// l'inverse : la densite d'un cote de paillette (m) ; hors [0,5 mm ; 25 mm] : bornee
		inline float32 NkCpDensiteDepuisTaille(float32 tailleM) {
			if (tailleM <= 0.f)
				return 1.f;
			const float64 d = math::NkLog(1.0 / ((float64)kNkCpDensiteMin * (float64)tailleM)) / math::NkLog((float64)kNkCpDensiteRapport);
			return d < 0.0 ? 0.f : (d > 1.0 ? 1.f : (float32)d);
		}

	} // namespace renderer
} // namespace nkentseu
