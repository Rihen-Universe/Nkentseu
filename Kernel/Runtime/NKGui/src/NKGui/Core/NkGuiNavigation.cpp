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
#include "NKGui/Core/NkGuiInput.h"
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

			/// L'ordre de LECTURE : de haut en bas, puis de gauche a droite. Deux
			/// controles dont les centres sont a moins d'une demi-hauteur sont
			/// sur la meme LIGNE (des boutons cote a cote d'une HBox n'ont pas
			/// tous le meme y au pixel pres).
			bool AvantDansLaLecture(const NkGuiNote &a, const NkGuiNote &b) noexcept {
				const float32 ya = a.rect.y + a.rect.h * 0.5f;
				const float32 yb = b.rect.y + b.rect.h * 0.5f;
				const float32 tol = (a.rect.h < b.rect.h ? a.rect.h : b.rect.h) * 0.5f;
				if (ya < yb - tol) {
					return true;
				}
				if (yb < ya - tol) {
					return false;
				}
				return a.rect.x < b.rect.x;
			}

			bool MemeCle(const char *a, const char *b) noexcept {
				int32 i = 0;
				while (a[i] != '\0' && a[i] == b[i]) {
					++i;
				}
				return a[i] == b[i];
			}

		} // namespace

		void NkGuiNavSourisBougee(NkGuiNavigation &nav) noexcept {
			nav.pilote = false;
			nav.phase = 0;
		}

		void NkGuiNavFocaliserCle(NkGuiNavigation &nav, const char *cle, bool montrer) noexcept {
			int32 i = 0;
			if (cle != nullptr) {
				for (; cle[i] != '\0' && i + 1 < static_cast<int32>(sizeof(nav.cleDemandee)); ++i) {
					nav.cleDemandee[i] = cle[i];
				}
			}
			nav.cleDemandee[i] = '\0';
			nav.demandeMontre = montrer;
		}

		bool NkGuiNavDepuisClavier(const NkGuiInput &in, NkGuiNavDirection &direction, bool &activer,
								   bool &annuler) noexcept {
			bool lu = false;
			if (in.KeyPressedRepeat(NkGuiKey::Up)) {
				direction = NkGuiNavDirection::Haut;
				lu = true;
			} else if (in.KeyPressedRepeat(NkGuiKey::Down)) {
				direction = NkGuiNavDirection::Bas;
				lu = true;
			} else if (in.KeyPressedRepeat(NkGuiKey::Left)) {
				direction = NkGuiNavDirection::Gauche;
				lu = true;
			} else if (in.KeyPressedRepeat(NkGuiKey::Right)) {
				direction = NkGuiNavDirection::Droite;
				lu = true;
			} else if (in.KeyPressedRepeat(NkGuiKey::Tab)) {
				direction = in.shiftDown ? NkGuiNavDirection::Precedent : NkGuiNavDirection::Suivant;
				lu = true;
			}
			if (in.KeyPressed(NkGuiKey::Enter)) {
				activer = true;
				lu = true;
			}
			if (in.KeyPressed(NkGuiKey::Escape)) {
				annuler = true;
				lu = true;
			}
			return lu;
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
			// ── (02/10) Un focus DEMANDE par l'hote, par sa cle stable ──
			//    Servi des que le controle est a l'ecran (le releve vient de
			//    l'image d'avant : un menu qui s'ouvre l'a a l'image suivante).
			if (nav.cleDemandee[0] != '\0') {
				for (int32 i = 0; i < nombre; ++i) {
					if (Focalisable(notes[i]) && notes[i].niveau == niveauHaut && MemeCle(notes[i].cle, nav.cleDemandee)) {
						actuel = &notes[i];
						nav.cleDemandee[0] = '\0';
						if (nav.demandeMontre) {
							nav.pilote = true;
						}
						break;
					}
				}
			}
			const bool demande = direction != NkGuiNavDirection::Aucune || activer;
			if (actuel == nullptr && demande) {
				// Premier geste : on montre OU l'on est, sans encore bouger. Le
				// premier controle dans l'ordre de lecture.
				for (int32 i = 0; i < nombre; ++i) {
					if (!Focalisable(notes[i]) || notes[i].niveau != niveauHaut) {
						continue;
					}
					if (actuel == nullptr || AvantDansLaLecture(notes[i], *actuel)) {
						actuel = &notes[i];
					}
				}
				direction = NkGuiNavDirection::Aucune;
				activer = false;
			} else if (actuel != nullptr &&
					   (direction == NkGuiNavDirection::Suivant || direction == NkGuiNavDirection::Precedent)) {
				// ── (02/10) L'ORDRE DE LECTURE, en bouclant : le suivant est le
				//    plus proche APRES l'actuel ; sans lui, le tout premier. ──
				const bool suivant = direction == NkGuiNavDirection::Suivant;
				const NkGuiNote *proche = nullptr;
				const NkGuiNote *bout = nullptr;
				for (int32 i = 0; i < nombre; ++i) {
					const NkGuiNote &n = notes[i];
					if (!Focalisable(n) || n.niveau != niveauHaut || n.id == actuel->id) {
						continue;
					}
					const bool apres = suivant ? AvantDansLaLecture(*actuel, n) : AvantDansLaLecture(n, *actuel);
					if (apres) {
						if (proche == nullptr ||
							(suivant ? AvantDansLaLecture(n, *proche) : AvantDansLaLecture(*proche, n))) {
							proche = &n;
						}
					} else if (bout == nullptr ||
							   (suivant ? AvantDansLaLecture(n, *bout) : AvantDansLaLecture(*bout, n))) {
						bout = &n;
					}
				}
				if (proche != nullptr) {
					actuel = proche;
				} else if (bout != nullptr) {
					actuel = bout;
				}
			} else if (actuel != nullptr && direction != NkGuiNavDirection::Aucune) {
				// ── Le plus proche dans la direction : ecart dans l'axe + deux
				//    fois l'ecart de travers. Sans le facteur 2, « en bas » sautait
				//    a un controle lointain en diagonale plutot qu'a celui dessous. ──
				const NkVec2 c0 = Centre(actuel->rect);
				const NkGuiNote *meilleur = nullptr;
				float32 meilleurScore = 1.0e30f;
				// (02/10) Le bout OPPOSE, pour boucler : le plus loin dans l'autre
				// sens, avec le moins de travers possible.
				const NkGuiNote *oppose = nullptr;
				float32 opposeScore = 1.0e30f;
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
					const float32 t = travers < 0.f ? -travers : travers;
					if (axe <= 1.f) {
						if (axe < -1.f) {
							// axe negatif : plus il est loin, plus le score baisse.
							const float32 s = 2.f * t + axe;
							if (s < opposeScore) {
								opposeScore = s;
								oppose = &n;
							}
						}
						continue; // pas dans la direction demandee
					}
					const float32 score = axe + 2.f * t;
					if (score < meilleurScore) {
						meilleurScore = score;
						meilleur = &n;
					}
				}
				if (meilleur != nullptr) {
					actuel = meilleur;
				} else if (nav.boucler && oppose != nullptr) {
					actuel = oppose;
				}
			}

			if (actuel != nullptr) {
				nav.focus = actuel->id;
				nav.focusRect = actuel->rect;
				int32 k = 0;
				for (; actuel->cle[k] != '\0' && k + 1 < static_cast<int32>(sizeof(nav.focusCle)); ++k) {
					nav.focusCle[k] = actuel->cle[k];
				}
				nav.focusCle[k] = '\0';
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
