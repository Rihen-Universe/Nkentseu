#pragma once
// =============================================================================
// NkcLabTheme — LA palette de l'atelier. Un seul endroit (HANDOFF §2.4 : « pas
// de couleur en dur dans le code des panneaux »).
//
// CHARTE : GITHUB DARK PRO
// ------------------------
// Decision de Rihen, 2026-08-06 : la charte teal RIHEN de HANDOFF §2.1 est
// REMPLACEE par GitHub Dark Pro. Raison : l'atelier est un outil de developpeur,
// regarde huit heures par jour a cote d'un editeur de code ; il doit avoir la
// meme temperature que lui, pas celle d'une plaquette de studio.
//
// Valeurs : primer/primitives de GitHub (canvas #0D1117, surfaces #161B22,
// bordure #30363D, texte #E6EDF3, accent #58A6FF), variante « Pro » = fonds plus
// contrastes et accents plus satures que le Dark par defaut.
//
// DEUX FAMILLES, ET LA DISTINCTION COMPTE
//
//   ctx.theme  — le CHROME (fonds, boutons, bordures, texte). Les widgets NKGui
//                y puisent seuls.
//
//   NkcPalette — le PLATEAU et ses signes de jeu (joueurs, coups legaux, danger).
//                Ce sont des couleurs de SENS, pas de decor : elles ne suivent
//                aucun theme utilisateur, sinon la lecture tactique changerait
//                avec l'humeur du theme.
//
// Les quatre teintes de joueur restent distinctes EN NIVEAUX DE GRIS
// (luminance 0,62 bleu / 0,60 orange / 0,66 vert / 0,58 violet) : un daltonien
// deuteranope distingue toujours les quatre camps.
// =============================================================================

#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace conqueror {

		using nkgui::NkColor;

		// ---------------------------------------------------------------------
		/// Les noms decrivent l'USAGE, jamais la teinte : « accent » et non
		/// « bleu », pour qu'un changement de charte ne rende pas le code menteur.
		struct NkcPalette {
				// ---- chrome (repris tel quel dans ctx.theme) -----------------
				static constexpr NkColor BgPrimary()	{ return {13, 17, 23, 255}; }	 // #0D1117 canvas
				static constexpr NkColor Panel()		{ return {22, 27, 34, 255}; }	 // #161B22 surface
				static constexpr NkColor Header()		{ return {22, 27, 34, 255}; }	 // #161B22
				static constexpr NkColor Button()		{ return {33, 38, 45, 255}; }	 // #21262D
				static constexpr NkColor ButtonHover()	{ return {48, 54, 61, 255}; }	 // #30363D
				static constexpr NkColor Emphasis()		{ return {31, 111, 235, 255}; }	 // #1F6FEB bouton actif
				static constexpr NkColor Accent()		{ return {88, 166, 255, 255}; }	 // #58A6FF
				static constexpr NkColor Border()		{ return {48, 54, 61, 255}; }	 // #30363D
				static constexpr NkColor Text()			{ return {230, 237, 243, 255}; } // #E6EDF3
				static constexpr NkColor TextDim()		{ return {139, 148, 158, 255}; } // #8B949E
				static constexpr NkColor Track()		{ return {1, 4, 9, 255}; }		 // #010409 inset

				// ---- plateau -------------------------------------------------
				static constexpr NkColor CellEmpty()	{ return {22, 27, 34, 255}; }	 // #161B22
				static constexpr NkColor CellBlocked()	{ return {1, 4, 9, 255}; }		 // #010409
				static constexpr NkColor CellEdge()		{ return {48, 54, 61, 255}; }	 // #30363D

				/// Teinte d'un joueur. Au-dela de 4 on boucle : le contrat borne a
				/// kMaxPlayers = 4, mais un module fantaisiste ne doit pas faire
				/// sortir l'atelier du tableau.
				static NkColor Player(int32 index) noexcept {
					static const NkColor kP[4] = {
						{88, 166, 255, 255},   // #58A6FF bleu   (accent.fg)
						{219, 109, 40, 255},   // #DB6D28 orange (severe.fg)
						{63, 185, 80, 255},	   // #3FB950 vert   (success.fg)
						{163, 113, 247, 255},  // #A371F7 violet (done.fg)
					};
					if (index < 0) return CellEmpty();
					return kP[index & 3];
				}

				// ---- signes de jeu -------------------------------------------
				/// Destination legale du coup en cours de construction.
				static constexpr NkColor MoveLegal()	{ return {63, 185, 80, 165}; }	 // #3FB950 a 65 %
				/// Totem ennemi qui SERAIT retourne : la lecture tactique centrale
				/// (« quelle surface de contact suis-je en train d'offrir ? »).
				static constexpr NkColor MoveThreat()	{ return {248, 81, 73, 255}; }	 // #F85149 danger.fg
				/// Dernier coup joue (anneau qui pulse puis s'eteint).
				static constexpr NkColor LastMove()		{ return {210, 153, 34, 255}; }	 // #D29922 attention.fg

				// ---- statuts -------------------------------------------------
				static constexpr NkColor Ok()			{ return {63, 185, 80, 255}; }	 // #3FB950
				static constexpr NkColor Warn()			{ return {210, 153, 34, 255}; }	 // #D29922
				static constexpr NkColor Error()		{ return {248, 81, 73, 255}; }	 // #F85149
		};

		/// Couleur avec alpha module — evite d'ecrire un NkColor litteral dans un
		/// panneau juste pour rendre une teinte de la palette translucide.
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
		/// l'Editor Kit y pose son propre theme puis RECHARGE le theme utilisateur
		/// sauvegarde ; ecrire avant serait ecrase sans bruit.
		inline void ApplyGitHubDarkPro(nkgui::NkGuiContext &ctx) noexcept {
			nkgui::NkGuiTheme &t = ctx.theme;
			t.bgPrimary	   = NkcPalette::BgPrimary();
			t.panel		   = NkcPalette::Panel();
			t.header	   = NkcPalette::Header();
			t.button	   = NkcPalette::Button();
			t.buttonHover  = NkcPalette::ButtonHover();
			t.buttonActive = NkcPalette::Emphasis();
			t.border	   = NkcPalette::Border();
			t.text		   = NkcPalette::Text();
			t.textDisabled = NkcPalette::TextDim();
			t.selection	   = NkcFade(NkcPalette::Emphasis(), 0.78f);
			t.accent	   = NkcPalette::Accent();
			t.track		   = NkcPalette::Track();
			t.tabBar	   = NkcPalette::Track();
			t.tab		   = NkcPalette::Panel();
			t.tabHover	   = NkcPalette::ButtonHover();
			t.tabActive	   = NkcPalette::BgPrimary();
			// GitHub ne bombe pas ses coins : 6 px sur les grandes surfaces, et le
			// chrome des onglets reste droit (c'est le shell qui en decide).
			t.rounding	   = 6.f;
			t.framePadX	   = 12.f;
			t.framePadY	   = 7.f;
		}

	} // namespace conqueror
} // namespace nkentseu
