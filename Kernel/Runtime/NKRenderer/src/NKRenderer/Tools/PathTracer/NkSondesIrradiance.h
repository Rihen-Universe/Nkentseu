#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    NKRenderer/Tools/PathTracer/NkSondesIrradiance.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LES SONDES D'IRRADIANCE CUITES PAR LE TRACEUR (10/11/2026, rendu-realiste)
// =============================================================================
//  Rodolf (10/11) : « oui » a l'eclairage par sondes avant le lancer materiel.
//  Une grille reguliere de sondes sur la scene ; chaque sonde lance des rayons du
//  traceur de chemins (NkTcRayonsSonde : la lumiere multiple, le ciel, le soleil et
//  les sources aux impacts) et garde deux cartes OCTAEDRIQUES de 8 x 8 texels :
//    - l'irradiance E(n) = integrale de L(w) max(0, n.w) dw (RGB ; a = sonde active,
//      0 si elle est enfermee dans un mur : trop de rayons frappent l'arriere des
//      surfaces) ;
//    - la visibilite : moyenne et moyenne du carre de la distance au premier impact,
//      autour de chaque direction (lobe cos^32). Le temps reel en tire une borne de
//      Chebyshev : une sonde que la geometrie cache du point ne l'eclaire pas -- c'est
//      ce qui empeche la lumiere de traverser un mur fin (Majercik et al. 2019,
//      « Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields »).
//  LE VOLUME : une texture 3D RGBA16F, largeur nx x 10, hauteur nz x 10, profondeur
//  2 x ny (tranche 2j : irradiance de la couche j, 2j+1 : visibilite) ; chaque carte
//  a un texel de bord (repli octaedrique) pour le filtrage bilineaire. Le nuanceur
//  qui la lit : Resources/NKRenderer/Shaders/Include/NkSondes.glsli (MEMES conventions :
//  octaedre replie sur l'axe Y, plan (x, z)).
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRenderer/Tools/PathTracer/NkTraceurChemins.h"

namespace nkentseu {
	namespace renderer {

		constexpr int32 kSdTexels = 8;			   ///< texels interieurs par cote d'une carte
		constexpr int32 kSdTuile = kSdTexels + 2; ///< avec le bord d'un texel

		struct NkSdGrille {
				float32 origine[3] = {0.f, 0.f, 0.f}; ///< position de la sonde (0, 0, 0)
				float32 pas = 1.f;					  ///< metres entre deux sondes
				int32 nx = 2, ny = 2, nz = 2;
		};

		struct NkSdReglages {
				int32 rayons = 768;		   ///< rayons par sonde
				float32 seuilDos = 0.25f;  ///< au-dela de cette part de rayons « de dos », la sonde est eteinte
				NkTcReglages traceur;	   ///< rebonds, ecretage, contre-epreuves du traceur
		};

		struct NkSdStats {
				float64 ms = 0.0;
				uint64 rayons = 0;
				int32 sondes = 0, eteintes = 0;
		};

		struct NkSdVolume {
				NkSdGrille grille;
				int32 W = 0, H = 0, D = 0;
				NkVector<uint16> texels; ///< W x H x D x 4 demi-flottants (RGBA16F)
		};

		/// Pose une grille reguliere sur la boite [mn, mx] : un pas d'au moins `pasMin`, elargi
		/// jusqu'a tenir dans `budget` sondes et dans les bornes d'une texture 3D (2048).
		bool NkSdPlacer(const float32 mn[3], const float32 mx[3], float32 pasMin, int32 budget, NkSdGrille &g);

		/// Cuit le volume sur la scene PREPAREE du traceur (NkTcPreparer), en parallele.
		bool NkSdCuire(const NkSdGrille &g, const NkSdReglages &r, NkSdVolume &v, NkSdStats *st);

		/// Lecture d'un texel (convertie en flottants) : `tranche` 0 irradiance, 1 visibilite.
		void NkSdTexel(const NkSdVolume &v, int32 ix, int32 iy, int32 iz, int32 tranche, int32 tx, int32 ty, float32 out[4]);

		/// L'octaedre (replie sur Y, plan (x, z)) : direction -> [-1, 1]^2 et retour.
		void NkSdOctEncoder(const float32 d[3], float32 p[2]);
		void NkSdOctDecoder(const float32 p[2], float32 d[3]);

		/// Conversion flottant -> demi-flottant (arrondi au plus proche) et retour.
		uint16 NkSdVersDemi(float32 f);
		float32 NkSdDepuisDemi(uint16 h);

		/// Banc sans scene externe (octaedre, demi-flottants, four blanc, mur) ; rend le nombre
		/// d'echecs. ATTENTION : il remplace la scene preparee du traceur.
		int32 NkSdBanc();

	} // namespace renderer
} // namespace nkentseu
