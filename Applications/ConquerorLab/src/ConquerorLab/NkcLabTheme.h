#pragma once
// =============================================================================
// NkcLabTheme — LA palette de l'atelier. Un seul endroit, comme exige par la
// direction artistique (HANDOFF §2.4 : « pas de couleur en dur dans le code des
// panneaux »).
//
// Deux familles, et la distinction compte :
//
//   ctx.theme  — le CHROME (fonds, boutons, bordures, texte). Ecrase apres Init
//                du shell par ApplyRihenTheme. Les widgets NKGui y puisent seuls.
//
//   NkcPalette — le PLATEAU et ses signes de jeu (joueurs, coups legaux, danger).
//                Ce sont des couleurs de SENS, pas de decor : elles ne doivent
//                jamais suivre un changement de theme utilisateur, sinon la
//                lecture tactique change avec l'humeur du theme.
//
// Les quatre teintes de joueur ont ete choisies pour rester distinctes EN
// NIVEAUX DE GRIS (luminance 0,62 / 0,66 / 0,73 / 0,58) : un daltonien
// deutéranope distingue toujours cyan / orange / vert / violet.
// =============================================================================

#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace conqueror {

		using nkgui::NkColor;

		// ---------------------------------------------------------------------
		/// Charte RIHEN. Les noms decrivent l'USAGE, jamais la teinte : « accent »
		/// et non « orange », pour qu'un changement de charte ne mente pas.
		struct NkcPalette {
				// ---- chrome (repris tel quel dans ctx.theme) -----------------
				static constexpr NkColor BgPrimary()	{ return {11, 34, 41, 255}; }	 // #0B2229
				static constexpr NkColor Panel()		{ return {18, 49, 58, 255}; }	 // #12313A
				static constexpr NkColor Header()		{ return {23, 64, 75, 255}; }	 // #17404B
				static constexpr NkColor Button()		{ return {29, 78, 90, 255}; }	 // #1D4E5A
				static constexpr NkColor ButtonHover()	{ return {39, 106, 121, 255}; }	 // #276A79
				static constexpr NkColor Accent()		{ return {232, 151, 63, 255}; }	 // #E8973F
				static constexpr NkColor Border()		{ return {31, 90, 104, 255}; }	 // #1F5A68
				static constexpr NkColor Text()			{ return {232, 241, 243, 255}; } // #E8F1F3
				static constexpr NkColor TextDim()		{ return {110, 140, 149, 255}; } // #6E8C95
				static constexpr NkColor Track()		{ return {10, 29, 35, 255}; }	 // #0A1D23

				// ---- plateau -------------------------------------------------
				static constexpr NkColor CellEmpty()	{ return {14, 42, 50, 255}; }	 // #0E2A32
				static constexpr NkColor CellBlocked()	{ return {7, 22, 25, 255}; }	 // #071619
				static constexpr NkColor CellEdge()		{ return {31, 90, 104, 255}; }	 // #1F5A68

				/// Teinte d'un joueur. Au-dela de 4 joueurs on boucle : le contrat
				/// borne a kMaxPlayers = 4, mais un module fantaisiste ne doit pas
				/// faire sortir l'atelier du tableau.
				static NkColor Player(int32 index) noexcept {
					static const NkColor kP[4] = {
						{79, 179, 199, 255},   // #4FB3C7 cyan
						{232, 151, 63, 255},   // #E8973F orange (accent RIHEN)
						{143, 203, 109, 255},  // #8FCB6D vert
						{199, 125, 212, 255},  // #C77DD4 violet
					};
					if (index < 0) return CellEmpty();
					return kP[index & 3];
				}

				// ---- signes de jeu -------------------------------------------
				/// Destination legale du coup en cours de construction.
				static constexpr NkColor MoveLegal()	{ return {143, 203, 109, 153}; } // #8FCB6D a 60 %
				/// Totem ennemi qui SERAIT retourne : la lecture tactique centrale
				/// (« quelle surface de contact suis-je en train d'offrir ? »).
				static constexpr NkColor MoveThreat()	{ return {232, 106, 90, 255}; }	 // #E86A5A
				/// Dernier coup joue (anneau qui pulse puis s'eteint).
				static constexpr NkColor LastMove()		{ return {232, 151, 63, 255}; }

				// ---- statuts de module ---------------------------------------
				static constexpr NkColor Ok()			{ return {143, 203, 109, 255}; }
				static constexpr NkColor Warn()			{ return {232, 151, 63, 255}; }
				static constexpr NkColor Error()		{ return {232, 106, 90, 255}; }
		};

		/// Couleur avec alpha force — evite d'ecrire un NkColor{...} litteral dans
		/// un panneau juste pour rendre une teinte de la palette translucide.
		inline NkColor NkcFade(NkColor c, float32 a) noexcept {
			const float32 v = a < 0.f ? 0.f : (a > 1.f ? 1.f : a);
			c.a = static_cast<uint8>(static_cast<float32>(c.a) * v + 0.5f);
			return c;
		}

		/// Melange lineaire — degrades de tuiles, pulsations, survol.
		inline NkColor NkcMix(NkColor a, NkColor b, float32 t) noexcept {
			const float32 k = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
			NkColor out;
			out.r = static_cast<uint8>(static_cast<float32>(a.r) + (static_cast<float32>(b.r) - static_cast<float32>(a.r)) * k);
			out.g = static_cast<uint8>(static_cast<float32>(a.g) + (static_cast<float32>(b.g) - static_cast<float32>(a.g)) * k);
			out.b = static_cast<uint8>(static_cast<float32>(a.b) + (static_cast<float32>(b.b) - static_cast<float32>(a.b)) * k);
			out.a = static_cast<uint8>(static_cast<float32>(a.a) + (static_cast<float32>(b.a) - static_cast<float32>(a.a)) * k);
			return out;
		}

		// ---------------------------------------------------------------------
		/// Ecrase le theme NKGui du shell. A appeler APRES `shell->Init(...)` :
		/// l'Editor Kit y pose son propre theme (GitHub Dark) et rechargerait le
		/// theme utilisateur par-dessus si on ecrivait avant.
		inline void ApplyRihenTheme(nkgui::NkGuiContext &ctx) noexcept {
			nkgui::NkGuiTheme &t = ctx.theme;
			t.bgPrimary	   = NkcPalette::BgPrimary();
			t.panel		   = NkcPalette::Panel();
			t.header	   = NkcPalette::Header();
			t.button	   = NkcPalette::Button();
			t.buttonHover  = NkcPalette::ButtonHover();
			t.buttonActive = NkcPalette::Accent();
			t.border	   = NkcPalette::Border();
			t.text		   = NkcPalette::Text();
			t.textDisabled = NkcPalette::TextDim();
			t.selection	   = NkcFade(NkcPalette::Accent(), 0.78f);
			t.accent	   = NkcPalette::Accent();
			t.track		   = NkcPalette::Track();
			t.tabBar	   = NkcPalette::BgPrimary();
			t.tab		   = NkcPalette::Panel();
			t.tabHover	   = NkcPalette::ButtonHover();
			t.tabActive	   = NkcPalette::Header();
			t.rounding	   = 6.f;
			t.framePadX	   = 12.f;
			t.framePadY	   = 7.f;
		}

	} // namespace conqueror
} // namespace nkentseu
