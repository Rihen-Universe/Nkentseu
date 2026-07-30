// =============================================================================
// Tutoriels3D / camera_input.h — ENTRÉES SOURIS + CLAVIER + TACTILE -> CAMÉRA
//
// Le moteur fournit NkOrbitCameraController3D (NKRenderer), volontairement
// INDÉPENDANT du système d'événements : il expose Rotate/Pan/Zoom, et c'est à
// l'application de traduire ses événements en appels dessus. Ce fichier est
// exactement cette traduction, réutilisable par les étapes 4 et 5, et il
// fonctionne sur TOUTES les plateformes :
//
//   Bureau (souris/clavier)                Mobile (tactile)
//   ─────────────────────────              ────────────────────────
//   drag GAUCHE ou MILIEU  = orbiter       1 doigt qui glisse = orbiter
//   Shift + drag           = panoramique   2 doigts qui glissent = panoramique
//   molette                = zoom          pincement 2 doigts = zoom
//   W/A/S/D                = déplacer la cible
//
// Un "tap" (clic ou toucher SANS glisser) est mis de côté et récupérable par
// l'application via ConsumeTap() — c'est ce que l'étape 5 utilise pour la
// sélection d'objets, sans entrer en conflit avec l'orbite au drag.
//
// Usage :
//   TutoCameraInput camInput;
//   camInput.Install();                    // branche les callbacks NKEvent
//   ... dans la boucle :
//   camInput.Update(dt);                   // WASD (état clavier continu)
//   camInput.orbit.Apply(cam);             // écrit position+cible dans la caméra
//   float32 tx, ty;
//   if (camInput.ConsumeTap(tx, ty)) { ... sélection ... }
// =============================================================================
#pragma once

#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKEvent/NkEventDispatcher.h" // NkInput (état clavier persistant, poll-based)
#include "NKMath/NkFunctions.h"		   // NkSqrt (distance de pincement)
#include "NKRenderer/Core/NkCameraController.h"

namespace tuto {

	using namespace nkentseu;

	struct TutoCameraInput {
			renderer::NkOrbitCameraController3D orbit;
			float32 moveSpeed = 3.f;	  // m/s pour WASD
			float32 tapThreshold = 10.f;  // px : en dessous, un press+release = un "tap"
			float32 pinchZoomScale = 0.02f; // sensibilité du pincement (px -> pas de zoom)

			// ── Branche souris + tactile. À appeler UNE fois. ────────────────────
			void Install() {
				NkEventSystem &events = NkEvents();

				// ── SOURIS (bureau) ──────────────────────────────────────────────
				events.AddEventCallback<NkMouseButtonPressEvent>([this](NkMouseButtonPressEvent *e) {
					if (e->IsLeft()) {
						mLeftDown = true;
						mDragAccum = 0.f;
						mPressX = (float32)e->GetX();
						mPressY = (float32)e->GetY();
					}
				});
				events.AddEventCallback<NkMouseButtonReleaseEvent>([this](NkMouseButtonReleaseEvent *e) {
					if (e->IsLeft()) {
						// Relâché sans (presque) bouger -> c'était un tap/clic de sélection.
						if (mLeftDown && mDragAccum < tapThreshold) {
							mTapPending = true;
							mTapX = (float32)e->GetX();
							mTapY = (float32)e->GetY();
						}
						mLeftDown = false;
					}
				});
				events.AddEventCallback<NkMouseMoveEvent>([this](NkMouseMoveEvent *e) {
					const bool left = e->IsButtonDown(NkMouseButton::NK_MB_LEFT);
					const bool middle = e->IsButtonDown(NkMouseButton::NK_MB_MIDDLE);
					if (!left && !middle)
						return;
					const float32 dx = (float32)e->GetDeltaX();
					const float32 dy = (float32)e->GetDeltaY();
					mDragAccum += (dx < 0.f ? -dx : dx) + (dy < 0.f ? -dy : dy);
					if (e->GetModifiers().shift)
						orbit.Pan(dx, dy);
					else
						orbit.Rotate(dx * -1.f, dy); // glisser à droite = tourner à droite
				});
				events.AddEventCallback<NkMouseWheelVerticalEvent>(
					[this](NkMouseWheelVerticalEvent *e) { orbit.Zoom((float32)e->GetDeltaY()); });

				// ── TACTILE (Android / iOS / HarmonyOS) ──────────────────────────
				events.AddEventCallback<NkTouchBeginEvent>([this](NkTouchBeginEvent *e) {
					mTouchCount += e->GetNumTouches();
					if (mTouchCount > 2)
						mTouchCount = 2;
					mDragAccum = 0.f;
					mPinchDist = -1.f; // re-mesuré au prochain move à 2 doigts
					mPressX = e->GetCentroidX();
					mPressY = e->GetCentroidY();
					mLastCX = e->GetCentroidX();
					mLastCY = e->GetCentroidY();
				});
				events.AddEventCallback<NkTouchMoveEvent>([this](NkTouchMoveEvent *e) {
					const uint32 n = e->GetNumTouches();
					if (n == 0)
						return;
					if (n == 1) {
						// 1 doigt : orbite (les deltas sont fournis par point).
						const NkTouchPoint &p = e->GetTouch(0);
						mDragAccum += (p.deltaX < 0.f ? -p.deltaX : p.deltaX) + (p.deltaY < 0.f ? -p.deltaY : p.deltaY);
						orbit.Rotate(p.deltaX * -1.f, p.deltaY);
						mPinchDist = -1.f;
						return;
					}
					// 2 doigts : pincement = zoom, déplacement du centroïde = pan.
					const NkTouchPoint &a = e->GetTouch(0);
					const NkTouchPoint &b = e->GetTouch(1);
					const float32 ddx = a.clientX - b.clientX, ddy = a.clientY - b.clientY;
					const float32 dist = math::NkSqrt(ddx * ddx + ddy * ddy);
					if (mPinchDist > 0.f)
						orbit.Zoom((dist - mPinchDist) * pinchZoomScale); // écarter = zoomer
					mPinchDist = dist;
					const float32 cx = e->GetCentroidX(), cy = e->GetCentroidY();
					orbit.Pan(cx - mLastCX, cy - mLastCY);
					mLastCX = cx;
					mLastCY = cy;
					mDragAccum += 1000.f; // 2 doigts -> jamais un tap
				});
				auto touchEnd = [this](float32 x, float32 y) {
					if (mTouchCount == 1 && mDragAccum < tapThreshold) {
						mTapPending = true;
						mTapX = x;
						mTapY = y;
					}
					if (mTouchCount > 0)
						mTouchCount--;
					mPinchDist = -1.f;
				};
				events.AddEventCallback<NkTouchEndEvent>([this, touchEnd](NkTouchEndEvent *e) {
					touchEnd(e->GetNumTouches() ? e->GetTouch(0).clientX : mPressX,
							 e->GetNumTouches() ? e->GetTouch(0).clientY : mPressY);
				});
				events.AddEventCallback<NkTouchCancelEvent>([this](NkTouchCancelEvent *) {
					mTouchCount = 0;
					mPinchDist = -1.f;
				});
			}

			// ── Clavier en état continu (WASD) : NkInput est rempli par PollEvents().
			void Update(float32 dt) {
				const float32 step = moveSpeed * dt;
				float32 fwd = 0.f, right = 0.f;
				if (NkInput.IsKeyDown(NkKey::NK_W))
					fwd += step;
				if (NkInput.IsKeyDown(NkKey::NK_S))
					fwd -= step;
				if (NkInput.IsKeyDown(NkKey::NK_D))
					right += step;
				if (NkInput.IsKeyDown(NkKey::NK_A))
					right -= step;
				if (fwd != 0.f || right != 0.f)
					orbit.MoveCameraRelative(right, 0.f, fwd);
			}

			// ── Récupère le dernier tap (clic ou toucher sans glisser), une fois.
			bool ConsumeTap(float32 &outX, float32 &outY) {
				if (!mTapPending)
					return false;
				mTapPending = false;
				outX = mTapX;
				outY = mTapY;
				return true;
			}

		private:
			// Souris
			bool mLeftDown = false;
			// Tap (souris ou tactile)
			bool mTapPending = false;
			float32 mTapX = 0.f, mTapY = 0.f;
			float32 mPressX = 0.f, mPressY = 0.f;
			float32 mDragAccum = 0.f;
			// Tactile
			int32 mTouchCount = 0;
			float32 mPinchDist = -1.f;
			float32 mLastCX = 0.f, mLastCY = 0.f;
	};

} // namespace tuto
