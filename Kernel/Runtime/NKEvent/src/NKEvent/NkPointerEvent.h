// =============================================================================
// NkPointerEvent.h — LECTURE UNIFIEE souris + doigt
//
// A QUOI SERT CE FICHIER
//   Une application 2D ne veut presque jamais savoir si l'utilisateur a une
//   souris ou un doigt : elle veut un GESTE — ca appuie ici, ca glisse la, ca
//   relache. Ce fichier fournit cette lecture, et rien d'autre.
//
// CE N'EST PAS UN REMPLACANT DE NKEVENT
//   NkMouseButtonPressEvent, NkTouchBeginEvent et les autres continuent
//   d'exister, d'etre emis et d'etre recevables exactement comme avant. Ici on
//   AJOUTE une lecture derivee ; on ne retire ni ne detourne rien. Une
//   application qui veut le brut le garde.
//
// POURQUOI IL EXISTE — mesure du 2026-09-01, pas une impression
//   NEUF applications du depot traduisaient elles-memes souris+doigt vers un
//   geste unique : Gemcrush, Mou, Nkoung, NK3DModeler, NKPA, RihenDefi,
//   NkVideoPlayer, NkAudioPlayer, Songoo. Neuf copies d'une fonction de
//   quarante lignes, qui divergent des qu'une seule est corrigee.
//   Le Kernel n'en portait rien : `grep NkPointer Kernel/` ne rendait que les
//   pointeurs intelligents de NKMemory.
//
// POURQUOI IL VIT DANS NKEVENT ET PAS DANS UNE COQUILLE D'APPLICATION
//   Parce qu'une application qui garde la maniere classique (sa propre boucle,
//   son propre nkmain) doit en beneficier aussi. Une brique posee dans la
//   coquille n'aurait servi que ceux qui adoptent la coquille — c'est-a-dire
//   pas les neuf.
//
// ⚠️ AUCUN .cpp, ET C'EST DELIBERE
//   Un `.cpp` dans un module cree une dependance dure pour TOUS ses dependants ;
//   un en-tete seul se compile chez le consommateur. C'est exactement ce qui
//   a fait que NKCanvas liait NKUI (module deprecie) sans que personne le
//   demande : `NkUICanvasBackend.cpp` existait. Ne pas en ajouter un ici.
//
// OU AJOUTER LA PROCHAINE CHOSE
//   - une phase de plus (survol, annulation systeme) : NkPointerPhase, puis le
//     `if` correspondant dans NkReadPointer ;
//   - un second doigt (pincement, rotation) : NE PAS l'ajouter ici. NKEvent
//     porte deja NkGesturePinchEvent / NkGestureRotateEvent, qui sont faits
//     pour ca. Ce fichier traite UN pointeur, c'est sa definition.
// =============================================================================
#pragma once

#include "NKCore/NkTypes.h"
#include "NKEvent/NkEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"

namespace nkentseu {

	/// Etat d'un pointeur au moment ou l'evenement a ete emis.
	enum class NkPointerPhase : uint8 {
		NK_POINTER_NONE = 0, ///< L'evenement lu ne concerne pas le pointeur
		NK_POINTER_DOWN,	 ///< Appui : clic gauche enfonce, ou doigt pose
		NK_POINTER_MOVE,	 ///< Deplacement (bouton enfonce ou non, cf. `pressed`)
		NK_POINTER_UP,		 ///< Relache : clic gauche relache, ou doigt leve
		NK_POINTER_CANCEL	 ///< Le systeme a repris la main (appel entrant, geste OS)
	};

	/// Un geste, quelle que soit sa provenance.
	///
	/// ⚠️ `x` et `y` sont en PIXELS CLIENT — le repere de la zone de dessin,
	/// origine en haut a gauche, hors bordure de fenetre. C'est le meme repere
	/// que celui d'une NkRenderTarget. Ne pas y melanger des coordonnees ecran
	/// (`GetScreenX`) : ce depot a deja paye un pick mort une journee entiere
	/// pour avoir compare des pixels de fenetre a des pixels de viseur.
	struct NkPointer {
			float32 x = 0.f;
			float32 y = 0.f;
			NkPointerPhase phase = NkPointerPhase::NK_POINTER_NONE;
			uint32 id = 0;			///< Identifiant du contact (0 = souris, ou premier doigt)
			bool fromTouch = false; ///< true = doigt. Utile pour dimensionner les cibles.

			bool IsValid() const noexcept {
				return phase != NkPointerPhase::NK_POINTER_NONE;
			}
	};

	/// Traduit UN evenement en geste. Rend false si l'evenement n'en est pas un.
	///
	/// @param event    l'evenement tel que la file le rend
	/// @param outPtr   rempli uniquement quand la fonction rend true
	/// @return         true si `event` portait un geste
	///
	/// ⚠️ SEUL LE BOUTON GAUCHE compte comme geste de pointeur. Le droit ouvre
	/// un menu contextuel, le milieu fait defiler : les confondre ferait
	/// selectionner en voulant ouvrir un menu. Une application qui veut le
	/// bouton droit lit NkMouseButtonPressEvent directement — il est toujours la.
	inline bool NkReadPointer(const NkEvent &event, NkPointer &outPtr) noexcept {

		// --- Souris -------------------------------------------------------
		if (const auto *press = event.As<NkMouseButtonPressEvent>()) {
			if (press->GetButton() != NkMouseButton::NK_MB_LEFT) {
				return false;
			}
			outPtr.x = static_cast<float32>(press->GetX());
			outPtr.y = static_cast<float32>(press->GetY());
			outPtr.phase = NkPointerPhase::NK_POINTER_DOWN;
			outPtr.id = 0;
			outPtr.fromTouch = false;
			return true;
		}
		if (const auto *release = event.As<NkMouseButtonReleaseEvent>()) {
			if (release->GetButton() != NkMouseButton::NK_MB_LEFT) {
				return false;
			}
			outPtr.x = static_cast<float32>(release->GetX());
			outPtr.y = static_cast<float32>(release->GetY());
			outPtr.phase = NkPointerPhase::NK_POINTER_UP;
			outPtr.id = 0;
			outPtr.fromTouch = false;
			return true;
		}
		if (const auto *move = event.As<NkMouseMoveEvent>()) {
			outPtr.x = static_cast<float32>(move->GetX());
			outPtr.y = static_cast<float32>(move->GetY());
			outPtr.phase = NkPointerPhase::NK_POINTER_MOVE;
			outPtr.id = 0;
			outPtr.fromTouch = false;
			return true;
		}

		// --- Doigt --------------------------------------------------------
		// ⚠️ GetNumTouches() == 0 EST POSSIBLE et n'est pas une anomalie : un
		// evenement tactile peut arriver sans contact (fin de sequence sur
		// certaines plateformes). Lire GetTouch(0) sans ce test lit hors du
		// tableau. Le test etait present dans les neuf copies — le perdre en
		// remontant le code serait la seule vraie regression possible ici.
		if (const auto *begin = event.As<NkTouchBeginEvent>()) {
			if (begin->GetNumTouches() == 0) {
				return false;
			}
			outPtr.x = begin->GetTouch(0).clientX;
			outPtr.y = begin->GetTouch(0).clientY;
			outPtr.phase = NkPointerPhase::NK_POINTER_DOWN;
			outPtr.id = 1;
			outPtr.fromTouch = true;
			return true;
		}
		if (const auto *move = event.As<NkTouchMoveEvent>()) {
			if (move->GetNumTouches() == 0) {
				return false;
			}
			outPtr.x = move->GetTouch(0).clientX;
			outPtr.y = move->GetTouch(0).clientY;
			outPtr.phase = NkPointerPhase::NK_POINTER_MOVE;
			outPtr.id = 1;
			outPtr.fromTouch = true;
			return true;
		}
		if (const auto *end = event.As<NkTouchEndEvent>()) {
			if (end->GetNumTouches() == 0) {
				return false;
			}
			outPtr.x = end->GetTouch(0).clientX;
			outPtr.y = end->GetTouch(0).clientY;
			outPtr.phase = NkPointerPhase::NK_POINTER_UP;
			outPtr.id = 1;
			outPtr.fromTouch = true;
			return true;
		}

		// L'annulation existe sur mobile et NULLE PART ailleurs : un appel
		// entrant, un geste systeme. Une application qui la confond avec un
		// relache valide un appui que l'utilisateur n'a jamais confirme.
		if (const auto *cancel = event.As<NkTouchCancelEvent>()) {
			outPtr.phase = NkPointerPhase::NK_POINTER_CANCEL;
			outPtr.id = 1;
			outPtr.fromTouch = true;
			if (cancel->GetNumTouches() > 0) {
				outPtr.x = cancel->GetTouch(0).clientX;
				outPtr.y = cancel->GetTouch(0).clientY;
			}
			return true;
		}

		return false;
	}

	/// Taille minimale conseillee d'une cible tactile, en pixels.
	///
	/// 9 mm environ, la recommandation commune d'Apple et de Google ramenee au
	/// meme nombre. `density` est le facteur d'echelle de l'ecran
	/// (NkWindow::GetDpiScale). Au doigt on vise large ; a la souris, le
	/// pointeur est precis et une cible enorme gaspille de la place.
	inline float32 NkPointerMinTargetPx(float32 density, bool forTouch) noexcept {
		const float32 base = forTouch ? 44.f : 24.f;
		return base * (density > 0.f ? density : 1.f);
	}

} // namespace nkentseu
