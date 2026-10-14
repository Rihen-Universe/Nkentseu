// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
#pragma once
// -----------------------------------------------------------------------------
// @File    Kernel/Runtime/NKEvent/src/NKEvent/NkToucheDisposition.h
// @Brief   LA TOUCHE QUE LA DISPOSITION NOMME (keycode), a cote de sa POSITION
//          (NkKey, NkScancode) ; et AltGr separe de Ctrl+Alt.
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// POURQUOI (14/10) : un utilisateur de NKCode sur AZERTY, rapporte par Rodolf :
// « Ctrl+Q selectionne tout quand Ctrl+A ne fait rien ; de plus il n'arrive pas
// a ecrire le symbole @ ». Deux causes :
//   1) NkKey suit la POSITION (les rangees US-QWERTY, voir NkScancodeToKey) : la
//      touche marquee Q sur AZERTY est a la place du A americain. C'est JUSTE
//      pour un jeu (ZQSD tombe sur WASD, les doigts ne bougent pas) et FAUX pour
//      un raccourci, qui se lit sur la lettre.
//   2) Windows envoie AltGr comme Ctrl gauche (simule) + Alt droit : AltGr+0
//      (« @ » en AZERTY) arrivait comme Ctrl+Alt+0. L'editeur de code jetait la
//      frappe parce que Ctrl etait tenu, et Ctrl+0 remettait le zoom a zero.
// Rodolf (14/10) : « on doit gerer les scancodes et les keycodes ». L'evenement
// garde les deux : GetKey() et GetScancode() pour la position (les jeux, le vol
// d'une camera), NkToucheDisposition() pour la lettre (les raccourcis).
//
// MUTATIONS DE BANC, NK_CLAVIER_MUTATION :
//   position    la disposition n'est pas lue : la position sert aux raccourcis
//               (l'etat d'avant le 14/10) ;
//   altgr-ctrl  AltGr reste Ctrl+Alt (l'etat d'avant le 14/10).
// Banc : Applications/NKCode/tools/banc_clavier.sh.
// -----------------------------------------------------------------------------

#include "NKPlatform/NkPlatformDetect.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkKeycodeMap.h"
#include <cstdlib>
#include <cstring>

namespace nkentseu {

	/// La mutation de banc demandee (voir l'en-tete). Lue une fois.
	inline bool NkClavierMutation(const char *nom) noexcept {
		static const char *const v = getenv("NK_CLAVIER_MUTATION");
		return v && nom && std::strcmp(v, nom) == 0;
	}

	/// La touche que la DISPOSITION nomme. Sous Windows, le code virtuel porte par
	/// l'evenement (GetNativeKey, le VK de WM_KEYDOWN) suit la disposition pour les
	/// lettres et les chiffres : la touche marquee A sur AZERTY donne VK 'A', quelle
	/// que soit sa place. A defaut de code natif (un evenement fabrique sans lui) ou
	/// sur une autre plateforme (son code natif n'a pas ce sens), la position.
	/// ⚠️ NON FAIT : la disposition sous X11, Wayland et Cocoa (elles rendent la
	///    position, comme avant le 14/10). A faire quand une de ces plateformes aura
	///    un utilisateur : le keysym (X11) ou charactersIgnoringModifiers (Cocoa).
	inline NkKey NkToucheDisposition(const NkKeyboardEvent &e) noexcept {
#if defined(NKENTSEU_PLATFORM_WINDOWS)
		if (e.GetNativeKey() != 0 && !NkClavierMutation("position")) {
			const NkKey k = NkKeycodeMap::NkKeyFromWin32VK(e.GetNativeKey(), e.IsExtended());
			if (k != NkKey::NK_UNKNOWN)
				return k;
		}
#endif
		return e.GetKey();
	}

	/// Ecrit ctrl, alt et altGr de `m` d'apres les quatre touches TENUES. Windows tient
	/// AltGr comme Ctrl gauche + Alt droit : les deux ensemble sont AltGr, et ni Ctrl ni
	/// Alt (un Ctrl droit ou un Alt gauche tenus EN PLUS restent, eux, Ctrl et Alt).
	/// ⚠️ LIMITE CONNUE : sur une disposition sans AltGr (US), Ctrl gauche + Alt droit
	///    tenus a la main se lisent aussi AltGr. Windows lui-meme les confond ; les
	///    raccourcis Ctrl+Alt de la famille passent par Alt GAUCHE.
	inline void NkSepareAltGr(NkModifierState &m, bool ctrlGauche, bool ctrlDroit, bool altGauche,
							  bool altDroit) noexcept {
		if (ctrlGauche && altDroit && !NkClavierMutation("altgr-ctrl")) {
			m.altGr = true;
			m.ctrl = ctrlDroit;
			m.alt = altGauche;
			return;
		}
		m.altGr = false;
		m.ctrl = ctrlGauche || ctrlDroit;
		m.alt = altGauche || altDroit;
	}

} // namespace nkentseu
