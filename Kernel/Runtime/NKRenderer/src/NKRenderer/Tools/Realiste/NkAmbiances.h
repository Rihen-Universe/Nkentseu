// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKRenderer/src/NKRenderer/Tools/Realiste/NkAmbiances.h
//          (13/11/2026) DEPLACE depuis Applications/NKCraft/src/NKCraft/Render/NkAmbiances.h : le rendu realiste vit
//          dans le moteur, pour toutes les applications 3D (Rodolf, 13/11). Espace nk3d -> renderer.
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// =============================================================================
//  LES AMBIANCES PREREGLEES (13/11/2026, rendu-realiste)
// =============================================================================
//  Rodolf (10/11, inspirations) : la MEME scene en un clic -- jour, couchant, nuit,
//  chaude, froide. Une ambiance pose ensemble ce qu'un eclairagiste regle a la main :
//  le soleil (direction, couleur, force), le ciel (trouble, luminosite vue), les
//  lampes (eteintes le jour, allumees le soir et la nuit : l'ambiance « jour » ou
//  « soir » de la description), l'exposition (la cle de l'automatique) et
//  l'etalonnage (contraste, saturation, chaleur : NkFinition). Le traceur et le
//  temps reel recoivent les memes valeurs : ligne de liste `ambiance_prereglage
//  <nom>` (les lignes suivantes peuvent encore corriger), boutons du panneau Rendu.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace renderer {

		struct NkAmbiance {
				const char *nom;		  ///< « jour », « couchant », « nuit », « chaude », « froide »
				const char *libelle;	  ///< le bouton
				float32 soleilAz, soleilEl; ///< degres (la convention de la liste : `soleil <az> <el> <force>`)
				float32 soleilForce;	  ///< temps reel
				float32 soleilLux;		  ///< traceur (`soleil_description`)
				float32 soleilCoul[3];	  ///< traceur (le temps reel tire sa couleur de la transmission de l'air)
				float32 turbidite;		  ///< trouble de l'air (2 tres clair, 8 voile)
				float32 cielAffiche;	  ///< luminosite du ciel VU (temps reel)
				const char *ambianceDesc; ///< « jour » (lampes eteintes) ou « soir » (lampes allumees) de la description
				bool soleil;			  ///< faux : la nuit
				float32 cle;			  ///< cle de l'exposition automatique
				float32 contraste, saturation, chaleur; ///< l'etalonnage (NkFinition)
				/// (13/11) le MINIMUM d'echantillons par pixel du traceur (0 : celui de la liste). La nuit, toute la
				/// lumiere vient de petites lampes : mesure (sejour, 960x540, contre 2048 ech.) erreur basse frequence
				/// 128 ech. 0,079 ; 256 : 0,052 ; 512 : 0,026 -- 32 candidates de lampes au lieu de 8 n'y gagnaient rien
				/// (0,083 a 128 ech., trois fois plus lent : les lampes tirees sont surtout dans d'autres pieces)
				int32 sppMinTraceur;
		};

		int32 NkAmbianceNombre();
		const NkAmbiance &NkAmbianceDe(int32 k);
		/// l'index de l'ambiance nommee, -1 inconnue
		int32 NkAmbianceIndex(const char *nom);

	} // namespace renderer
} // namespace nkentseu
