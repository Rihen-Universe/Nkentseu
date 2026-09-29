//
// NkUnkenyEntreesJeu.cpp
// =============================================================================
// Description :
//   Clavier, souris, doigts et manettes -> NkActions, par la table de
//   liaisons. Voir NkUnkenyEntreesJeu.h pour les regles (somme bornee, zone
//   morte radiale, joystick flottant, capture d'une entree).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Entree/NkUnkenyEntreesJeu.h"

#include "NKMath/NkFunctions.h"

namespace nkentseu {
	namespace unkeny {

		namespace {

			constexpr uint32 kNbTouches = static_cast<uint32>(NkKey::NK_KEY_MAX);
			constexpr uint32 kNbSouris = static_cast<uint32>(NkMouseButton::NK_MOUSE_BUTTON_MAX);
			constexpr uint32 kNbBoutons = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);

			/// L'autre axe du meme stick, ou l'axe lui-meme si ce n'est pas un stick.
			NkGamepadAxis AxeJumeau(NkGamepadAxis a, bool &estStick) noexcept {
				estStick = true;
				switch (a) {
					case NkGamepadAxis::NK_GP_AXIS_LX: return NkGamepadAxis::NK_GP_AXIS_LY;
					case NkGamepadAxis::NK_GP_AXIS_LY: return NkGamepadAxis::NK_GP_AXIS_LX;
					case NkGamepadAxis::NK_GP_AXIS_RX: return NkGamepadAxis::NK_GP_AXIS_RY;
					case NkGamepadAxis::NK_GP_AXIS_RY: return NkGamepadAxis::NK_GP_AXIS_RX;
					default:                           break;
				}
				estStick = false;
				return a;
			}

			/// Zone morte AXIALE, avec remise a l'echelle de la course restante.
			float32 ZoneMorteAxiale(float32 v, float32 zm) noexcept {
				const float32 m = math::NkFabs(v);
				if (m <= zm) {
					return 0.f;
				}
				const float32 k = (m - zm) / (1.f - zm);
				return v < 0.f ? -k : k;
			}

		} // namespace

		NkEntreesJeu::NkEntreesJeu() noexcept {
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				mManetteDe[j] = j;
			}
			for (uint32 k = 0; k < kNbTouches; ++k) {
				mTouches[k] = false;
			}
			for (uint32 b = 0; b < kNbSouris; ++b) {
				mSouris[b] = false;
			}
			for (uint32 p = 0; p < NK_MAX_GAMEPADS; ++p) {
				for (uint32 b = 0; b < kNbBoutons; ++b) {
					mCaptureBoutonsAvant[p][b] = false;
				}
			}
		}

		NkActions &NkEntreesJeu::Actions(int32 joueur) noexcept {
			if (joueur < 0 || joueur >= NK_UNKENY_JOUEURS_MAX) {
				return mActions[0];
			}
			return mActions[joueur];
		}

		const NkActions &NkEntreesJeu::Actions(int32 joueur) const noexcept {
			if (joueur < 0 || joueur >= NK_UNKENY_JOUEURS_MAX) {
				return mActions[0];
			}
			return mActions[joueur];
		}

		void NkEntreesJeu::AssignerManette(int32 joueur, int32 manette) noexcept {
			if (joueur < 0 || joueur >= NK_UNKENY_JOUEURS_MAX) {
				return;
			}
			mManetteDe[joueur] = (manette >= 0 && manette < static_cast<int32>(NK_MAX_GAMEPADS)) ? manette : -1;
		}

		int32 NkEntreesJeu::ManetteDe(int32 joueur) const noexcept {
			if (joueur < 0 || joueur >= NK_UNKENY_JOUEURS_MAX) {
				return -1;
			}
			return mManetteDe[joueur];
		}

		void NkEntreesJeu::PoserSurface(float32 x, float32 y, float32 l, float32 h) noexcept {
			mSurfX = x;
			mSurfY = y;
			mSurfL = l;
			mSurfH = h;
		}

		bool NkEntreesJeu::ToucheTenue(NkKey touche) const noexcept {
			const uint32 k = static_cast<uint32>(touche);
			return k < kNbTouches && mTouches[k];
		}

		uint32 NkEntreesJeu::DoigtsSuivis() const noexcept {
			uint32 n = 0;
			for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
				if (mDoigts[i].actif) {
					++n;
				}
			}
			return n;
		}

		void NkEntreesJeu::Normaliser(float32 x, float32 y, float32 &u, float32 &v) const noexcept {
			if (mSurfL <= 0.f || mSurfH <= 0.f) {
				// Hors de toute zone : sans surface, un doigt n'a pas de place.
				u = -1.f;
				v = -1.f;
				return;
			}
			u = (x - mSurfX) / mSurfL;
			v = (y - mSurfY) / mSurfH;
		}

		int32 NkEntreesJeu::StickSous(float32 u, float32 v) const noexcept {
			for (int32 i = 0; i < mLiaisons.Nombre(); ++i) {
				const NkLiaison &l = mLiaisons[i];
				if (l.nature != NkNatureLiaison::NK_ZONE_STICK_X && l.nature != NkNatureLiaison::NK_ZONE_STICK_Y) {
					continue;
				}
				if (!l.zone.Contient(u, v)) {
					continue;
				}
				// Un seul doigt par joystick : le second doigt pose dans la meme
				// zone n'en prend pas le controle (il peut viser un bouton).
				bool pris = false;
				for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
					const NkDoigt &f = mDoigts[d];
					if (f.actif && f.stick >= 0 && mLiaisons[f.stick].zone == l.zone &&
						mLiaisons[f.stick].joueur == l.joueur) {
						pris = true;
						break;
					}
				}
				if (!pris) {
					return i;
				}
			}
			return -1;
		}

		// =====================================================================
		// La capture d'une entree (le menu « appuyez sur une touche »)
		// =====================================================================

		bool NkEntreesJeu::CapturerProchaineEntree(int32 indice) noexcept {
			if (indice < 0 || indice >= mLiaisons.Nombre() ||
				mLiaisons[indice].nature != NkNatureLiaison::NK_ENTREE) {
				return false;
			}
			mCapture = indice;
			return true;
		}

		void NkEntreesJeu::Capturer(const NkInputCode &code) noexcept {
			if (mCapture < 0) {
				return;
			}
			mLiaisons.Relier(mCapture, code);
			mCapture = -1;
		}

		void NkEntreesJeu::CapturerManettes(const NkGamepadSystem *manettes) noexcept {
			for (uint32 p = 0; p < NK_MAX_GAMEPADS; ++p) {
				const bool branchee = manettes != nullptr && manettes->IsConnected(p);
				for (uint32 b = 0; b < kNbBoutons; ++b) {
					const bool bas = branchee && manettes->IsButtonDown(p, static_cast<NkGamepadButton>(b));
					if (mCapture >= 0 && bas && !mCaptureBoutonsAvant[p][b] && b != 0u) {
						Capturer(NkInputCode::Gamepad(static_cast<NkGamepadButton>(b)));
					}
					mCaptureBoutonsAvant[p][b] = bas;
				}
				if (mCapture < 0 || !branchee) {
					continue;
				}
				// Les axes des sticks et des gachettes ; pas ceux de la croix, qui
				// a deja ses quatre boutons.
				static const NkGamepadAxis kAxes[] = {
					NkGamepadAxis::NK_GP_AXIS_LX, NkGamepadAxis::NK_GP_AXIS_LY, NkGamepadAxis::NK_GP_AXIS_RX,
					NkGamepadAxis::NK_GP_AXIS_RY, NkGamepadAxis::NK_GP_AXIS_LT, NkGamepadAxis::NK_GP_AXIS_RT,
				};
				for (NkGamepadAxis a : kAxes) {
					if (math::NkFabs(manettes->GetAxis(p, a)) > 0.5f) {
						Capturer(NkInputCode::GamepadAxis(a));
						break;
					}
				}
			}
		}

		// =====================================================================
		// Les evenements
		// =====================================================================

		bool NkEntreesJeu::Lire(const NkEvent &e) noexcept {
			if (const auto *k = e.As<NkKeyPressEvent>()) {
				const uint32 i = static_cast<uint32>(k->GetKey());
				if (mCapture >= 0) {
					Capturer(NkInputCode::Key(k->GetKey()));
					return true;
				}
				if (i < kNbTouches) {
					mTouches[i] = true;
				}
				return mLiaisons.Concerne(NkInputCode::Key(k->GetKey()));
			}
			if (const auto *k = e.As<NkKeyRepeatEvent>()) {
				return mLiaisons.Concerne(NkInputCode::Key(k->GetKey()));
			}
			if (const auto *k = e.As<NkKeyReleaseEvent>()) {
				const uint32 i = static_cast<uint32>(k->GetKey());
				if (i < kNbTouches) {
					mTouches[i] = false;
				}
				return mLiaisons.Concerne(NkInputCode::Key(k->GetKey()));
			}
			if (const auto *m = e.As<NkMouseButtonPressEvent>()) {
				const uint32 i = static_cast<uint32>(m->GetButton());
				if (mCapture >= 0) {
					Capturer(NkInputCode::Mouse(m->GetButton()));
					return true;
				}
				if (i < kNbSouris) {
					mSouris[i] = true;
				}
				return mLiaisons.Concerne(NkInputCode::Mouse(m->GetButton()));
			}
			if (const auto *m = e.As<NkMouseButtonReleaseEvent>()) {
				const uint32 i = static_cast<uint32>(m->GetButton());
				if (i < kNbSouris) {
					mSouris[i] = false;
				}
				return mLiaisons.Concerne(NkInputCode::Mouse(m->GetButton()));
			}
			if (const auto *w = e.As<NkMouseWheelVerticalEvent>()) {
				mMolette += static_cast<float32>(w->GetDeltaY());
				return mLiaisons.Concerne(NkInputCode::Wheel(false));
			}
			if (const auto *w = e.As<NkMouseWheelHorizontalEvent>()) {
				mMoletteH += static_cast<float32>(w->GetDeltaX());
				return mLiaisons.Concerne(NkInputCode::Wheel(true));
			}

			// ── Les doigts ──
			const NkEventType::Value type = e.GetType();
			if (type == NkEventType::NK_TOUCH_CANCEL) {
				for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
					mDoigts[d].actif = false;
				}
				return true;
			}
			if (type != NkEventType::NK_TOUCH_BEGIN && type != NkEventType::NK_TOUCH_MOVE &&
				type != NkEventType::NK_TOUCH_END) {
				return false;
			}
			const NkTouchEvent &t = static_cast<const NkTouchEvent &>(e);
			bool pris = false;
			for (uint32 k = 0; k < t.GetNumTouches(); ++k) {
				const NkTouchPoint &p = t.GetTouch(k);
				int32 slot = -1;
				int32 libre = -1;
				for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
					if (mDoigts[d].actif && mDoigts[d].id == p.id) {
						slot = static_cast<int32>(d);
						break;
					}
					if (!mDoigts[d].actif && libre < 0) {
						libre = static_cast<int32>(d);
					}
				}
				if (type == NkEventType::NK_TOUCH_BEGIN && slot < 0) {
					slot = libre;
					if (slot < 0) {
						continue;
					}
					NkDoigt &f = mDoigts[slot];
					f.id = p.id;
					f.actif = true;
					Normaliser(p.clientX, p.clientY, f.u, f.v);
					f.u0 = f.u;
					f.v0 = f.v;
					f.stick = StickSous(f.u, f.v);
				}
				if (slot < 0) {
					continue;
				}
				NkDoigt &f = mDoigts[slot];
				Normaliser(p.clientX, p.clientY, f.u, f.v);
				if (f.stick >= 0) {
					pris = true;
				}
				for (int32 i = 0; i < mLiaisons.Nombre() && !pris; ++i) {
					const NkLiaison &l = mLiaisons[i];
					if (l.nature == NkNatureLiaison::NK_ZONE_BOUTON && l.zone.Contient(f.u, f.v)) {
						pris = true;
					}
				}
				if (type == NkEventType::NK_TOUCH_END) {
					f.actif = false;
					f.stick = -1;
				}
			}
			return pris;
		}

		// =====================================================================
		// La valeur d'une liaison
		// =====================================================================

		float32 NkEntreesJeu::ValeurStick(const NkLiaison &l) const noexcept {
			for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
				const NkDoigt &f = mDoigts[d];
				if (!f.actif || f.stick < 0) {
					continue;
				}
				const NkLiaison &capteur = mLiaisons[f.stick];
				if (!(capteur.zone == l.zone) || capteur.joueur != l.joueur) {
					continue;
				}
				// En PIXELS, puis rapporte au rayon : un joystick doit etre rond
				// a l'ecran, pas etire comme la surface.
				const float32 r = l.rayon * mSurfH;
				if (r <= 0.f) {
					return 0.f;
				}
				float32 x = (f.u - f.u0) * mSurfL / r;
				float32 y = -(f.v - f.v0) * mSurfH / r; // l'ecran descend, le stick monte
				float32 m = math::NkSqrt(x * x + y * y);
				if (m > 1.f) {
					x /= m;
					y /= m;
					m = 1.f;
				}
				if (m <= l.zoneMorte || m <= 0.f) {
					return 0.f;
				}
				const float32 k = (m - l.zoneMorte) / (1.f - l.zoneMorte) / m;
				const float32 axe = (l.nature == NkNatureLiaison::NK_ZONE_STICK_X) ? x * k : y * k;
				return axe * l.echelle;
			}
			return 0.f;
		}

		float32 NkEntreesJeu::Valeur(const NkLiaison &l, int32 joueur, const NkGamepadSystem *manettes) const noexcept {
			switch (l.nature) {
				case NkNatureLiaison::NK_ZONE_BOUTON: {
					for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
						const NkDoigt &f = mDoigts[d];
						// Un doigt qui tient un joystick ne presse pas un bouton
						// en y glissant : c'est la regle de tous les jeux mobiles.
						if (f.actif && f.stick < 0 && l.zone.Contient(f.u, f.v)) {
							return l.echelle;
						}
					}
					return 0.f;
				}
				case NkNatureLiaison::NK_ZONE_STICK_X:
				case NkNatureLiaison::NK_ZONE_STICK_Y:
					return ValeurStick(l);
				case NkNatureLiaison::NK_ENTREE:
					break;
			}

			const uint32 c = l.code.code;
			switch (l.code.device) {
				case NkInputDevice::NK_KEYBOARD:
					return (c < kNbTouches && mTouches[c]) ? l.echelle : 0.f;
				case NkInputDevice::NK_MOUSE:
					return (c < kNbSouris && mSouris[c]) ? l.echelle : 0.f;
				case NkInputDevice::NK_MOUSEWHEEL:
					return (c == 1u ? mMoletteH : mMolette) * l.echelle;
				case NkInputDevice::NK_GAMEPAD: {
					const int32 pad = ManetteDe(joueur);
					if (manettes == nullptr || pad < 0) {
						return 0.f;
					}
					return manettes->IsButtonDown(static_cast<uint32>(pad), static_cast<NkGamepadButton>(c)) ? l.echelle
																										 : 0.f;
				}
				case NkInputDevice::NK_GAMEPAD_AXIS: {
					const int32 pad = ManetteDe(joueur);
					if (manettes == nullptr || pad < 0) {
						return 0.f;
					}
					const NkGamepadAxis axe = static_cast<NkGamepadAxis>(c);
					bool estStick = false;
					const NkGamepadAxis jumeau = AxeJumeau(axe, estStick);
					const float32 v = manettes->GetAxis(static_cast<uint32>(pad), axe);
					if (!estStick) {
						return ZoneMorteAxiale(v, l.zoneMorte) * l.echelle;
					}
					// ── Zone morte RADIALE : sur la longueur du vecteur du stick ──
					const float32 w = manettes->GetAxis(static_cast<uint32>(pad), jumeau);
					const float32 m = math::NkSqrt(v * v + w * w);
					if (m <= l.zoneMorte || m <= 0.f) {
						return 0.f;
					}
					const float32 mb = m > 1.f ? 1.f : m;
					const float32 k = (mb - l.zoneMorte) / (1.f - l.zoneMorte) / m;
					return v * k * l.echelle;
				}
			}
			return 0.f;
		}

		// =====================================================================
		// La trame
		// =====================================================================

		void NkEntreesJeu::Trame(const NkGamepadSystem *manettes) noexcept {
			CapturerManettes(manettes);
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				NkActions &actions = mActions[j];
				actions.NouvelleTrame();
				float32 somme[NK_UNKENY_ACTIONS_MAX] = {};
				bool liee[NK_UNKENY_ACTIONS_MAX] = {};
				for (int32 i = 0; i < mLiaisons.Nombre(); ++i) {
					const NkLiaison &l = mLiaisons[i];
					if (l.joueur != j && l.joueur != NK_UNKENY_TOUS_LES_JOUEURS) {
						continue;
					}
					somme[l.action] += Valeur(l, j, manettes);
					liee[l.action] = true;
				}
				// ⚠️ SEULES LES ACTIONS LIEES sont reecrites. Une action qu'aucune
				//    liaison ne vise reste celle que le programme a posee : un
				//    banc, un script ou une IA la pilotent sans qu'une trame la
				//    remette a zero derriere eux.
				for (int32 a = 0; a < NK_UNKENY_ACTIONS_MAX; ++a) {
					if (liee[a]) {
						actions.Poser(a, somme[a]);
					}
				}
			}
			// La molette est un DELTA : consommee une fois, par cette trame.
			mMolette = 0.f;
			mMoletteH = 0.f;
		}

		void NkEntreesJeu::ToutRelacher() noexcept {
			for (uint32 k = 0; k < kNbTouches; ++k) {
				mTouches[k] = false;
			}
			for (uint32 b = 0; b < kNbSouris; ++b) {
				mSouris[b] = false;
			}
			for (uint32 d = 0; d < NK_MAX_TOUCH_POINTS; ++d) {
				mDoigts[d].actif = false;
				mDoigts[d].stick = -1;
			}
			mMolette = 0.f;
			mMoletteH = 0.f;
			mCapture = -1;
			for (int32 j = 0; j < NK_UNKENY_JOUEURS_MAX; ++j) {
				mActions[j].ToutRelacher();
			}
		}

	} // namespace unkeny
} // namespace nkentseu
