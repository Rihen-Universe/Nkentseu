#pragma once
// =============================================================================
// NkLogHeartbeat.h — Le BATTEMENT : faire parler un journal pendant que
//                    l'application vit, sans le rendre illisible.
//
// LE PROBLÈME QU'IL RÉSOUT, et il a été diagnostiqué de travers deux fois avant
// d'être compris (2026-08-15) :
//
//   Un journal Nkentseu écrit une salve au démarrage — mesuré à 47 lignes sur
//   Galaxy S22+ — puis PLUS RIEN. Zéro ligne sur quinze secondes de régime
//   établi : rien ne journalise par image, et c'est une bonne propriété.
//   Mais pour qui enquête pendant que l'application tourne, le fichier paraît
//   FIGÉ. À la fermeture, l'extinction ajoute ses propres lignes — et un fichier
//   figé qui se remet à grandir au moment où l'on ferme RESSEMBLE EXACTEMENT à
//   un vidage de tampon.
//
//   D'où le faux diagnostic : « le puits est tamponné, il faut vider plus
//   souvent ». Mesuré, c'est faux — `NkFileSink` appelle `setvbuf(_IONBF)`, et
//   tuer le processus de force ne perd ni ne révèle aucune ligne. Il n'y a rien
//   en attente. **Vider plus souvent une file vide ne produit aucune ligne.**
//
//   Le manque n'était donc pas une politique de vidage : c'est qu'il n'y avait
//   RIEN À DIRE pendant la vie de l'application.
//
// CE QUE CE FICHIER FAIT, ET SURTOUT CE QU'IL NE FAIT PAS :
//   - il ne journalise RIEN lui-même. Seule l'application sait ce qui vaut la
//     peine d'être dit — images par seconde, état d'une machine, compteurs. Ce
//     type ne décide que du QUAND ;
//   - il n'ouvre AUCUN fil et n'installe aucune minuterie. Le battement est
//     interrogé depuis la boucle qui tourne déjà. Un fil de plus pour écrire une
//     ligne toutes les deux secondes serait payé en complexité et en réveils ;
//   - il est ÉTEINT PAR DÉFAUT (`intervalMs = 0`). Les 47-lignes-puis-zéro sont
//     une propriété précieuse, et un battement toujours actif la détruirait :
//     un journal qui parle sans arrêt est aussi illisible qu'un journal muet.
//     Éteint, `ShouldBeat()` coûte une comparaison d'entier.
//
// USAGE :
//     NkHeartbeat beat;
//     beat.SetInterval(500);            // 0 = éteint ; ici une ligne / 500 ms
//     while (running) {
//         ...
//         if (beat.ShouldBeat())
//             logger.Infof("[Viewer] %.1f img/s, %ux%u", fps, w, h);
//     }
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#ifndef __NKENTSEU_NKLOGHEARTBEAT_H__
#define __NKENTSEU_NKLOGHEARTBEAT_H__

#include "NKCore/NkTypes.h"
#include "NKTime/NkChrono.h"

namespace nkentseu {

	class NkHeartbeat {
		public:
			NkHeartbeat() = default;
			explicit NkHeartbeat(uint32 intervalMs) : mIntervalMs(intervalMs) {}

			/// Intervalle minimal entre deux battements, en millisecondes.
			/// **0 éteint le battement**, et c'est la valeur par défaut.
			void SetInterval(uint32 intervalMs) {
				mIntervalMs = intervalMs;
				mPrimed = false;
			}

			uint32 GetInterval() const {
				return mIntervalMs;
			}
			bool IsEnabled() const {
				return mIntervalMs > 0;
			}

			/// Vrai AU PLUS une fois par intervalle ; faux immédiatement si
			/// éteint. À appeler depuis la boucle : c'est l'appel qui fait
			/// avancer le temps, pas une minuterie.
			///
			/// Le premier appel après `SetInterval` bat TOUT DE SUITE : quand on
			/// allume un instrument, on veut une ligne maintenant — pas dans
			/// deux secondes, à se demander s'il fonctionne.
			bool ShouldBeat() {
				if (mIntervalMs == 0)
					return false;

				const nkentseu::NkElapsedTime now = NkChrono::Now();
				if (!mPrimed) {
					mPrimed = true;
					mLast = now;
					mDernierIntervalMs = 0.0; // aucun intervalle precedent a rapporter
					return true;
				}

				// Soustraction de deux instants d'une horloge MONOTONE : un
				// changement d'heure système ne peut donc pas faire taire le
				// battement pendant des heures, ni le déclencher en rafale.
				const int64 ecoule = (now - mLast).ToMilliseconds();
				if (ecoule >= (int64)mIntervalMs) {
					mLast = now;
					mDernierIntervalMs = (float64)ecoule;
					return true;
				}
				return false;
			}

			/// Temps RÉELLEMENT écoulé depuis le battement précédent, en ms.
			///
			/// ⚠️ À utiliser à la place de `GetInterval()` pour tout calcul de
			/// débit, et ce n'est pas un détail de confort : un battement ne peut
			/// se déclencher qu'AU MOMENT OÙ ON L'INTERROGE, donc à une frontière
			/// d'image. L'intervalle réel est le premier multiple de la période
			/// d'image qui dépasse la demande — jamais la demande elle-même.
			///
			/// Coût de l'ignorer, mesuré le 2026-08-15 sur le viewer caméra :
			/// à 500 ms de demande, 42 img/s ; à 50 ms, **56 img/s** pour la même
			/// application dans les mêmes conditions. 32 % d'écart, entièrement
			/// imputable à la division par la période DEMANDÉE. J'ai lu cet écart
			/// comme une instabilité du programme et rapporté « 35 à 55 img/s » :
			/// c'était mon instrument qui variait, pas le programme.
			float64 GetLastIntervalMs() const {
				return mDernierIntervalMs;
			}

			/// Repousser le prochain battement sans en émettre un. Utile quand
			/// l'application vient de journaliser autre chose : on ne veut pas
			/// deux lignes collées.
			void Defer() {
				mLast = NkChrono::Now();
				mPrimed = true;
			}

		private:
			uint32 mIntervalMs = 0; ///< 0 = ÉTEINT (défaut assumé, voir en-tête).
			bool mPrimed = false;
			float64 mDernierIntervalMs = 0.0; ///< Ecart REEL entre les deux derniers battements.
			nkentseu::NkElapsedTime mLast{};
	};

} // namespace nkentseu

#endif // __NKENTSEU_NKLOGHEARTBEAT_H__
