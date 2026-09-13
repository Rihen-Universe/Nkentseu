#pragma once
// =============================================================================
// NkIGlyphSource.h — DE QUOI DESSINER UN GLYPHE, SANS SAVOIR QUI LE FOURNIT
// -----------------------------------------------------------------------------
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// LICENCE : Proprietary - All Rights Reserved (voir LICENSE)
//
// =============================================================================
//  POURQUOI CETTE INTERFACE EXISTE, ET POURQUOI ELLE EST DANS NKCore
// =============================================================================
//  Le codec SVG de NKImage doit peindre <text>. Les contours de glyphes vivent
//  dans NKFont. Faire dependre NKImage de NKFont reglait le probleme d'un trait
//  -- et faisait payer les polices embarquees a TOUT consommateur de NKImage,
//  y compris celui qui ne veut que decoder un PNG. Nkentseu vise `Bare` (une
//  console sans OS) et le Web : ce poids n'y est pas negociable.
//
//  Arbitrage du 2026-09-05, sortie 2 de la porte des cycles (`f7dbcf0`) :
//  LE BAS DEFINIT, LE HAUT IMPLEMENTE ET INJECTE.
//    - NKCore (que NKImage et NKFont voient deja tous les deux) declare CE
//      CONTRAT et rien d'autre ;
//    - NKFont l'implemente (`NkFontGlyphSource`) ;
//    - NKImage le CONSOMME sans connaitre NKFont ;
//    - l'application choisit : elle injecte, ou elle n'injecte pas et le texte
//      est SAUTE ET DIT -- jamais rendu vide en silence.
//  Aucune arete NKImage <-> NKFont n'existe dans les .jenga : c'est verifiable
//  par un grep, et le banc le verifie.
//
//  POURQUOI UN RECEPTEUR (`NkIGlyphSink`) ET PAS UN TABLEAU DE POINTS. Rendre
//  un conteneur obligerait cette interface a connaitre NkVector, donc NKCore a
//  dependre de NKContainers ; et cela imposerait une allocation par glyphe.
//  Un recepteur ne coute rien, n'alloue rien, et laisse l'appelant aplatir les
//  courbes avec SA tolerance -- celle du rasteriseur qui va les remplir.
//
//  CONVENTION D'ORIENTATION, et elle est l'inverse de celle des polices : ici
//  Y VA VERS LE BAS (repere d'image), l'origine est le point de depart du
//  glyphe SUR LA LIGNE DE BASE. C'est l'implementation qui retourne l'axe, une
//  fois, au bon endroit -- pas chaque appelant.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {

	// =========================================================================
	// NkIGlyphSink — recoit les contours d'un glyphe, segment par segment
	// =========================================================================
	class NkIGlyphSink {
		public:
			virtual ~NkIGlyphSink() = default;

			/// Ouvre un contour. Un glyphe en a un ou plusieurs (le « o » en a deux).
			virtual void GlyphMoveTo(float32 x, float32 y) noexcept = 0;
			virtual void GlyphLineTo(float32 x, float32 y) noexcept = 0;
			/// Bezier quadratique (TrueType).
			virtual void GlyphQuadTo(float32 cx, float32 cy, float32 x, float32 y) noexcept = 0;
			/// Bezier cubique (CFF / PostScript).
			virtual void GlyphCubicTo(float32 c1x, float32 c1y, float32 c2x, float32 c2y, float32 x,
									  float32 y) noexcept = 0;
			/// Ferme le contour courant. TOUJOURS appele avant d'en ouvrir un autre
			/// et a la fin du glyphe : un contour laisse ouvert fait fuir un
			/// remplissage nonzero sur toute la ligne.
			virtual void GlyphClose() noexcept = 0;
	};

	// =========================================================================
	// NkIGlyphSource — la fonte, vue par celui qui veut juste dessiner
	// =========================================================================
	class NkIGlyphSource {
		public:
			virtual ~NkIGlyphSource() = default;

			/// Choisit la fonte a utiliser pour les appels suivants.
			/// @param family Nom demande (« Inter », « Arial »...) ; peut etre nul.
			/// @param weight Graisse CSS (400 = normal, 700 = gras).
			/// @return true si la fonte DEMANDEE a ete trouvee ; false si un repli a
			///         ete pris. Dans les deux cas les appels suivants fonctionnent --
			///         mais un false doit etre DIT par l'appelant, jamais avale :
			///         rendre autre chose que ce qui est demande sans le dire est le
			///         vrai defaut.
			virtual bool SelectFace(const char *family, int32 weight) noexcept = 0;

			/// Le nom de la fonte reellement active (pour pouvoir dire le repli).
			virtual const char *ActiveFamily() const noexcept = 0;

			/// L'avance horizontale du point de code, en unites utilisateur, pour un
			/// corps donne. @p fontSize est le CADRATIN (em), comme en CSS et en SVG.
			virtual float32 Advance(uint32 codepoint, float32 fontSize) const noexcept = 0;

			/// La GRAISSE reellement active (400 par defaut). Un appelant qui a
			/// demande 700 et recoit 400 sait qu'il doit simuler -- ou renoncer.
			/// Non pure : une source qui n'a qu'une coupe n'a rien a declarer.
			virtual int32 ActiveWeight() const noexcept {
				return 400;
			}

			/// Les metriques verticales de la fonte active, pour un corps donne :
			/// @p ascent au-dessus de la ligne de base (positif), @p descent en
			/// dessous (positif lui aussi). Sert a `dominant-baseline`.
			///
			/// NON PURE, et volontairement : ajouter une methode pure ici casserait
			/// toute implementation ecrite ailleurs. Le defaut rend `false`, et
			/// l'appelant sait alors qu'il n'a pas les metriques -- plutot que de
			/// recevoir des zeros qu'il prendrait pour des mesures.
			virtual bool Metrics(float32 fontSize, float32 &ascent, float32 &descent) const noexcept {
				(void)fontSize;
				ascent = 0.f;
				descent = 0.f;
				return false;
			}

			/// Emet les contours du point de code dans @p sink. L'origine est
			/// (@p penX, @p baselineY), Y VERS LE BAS.
			/// @return false si le glyphe n'existe pas ou n'a aucun contour (une
			///         espace, par exemple) -- ce n'est pas une erreur.
			virtual bool Outline(uint32 codepoint, float32 fontSize, float32 penX, float32 baselineY,
								 NkIGlyphSink &sink) const noexcept = 0;
	};

} // namespace nkentseu
