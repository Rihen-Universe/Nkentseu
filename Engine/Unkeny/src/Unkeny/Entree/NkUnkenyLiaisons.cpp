//
// NkUnkenyLiaisons.cpp
// =============================================================================
// Description :
//   La table de liaisons : ajout, reconfiguration, et aller-retour texte.
//   Le decoupage en mots et la lecture des nombres sont ceux de
//   NkLoadBindings (NKEvent) : Unkeny ne recopie pas un second analyseur.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Entree/NkUnkenyLiaisons.h"

#include <cstdio>

namespace nkentseu {
	namespace unkeny {

		namespace {

			bool JoueurValide(int32 joueur) noexcept {
				return joueur == NK_UNKENY_TOUS_LES_JOUEURS || (joueur >= 0 && joueur < NK_UNKENY_JOUEURS_MAX);
			}

			bool ActionValide(int32 action) noexcept {
				return action >= 0 && action < NK_UNKENY_ACTIONS_MAX;
			}

			/// Ecrit un nombre sous une forme que detail::NkLireNombre relit :
			/// JAMAIS de notation exponentielle (« 1e-05 » serait refuse), d'ou
			/// %.4f et pas %g.
			NkString TexteNombre(float32 v) {
				char b[32];
				std::snprintf(b, sizeof(b), "%.4f", static_cast<double>(v));
				return NkString(b);
			}

			bool Prefixe(const NkString &mot, const char *prefixe, NkString &reste) {
				const char *m = mot.Data();
				if (m == nullptr) {
					return false;
				}
				usize k = 0;
				while (prefixe[k] != '\0') {
					if (m[k] != prefixe[k]) {
						return false;
					}
					++k;
				}
				reste = NkString(m + k);
				return true;
			}

		} // namespace

		// =====================================================================
		// Les noms
		// =====================================================================

		bool NkLiaisons::NommerAction(int32 action, const char *nom) noexcept {
			if (!ActionValide(action) || nom == nullptr) {
				return false;
			}
			int32 k = 0;
			while (nom[k] != '\0' && k < NK_UNKENY_NOM_ACTION_MAX - 1) {
				// Un nom est UN mot du fichier : une espace le couperait en deux.
				mNoms[action][k] = (nom[k] == ' ' || nom[k] == '\t') ? '_' : nom[k];
				++k;
			}
			mNoms[action][k] = '\0';
			++mVersion;
			return true;
		}

		const char *NkLiaisons::NomAction(int32 action) const noexcept {
			if (!ActionValide(action)) {
				return "";
			}
			return mNoms[action];
		}

		int32 NkLiaisons::ActionParNom(const char *nom) const noexcept {
			if (nom == nullptr || nom[0] == '\0') {
				return -1;
			}
			// « #3 » : l'action d'indice 3, nommee ou non. C'est la forme
			// qu'Ecrire emploie pour une action que le jeu n'a pas nommee.
			if (nom[0] == '#') {
				int32 v = 0;
				int32 k = 1;
				if (nom[k] == '\0') {
					return -1;
				}
				while (nom[k] >= '0' && nom[k] <= '9') {
					v = v * 10 + (nom[k] - '0');
					++k;
				}
				return (nom[k] == '\0' && ActionValide(v)) ? v : -1;
			}
			for (int32 a = 0; a < NK_UNKENY_ACTIONS_MAX; ++a) {
				if (mNoms[a][0] != '\0' && NkInputNameEquals(mNoms[a], nom)) {
					return a;
				}
			}
			return -1;
		}

		// =====================================================================
		// Lier
		// =====================================================================

		int32 NkLiaisons::Lier(const NkLiaison &liaison) noexcept {
			if (mNombre >= NK_UNKENY_LIAISONS_MAX || !ActionValide(liaison.action) || !JoueurValide(liaison.joueur)) {
				return -1;
			}
			mLiaisons[mNombre] = liaison;
			if (mLiaisons[mNombre].zoneMorte < 0.f) {
				mLiaisons[mNombre].zoneMorte = 0.f;
			}
			// Une zone morte de 1 ne laisserait plus rien passer, et la remise a
			// l'echelle diviserait par zero.
			if (mLiaisons[mNombre].zoneMorte > 0.95f) {
				mLiaisons[mNombre].zoneMorte = 0.95f;
			}
			++mNombre;
			++mVersion;
			return mNombre - 1;
		}

		int32 NkLiaisons::LierTouche(int32 action, NkKey touche, float32 echelle, int32 joueur) noexcept {
			NkLiaison l;
			l.action = action;
			l.joueur = joueur;
			l.code = NkInputCode::Key(touche);
			l.echelle = echelle;
			return Lier(l);
		}

		int32 NkLiaisons::LierSouris(int32 action, NkMouseButton bouton, float32 echelle, int32 joueur) noexcept {
			NkLiaison l;
			l.action = action;
			l.joueur = joueur;
			l.code = NkInputCode::Mouse(bouton);
			l.echelle = echelle;
			return Lier(l);
		}

		int32 NkLiaisons::LierBouton(int32 action, NkGamepadButton bouton, float32 echelle, int32 joueur) noexcept {
			NkLiaison l;
			l.action = action;
			l.joueur = joueur;
			l.code = NkInputCode::Gamepad(bouton);
			l.echelle = echelle;
			return Lier(l);
		}

		int32 NkLiaisons::LierAxe(int32 action, NkGamepadAxis axe, float32 echelle, float32 zoneMorte,
								  int32 joueur) noexcept {
			NkLiaison l;
			l.action = action;
			l.joueur = joueur;
			l.code = NkInputCode::GamepadAxis(axe);
			l.echelle = echelle;
			l.zoneMorte = zoneMorte;
			return Lier(l);
		}

		int32 NkLiaisons::LierZone(int32 action, const NkZoneEcran &zone, float32 echelle, int32 joueur) noexcept {
			NkLiaison l;
			l.action = action;
			l.joueur = joueur;
			l.nature = NkNatureLiaison::NK_ZONE_BOUTON;
			l.zone = zone;
			l.echelle = echelle;
			return Lier(l);
		}

		int32 NkLiaisons::LierStick(int32 actionX, int32 actionY, const NkZoneEcran &zone, float32 rayon,
									float32 zoneMorte, int32 joueur) noexcept {
			if (mNombre + 2 > NK_UNKENY_LIAISONS_MAX || !ActionValide(actionX) || !ActionValide(actionY)) {
				return -1;
			}
			NkLiaison x;
			x.action = actionX;
			x.joueur = joueur;
			x.nature = NkNatureLiaison::NK_ZONE_STICK_X;
			x.zone = zone;
			x.rayon = rayon > 0.f ? rayon : 0.08f;
			x.zoneMorte = zoneMorte;
			NkLiaison y = x;
			y.action = actionY;
			y.nature = NkNatureLiaison::NK_ZONE_STICK_Y;
			const int32 i = Lier(x);
			if (i < 0) {
				return -1;
			}
			if (Lier(y) < 0) {
				Retirer(i);
				return -1;
			}
			return i;
		}

		bool NkLiaisons::Relier(int32 indice, const NkInputCode &entree) noexcept {
			if (indice < 0 || indice >= mNombre || mLiaisons[indice].nature != NkNatureLiaison::NK_ENTREE) {
				return false;
			}
			mLiaisons[indice].code = entree;
			++mVersion;
			return true;
		}

		bool NkLiaisons::Retirer(int32 indice) noexcept {
			if (indice < 0 || indice >= mNombre) {
				return false;
			}
			// L'ORDRE est garde (pas d'echange avec la derniere) : c'est lui que
			// le texte ecrit, et un menu de touches qui se reordonne a chaque
			// retrait perd l'utilisateur.
			for (int32 i = indice; i + 1 < mNombre; ++i) {
				mLiaisons[i] = mLiaisons[i + 1];
			}
			--mNombre;
			++mVersion;
			return true;
		}

		void NkLiaisons::RetirerAction(int32 action, int32 joueur) noexcept {
			int32 i = 0;
			while (i < mNombre) {
				const NkLiaison &l = mLiaisons[i];
				const bool memeJoueur = (joueur == NK_UNKENY_TOUS_LES_JOUEURS) || l.joueur == joueur;
				if (l.action == action && memeJoueur) {
					Retirer(i);
				} else {
					++i;
				}
			}
		}

		void NkLiaisons::Vider() noexcept {
			mNombre = 0;
			++mVersion;
		}

		void NkLiaisons::RemplirCarte(NkInputMap &carte, int32 contexte, int32 joueur, NkVector<int32> *origine) const {
			NkInputContext *ctx = carte.Context(contexte);
			if (ctx == nullptr) {
				return;
			}
			// Les 32 actions d'Unkeny, dans l'ordre de leurs indices : l'action a
			// d'Unkeny est l'action a de la carte.
			while (carte.ActionCount() < NK_UNKENY_ACTIONS_MAX) {
				char nom[8];
				std::snprintf(nom, sizeof(nom), "#%d", carte.ActionCount());
				carte.DeclareAction(nom, NkInputValueType::NK_INPUT_AXIS1D);
			}
			ctx->bindings.Clear();
			if (origine != nullptr) {
				origine->Clear();
			}
			for (int32 i = 0; i < mNombre; ++i) {
				const NkLiaison &l = mLiaisons[i];
				if (l.joueur != joueur && l.joueur != NK_UNKENY_TOUS_LES_JOUEURS) {
					continue;
				}
				NkInputBinding b;
				switch (l.nature) {
					case NkNatureLiaison::NK_ENTREE: {
						b = NkInputBinding::Code(l.action, l.code).WithScale(l.echelle);
						// La zone morte d'Unkeny ne vaut que pour un axe : radiale sur un
						// stick (son jumeau mesure la longueur), axiale ailleurs.
						if (l.code.device == NkInputDevice::NK_GAMEPAD_AXIS && l.zoneMorte > 0.f) {
							const NkGamepadAxis a = static_cast<NkGamepadAxis>(l.code.code);
							const bool stick = a == NkGamepadAxis::NK_GP_AXIS_LX || a == NkGamepadAxis::NK_GP_AXIS_LY ||
											   a == NkGamepadAxis::NK_GP_AXIS_RX || a == NkGamepadAxis::NK_GP_AXIS_RY;
							b = b.WithDeadZone(stick ? NkInputDeadZone::NK_INPUT_DEADZONE_RADIAL
													 : NkInputDeadZone::NK_INPUT_DEADZONE_AXIAL,
											   l.zoneMorte);
						}
						break;
					}
					case NkNatureLiaison::NK_ZONE_BOUTON:
						b = NkInputBinding::ScreenButton(l.action, l.zone.x, l.zone.y, l.zone.l, l.zone.h)
								.WithScale(l.echelle);
						break;
					case NkNatureLiaison::NK_ZONE_STICK_X:
					case NkNatureLiaison::NK_ZONE_STICK_Y: {
						b = NkInputBinding::ScreenStick(l.action, l.zone.x, l.zone.y, l.zone.l, l.zone.h, l.rayon);
						if (l.zoneMorte > 0.f) {
							b = b.WithDeadZone(NkInputDeadZone::NK_INPUT_DEADZONE_RADIAL, l.zoneMorte);
						}
						if (l.nature == NkNatureLiaison::NK_ZONE_STICK_Y) {
							// L'echelle PUIS la permutation : c'est le Y mis a l'echelle
							// qui devient la valeur de l'axe 1D.
							b = b.WithScale(1.f, l.echelle).SwappedXY();
						} else {
							b = b.WithScale(l.echelle, 1.f);
						}
						break;
					}
				}
				ctx->bindings.PushBack(b);
				if (origine != nullptr) {
					origine->PushBack(i);
				}
			}
			carte.BindingsChanged();
		}


		int32 NkLiaisons::Trouver(int32 action, const NkInputCode &entree) const noexcept {
			for (int32 i = 0; i < mNombre; ++i) {
				const NkLiaison &l = mLiaisons[i];
				if (l.nature == NkNatureLiaison::NK_ENTREE && l.action == action && l.code == entree) {
					return i;
				}
			}
			return -1;
		}

		bool NkLiaisons::Concerne(const NkInputCode &entree) const noexcept {
			for (int32 i = 0; i < mNombre; ++i) {
				const NkLiaison &l = mLiaisons[i];
				if (l.nature == NkNatureLiaison::NK_ENTREE && l.code.device == entree.device &&
					l.code.code == entree.code) {
					return true;
				}
			}
			return false;
		}

		// =====================================================================
		// Le texte
		// =====================================================================

		NkString NkLiaisons::Ecrire() const {
			NkString s = "# Liaisons Unkeny, ecrites par NkLiaisons::Ecrire.\n"
						 "# lier  <action> <entree> <echelle> zm=<zone morte> j=<joueur|*>\n"
						 "# zone  <action> <x> <y> <l> <h> <echelle> j=<joueur|*>\n"
						 "# stickx|sticky <action> <x> <y> <l> <h> <echelle> r=<rayon> zm=<zone morte> j=<joueur|*>\n";
			for (int32 i = 0; i < mNombre; ++i) {
				const NkLiaison &l = mLiaisons[i];
				NkString nom = mNoms[l.action][0] != '\0' ? NkString(mNoms[l.action]) : NkString::Fmt("#{0}", l.action);
				const NkString joueur =
					l.joueur == NK_UNKENY_TOUS_LES_JOUEURS ? NkString("j=*") : NkString::Fmt("j={0}", l.joueur);
				const NkString zone = TexteNombre(l.zone.x) + " " + TexteNombre(l.zone.y) + " " +
									  TexteNombre(l.zone.l) + " " + TexteNombre(l.zone.h);
				const NkString echelle = TexteNombre(l.echelle);
				const NkString zoneMorte = TexteNombre(l.zoneMorte);
				switch (l.nature) {
					case NkNatureLiaison::NK_ENTREE:
						s += "lier " + nom + " " + l.code.ToString() + " " + echelle + " zm=" + zoneMorte + " " + joueur +
							 "\n";
						break;
					case NkNatureLiaison::NK_ZONE_BOUTON:
						s += "zone " + nom + " " + zone + " " + echelle + " " + joueur + "\n";
						break;
					case NkNatureLiaison::NK_ZONE_STICK_X:
					case NkNatureLiaison::NK_ZONE_STICK_Y:
						s += NkString(l.nature == NkNatureLiaison::NK_ZONE_STICK_X ? "stickx " : "sticky ") + nom + " " +
							 zone + " " + echelle + " r=" + TexteNombre(l.rayon) + " zm=" + zoneMorte + " " + joueur + "\n";
						break;
				}
			}
			return s;
		}

		NkRapportLiaisons NkLiaisons::Lire(const NkString &texte, bool viderAvant) {
			NkRapportLiaisons rapport;
			if (viderAvant) {
				Vider();
			}
			const char *p = texte.Data();
			if (p == nullptr) {
				return rapport;
			}
			uint32 numero = 0;
			while (*p != '\0') {
				const char *debut = p;
				while (*p != '\0' && *p != '\n') {
					++p;
				}
				const NkString ligne(debut, static_cast<nk_size>(p - debut));
				if (*p == '\n') {
					++p;
				}
				++numero;

				NkString mots[12];
				const uint32 n = detail::NkDecouperMots(ligne, mots, 12);
				if (n == 0 || (mots[0].Data() != nullptr && mots[0].Data()[0] == '#')) {
					continue;
				}
				const bool estLier = NkInputNameEquals(mots[0].Data(), "lier");
				const bool estZone = NkInputNameEquals(mots[0].Data(), "zone");
				const bool estStickX = NkInputNameEquals(mots[0].Data(), "stickx");
				const bool estStickY = NkInputNameEquals(mots[0].Data(), "sticky");
				if (!estLier && !estZone && !estStickX && !estStickY) {
					rapport.erreurs.PushBack(NkString::Fmt(
						"ligne {0} : attendu « lier », « zone », « stickx » ou « sticky », lu « {1} »", numero,
						mots[0]));
					continue;
				}
				if (n < 3) {
					rapport.erreurs.PushBack(NkString::Fmt("ligne {0} : il manque l'action ou l'entree", numero));
					continue;
				}
				NkLiaison l;
				l.action = ActionParNom(mots[1].Data());
				if (l.action < 0) {
					rapport.erreurs.PushBack(NkString::Fmt(
						"ligne {0} : action « {1} » inconnue, nommez-la avant de lire", numero, mots[1]));
					continue;
				}
				uint32 suivant = 2;
				if (estLier) {
					bool valide = false;
					l.code = NkInputCode::FromString(mots[2], &valide);
					if (!valide) {
						rapport.erreurs.PushBack(NkString::Fmt("ligne {0} : entree inconnue « {1} »", numero, mots[2]));
						continue;
					}
					suivant = 3;
				} else {
					if (n < 6) {
						rapport.erreurs.PushBack(NkString::Fmt("ligne {0} : une zone demande x y l h", numero));
						continue;
					}
					float32 v[4] = {};
					bool ok = true;
					for (uint32 k = 0; k < 4; ++k) {
						ok = ok && detail::NkLireNombre(mots[2 + k], v[k]);
					}
					if (!ok) {
						rapport.erreurs.PushBack(NkString::Fmt("ligne {0} : zone illisible", numero));
						continue;
					}
					l.zone = NkZoneEcran{v[0], v[1], v[2], v[3]};
					l.nature = estZone ? NkNatureLiaison::NK_ZONE_BOUTON
									   : (estStickX ? NkNatureLiaison::NK_ZONE_STICK_X : NkNatureLiaison::NK_ZONE_STICK_Y);
					suivant = 6;
				}

				// Les options, dans n'importe quel ordre.
				bool optionsOk = true;
				for (uint32 k = suivant; k < n && optionsOk; ++k) {
					NkString reste;
					float32 v = 0.f;
					if (Prefixe(mots[k], "zm=", reste)) {
						optionsOk = detail::NkLireNombre(reste, v);
						l.zoneMorte = v;
					} else if (Prefixe(mots[k], "r=", reste)) {
						optionsOk = detail::NkLireNombre(reste, v) && v > 0.f;
						l.rayon = v;
					} else if (Prefixe(mots[k], "j=", reste)) {
						if (reste == "*") {
							l.joueur = NK_UNKENY_TOUS_LES_JOUEURS;
						} else {
							optionsOk = detail::NkLireNombre(reste, v);
							l.joueur = static_cast<int32>(v);
						}
					} else {
						optionsOk = detail::NkLireNombre(mots[k], v);
						l.echelle = v;
					}
					if (!optionsOk) {
						rapport.erreurs.PushBack(
							NkString::Fmt("ligne {0} : option illisible « {1} »", numero, mots[k]));
					}
				}
				if (!optionsOk) {
					continue;
				}
				if (Lier(l) < 0) {
					rapport.erreurs.PushBack(NkString::Fmt("ligne {0} : table pleine ou joueur hors plage", numero));
					continue;
				}
				++rapport.appliquees;
			}
			return rapport;
		}

	} // namespace unkeny
} // namespace nkentseu
