//
// NkUnkenyActionsStandard.cpp
// =============================================================================
// Les actions standard : noms, indices, liaisons par defaut (voir l'en-tete).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "Unkeny/Entree/NkUnkenyActionsStandard.h"

#include "Unkeny/Entree/NkUnkenyLiaisons.h"

#include <cstring>

namespace nkentseu {
	namespace unkeny {

		namespace {
			// Dans l'ordre des indices : kNoms[NK_ACTION_SAUTER] == "Sauter".
			const char *const kNoms[NK_ACTIONS_STANDARD] = {"Avancer", "Sauter", "Monter", "Action", "Pause"};
		} // namespace

		const char *NkNomActionStandard(int32 action) noexcept {
			return action >= 0 && action < NK_ACTIONS_STANDARD ? kNoms[action] : nullptr;
		}

		int32 NkActionStandardParNom(const char *nom) noexcept {
			if (nom == nullptr) {
				return -1;
			}
			for (int32 i = 0; i < NK_ACTIONS_STANDARD; ++i) {
				if (std::strcmp(kNoms[i], nom) == 0) {
					return i;
				}
			}
			return -1;
		}

		bool NkActionStandardEstUnAxe(int32 action) noexcept {
			return action == NK_ACTION_AVANCER || action == NK_ACTION_MONTER;
		}

		void NkNommerActionsStandard(NkLiaisons &liaisons) noexcept {
			for (int32 i = 0; i < NK_ACTIONS_STANDARD; ++i) {
				liaisons.NommerAction(i, kNoms[i]);
			}
		}

		int32 NkIndiceAction(const NkLiaisons &liaisons, const char *nom) noexcept {
			const int32 i = liaisons.ActionParNom(nom);
			return i >= 0 ? i : NkActionStandardParNom(nom);
		}

		void NkLiaisonsStandard(NkLiaisons &l) noexcept {
			l.Vider();
			NkNommerActionsStandard(l);
			// Clavier : ZQSD en AZERTY et WASD en QWERTY sont les MEMES touches
			// physiques, et NkKey les nomme par leur position QWERTY.
			l.LierTouche(NK_ACTION_AVANCER, NkKey::NK_D, 1.f);
			l.LierTouche(NK_ACTION_AVANCER, NkKey::NK_A, -1.f);
			l.LierTouche(NK_ACTION_AVANCER, NkKey::NK_RIGHT, 1.f);
			l.LierTouche(NK_ACTION_AVANCER, NkKey::NK_LEFT, -1.f);
			l.LierTouche(NK_ACTION_MONTER, NkKey::NK_W, 1.f);
			l.LierTouche(NK_ACTION_MONTER, NkKey::NK_S, -1.f);
			l.LierTouche(NK_ACTION_MONTER, NkKey::NK_UP, 1.f);
			l.LierTouche(NK_ACTION_MONTER, NkKey::NK_DOWN, -1.f);
			l.LierTouche(NK_ACTION_SAUTER, NkKey::NK_SPACE);
			l.LierTouche(NK_ACTION_ACTION, NkKey::NK_E);
			l.LierTouche(NK_ACTION_PAUSE, NkKey::NK_P);
			// Manette : pour TOUS les joueurs, chacun sur la sienne.
			l.LierAxe(NK_ACTION_AVANCER, NkGamepadAxis::NK_GP_AXIS_LX, 1.f, 0.2f);
			l.LierAxe(NK_ACTION_MONTER, NkGamepadAxis::NK_GP_AXIS_LY, 1.f, 0.2f);
			l.LierBouton(NK_ACTION_AVANCER, NkGamepadButton::NK_GP_DPAD_RIGHT, 1.f);
			l.LierBouton(NK_ACTION_AVANCER, NkGamepadButton::NK_GP_DPAD_LEFT, -1.f);
			l.LierBouton(NK_ACTION_SAUTER, NkGamepadButton::NK_GP_SOUTH);
			l.LierBouton(NK_ACTION_ACTION, NkGamepadButton::NK_GP_WEST);
			l.LierBouton(NK_ACTION_PAUSE, NkGamepadButton::NK_GP_START);
			// Doigt : joystick virtuel a gauche, Sauter en bas a droite.
			l.LierStick(NK_ACTION_AVANCER, NK_ACTION_MONTER, NkZoneEcran{0.f, 0.35f, 0.45f, 0.65f}, 0.08f, 0.15f);
			l.LierZone(NK_ACTION_SAUTER, NkZoneEcran{0.7f, 0.55f, 0.3f, 0.45f});
		}

	} // namespace unkeny
} // namespace nkentseu
