#pragma once
// -----------------------------------------------------------------------------
// @File    Engine/NKEditorKit/src/NKEditorKit/NkAiThreadPaint.h
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @Brief   LA TRANSCRIPTION : le plan du panneau IA devient des commandes de
//          dessin, et RIEN de plus. Aucune geometrie ne vit ici.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// ⚠️ LA REGLE DE CE FICHIER : IL NE CALCULE AUCUNE POSITION.
//    Il lit les rectangles publies et les rend au peintre. La seule
//    arithmetique autorisee est la TRANSLATION d'origine (`ox`, `oy`),
//    uniforme -- et, pour les icones tracees, des PROPORTIONS du rectangle
//    recu (une horloge dessinee dans le carre que le plan lui a donne).
//
// IL N'INVENTE PAS NON PLUS DE COULEUR
//   Chaque piece porte son ROLE ; la couleur est demandee au peintre
//   (`ColorOf`). C'est par ce chemin -- et lui seul -- que le panneau suit le
//   theme clair.
//
// ⚠️ LE TEXTE N'EST PAS DANS LE PLAN, ET C'EST DELIBERE.
//    Le plan porte des TRANCHES (debut, longueur). La transcription retrouve la
//    chaine dans le fil, PAR L'IDENTIFIANT DU BLOC -- jamais un pointeur qui
//    survivrait a son proprietaire. Le chrome (titre, saisie, pastilles, menu)
//    n'appartient a aucun bloc : il arrive par `NkAiChromeTextes`, et chaque
//    piece dit sa SOURCE.
//
// 21/09 : LES POLICES. `TextePolice` (contrat du kit) porte le gras et la
// chasse fixe. Un peintre qui ne les a pas se RABAT sur sa police unique : le
// contenu reste juste, la forme perd la chasse fixe -- et ce repli est ecrit
// dans `NkComponentPaint`, pas cache ici.
// -----------------------------------------------------------------------------

#include "NKEditorKit/NkAiThreadLayout.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		/// Ce que le CHROME affiche : il n'appartient a aucun bloc du fil, donc
		/// l'identifiant ne peut pas le trouver. Passe a part, et facultatif.
		struct NkAiChromeTextes {
				const char *titre = nullptr;
				const char *saisie = nullptr;
				const char *invite = nullptr;
				const char *duree = nullptr;
				const char *lieu = nullptr;
				const char *modele = nullptr;
				const char *modeleDetail = nullptr;
				const char *mode = nullptr;
				const char *indication = nullptr;
				const char *const *menu = nullptr;
				const char *const *menuDetail = nullptr;
				uint32 menuN = 0;
				const NkAiActionsFil *actions = nullptr;
				/// ⚠️ ANCIEN NOM, garde pour les appelants du 20/09 : le texte du
				///    composeur. `saisie` le remplace.
				const char *composeur = nullptr;
		};

		namespace aipaint {

			/// Sinus et cosinus maison (zero-STL) : un arc d'indicateur n'a pas
			/// besoin de plus que quatre termes de serie.
			inline float32 Sin(float32 x) {
				while (x > 3.14159265f)
					x -= 6.2831853f;
				while (x < -3.14159265f)
					x += 6.2831853f;
				const float32 x2 = x * x;
				return x * (1.f - x2 / 6.f * (1.f - x2 / 20.f * (1.f - x2 / 42.f * (1.f - x2 / 72.f))));
			}
			inline float32 Cos(float32 x) {
				return Sin(x + 1.57079633f);
			}

			/// La tranche de texte que porte une piece, ou `false` si elle n'en
			/// porte pas (fonds, puces, icones).
			inline bool TextePiece(const NkAiFil &fil, const NkAiRectPublie &r, const NkAiChromeTextes &c,
								   const char *&debut, const char *&fin) {
				debut = fin = nullptr;
				const char *src = nullptr;
				if (r.piece == NkAiPiece::GouttiereIn) {
					debut = "IN";
					fin = debut + 2;
					return true;
				}
				if (r.piece == NkAiPiece::GouttiereOut) {
					debut = "OUT";
					fin = debut + 3;
					return true;
				}
				if (r.source == NkAiSource::EffetBloc && r.blocId != 0u) {
					uint32 i = 0;
					if (!fil.TrouverParId(r.blocId, i))
						return false;
					src = fil.At(i).effet.CStr();
				} else if (r.source != NkAiSource::Aucune) {
					switch (r.source) {
						case NkAiSource::Titre:		   src = c.titre; break;
						case NkAiSource::Saisie:	   src = c.saisie ? c.saisie : c.composeur; break;
						case NkAiSource::Invite:	   src = c.invite; break;
						case NkAiSource::Duree:		   src = c.duree; break;
						case NkAiSource::Lieu:		   src = c.lieu; break;
						case NkAiSource::Modele:	   src = c.modele; break;
						case NkAiSource::ModeleDetail: src = c.modeleDetail; break;
						case NkAiSource::Mode:		   src = c.mode; break;
						case NkAiSource::Indication:   src = c.indication; break;
						case NkAiSource::MenuTexte:
							src = (c.menu && r.debut < c.menuN) ? c.menu[r.debut] : nullptr;
							if (src) {
								debut = src;
								fin = nullptr;
								return src[0] != 0;
							}
							return false;
						case NkAiSource::MenuDetail:
							src = (c.menuDetail && r.debut < c.menuN) ? c.menuDetail[r.debut] : nullptr;
							if (src) {
								debut = src;
								fin = nullptr;
								return src[0] != 0;
							}
							return false;
						case NkAiSource::Action:
							src = (c.actions && r.debut < 2u) ? c.actions->libelle[r.debut] : nullptr;
							if (src) {
								debut = src;
								fin = nullptr;
								return src[0] != 0;
							}
							return false;
						default: return false;
					}
				} else if (r.blocId != 0u) {
					uint32 i = 0;
					if (!fil.TrouverParId(r.blocId, i))
						return false;
					const NkAiBlocDonnees &b = fil.At(i);
					switch (r.piece) {
						case NkAiPiece::Titre:	  src = b.titre.CStr(); break;
						case NkAiPiece::TexteIn:  src = b.entree.CStr(); break;
						case NkAiPiece::TexteOut: src = b.sortie.CStr(); break;
						case NkAiPiece::Effet:	  src = b.effet.CStr(); break;
						case NkAiPiece::Fragment:
							// Le bloc dit lui-meme ce qui va sur sa ligne : le motif pour
							// un refus ou un echec, la mesure pour un effet, le texte
							// sinon. LA MEME REGLE que la mesure (NkAiFilMesurer) -- deux
							// regles ecrites separement finissent toujours par diverger.
							if (b.type == NkAiBloc::Refus || b.type == NkAiBloc::Echec)
								src = b.motif.CStr();
							else if (b.type == NkAiBloc::Effet)
								src = b.effet.CStr();
							else
								src = b.texte.CStr();
							break;
						default: return false;
					}
				} else
					return false;
				if (!src)
					return false;
				// la tranche, bornee a la longueur reelle (une chaine qui a
				// raccourci entre la mesure et la peinture ne se lit pas au-dela)
				uint32 n = 0;
				while (src[n] && n < r.debut + r.longueur)
					++n;
				if (n <= r.debut)
					return false;
				debut = src + r.debut;
				fin = src + n;
				return true;
			}

			/// Une piece se peint-elle ? Les zones (`Texte`, `TitreConversation`)
			/// ne se peignent PAS : elles sont publiees pour etre cliquees et
			/// interrogees, leurs glyphes viennent des fragments. Une piece de texte
			/// sans texte ne se peint pas non plus -- le banc et l'ecran comptent
			/// pareil.
			inline bool PieceSePeint(const NkAiFil &fil, const NkAiRectPublie &r, const NkAiChromeTextes &c) {
				switch (r.piece) {
					case NkAiPiece::Texte:
					case NkAiPiece::TitreConversation:
						return false;
					case NkAiPiece::Titre:
					case NkAiPiece::Fragment:
					case NkAiPiece::TexteIn:
					case NkAiPiece::TexteOut:
					case NkAiPiece::Effet:
					case NkAiPiece::ChromeTexte:
					case NkAiPiece::ComposeurTexte:
					case NkAiPiece::GouttiereIn:
					case NkAiPiece::GouttiereOut: {
						const char *a = nullptr, *b = nullptr;
						return TextePiece(fil, r, c, a, b);
					}
					default:
						return r.w > 0.f && r.h > 0.f;
				}
			}

			inline float32 Arrondi(NkAiPiece p) {
				switch (p) {
					case NkAiPiece::Cadre:
					case NkAiPiece::BoiteOutil:
					case NkAiPiece::MenuFond:
						return 6.f;
					case NkAiPiece::ComposeurCadre:
						return 10.f;
					case NkAiPiece::FondOut:
					case NkAiPiece::MenuLigne:
					case NkAiPiece::Action:
						return 4.f;
					case NkAiPiece::FondCode:
						return 3.f;
					default:
						return 0.f;
				}
			}

			inline uint32 AvecAlpha(uint32 rgba, float32 a) {
				const uint32 base = rgba & 0xFFu;
				uint32 k = (uint32)((float32)base * a + 0.5f);
				if (k > 255u)
					k = 255u;
				return (rgba & 0xFFFFFF00u) | k;
			}

			/// UN CERCLE AU TRAIT, sans creux rempli : l'icone se pose sur n'importe
			/// quel fond (en-tete, composeur, menu) sans en supposer la couleur.
			inline void Anneau(NkComponentPaint &p, float32 cx, float32 cy, float32 R, uint16 role, float32 ep) {
				const int32 n = 20;
				for (int32 i = 0; i < n; ++i) {
					const float32 a0 = 6.2831853f * (float32)i / (float32)n;
					const float32 a1 = 6.2831853f * (float32)(i + 1) / (float32)n;
					(void)p.Line(cx + R * Cos(a0), cy + R * Sin(a0), cx + R * Cos(a1), cy + R * Sin(a1), role, ep);
				}
			}
			/// L'horloge, TRACEE : le kit n'a aucun atlas.
			inline void Horloge(NkComponentPaint &p, const NkPaintRect &r, uint16 role) {
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				Anneau(p, cx, cy, r.w * 0.44f, role, 1.2f);
				(void)p.Line(cx, cy, cx, cy - r.h * 0.26f, role, 1.2f);
				(void)p.Line(cx, cy, cx + r.w * 0.20f, cy, role, 1.2f);
			}
			/// La bulle portant un « + » : la conversation neuve.
			inline void BullePlus(NkComponentPaint &p, const NkPaintRect &r, uint16 role) {
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
				Anneau(p, cx, cy, r.w * 0.44f, role, 1.2f);
				// la queue de la bulle, en bas a gauche
				(void)p.Line(r.x + r.w * 0.20f, r.y + r.h * 0.80f, r.x + r.w * 0.06f, r.y + r.h * 0.98f, role, 1.2f);
				const float32 b = r.w * 0.20f;
				(void)p.Line(cx - b, cy, cx + b, cy, role, 1.2f);
				(void)p.Line(cx, cy - b, cx, cy + b, role, 1.2f);
			}
			inline void Plus(NkComponentPaint &p, const NkPaintRect &r, uint16 role) {
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, b = r.w * 0.4f;
				(void)p.Line(cx - b, cy, cx + b, cy, role, 1.4f);
				(void)p.Line(cx, cy - b, cx, cy + b, role, 1.4f);
			}
			inline void Commandes(NkComponentPaint &p, const NkPaintRect &r, uint16 role) {
				(void)p.Line(r.x, r.y, r.x + r.w, r.y, role, 1.f);
				(void)p.Line(r.x + r.w, r.y, r.x + r.w, r.y + r.h, role, 1.f);
				(void)p.Line(r.x + r.w, r.y + r.h, r.x, r.y + r.h, role, 1.f);
				(void)p.Line(r.x, r.y + r.h, r.x, r.y, role, 1.f);
				(void)p.Line(r.x + r.w * 0.62f, r.y + r.h * 0.22f, r.x + r.w * 0.38f, r.y + r.h * 0.78f, role, 1.2f);
			}
			/// L'arc qui tourne. `phase` en [0, 1) : pose par l'HOTE (horloge), le
			/// peintre ne lit aucune horloge -- deux transcriptions des memes
			/// entrees rendent le meme flux (23h).
			inline void Activite(NkComponentPaint &p, const NkPaintRect &r, uint16 role, float32 phase) {
				const float32 cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, R = r.w * 0.42f;
				const int32 n = 18;
				const float32 a0 = phase * 6.2831853f;
				for (int32 i = 0; i < n; ++i) {
					const float32 t0 = a0 + 4.71238898f * (float32)i / (float32)n;
					const float32 t1 = a0 + 4.71238898f * (float32)(i + 1) / (float32)n;
					(void)p.Line(cx + R * Cos(t0), cy + R * Sin(t0), cx + R * Cos(t1),
								 cy + R * Sin(t1), role, 1.6f);
				}
			}
			/// L'eclair du mode automatique.
			inline void Eclair(NkComponentPaint &p, const NkPaintRect &r, uint32 rgba) {
				const float32 x = r.x, y = r.y, w = r.w, h = r.h;
				const float32 xy[12] = {x + w * 0.60f, y + h * 0.00f, x + w * 0.10f, y + h * 0.58f,
										x + w * 0.48f, y + h * 0.58f, x + w * 0.38f, y + h * 1.00f,
										x + w * 0.90f, y + h * 0.40f, x + w * 0.52f, y + h * 0.40f};
				(void)p.PolygonHex(xy, 6, rgba);
			}
			inline void Coche(NkComponentPaint &p, const NkPaintRect &r, uint16 role) {
				const float32 x = r.x + r.w - 18.f, cy = r.y + r.h * 0.5f;
				(void)p.Line(x, cy, x + 4.f, cy + 4.f, role, 1.5f);
				(void)p.Line(x + 4.f, cy + 4.f, x + 11.f, cy - 4.f, role, 1.5f);
			}

		} // namespace aipaint

		/// Transcrit un plan. `ox`, `oy` : l'origine du plan a l'ecran. `phase` :
		/// la phase de l'indicateur d'activite, posee par l'hote.
		/// ⚠️ ELLE NE LIT NI LA SOURIS NI L'HORLOGE : deux appels avec les memes
		///    entrees produisent le meme flux de commandes (23h).
		inline void NkAiFilPeindre(NkComponentPaint &p, const NkAiFil &fil, const NkAiPlan &plan, float32 ox, float32 oy,
								   const NkAiChromeTextes &chrome = NkAiChromeTextes{}, float32 phase = 0.f) {
			// ── PASSE 1 : LE RAIL, SOUS LES PUCES ──
			for (uint32 i = 0; i < plan.Pieces(); ++i) {
				const NkAiRectPublie &r = plan.Piece(i);
				if (r.piece != NkAiPiece::Rail)
					continue;
				p.Fill({r.x + ox, r.y + oy, r.w, r.h}, (uint16)r.role, 0.f);
			}
			// ── PASSE 2 : LE RESTE, DANS L'ORDRE DU PLAN ──
			for (uint32 i = 0; i < plan.Pieces(); ++i) {
				const NkAiRectPublie &r = plan.Piece(i);
				if (r.piece == NkAiPiece::Rail)
					continue;
				// ⚠️ LA SEULE ARITHMETIQUE DU FICHIER, et elle est uniforme.
				const NkPaintRect rect = {r.x + ox, r.y + oy, r.w, r.h};
				const uint16 role = (uint16)r.role;
				const bool eteint = (r.drapeaux & kAiEteint) != 0u;
				const bool survol = (r.drapeaux & kAiSurvol) != 0u;

				switch (r.piece) {
					case NkAiPiece::Texte:
					case NkAiPiece::TitreConversation:
						break; // des ZONES : cliquees, jamais peintes
					case NkAiPiece::Cadre:
					case NkAiPiece::BoiteOutil:
					case NkAiPiece::MenuFond:
					case NkAiPiece::ComposeurCadre:
						// Plein PUIS creuse d'un pixel : `Outline` prend deux roles,
						// et le second n'est pas decoratif.
						p.Outline(rect, (uint16)NkRole::Border, role, aipaint::Arrondi(r.piece));
						break;
					case NkAiPiece::FondCode:
						p.Outline(rect, (uint16)NkRole::Border, role, aipaint::Arrondi(r.piece));
						break;
					case NkAiPiece::FondIn:
					case NkAiPiece::FondOut:
						p.Fill(rect, role, aipaint::Arrondi(r.piece));
						break;
					case NkAiPiece::Estompe: {
						// L'ESTOMPE EST UN DEGRADE. Le contrat n'a pas de degrade :
						// cinq bandes d'opacite croissante le rendent a l'oeil, dans
						// le rectangle publie -- rien ne sort de lui.
						const uint32 c = p.ColorOf(role);
						for (int32 k = 0; k < 5; ++k) {
							const float32 hk = rect.h / 5.f;
							p.FillColor({rect.x, rect.y + hk * (float32)k, rect.w, hk},
										aipaint::AvecAlpha(c, 0.18f + 0.18f * (float32)k), 0.f);
						}
						break;
					}
					case NkAiPiece::Puce:
						p.Ellipse(rect, role);
						break;
					case NkAiPiece::Filet:
					case NkAiPiece::Separateur:
					case NkAiPiece::Caret:
						p.Fill(rect, role, 0.f);
						break;
					case NkAiPiece::IconeHistorique:
						aipaint::Horloge(p, rect, survol ? (uint16)NkRole::Text : role);
						break;
					case NkAiPiece::IconeNouvelle:
						aipaint::BullePlus(p, rect, survol ? (uint16)NkRole::Text : role);
						break;
					case NkAiPiece::BoutonPlus:
						if (survol)
							p.Fill({rect.x - 3.f, rect.y - 3.f, rect.w + 6.f, rect.h + 6.f}, (uint16)NkRole::ButtonBg, 4.f);
						aipaint::Plus(p, rect, survol ? (uint16)NkRole::Text : role);
						break;
					case NkAiPiece::BoutonCommandes:
						if (survol)
							p.Fill({rect.x - 3.f, rect.y - 3.f, rect.w + 6.f, rect.h + 6.f}, (uint16)NkRole::ButtonBg, 4.f);
						aipaint::Commandes(p, rect, survol ? (uint16)NkRole::Text : role);
						break;
					case NkAiPiece::Activite:
						aipaint::Activite(p, rect, role, phase);
						break;
					case NkAiPiece::Horloge:
						aipaint::Horloge(p, rect, role);
						break;
					case NkAiPiece::PastilleLieu: {
						p.Fill(rect, (uint16)NkRole::Border, rect.h * 0.5f);
						const float32 d = 7.f;
						p.Ellipse({rect.x + 10.f, rect.y + (rect.h - d) * 0.5f, d, d}, role);
						break;
					}
					case NkAiPiece::PastilleModele:
					case NkAiPiece::PastilleMode:
						if (r.piece == NkAiPiece::PastilleModele || survol)
							p.Fill(rect, survol ? (uint16)NkRole::CodeOutBg : (uint16)NkRole::Border, rect.h * 0.5f);
						if (r.piece == NkAiPiece::PastilleMode)
							aipaint::Eclair(p, {rect.x + 4.f, rect.y + rect.h * 0.18f, 10.f, rect.h * 0.64f},
											p.ColorOf(role));
						break;
					case NkAiPiece::Envoi: {
						const bool arret = (r.drapeaux & kAiArret) != 0u;
						if (eteint)
							p.Fill(rect, (uint16)NkRole::ButtonBg, 5.f);
						else
							p.Fill(rect, role, 5.f);
						const uint16 fl = eteint ? (uint16)NkRole::TextMuted : (uint16)NkRole::TextOnAccent;
						const float32 cx = rect.x + rect.w * 0.5f, cy = rect.y + rect.h * 0.5f;
						if (arret) {
							const float32 s = rect.w * 0.30f;
							p.Fill({cx - s * 0.5f, cy - s * 0.5f, s, s}, fl, 1.5f);
						} else {
							const float32 h = rect.h * 0.26f;
							(void)p.Line(cx, cy - h, cx, cy + h, fl, 1.8f);
							(void)p.Line(cx, cy - h, cx - h * 0.8f, cy - h * 0.2f, fl, 1.8f);
							(void)p.Line(cx, cy - h, cx + h * 0.8f, cy - h * 0.2f, fl, 1.8f);
						}
						break;
					}
					case NkAiPiece::MenuLigne:
						if (survol)
							p.Fill(rect, (uint16)NkRole::ButtonBg, aipaint::Arrondi(r.piece));
						if ((r.drapeaux & kAiActif) != 0u)
							aipaint::Coche(p, rect, (uint16)NkRole::AccentUi);
						break;
					case NkAiPiece::Action:
						p.Outline(rect, (uint16)NkRole::Border, survol && !eteint ? (uint16)NkRole::ButtonBg
																				 : (uint16)NkRole::PanelBg,
								  aipaint::Arrondi(r.piece));
						{
							const char *a = nullptr, *b = nullptr;
							if (aipaint::TextePiece(fil, r, chrome, a, b))
								p.TextePolice({rect.x + 10.f, rect.y, rect.w - 20.f, rect.h}, a, b,
											  eteint ? (uint16)NkRole::TextMuted : role, 0u);
						}
						break;
					case NkAiPiece::TexteIn:
					case NkAiPiece::TexteOut: {
						// LE CODE SE COUPE NET AU BORD (la capture) : rogne, jamais ellipse.
						const char *a = nullptr, *b = nullptr;
						if (!aipaint::TextePiece(fil, r, chrome, a, b))
							break;
						p.PushClip(rect);
						p.TextePolice(rect, a, b, role, (uint8)r.police);
						p.PopClip();
						break;
					}
					default: {
						const char *a = nullptr, *b = nullptr;
						if (!aipaint::TextePiece(fil, r, chrome, a, b))
							break;
						// LES LIGNES DE MENU SONT ROGNEES a leur rectangle : leur texte vient
						// entier de l'hote (un motif peut etre long), le plan lui a donne sa place.
						if (r.source == NkAiSource::MenuTexte || r.source == NkAiSource::MenuDetail) {
							p.PushClip(rect);
							p.TextePolice(rect, a, b, role, (uint8)r.police);
							p.PopClip();
							break;
						}
						// ⚠️ UNE PIECE DE TEXTE SANS TEXTE NE SE PEINT PAS (23d).
						if (!a || a == b || !a[0])
							break;
						if ((r.drapeaux & kAiEllipse) != 0u) {
							// « ... » colle a la tranche, dans la meme commande : aucune
							// position a calculer pour les points de suite.
							char tmp[600];
							uint32 n = 0;
							for (const char *q = a; (b ? q < b : *q != 0) && *q && n + 4u < (uint32)sizeof(tmp); ++q)
								tmp[n++] = *q;
							tmp[n++] = '.';
							tmp[n++] = '.';
							tmp[n++] = '.';
							tmp[n] = 0;
							p.TextePolice(rect, tmp, nullptr, role, (uint8)r.police);
						} else
							p.TextePolice(rect, a, b, role, (uint8)r.police);
						break;
					}
				}
			}
		}

	} // namespace editorkit
} // namespace nkentseu
