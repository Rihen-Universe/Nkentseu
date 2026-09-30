//
// NkGuiNavigation.cpp
// =============================================================================
// Description :
//   La navigation au focus, sur le releve de l'introspection. Voir
//   NkGuiNavigation.h.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NKGui/Core/NkGuiNavigation.h"

#include "NKGui/Core/NkGuiContext.h"
#include "NKGui/Core/NkGuiIntrospect.h"

namespace nkentseu {
	namespace nkgui {

		namespace {

			/// Ce qu'on peut viser : ce qu'on peut ACTIVER. Un libelle, une
			/// region calculee ou une mesure ne se focalisent pas.
			bool Focalisable(const NkGuiNote &n) noexcept {
				if ((n.etats & (NK_GUI_ETAT_GRISE | NK_GUI_ETAT_VIDE | NK_GUI_ETAT_HORS_VUE)) != 0) {
					return false;
				}
				switch (n.nature) {
					case NkGuiNature::Bouton:
					case NkGuiNature::Case:
					case NkGuiNature::Element:
					case NkGuiNature::Onglet:
					case NkGuiNature::Champ:
					case NkGuiNature::Reglage:
					case NkGuiNature::EntreeMenu:
					case NkGuiNature::Menu:
					case NkGuiNature::Section:
						return true;
					default:
						return false;
				}
			}

			NkVec2 Centre(const NkRect &r) noexcept {
				return NkVec2{r.x + r.w * 0.5f, r.y + r.h * 0.5f};
			}

		} // namespace

		void NkGuiNavSourisBougee(NkGuiNavigation &nav) noexcept {
			nav.pilote = false;
			nav.phase = 0;
		}

		void NkGuiNavAvancer(NkGuiContext &ctx, NkGuiNavigation &nav, NkGuiNavDirection direction, bool activer,
							 bool annuler) noexcept {
			if (!nav.actif) {
				return;
			}
			// Le releve est la carte de l'ecran : sans lui, rien a viser. Il se
			// remplit a partir de l'image SUIVANTE.
			if (!NkGuiIntrospectActif(ctx)) {
				NkGuiIntrospectActiver(ctx, true);
			}
			int32 nombre = 0;
			const NkGuiNote *notes = NkGuiIntrospectNotes(ctx, nombre);

			// ── Un menu ou une fenetre surgissante ouverte garde le focus pour
			//    elle : on ne navigue que dans la couche la plus haute. ──
			int32 niveauHaut = -1000;
			for (int32 i = 0; i < nombre; ++i) {
				if (Focalisable(notes[i]) && notes[i].niveau > niveauHaut) {
					niveauHaut = notes[i].niveau;
				}
			}

			// ── Le focus existe-t-il encore ? ──
			const NkGuiNote *actuel = nullptr;
			for (int32 i = 0; i < nombre; ++i) {
				if (notes[i].id == nav.focus && Focalisable(notes[i]) && notes[i].niveau == niveauHaut) {
					actuel = &notes[i];
					break;
				}
			}
			const bool demande = direction != NkGuiNavDirection::Aucune || activer;
			if (actuel == nullptr && demande) {
				// Premier geste : on montre OU l'on est, sans encore bouger. Le
				// premier controle, en haut a gauche.
				for (int32 i = 0; i < nombre; ++i) {
					if (!Focalisable(notes[i]) || notes[i].niveau != niveauHaut) {
						continue;
					}
					if (actuel == nullptr || notes[i].rect.y < actuel->rect.y ||
						(notes[i].rect.y == actuel->rect.y && notes[i].rect.x < actuel->rect.x)) {
						actuel = &notes[i];
					}
				}
				direction = NkGuiNavDirection::Aucune;
				activer = false;
			} else if (actuel != nullptr && direction != NkGuiNavDirection::Aucune) {
				// ── Le plus proche dans la direction : ecart dans l'axe + deux
				//    fois l'ecart de travers. Sans le facteur 2, « en bas » sautait
				//    a un controle lointain en diagonale plutot qu'a celui dessous. ──
				const NkVec2 c0 = Centre(actuel->rect);
				const NkGuiNote *meilleur = nullptr;
				float32 meilleurScore = 1.0e30f;
				for (int32 i = 0; i < nombre; ++i) {
					const NkGuiNote &n = notes[i];
					if (!Focalisable(n) || n.niveau != niveauHaut || n.id == actuel->id) {
						continue;
					}
					const NkVec2 c = Centre(n.rect);
					const float32 dx = c.x - c0.x;
					const float32 dy = c.y - c0.y;
					float32 axe = 0.f;
					float32 travers = 0.f;
					switch (direction) {
						case NkGuiNavDirection::Bas:    axe = dy;  travers = dx; break;
						case NkGuiNavDirection::Haut:   axe = -dy; travers = dx; break;
						case NkGuiNavDirection::Droite: axe = dx;  travers = dy; break;
						case NkGuiNavDirection::Gauche: axe = -dx; travers = dy; break;
						default:                        break;
					}
					if (axe <= 1.f) {
						continue; // pas dans la direction demandee
					}
					const float32 score = axe + 2.f * (travers < 0.f ? -travers : travers);
					if (score < meilleurScore) {
						meilleurScore = score;
						meilleur = &n;
					}
				}
				if (meilleur != nullptr) {
					actuel = meilleur;
				}
			}

			if (actuel != nullptr) {
				nav.focus = actuel->id;
				nav.focusRect = actuel->rect;
			}
			if (demande) {
				nav.pilote = true;
			}
			if (!nav.pilote || nav.focus == NKGUI_ID_NONE) {
				return;
			}

			// ── Le pointeur virtuel : le controle focalise est SURVOLE ──
			ctx.input.mousePos = Centre(nav.focusRect);

			// ── L'activation : viser, appuyer, relacher (trois images) ──
			if (activer && nav.phase == 0) {
				nav.phase = 1;
			}
			switch (nav.phase) {
				case 1:
					ctx.input.mouseDown[0] = false;
					nav.phase = 2;
					break;
				case 2:
					ctx.input.mouseDown[0] = true;
					nav.phase = 3;
					break;
				case 3:
					ctx.input.mouseDown[0] = false;
					nav.phase = 0;
					break;
				default:
					break;
			}

			// ── Annuler : Echap, pose une image puis relache ──
			if (nav.echapPose) {
				ctx.input.SetKey(NkGuiKey::Escape, false);
				nav.echapPose = false;
			} else if (annuler) {
				ctx.input.SetKey(NkGuiKey::Escape, true);
				nav.echapPose = true;
			}
		}

		void NkGuiNavDessiner(NkGuiContext &ctx, const NkGuiNavigation &nav) noexcept {
			if (!nav.actif || !nav.pilote || nav.focus == NKGUI_ID_NONE) {
				return;
			}
			const NkRect r{nav.focusRect.x - 2.f, nav.focusRect.y - 2.f, nav.focusRect.w + 4.f, nav.focusRect.h + 4.f};
			ctx.dlOverlay.AddRect(r, ctx.theme.accent, 2.f, ctx.theme.rounding);
		}

	} // namespace nkgui
} // namespace nkentseu
