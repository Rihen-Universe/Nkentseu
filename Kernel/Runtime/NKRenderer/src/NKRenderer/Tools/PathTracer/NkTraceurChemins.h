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
#include "NKRenderer/Tools/PathTracer/NkFinitionImage.h" // (13/11) la finition de l'image

namespace nkentseu {
	namespace renderer {

		struct NkTcCamera {
				float32 oeil[3] = {0.f, 1.6f, 5.f};
				float32 cible[3] = {0.f, 1.6f, 0.f};
				float32 focaleMM = 24.f; ///< capteur VERTICAL de 36 mm (comme NkCameraPoser)
				int32 L = 1280, H = 800;
				/// (13/11/2026) LA PROFONDEUR DE CHAMP (lentille mince) : le nombre d'ouverture f/N (0 : stenope, tout
				/// net -- l'image d'avant) ; le rayon de la lentille vaut focale / (2 N). `miseAuPoint` : la distance
				/// nette en metres le long de l'axe (0 : celle de la cible).
				float32 ouverture = 0.f;
				float32 miseAuPoint = 0.f;
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
				bool sansVernis = false; ///< (10/11) contre-epreuve : ni vernis ni paillettes (la base seule)
				/// (10/11) LE MODE ARGILE : toute matiere opaque devient un blanc mat (albedo 0,8, rugosite 0,6,
				/// ni metal, ni carte, ni vernis, ni lustre) ; le verre reste verre, l'emission reste ; exposition
				/// « haute cle » (+1 IL). La modelisation et la lumiere seules, comme une maquette d'argile.
				bool argile = false;
				/// (12/11) LES REGLAGES DE L'IMAGE (le mode argile de NKCraft les pose ; ailleurs, les defauts
				/// donnent l'image d'avant) : la balance « monde » neutralise la teinte MOYENNE de l'image (en argile :
				/// des seules surfaces d'argile, albedo 0,8 -- toutes de la meme couleur, leur teinte est celle de
				/// la lumiere) ; la cle de l'exposition automatique ; une exposition manuelle (EV, < -100 : celle de
				/// la scene) ; les niveaux de gris.
				bool balanceMonde = false;
				float32 cleExpo = 0.18f;
				float32 evManuel = -1000.f;
				bool gris = false;
				// (10/11/2026) LES LUCIOLES des surfaces sombres et brillantes (ecrans, bureaux noirs) :
				// le disque solaire et l'emission ne comptent plus par un rebond GGX (la lumiere directe
				// les compte deja : double compte et pics geants), la rugosite est regularisee apres le
				// premier rebond (>= 0,3), l'emission vue apres un rebond est ecretee. Faux : le chemin
				// d'avant (contre-epreuve, NK_TRACEUR_LUCIOLES=1).
				bool regulariser = true;
				// (10/11/2026) LA BALANCE DES BLANCS (avant l'exposition) : `balanceK` > 0 neutralise une
				// lumiere de cette temperature (K) ; `balanceX`, `balanceY` >= 0 : la moyenne d'une fenetre
				// de (2 balanceR + 1)^2 pixels posee sur une surface blanche devient grise. Rien par defaut.
				float32 balanceK = 0.f;
				int32 balanceX = -1, balanceY = -1, balanceR = 8;
				/// (13/11/2026) LA FINITION (halo, etoiles, aberration, vignette, etalonnage : NkFinitionImage) ; a
				/// zero par defaut
				NkFinition fin;
				/// (13/11/2026) L'ECHANTILLONNAGE ADAPTATIF (mode final) : `spp` devient le MAXIMUM ; au-dela de
				/// `adaptMin` echantillons, un pixel dont l'erreur type de la moyenne passe sous `adaptSeuil` x sa
				/// luminance (plancher : `adaptPlancherRel` x la luminance moyenne de l'image) ne recoit plus rien ;
				/// on s'arrete quand presque tous ont converge (ou au temps). 0 : l'ancien rendu uniforme.
				float32 adaptSeuil = 0.f;
				int32 adaptMin = 32;
				float32 adaptPlancherRel = 0.25f;
				float32 adaptPlancher = 0.f; ///< pose par le rendu (luminance absolue), pas par l'appelant
				/// (13/11/2026) LE DEBRUITEUR : 2 = le nouveau (direct et indirect a part, variance prefiltree, a-trous
				/// de type SVGF) ; 1 = l'ancien (la contre-epreuve du banc)
				int32 debruiteur = 2;
				/// (13/11/2026) LES CANDIDATES de l'echantillonnage des lampes (RIS, 1 a 32) : 8 par defaut (l'image
				/// d'avant) ; une scene aux centaines de lampes (la nuit) en demande plus
				int32 risCandidats = 8;
				/// (13/11/2026) LA LUMIERE VOLUMETRIQUE : un milieu homogene (brouillard, poussiere) dans une boite --
				/// extinction `volDensite` par metre (0 : aucun, l'image d'avant), albedo de diffusion, phase de
				/// Henyey-Greenstein `volG` (> 0 : vers l'avant, les rayons de soleil) ; la boite `volBoite` (min xyz, max
				/// xyz) si `volBoiteDonnee`, sinon celle de la scene
				float32 volDensite = 0.f;
				float32 volAlbedo = 0.9f;
				float32 volG = 0.3f;
				float32 volBoite[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
				bool volBoiteDonnee = false;
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
				int32 carteOff[16] = {0}, carteW[16] = {0}, carteH[16] = {0}; // (10/11) 16 classes au plus (kMatClasses)
				float32 soleilDir[3] = {0.f, 1.f, 0.f}, soleilE[3] = {0.f, 0.f, 0.f};
				float32 zenith[3] = {0.f, 0.f, 0.f}, horizon[3] = {0.f, 0.f, 0.f};
				float32 cielIntensite = 1.f;
				int32 offCiel = 0, cielW = 0, cielH = 0; ///< (10/11) le ciel en table, a la suite de la scene
				/// (10/11) le vernis et les paillettes, un vec4 par matiere (vernis, rugosite du vernis,
				/// paillettes, taille) a la suite ; 0 : aucune matiere vernie (le nuanceur lit
				/// cartes[13].w, a 0 dans les executables d'avant : la base seule, comme avant)
				int32 offVernis = 0;
				/// (11/11/2026) les cartes d'objet, le masque et la feuille mince (NkScMateriau::carte, masque,
				/// translucide) : une table a la suite de la scene, lue par P.cartes[15].w ; 0 : aucune
				int32 offObjet = 0;
		};
		bool NkTcEmpaqueter(NkTcPaquet &p);
		/// (13/11/2026) un entier rendu AU BIT PRES dans un flottant du paquet (son motif binaire ; le nuanceur le relit par
		/// floatBitsToInt) : les indices et decalages du paquet depassent 2^24 dans une grande scene
		float32 NkTcBits(int32 v);
		/// (13/11/2026) le milieu participant du rendu qui commence (NkTcReglages.vol*) ; et sa boite / ses coefficients
		/// (extinction, albedo, phase) pour la carte graphique -- faux sans milieu
		void NkTcPoserMilieu(const NkTcReglages &r);
		bool NkTcMilieuBoite(float32 *mn3, float32 *mx3, float32 *sag3);
		/// (13/11) le banc du milieu participant (processeur) contre des valeurs exactes : four blanc, diffusion simple,
		/// sens de la phase, phase tiree ; renvoie le nombre d'echecs (NK_REALISTE=banc_milieu dans NKCraft)
		int32 NkTcBancMilieu();
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
