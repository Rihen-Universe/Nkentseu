// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Realiste/NkRenduRealisteMoteur.h
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
// (13/11/2026, rendu-realiste) L'API DU RENDU REALISTE DANS LE MOTEUR. Rodolf (13/11) : « ces systemes de rendu seront
// utilises dans toutes nos applications 3D : Noge, NKScena, NKAnima, etc. ». Ce que NKCraft calculait lui-meme
// (NKCraft/Render/NkRenduRealiste.cpp) et qu'une autre application doit pouvoir appeler SANS connaitre NKCraft :
//   - l'effet d'une ambiance prereglee (soleil, ciel, lampes, exposition, etalonnage, echantillons minimum) ;
//   - le soleil du temps reel (direction, couleur par la transmission de l'air, intensite) ;
//   - la nuit du temps reel (ciel cache, fond de nuit, pas d'IBL du ciel, ciel procedural presque noir, vitre en
//     miroir sombre) et l'ambiance (IBL) qui en decoule ;
//   - la cle d'exposition du temps reel (x 1,5 : NkArgileReglages.h, kNkArgileCleTempsReel).
// Le moteur CALCULE ; l'HOTE (NKCraft par NkDemo3DHost, demain Noge, NKScena, NkAnimaEditor par leur NkRenderer de
// vue) APPLIQUE. Le traceur (NkTraceurChemins / NkTraceurGPU), la finition, le debruiteur et le volumetrique sont
// deja dans le moteur (Tools/PathTracer).
//
// OU CHAQUE APPLICATION S'Y BRANCHERA (ecrit, pas fait) :
//   - Noge : sa vue (NkRenderer) applique NkRrSoleilTempsReel / NkRrNuitTempsReel a son ciel et a son IBL ; un
//     adaptateur ECS (maillages + materiaux) -> NkScArchviz donne la scene au traceur (NkTcPreparer / NkTcRendre) ;
//   - NKScena : idem depuis son graphe de scene ; le rendu final hors ligne par le traceur ;
//   - NkAnimaEditor : la vue d'animation applique les ambiances ; une image du personnage par le traceur.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"
#include "NKRenderer/Tools/Realiste/NkAmbiances.h"
#include "NKRenderer/Tools/PathTracer/NkSceneArchviz.h"
#include "NKRenderer/Tools/PathTracer/NkSondesIrradiance.h"
#include "NKRenderer/Tools/PathTracer/NkTraceurChemins.h"
#include "NKRHI/Core/NkTypes.h"

namespace nkentseu {
	namespace renderer {
		class NkRenderer;

		/// L'EFFET D'UNE AMBIANCE PREREGLEE : ce qu'elle pose dans les reglages d'un rendu (deplace de
		/// NkRenduRealiste.cpp, AmbianceDansListe, a l'identique).
		struct NkRrEffetAmbiance {
				float32 soleil[3] = {0.f, 0.f, 0.f}; ///< azimut, hauteur (0,5 deg au moins), force du temps reel
				float32 turbidite = 3.f;
				float32 cielAffiche = 1.f;
				const char *ambianceDesc = "jour"; ///< l'ambiance de la description : « jour » (lampes eteintes) ou « soir »
				bool descSoleil = false;		   ///< le soleil de la description (traceur) : faux la nuit
				float32 descSol[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f}; ///< azimut, hauteur, lux, r, g, b
				bool sansSoleil = false; ///< la nuit : ni soleil...
				bool sansCiel = false;	 ///< ... ni ciel
				bool sansEmission = false; ///< le jour : les lampes eteintes
				bool expoAuto = true;
				float32 cle = 0.18f; ///< la cle de l'exposition automatique (celle du traceur)
				float32 contraste = 1.f, saturation = 1.f, chaleur = 0.f;
				int32 indexAmbiance = -1;
				int32 sppMinTraceur = 0;
		};
		NkRrEffetAmbiance NkRrEffetDeLAmbiance(const NkAmbiance &a);

		/// la transmission de l'air au soleil d'une hauteur donnee (Rayleigh + aerosols de la turbidite), rgb
		void NkRrTransmissionSoleil(float32 hauteurDeg, float32 turbidite, float32 *rgb3);

		/// LE SOLEIL DU TEMPS REEL : direction (vers la scene), couleur (la transmission relative au zenith, maximum 1),
		/// intensite (la force ; avec `physique`, attenuee par la transmission verte) ; `blanc` : un soleil blanc
		/// (l'argile en jour neutre)
		struct NkRrSoleilTempsReel {
				float32 dir[3] = {0.f, -1.f, 0.f};
				float32 couleur[3] = {1.f, 1.f, 1.f};
				float32 intensite = 0.f;
		};
		NkRrSoleilTempsReel NkRrCalculerSoleil(float32 azimutDeg, float32 hauteurDeg, float32 force, float32 turbidite,
											   bool physique, bool blanc);

		/// LA NUIT DU TEMPS REEL (banc du 13/11 : vitre sombre, lampes non brulees, piece lisible). `sansCiel` : la
		/// scene n'a pas de ciel ; `prereglee` : une ambiance prereglee est posee. La nuit n'existe qu'avec les deux.
		/// NK_NUIT_MUTATION=1 : la contre-epreuve du banc de la nuit (fond, IBL et vitre d'avant ; le ciel reste cache).
		struct NkRrNuitTempsReel {
				bool cielCache = false;	 ///< le ciel n'est pas dessine (son modele 0 restait un aplat clair)
				bool nuit = false;		 ///< pas d'IBL du ciel ; ciel procedural presque noir ; vitre en miroir sombre
				bool fondNuit = false;	 ///< le fond derriere la vitre (sinon clair, brule en blanc par l'exposition)
				float32 fond[3] = {0.006f, 0.008f, 0.014f};
				float32 cielProcedural[3] = {0.004f, 0.005f, 0.008f}; ///< haut, horizon et sol du ciel procedural
				float32 vitreAlbedo[3] = {0.02f, 0.022f, 0.028f};	  ///< la vitre de nuit : PBR opaque...
				float32 vitreRugosite = 0.05f;						  ///< ... lisse (reste le reflet de la piece)
		};
		NkRrNuitTempsReel NkRrCalculerNuit(bool sansCiel, bool prereglee);

		/// L'AMBIANCE (IBL) DU TEMPS REEL : la porte du moteur borne l'intensite a 2, la teinte (blanc x facteur) porte
		/// le reste ; 0 sans IBL ou la nuit
		struct NkRrAmbiantTempsReel {
				float32 intensite = 0.f;
				float32 teinte[3] = {1.f, 1.f, 1.f};
		};
		NkRrAmbiantTempsReel NkRrCalculerAmbiant(float32 force, bool sansIbl, bool nuit);

		/// la cle d'exposition automatique du temps reel pour une cle du traceur (x kNkArgileCleTempsReel) ; 0,18 sans cle
		float32 NkRrCleTempsReel(float32 cleTraceur);

		// ── LE TRACEUR : le soleil et le ciel physique de la description (deplaces de DescriptionCielSoleil) ──────
		/// le soleil d'une description dans la scene du traceur : `sol6` = azimut, hauteur (deg), lux, r, g, b
		void NkRrSoleilDescription(NkScArchviz &s, const float32 *sol6);
		/// le ciel physique (Hosek-Wilkie, le soleil de la scene) en table lat-long `kNkRrCielW` x `kNkRrCielH`, a
		/// ECLAIREMENT HORIZONTAL EGAL a celui du degrade d'avant ; rend le facteur applique
		constexpr int32 kNkRrCielW = 256, kNkRrCielH = 128;
		float32 NkRrCielPhysique(NkScArchviz &s, float32 turbidite);

		// ── LES SONDES D'IRRADIANCE du temps reel (deplacees de NkRenduRealiste.cpp) ───────────────────────────
		/// la scene des sondes selon l'ambiance : soleil blanc (argile en jour neutre), sans soleil, sans ciel (la nuit)
		void NkRrAjusterSceneSondes(NkScArchviz &s, bool soleilBlanc, bool sansSoleil, bool sansCiel);
		/// le volume des sondes sur la carte : sa texture 3D (recreee si la taille change), son echantillonneur
		struct NkRrSondesGPU {
				NkTextureHandle tex;
				NkSamplerHandle samp;
				int32 W = 0, H = 0, D = 0;
		};
		/// la montee du volume dans sa texture et sa liaison au rendu 3D (NkRender3D::SetSondes) ; `mode` : 2 visibilite
		/// + fondu dehors, 3 sans visibilite, 4 sans fondu, 5 sans la reflexion par les sondes (contre-epreuves)
		bool NkRrMonterSondes(NkRenderer *r, const NkSdGrille &g, const NkSdVolume &v, float32 mode, NkRrSondesGPU &gpu);

		// ── POUR UNE AUTRE APPLICATION : l'ambiance dans les reglages du traceur ──────────────────────────────────
		/// l'effet d'une ambiance pose dans la scene et les reglages du traceur : le soleil de la description (sinon
		/// celui de la scene), sans soleil ni ciel la nuit, l'exposition automatique a sa cle, l'etalonnage, le minimum
		/// d'echantillons. (NKCraft passe par sa liste, qui peut corriger chaque valeur : meme effet, NkRrEffetDeLAmbiance.)
		void NkRrPoserAmbianceTraceur(const NkRrEffetAmbiance &fx, NkScArchviz &s, NkTcReglages &r);

		/// LE BANC SANS FENETRE ET SANS NKCraft (NkRenduRealisteBanc.cpp) : une scene programmee (un sol, une boite),
		/// trois ambiances (jour, couchant, nuit) posees par NkRrPoserAmbianceTraceur, trois images au traceur sur le
		/// processeur (160 x 90) ecrites dans `dossier` ; verdicts chiffres (jour eclaire, couchant plus chaud, nuit
		/// sombre). Contre-epreuve : NK_AMBIANCE_MUTATION=1 (NkRrEffetDeLAmbiance rend le jour pour toute ambiance).
		/// Appele par Nogee (`--banc-rendu-realiste=<dossier>`), qui ne lie pas NKCraft. Renvoie le nombre d'echecs.
		int32 NkRrBancSansFenetre(const char *dossier);

	} // namespace renderer
} // namespace nkentseu
