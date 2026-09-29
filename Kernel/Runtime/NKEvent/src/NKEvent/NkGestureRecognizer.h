//
// NkGestureRecognizer.h
// =============================================================================
// Description :
//   Reconnaisseur de gestes INDEPENDANT DE LA PLATEFORME. Il lit les contacts
//   bruts (NkTouchBeginEvent / Move / End / Cancel, jusqu'a 32 points) et
//   EMET les gestes deja declares dans NkTouchEvent.h : tape (simple, double,
//   multiple), appui long, glisser (pan), pincer (zoom), rotation, balayage.
//
// Caracteristiques :
//   - Aucune plateforme n'emettait ces gestes (grep du 29/09 : zero emetteur,
//     zero consommateur). Les ecrire UNE fois ici, au-dessus des contacts, les
//     donne a toutes : Android, iOS, Web, HarmonyOS, et tout ce qui viendra.
//   - Le temps est FOURNI par l'appelant (millisecondes), jamais lu ici : c'est
//     ce qui rend le banc deterministe. NkEventSystem passe l'horodatage de
//     l'evenement tactile, et l'horloge de NKTime pour l'appui long.
//   - Les contacts sont suivis PAR IDENTIFIANT : Android ne met dans
//     Begin/End que le doigt qui change, le Web aussi, mais un Move Android
//     porte tous les doigts. Suivre par position dans le tableau aurait
//     confondu les deux conventions.
//   - Aucune allocation : 32 contacts en tableau fixe.
//
// Algorithmes implementes :
//   - tape      : tous les doigts leves sans avoir quitte la tolerance, en
//                 moins de `tapMaxMs` ; `tapCount` croit si la tape suivante
//                 commence moins de `doubleTapMaxMs` apres la precedente, pres
//                 d'elle, avec le meme nombre de doigts ;
//   - appui long: immobile au-dela de `longPressMs` (emis UNE fois, par
//                 OnTouchEvent ou Update) ; la tape du relachement est alors
//                 supprimee ;
//   - pan       : le centroide des doigts, des qu'un doigt sort de la
//                 tolerance. Le premier pan porte tout le trajet depuis
//                 l'appui : la somme des deltas vaut le deplacement total ;
//   - balayage  : au relachement, trajet >= `swipeMinPx`, en moins de
//                 `swipeMaxMs`, a une vitesse moyenne >= `swipeMinSpeed` ;
//                 direction = axe dominant (y vers le BAS, comme l'ecran) ;
//   - pincer    : distance entre les deux premiers doigts, rapport cumule ;
//                 demarre au-dela de `pinchStartRatio` ;
//   - rotation  : angle de la meme paire, en degres, >0 = horaire a l'ecran ;
//                 demarre au-dela de `rotateStartDeg`.
//
// ⚠️ LES EVENEMENTS DE GESTE N'ONT PAS DE PHASE, et ce fichier n'en ajoute
//    pas : ils sont declares ainsi dans NkTouchEvent.h, et la regle est de les
//    reutiliser, pas de les redefinir. Qui veut savoir si un pincement est EN
//    COURS lit IsPinching() / IsPanning() ici.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_EVENT_NKGESTURERECOGNIZER_H__
#define __NKENTSEU_EVENT_NKGESTURERECOGNIZER_H__

#include "NKCore/NkTypes.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKEvent/NkEvent.h"
#include "NKEvent/NkEventApi.h"
#include "NKEvent/NkTouchEvent.h"

namespace nkentseu {

	/// Les seuils. Les valeurs par defaut sont celles des systemes mobiles
	/// (Android ViewConfiguration, UIKit), ramenees en pixels client.
	/// ⚠️ EN PIXELS, PAS EN MILLIMETRES : un ecran dense demande des seuils
	///    plus grands. L'appelant multiplie par la densite (GetDpiScale) s'il
	///    veut la meme sensation partout.
	struct NkGestureConfig {
			float32 slopPx = 10.f;			///< tolerance : en deca, le doigt n'a pas bouge
			float32 tapMaxMs = 250.f;		///< au-dela, ce n'est plus une tape
			float32 doubleTapMaxMs = 300.f; ///< entre le relachement d'une tape et l'appui suivant
			float32 doubleTapSlopPx = 40.f; ///< distance maximale entre deux tapes enchainees
			float32 longPressMs = 500.f;
			float32 swipeMinPx = 50.f;
			float32 swipeMaxMs = 400.f;
			float32 swipeMinSpeed = 500.f;	 ///< pixels par seconde, vitesse MOYENNE du geste
			float32 pinchStartRatio = 0.05f; ///< 5 % d'ecart de distance
			float32 rotateStartDeg = 5.f;
	};

	/// Recoit chaque geste reconnu. L'evenement vit le temps de l'appel.
	using NkGestureSink = NkFunction<void(NkEvent &)>;

	class NKENTSEU_EVENT_CLASS_EXPORT NkGestureRecognizer {
		public:
			NkGestureRecognizer() noexcept;

			void SetConfig(const NkGestureConfig &config) noexcept;
			const NkGestureConfig &GetConfig() const noexcept;

			/// Ou partent les gestes. Sans destinataire, ils sont reconnus et
			/// comptes, mais personne ne les recoit.
			void SetSink(NkGestureSink sink);

			/// Lit un evenement. Tout ce qui n'est pas un contact tactile est
			/// ignore (rend 0).
			/// @param event  l'evenement tel que la file le rend
			/// @param timeMs son instant, en millisecondes (base libre, mais la
			///               MEME que celle passee a Update)
			/// @return le nombre de gestes emis par cet appel
			uint32 OnTouchEvent(const NkEvent &event, float64 timeMs) noexcept;

			/// Fait passer le temps sans contact nouveau : c'est ce qui declenche
			/// l'appui long d'un doigt immobile, qui ne produit AUCUN evenement.
			/// @return le nombre de gestes emis (0 ou 1)
			uint32 Update(float64 timeMs) noexcept;

			/// Oublie tout : contacts, geste en cours, tape precedente.
			void Reset() noexcept;

			uint32 GetActiveContactCount() const noexcept;
			bool IsPanning() const noexcept;
			bool IsPinching() const noexcept;
			bool IsRotating() const noexcept;
			/// Total des gestes emis depuis la construction (diagnostic).
			uint64 GetEmittedCount() const noexcept;

		private:
			struct NkContact {
					uint64 id = 0;
					bool active = false;
					float32 x0 = 0.f;
					float32 y0 = 0.f;
					float32 x = 0.f;
					float32 y = 0.f;
			};

			int32 FindContact(uint64 id) const noexcept;
			int32 FreeSlot() const noexcept;
			void Centroid(float32 &cx, float32 &cy) const noexcept;
			bool FirstPair(int32 &a, int32 &b) const noexcept;
			void Rereference() noexcept;
			void StartSession(float64 timeMs) noexcept;
			void EndSession(float64 timeMs) noexcept;
			uint32 ProcessMotion(float64 timeMs) noexcept;
			uint32 CheckLongPress(float64 timeMs) noexcept;
			void Emit(NkEvent &event) noexcept;

			NkGestureConfig mConfig;
			NkGestureSink mSink;

			NkContact mContacts[NK_MAX_TOUCH_POINTS];
			uint32 mCount = 0;
			uint64 mWindowId = 0;

			// ── La session : du premier doigt pose au dernier leve ──
			bool mSession = false;
			bool mCancelled = false;
			bool mMoved = false;
			bool mLongPressDone = false;
			bool mPanActive = false;
			float64 mSessionStartMs = 0.0;
			uint32 mMaxFingers = 0;
			float32 mStartCx = 0.f;
			float32 mStartCy = 0.f;
			// Reference du pan : le centroide au dernier pan emis. Recalee quand
			// le nombre de doigts change, sinon le centroide SAUTE et le pan
			// suivant porterait un deplacement que personne n'a fait.
			float32 mRefCx = 0.f;
			float32 mRefCy = 0.f;
			// Centroide au mouvement precedent : il cumule le trajet du balayage
			// et estime la vitesse du pan.
			float32 mPrevCx = 0.f;
			float32 mPrevCy = 0.f;
			float64 mPrevMs = 0.0;
			float32 mTravelX = 0.f;
			float32 mTravelY = 0.f;
			float32 mVelX = 0.f;
			float32 mVelY = 0.f;

			// ── La paire (pincer / tourner) ──
			bool mPairValid = false;
			uint64 mPairA = 0;
			uint64 mPairB = 0;
			float32 mPairD0 = 1.f;
			float32 mPairAngle0 = 0.f;
			float32 mScaleBase = 1.f;
			float32 mScale = 1.f;
			float32 mAngleBase = 0.f;
			float32 mAngle = 0.f;
			bool mPinchActive = false;
			bool mRotateActive = false;

			// ── La tape precedente (tapes enchainees) ──
			bool mHasLastTap = false;
			float64 mLastTapUpMs = 0.0;
			float32 mLastTapX = 0.f;
			float32 mLastTapY = 0.f;
			uint32 mLastTapCount = 0;
			uint32 mLastTapFingers = 0;

			uint64 mEmitted = 0;
			uint32 mEmittedThisCall = 0;
			bool mDansAppel = false; ///< garde contre la reentree (voir OnTouchEvent)
	};

} // namespace nkentseu

#endif // __NKENTSEU_EVENT_NKGESTURERECOGNIZER_H__
