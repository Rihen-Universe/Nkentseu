// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NKRenderer/Tools/Realiste/NkArgileReglages.h (12/11/2026, rendu-realiste)
// (13/11/2026) DEPLACE depuis Applications/NKCraft/src/NKCraft/Render/NkArgileReglages.h : le rendu realiste vit dans
// le moteur, pour toutes les applications 3D (Rodolf, 13/11). Espace nk3d -> renderer.
//
// LES REGLAGES DU MODE ARGILE (Rodolf, 12/11 : « ca peut etre une option de reglage dans nkcraft », au sujet du rendu
// argile jaunatre) : l'eclairage (jour neutre ou l'ambiance de la scene), la balance des blancs (automatique sur
// l'argile ou une pipette, une temperature, aucune), l'exposition (automatique a sa cle, ou manuelle en EV), la
// couleur ou les niveaux de gris ; deux prereglages en un clic. Sauves dans le document (objet « argile » du
// rendu), lus par une liste du mode realiste (lignes `argile_*`) et par des variables (NK_REALISTE_ARGILE_*).
#pragma once

#include "NKCore/NkTypes.h"

namespace nkentseu {
	class NkArchive;
	namespace renderer {
		struct NkTcReglages;

		/// (13/11) LA CLE DE REFERENCE (Rodolf : « 0,13 assombrit trop » ; un mur blanc doit sortir entre 0,65 et
		/// 0,75 sans bruler). Mesure (sejour de l'archviz figee, jour, traceur GPU, mur blanc AFFICHE en sRGB,
		/// outils\mesure_argile.py blanc) : cle 0,13 -> 0,534 ; 0,20 -> 0,655 ; 0,24 -> 0,702 ; 0,28 -> 0,739 ;
		/// 0,36 -> 0,792 ; brules 0 % partout. Retenue : 0,24 (le milieu de la fourchette).
		constexpr float32 kNkArgileCleReference = 0.24f;
		/// POURQUOI LE TEMPS REEL RECOIT LA CLE x 1,5 : les deux moteurs ne MESURENT PAS l'exposition de la meme
		/// facon, et une cle n'a de sens que rapportee a sa mesure (exposition = cle / luminance mesuree).
		///   - le traceur (NkTraceurChemins.cpp, fin de l'image) : la moyenne geometrique de la luminance de TOUS
		///     les pixels, a poids egal, avant son ACES ;
		///   - le temps reel (pp_autoexposure.frag.nksl) : une grille de 16 x 16 echantillons seulement, PONDEREE
		///     AU CENTRE (1 au centre, 0,4 aux coins), FENETREE (les plus sombres et les plus clairs ecartes de la
		///     moyenne), le HALO compte dans la luminance, puis son propre rendu des tons.
		/// Dans le sejour, le centre de l'image tombe sur la fenetre et les taches de soleil, et le halo s'y ajoute :
		/// la luminance mesuree par le temps reel est plus forte, il expose plus bas, et a cle egale son mur sort
		/// plus sombre. Mesure (le meme sejour, sondes) : cle 0,13 -> 0,490 ; 0,24 -> 0,601 ; 0,36 -> 0,698 ;
		/// 0,42 -> 0,749 ; brules 0 %. La cle passee au temps reel est donc cle x 1,5 : une meme cle donne le meme mur
		/// dans les deux moteurs (0,702 au traceur, 0,709 au temps reel). Accord de Rodolf (13/11). Ce facteur vaut
		/// pour cette mesure : si l'une des deux mesures change, il se re-mesure.
		constexpr float32 kNkArgileCleTempsReel = 1.5f;

		struct NkArgileReglages {
				int32 eclairage = 0;				   ///< 0 jour neutre (soleil blanc), 1 l'ambiance de la scene
				int32 balance = 0;					   ///< 0 automatique, 1 temperature, 2 aucune
				float32 pipette[2] = {-1.f, -1.f};	   ///< balance automatique : un point de l'image (0..1) ; < 0 : la teinte moyenne de l'argile
				float32 temperatureK = 5500.f;		   ///< balance par temperature : la lumiere a neutraliser
				int32 exposition = 0;				   ///< 0 automatique, 1 manuelle
				float32 cle = kNkArgileCleReference;   ///< la cle de l'exposition automatique (celle du traceur)
				float32 ev = 14.f;					   ///< l'exposition manuelle (EV)
				bool gris = false;					   ///< niveaux de gris
		};

		enum : int32 { kNkArgileReference = 0, kNkArgileChaude = 1, kNkArgilePrereglages = 2 };
		/// « Argile de reference » : jour neutre, balance automatique, cle 0,24 (kNkArgileCleReference), gris.
		/// « Argile chaude » : l'ambiance de la scene, sans balance, cle 0,16, couleur.
		NkArgileReglages NkArgilePrereglage(int32 k);
		/// la cle a donner a l'exposition automatique du TEMPS REEL (cle x kNkArgileCleTempsReel, bornee a 1)
		float32 NkArgileCleTempsReel(const NkArgileReglages &a);
		const char *NkArgilePrereglageNom(int32 k);
		/// le prereglage auquel ces reglages sont egaux, -1 aucun
		int32 NkArgileQuelPrereglage(const NkArgileReglages &a);
		bool NkArgileEgaux(const NkArgileReglages &a, const NkArgileReglages &b);

		/// Une ligne de liste (`argile_prereglage reference`, `argile_eclairage jour|ambiance`,
		/// `argile_balance auto [x y] | <K> | aucune`, `argile_exposition auto <cle> | manuelle <EV>`,
		/// `argile_couleur couleur|gris`) : le mot et le reste. Faux si le mot n'est pas a l'argile.
		bool NkArgileLireLigne(NkArgileReglages &a, const char *mot, const char *reste);
		/// Les variables NK_REALISTE_ARGILE_PREREGLAGE, _ECLAIRAGE, _BALANCE, _EXPOSITION, _COULEUR (meme syntaxe).
		void NkArgileLireEnv(NkArgileReglages &a);

		/// Le document : l'objet « argile » (version, actif, reglages).
		void NkArgileCapturer(const NkArgileReglages &a, bool actif, NkArchive &out);
		bool NkArgileRestaurer(const NkArchive &in, NkArgileReglages &a, bool &actif);

		/// Les reglages du traceur (balance, exposition, gris) pour une image L x H.
		void NkArgileVersTraceur(const NkArgileReglages &a, int32 L, int32 H, renderer::NkTcReglages &r);
		/// Les gains de la balance par temperature (luminance constante) ; 1, 1, 1 sinon.
		void NkArgileGainsTemperature(const NkArgileReglages &a, float32 *gains3);
		/// Corrige une image du temps reel deja ecrite (PNG) : balance automatique (teinte moyenne des pixels ni
		/// brules ni noirs, ou la pipette), temperature, gris. Rend vrai si l'image a ete reecrite.
		bool NkArgileCorrigerImage(const NkArgileReglages &a, const char *png, float32 *gainsVus3);

		/// Le banc du document : chaque reglage, et les deux prereglages, passent par le JSON du projet
		/// (NkJSONWriter puis NkJSONReader) et reviennent identiques. Rend le nombre d'echecs.
		int32 NkArgileBancDocument();

	} // namespace renderer
} // namespace nkentseu
