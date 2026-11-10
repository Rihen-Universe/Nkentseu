#pragma once
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/PathTracer/NkTraceurChemins.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LE TRACEUR DE CHEMINS DE « RENDRE L'IMAGE » (09/11/2026, rendu-realiste)
// =============================================================================
//  Decision du coordinateur (09/11) apres la mesure : le temps reel ne fait pas la
//  lumiere d'une piece (correlation 0,18 a 0,33 contre Cycles), le traceur de
//  chemins la fait. Celui-ci est le NOTRE, sur processeur d'abord (le plus sur a
//  prouver), toutes les unites de calcul :
//    - BVH a decoupe par surface (SAH, 12 casiers ; MacDonald et Booth 1990) ;
//    - intersection de Moller et Trumbore (1997) ;
//    - estimateur de chemin avec lancer vers les lumieres (NEE) : soleil (disque de
//      0,53 deg), sources locales choisies par REECHANTILLONNAGE d'importance (RIS,
//      Talbot 2005 : 8 candidates tirees selon la puissance, une gardee selon sa
//      contribution estimee) ; ciel en degrade par echantillonnage de la BSDF ;
//    - BSDF : Lambert + micro-facettes GGX (Walter 2007) avec Fresnel de Schlick,
//      metal par F0 = albedo ; VERRE : dielectrique lisse n = 1,5 (Fresnel exact,
//      reflexion ou refraction tirees), et « verre mince » pour les rayons d'ombre
//      (transmission (1 - F) x teinte : le soleil et les lampes passent la vitre) ;
//    - roulette russe apres 3 rebonds, 8 rebonds au plus, ecretage des pics
//      indirects (borne ecrite) ;
//    - DEBRUITEUR ecrit ici : ondelette a trous a arretes preservees (Dammertz et
//      al. 2010), sur l'eclairement demodule (couleur / albedo), guide par albedo,
//      normale et variance de luminance ; 5 niveaux ;
//    - exposition EV100 (facteur 1000 / (1,2 x 2^EV), unites de scene = lux x 1e-3)
//      ou automatique (cle 0,18 sur la moyenne geometrique), ACES de Narkowicz (le
//      meme ajustement que pp_tonemap.frag.nksl), sRGB.
//  Il lit NkScArchviz (NkSceneArchviz.h) : la meme description que la reference.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKRenderer/Tools/PathTracer/NkSceneArchviz.h"

namespace nkentseu {
	namespace renderer {

		struct NkTcCamera {
				float32 oeil[3] = {0.f, 1.6f, 5.f};
				float32 cible[3] = {0.f, 1.6f, 0.f};
				float32 focaleMM = 24.f; ///< capteur VERTICAL de 36 mm (comme NkCameraPoser)
				int32 L = 1280, H = 800;
		};

		struct NkTcReglages {
				int32 spp = 64;			   ///< echantillons par pixel vises
				float32 secondesMax = 0.f; ///< 0 : sans limite de temps
				int32 rebonds = 8;
				bool debruiter = true;
				bool expoAuto = false;	   ///< sinon l'EV de la scene
				float32 ecretage = 20.f;   ///< borne de luminance d'une contribution indirecte
				// contre-epreuves
				bool sansSoleil = false, sansCiel = false, sansSources = false, unRebond = false;
				bool sansCartes = false;
		};

		struct NkTcStats {
				float64 msPreparation = 0.0, msRendu = 0.0, msDebruitage = 0.0;
				int32 sppFaits = 0;
				int32 fils = 0;
				uint64 rayons = 0;
		};

		/// Prepare la scene (BVH, cartes procedurales chargees depuis `dossierCartes`).
		bool NkTcPreparer(const NkScArchviz &s, const char *dossierCartes, NkTcStats *stats);

		/// Rend une vue. Ecrit `png` (debruite si demande) et `<png sans .png>_brut.png` (sans
		/// debruiteur). Rend faux si la scene n'est pas preparee ou si l'ecriture echoue.
		bool NkTcRendre(const NkTcCamera &cam, const NkTcReglages &r, const char *png, NkTcStats *stats);

		/// Banc de justesse sans scene externe (four blanc, verre, convergence) ; rend le nombre
		/// d'echecs. Ecrit le compte rendu sur la sortie.
		int32 NkTcBanc();

		/// (10/11/2026) LA SCENE PREPAREE A PLAT, pour la carte graphique (NkTraceurGPU) : un seul
		/// tableau de vec4 (noeuds, triangles dans l'ordre des feuilles, sommets, matieres, sources)
		/// et les cartes en RGBA8 -- trois tampons en tout : DX11 n'a que huit vues UAV.
		struct NkTcPaquet {
				NkVector<float32> scene; ///< vec4 a plat
				NkVector<uint32> cartes;
				int32 offNoeuds = 0, offTris = 0, offSommets = 0, offMats = 0, offSources = 0, nSources = 0;
				float32 puissanceTotale = 0.f;
				int32 carteOff[13] = {0}, carteW[13] = {0}, carteH[13] = {0};
				float32 soleilDir[3] = {0.f, 1.f, 0.f}, soleilE[3] = {0.f, 0.f, 0.f};
				float32 zenith[3] = {0.f, 0.f, 0.f}, horizon[3] = {0.f, 0.f, 0.f};
				float32 cielIntensite = 1.f;
		};
		bool NkTcEmpaqueter(NkTcPaquet &p);
		/// La fin commune (moyennes, exposition, image brute, debruiteur, image) depuis les sommes de la
		/// carte graphique : `o4` = 4 sections de L x H vec4 (couleur, albedo, normale, (profondeur,
		/// luminance au carre)), sommees sur `spp` echantillons.
		/// (10/11/2026) LES RAYONS D'UNE SONDE D'IRRADIANCE (NkSondesIrradiance) sur la scene preparee :
		/// `n` rayons depuis `o` dans les directions `dirs` (n x 3). Par rayon : la radiance arrivante
		/// `rad3` (n x 3 ; rayon « secondaire » : ni emission vue ni disque solaire, la lumiere aux
		/// impacts et le ciel compris), la distance du premier impact bornee a `dmax` (`dist`, une vitre
		/// compte comme un mur) et `dos` = 1 si le rayon frappe l'arriere d'une surface opaque, 2 s'il
		/// s'echappe vers le ciel sans rien toucher, 0 sinon. Ajoute
		/// les rayons lances a `*rayons`. Sans etat partage : appelable depuis plusieurs fils.
		bool NkTcRayonsSonde(const float32 o[3], const float32 *dirs, int32 n, uint64 graine, float32 dmax,
							 const NkTcReglages &r, float32 *rad3, float32 *dist, uint8 *dos, uint64 *rayons);

		bool NkTcFinaliserSommes(const NkTcCamera &cam, const NkTcReglages &r, const char *png, const float32 *o4, int32 spp,
								 float64 msRendu, uint64 rayons, NkTcStats *stats);

	} // namespace renderer
} // namespace nkentseu
