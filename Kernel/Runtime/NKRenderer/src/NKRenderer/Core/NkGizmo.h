#pragma once
// =============================================================================
// NkGizmo.h — NKRenderer Core
//
// Gizmo de transformation 3D RÉUTILISABLE (façon Blender) : translate / rotate /
// scale / combiné, poignées axe(1)/plan(2)/uniforme, orientation Global/Local/
// Normal, MULTI-sélection (marqueur par objet + un seul gizmo au barycentre, ou
// origines individuelles en Local), rendu OVERLAY.
//
// DÉCOUPLÉ : aucune dépendance à NKEvent ni à NkRender3D. L'application fournit
//   - une caméra (SetCamera : position/cible/FOV/viewport),
//   - une liste de cibles (NkGizmoTarget : matrice de base + demi-extent + rayon),
//   - un état d'input (NkGizmoInput : souris + modificateurs),
// et récupère le transform final via Apply(i, base). Le dessin passe par un simple
// callback `drawLine(a, b, color)` (l'app le branche sur NkRender3D::DrawDebugLine
// avec overlay=true, ou tout autre backend de lignes).
//
// Usage type (dans la boucle de rendu de l'app) :
//   gizmo.SetCamera(camPos, camTarget, 60.f, w, h);
//   // construire targets[] (transforms de BASE, sans le décalage utilisateur)
//   for (i) dc.transform = gizmo.Apply(i, targets[i].base);   // draw
//   gizmo.Update(targets, count, input);                      // pick + drag
//   gizmo.Draw([&](NkVec3f a, NkVec3f b, NkVec4f c){ r3d->DrawDebugLine(a,b,c,0.f,true); });
//   // touches : TAB->gizmo.CycleMode(), N->gizmo.CycleOrientation(), Suppr->gizmo.ResetSelected()
// =============================================================================
#include "NkCamera.h" // NkVec3f / NkVec4f / NkMat4f / NkAngle (via NKMath)
#include <cmath>

namespace nkentseu {
	namespace renderer {

		// Objet manipulable. base = transform de rendu SANS décalage utilisateur ;
		// localHalf = demi-extent du mesh en espace modèle (marqueur OBB) ; pickRadius
		// = rayon de la sphère englobante (ray-pick).
		struct NkGizmoTarget {
				NkMat4f base;
				NkVec3f localHalf;
				float32 pickRadius;
		};

		// État d'entrée fourni chaque frame (indépendant du système d'événements).
		struct NkGizmoInput {
				float32 mouseX = 0.f, mouseY = 0.f;	  // position curseur (pixels)
				float32 mouseDX = 0.f, mouseDY = 0.f; // delta curseur (pixels)
				bool leftPressed = false;			  // front montant du clic gauche
				bool leftDown = false;				  // bouton gauche maintenu
				bool shiftDown = false;				  // modificateur multi-sélection
				bool ctrlDown = false;				  // SNAP : transforme par pas fixes (façon Blender)
				int32 lockAxis = -1;				  // VERROU d'axe pendant le drag : 0=X 1=Y 2=Z (-1=aucun)
		};

		class NkGizmo3D {
			public:
				enum { MODE_TRANSLATE = 0, MODE_ROTATE = 1, MODE_SCALE = 2, MODE_COMBINE = 3 };

				enum { ORIENT_GLOBAL = 0, ORIENT_LOCAL = 1, ORIENT_NORMAL = 2 };

				static const int32 kMax = 256;

				NkGizmo3D() {
					for (int32 i = 0; i < kMax; i++)
						mRot[i] = NkMat4f::Identity();
				}

				// ── Config ────────────────────────────────────────────────────────
				void SetCamera(NkVec3f camPos, NkVec3f camTarget, float32 fovYDeg, float32 vpW, float32 vpH) {
					mCamPos = camPos;
					mFwd = Norm(NkVec3f{camTarget.x - camPos.x, camTarget.y - camPos.y, camTarget.z - camPos.z});
					mRgt = Norm(Cross(mFwd, NkVec3f{0.f, 1.f, 0.f}));
					mUp = Cross(mRgt, mFwd);
					mThY = tanf((fovYDeg * 0.5f) * 3.14159265f / 180.f);
					mThX = mThY * vpW / vpH;
					mVpW = vpW;
					mVpH = vpH;
				}

				void CycleMode() {
					mMode = (mMode + 1) % 4;
				}

				void CycleOrientation() {
					mOrient = (mOrient + 1) % 3;
				}

				// Choix EXPLICITE du mode/orientation par valeur (enum/entier) — pas de cycle.
				void SetMode(int32 m) {
					mMode = m & 3;
				}

				void SetOrientation(int32 o) {
					mOrient = ((o % 3) + 3) % 3;
				}

				// Choix EXPLICITE par NOM (insensible à la casse, 1re lettre) :
				//   mode   : "translate" | "rotate" | "scale" | "combine"
				//   orient : "global" | "local" | "normal"
				void SetModeByName(const char *name) {
					char c = Lower0(name);
					if (!c)
						return;
					if (c == 't')
						mMode = MODE_TRANSLATE;
					else if (c == 'r')
						mMode = MODE_ROTATE;
					else if (c == 's')
						mMode = MODE_SCALE;
					else if (c == 'c')
						mMode = MODE_COMBINE;
				}

				void SetOrientationByName(const char *name) {
					char c = Lower0(name);
					if (!c)
						return;
					if (c == 'g')
						mOrient = ORIENT_GLOBAL;
					else if (c == 'l')
						mOrient = ORIENT_LOCAL;
					else if (c == 'n')
						mOrient = ORIENT_NORMAL;
				}

				int32 Mode() const {
					return mMode;
				}

				int32 Orientation() const {
					return mOrient;
				}

				// Pas de SNAP (quand ctrlDown) : translate (unités monde), rotation (degrés),
				// échelle (delta). LIBREMENT définis par l'application (défauts 0.5 / 15° / 0.1).
				void SetSnapSteps(float32 translate, float32 rotateDeg, float32 scale) {
					SetSnapTranslate(translate);
					SetSnapRotateDeg(rotateDeg);
					SetSnapScale(scale);
				}

				void SetSnapTranslate(float32 v) {
					if (v > 0.f)
						mSnapT = v;
				}

				void SetSnapRotateDeg(float32 v) {
					if (v > 0.f)
						mSnapRdeg = v;
				}

				void SetSnapScale(float32 v) {
					if (v > 0.f)
						mSnapS = v;
				}

				float32 SnapTranslate() const {
					return mSnapT;
				}

				float32 SnapRotateDeg() const {
					return mSnapRdeg;
				}

				float32 SnapScale() const {
					return mSnapS;
				}

				// ── Sélection ─────────────────────────────────────────────────────
				bool HasSelection() const {
					return mHaveSel;
				}

				int32 ActiveIndex() const {
					return mSelId;
				}

				bool IsSelected(int32 i) const {
					return (i >= 0 && i < kMax) ? mSel[i] : false;
				}

				bool IsDragging() const {
					return mDragging;
				}

				void ClearSelection() {
					for (int32 i = 0; i < kMax; i++)
						mSel[i] = false;
					mSelId = -1;
				}

				void SelectAll() {
					for (int32 i = 0; i < mCount; i++)
						mSel[i] = true;
					if (mCount > 0)
						mSelId = mCount - 1;
				}

				void ResetSelected() {
					for (int32 i = 0; i < kMax; i++)
						if (mSel[i]) {
							mTr[i] = {0.f, 0.f, 0.f};
							mRot[i] = NkMat4f::Identity();
							mScale[i] = {0.f, 0.f, 0.f};
						}
				}

				// Effacements sélectifs (Alt+G / Alt+R / Alt+S façon Blender) sur la sélection.
				void ClearSelectedTranslate() {
					for (int32 i = 0; i < kMax; i++)
						if (mSel[i])
							mTr[i] = {0.f, 0.f, 0.f};
				}

				void ClearSelectedRotation() {
					for (int32 i = 0; i < kMax; i++)
						if (mSel[i])
							mRot[i] = NkMat4f::Identity();
				}

				void ClearSelectedScale() {
					for (int32 i = 0; i < kMax; i++)
						if (mSel[i])
							mScale[i] = {0.f, 0.f, 0.f};
				}

				// Transform final (base + décalage utilisateur) pour la cible i.
				NkMat4f Apply(int32 i, const NkMat4f &base) const {
					if (i < 0 || i >= kMax)
						return base;
					const NkVec3f t = mTr[i], s = mScale[i];
					const NkVec3f c = base * NkVec3f{0.f, 0.f, 0.f};
					NkMat4f about = NkMat4f::Translate(c) * mRot[i] *
									NkMat4f::Scale({1.f + s.x, 1.f + s.y, 1.f + s.z}) *
									NkMat4f::Translate({-c.x, -c.y, -c.z});
					return NkMat4f::Translate(t) * about * base;
				}

				// ── Boucle par frame : pick + drag ────────────────────────────────
				void Update(const NkGizmoTarget *targets, int32 count, const NkGizmoInput &in) {
					if (count > kMax)
						count = kMax;
					mCount = count;
					for (int32 i = 0; i < count; i++) {
						mComposed[i] = Apply(i, targets[i].base);
						mHalf[i] = targets[i].localHalf;
						mPickR[i] = targets[i].pickRadius;
					}
					// Pivot (barycentre des centres sélectionnés) + orientation.
					int32 selCount = 0;
					NkVec3f pivot = {0.f, 0.f, 0.f};
					for (int32 i = 0; i < count; i++)
						if (mSel[i]) {
							selCount++;
							pivot = pivot + Ctr(i);
						}
					if (selCount > 0)
						pivot = pivot * (1.f / (float32)selCount);
					mPivot = pivot;
					mHaveSel = (selCount > 0);
					mPivDist = Len(NkVec3f{pivot.x - mCamPos.x, pivot.y - mCamPos.y, pivot.z - mCamPos.z});
					// Taille ÉCRAN-CONSTANTE (façon Blender) : la taille MONDE du gizmo est
					// proportionnelle à la distance caméra->pivot -> taille écran ~constante.
					// Plancher très bas (0.02) pour rester proportionnel même en zoom très
					// proche (avant : 0.35 le figeait et il grossissait à l'écran de près).
					mGL = NkGMax(0.02f, 0.14f * mPivDist);
					mGB[0] = {1, 0, 0};
					mGB[1] = {0, 1, 0};
					mGB[2] = {0, 0, 1};
					if (mOrient != ORIENT_GLOBAL && mSelId >= 0 && mSelId < count) {
						const NkMat4f &M = mComposed[mSelId];
						NkVec3f o = M * NkVec3f{0.f, 0.f, 0.f};
						mGB[0] = Norm((M * NkVec3f{1, 0, 0}) - o);
						mGB[1] = Norm((M * NkVec3f{0, 1, 0}) - o);
						mGB[2] = Norm((M * NkVec3f{0, 0, 1}) - o);
					}

					// Interaction.
					if (in.leftPressed)
						DoPick(in);
					if (mDragging) {
						if (!in.leftDown || !mHaveSel)
							mDragging = false;
						else
							DoDrag(in);
					}
				}

				// ── Dessin (via callback de ligne) ────────────────────────────────
				// drawLine(NkVec3f a, NkVec3f b, NkVec4f color)
				template <class DrawLine> void Draw(DrawLine drawLine) const {
					if (!mHaveSel)
						return;
					GH hs[32];
					int32 nh = BuildHandles(hs);
					for (int32 hi = 0; hi < nh; hi++) {
						const NkVec4f col = HandleColor(hs[hi]);
						const bool active =
							mDragging && mGOp == hs[hi].op && mGMask == hs[hi].mask && mGKind == hs[hi].kind;
						const int32 tPx = active ? 7 : 4;
						ForEachSeg(hs[hi], mPivot, mGL,
								   [&](NkVec3f A, NkVec3f B) { ThickLine(A, B, col, tPx, drawLine); });
					}
					// Marqueurs OBB (par objet sélectionné).
					for (int32 i = 0; i < mCount; i++)
						if (mSel[i]) {
							const NkMat4f &M = mComposed[i];
							NkVec3f hh = mHalf[i];
							const NkVec4f Y =
								(i == mSelId) ? NkVec4f{1.f, 0.6f, 0.05f, 1.f} : NkVec4f{1.f, 0.85f, 0.1f, 1.f};
							NkVec3f c000 = Corner(M, hh, -1, -1, -1), c100 = Corner(M, hh, +1, -1, -1),
									c010 = Corner(M, hh, -1, +1, -1), c110 = Corner(M, hh, +1, +1, -1);
							NkVec3f c001 = Corner(M, hh, -1, -1, +1), c101 = Corner(M, hh, +1, -1, +1),
									c011 = Corner(M, hh, -1, +1, +1), c111 = Corner(M, hh, +1, +1, +1);
							ThickLine(c000, c100, Y, 2, drawLine);
							ThickLine(c010, c110, Y, 2, drawLine);
							ThickLine(c001, c101, Y, 2, drawLine);
							ThickLine(c011, c111, Y, 2, drawLine);
							ThickLine(c000, c010, Y, 2, drawLine);
							ThickLine(c100, c110, Y, 2, drawLine);
							ThickLine(c001, c011, Y, 2, drawLine);
							ThickLine(c101, c111, Y, 2, drawLine);
							ThickLine(c000, c001, Y, 2, drawLine);
							ThickLine(c100, c101, Y, 2, drawLine);
							ThickLine(c010, c011, Y, 2, drawLine);
							ThickLine(c110, c111, Y, 2, drawLine);
						}
				}

			private:
				struct GH {
						int32 op, mask, kind;
				}; // op:0=T 1=R 2=S ; kind:0=axe 1=plan 2=centre 3=anneau

				// ── Maths de base ─────────────────────────────────────────────────
				static float32 Dot(NkVec3f a, NkVec3f b) {
					return a.x * b.x + a.y * b.y + a.z * b.z;
				}

				static NkVec3f Cross(NkVec3f a, NkVec3f b) {
					return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
				}

				static float32 Len(NkVec3f v) {
					return sqrtf(Dot(v, v));
				}

				static NkVec3f Norm(NkVec3f v) {
					float32 l = Len(v);
					return (l > 1e-6f) ? NkVec3f{v.x / l, v.y / l, v.z / l} : v;
				}

				static float32 NkGMax(float32 a, float32 b) {
					return a > b ? a : b;
				}

				static char Lower0(const char *s) {
					if (!s || !s[0])
						return 0;
					char c = s[0];
					return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
				}

				static void AddComp(NkVec3f &v, int32 a, float32 x) {
					if (a == 0)
						v.x += x;
					else if (a == 1)
						v.y += x;
					else
						v.z += x;
				}

				static int32 AxisOf(int32 m) {
					return (m & 1) ? 0 : (m & 2) ? 1 : 2;
				}

				static void PlaneAx(int32 m, int32 &i, int32 &j) {
					int32 f = -1;
					i = 0;
					j = 0;
					for (int32 k = 0; k < 3; k++)
						if (m & (1 << k)) {
							if (f < 0) {
								i = k;
								f = k;
							} else
								j = k;
						}
				}

				static float32 CenterR(int32 op, float32 L) {
					return (op == 2) ? 0.16f * L : 0.13f * L;
				}

				static NkVec3f Corner(const NkMat4f &M, NkVec3f hh, float32 sx, float32 sy, float32 sz) {
					return M * NkVec3f{sx * hh.x, sy * hh.y, sz * hh.z};
				}

				NkVec3f Ctr(int32 i) const {
					return mComposed[i] * NkVec3f{0.f, 0.f, 0.f};
				}

				bool Project(NkVec3f P, float32 &px, float32 &py) const {
					NkVec3f v = {P.x - mCamPos.x, P.y - mCamPos.y, P.z - mCamPos.z};
					float32 zc = Dot(v, mFwd);
					if (zc <= 1e-3f)
						return false;
					float32 nx = Dot(v, mRgt) / (zc * mThX), ny = Dot(v, mUp) / (zc * mThY);
					px = (nx * 0.5f + 0.5f) * mVpW;
					py = (0.5f - ny * 0.5f) * mVpH;
					return true;
				}

				static float32 SegDist(float32 px, float32 py, float32 ax, float32 ay, float32 bx, float32 by) {
					float32 dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy;
					float32 t = (l2 > 1e-6f) ? ((px - ax) * dx + (py - ay) * dy) / l2 : 0.f;
					t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
					float32 cx = ax + t * dx, cy = ay + t * dy;
					return sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
				}

				int32 BuildHandles(GH hs[32]) const {
					int32 nh = 0;
					const bool cT = (mMode == 0 || mMode == 3), cR = (mMode == 1 || mMode == 3),
							   cS = (mMode == 2 || mMode == 3);
					if (cT) {
						hs[nh++] = {0, 1, 0};
						hs[nh++] = {0, 2, 0};
						hs[nh++] = {0, 4, 0};
						hs[nh++] = {0, 3, 1};
						hs[nh++] = {0, 6, 1};
						hs[nh++] = {0, 5, 1};
						hs[nh++] = {0, 7, 2};
					}
					if (cR) {
						hs[nh++] = {1, 1, 3};
						hs[nh++] = {1, 2, 3};
						hs[nh++] = {1, 4, 3};
					}
					if (cS) {
						hs[nh++] = {2, 1, 0};
						hs[nh++] = {2, 2, 0};
						hs[nh++] = {2, 4, 0};
						hs[nh++] = {2, 3, 1};
						hs[nh++] = {2, 6, 1};
						hs[nh++] = {2, 5, 1};
						hs[nh++] = {2, 7, 2};
					}
					return nh;
				}

				// Segments monde d'une poignée (hit-test ET dessin partagent la géométrie).
				template <class CB> void ForEachSeg(const GH &h, NkVec3f C, float32 L, CB cb) const {
					const bool combine = (mMode == 3);
					const float32 Lt = L, Ls = combine ? 0.55f * L : L, Lr = combine ? 0.90f * L : L;
					if (h.kind == 0) {
						int32 a = AxisOf(h.mask);
						NkVec3f dir = mGB[a], u = mGB[(a + 1) % 3], w = mGB[(a + 2) % 3];
						float32 len = (h.op == 0) ? Lt : Ls;
						NkVec3f E = C + dir * len;
						cb(C, E);
						if (h.op == 0) {
							float32 hl = 0.20f * L, hw = 0.07f * L;
							NkVec3f b = E - dir * hl;
							NkVec3f q0 = b + u * hw + w * hw, q1 = b - u * hw + w * hw, q2 = b - u * hw - w * hw,
									q3 = b + u * hw - w * hw;
							cb(E, q0);
							cb(E, q1);
							cb(E, q2);
							cb(E, q3);
							cb(q0, q1);
							cb(q1, q2);
							cb(q2, q3);
							cb(q3, q0);
						} else {
							float32 hb = 0.06f * L;
							NkVec3f cc[8];
							for (int32 j = 0; j < 8; j++)
								cc[j] = E + u * ((j & 1 ? 1.f : -1.f) * hb) + w * ((j & 2 ? 1.f : -1.f) * hb) +
										dir * ((j & 4 ? 1.f : -1.f) * hb);
							const int32 ed[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
													 {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
							for (int32 j = 0; j < 12; j++)
								cb(cc[ed[j][0]], cc[ed[j][1]]);
						}
					} else if (h.kind == 1) {
						int32 i, j;
						PlaneAx(h.mask, i, j);
						NkVec3f di = mGB[i], dj = mGB[j];
						float32 a0 = (h.op == 0) ? 0.22f * L : 0.32f * L, b0 = (h.op == 0) ? 0.45f * L : 0.58f * L;
						NkVec3f p00 = C + di * a0 + dj * a0, p10 = C + di * b0 + dj * a0, p11 = C + di * b0 + dj * b0,
								p01 = C + di * a0 + dj * b0;
						cb(p00, p10);
						cb(p10, p11);
						cb(p11, p01);
						cb(p01, p00);
					} else if (h.kind == 2) {
						float32 rU = CenterR(h.op, L);
						NkVec3f prev{};
						for (int32 k = 0; k <= 28; k++) {
							float32 t = (float32)k / 28.f * 6.2831853f;
							NkVec3f P = C + (mRgt * cosf(t) + mUp * sinf(t)) * rU;
							if (k > 0)
								cb(prev, P);
							prev = P;
						}
					} else {
						int32 a = AxisOf(h.mask);
						NkVec3f u = mGB[(a + 1) % 3], w = mGB[(a + 2) % 3];
						NkVec3f prev{};
						for (int32 k = 0; k <= 48; k++) {
							float32 t = (float32)k / 48.f * 6.2831853f;
							NkVec3f P = C + (u * cosf(t) + w * sinf(t)) * Lr;
							if (k > 0)
								cb(prev, P);
							prev = P;
						}
					}
				}

				NkVec4f HandleColor(const GH &h) const {
					const NkVec4f ACOL[3] = {
						{1.00f, 0.15f, 0.20f, 1.f}, {0.35f, 0.95f, 0.10f, 1.f}, {0.15f, 0.45f, 1.00f, 1.f}};
					if (mDragging && mGOp == h.op && mGMask == h.mask && mGKind == h.kind)
						return {1.f, 0.92f, 0.15f, 1.f};
					if (h.kind == 2)
						return {0.95f, 0.95f, 0.95f, 1.f};
					if (h.kind == 1) {
						int32 nrm = (h.mask == 3) ? 2 : (h.mask == 6) ? 0 : 1;
						return ACOL[nrm];
					}
					return ACOL[AxisOf(h.mask)];
				}

				// Ligne épaisse (N copies décalées perp écran) via le callback de ligne.
				template <class DrawLine>
				void ThickLine(NkVec3f A, NkVec3f B, NkVec4f col, int32 tPx, DrawLine &dl) const {
					float32 ax, ay, bx, by;
					bool oa = Project(A, ax, ay), ob = Project(B, bx, by);
					NkVec3f mid = {(A.x + B.x) * 0.5f, (A.y + B.y) * 0.5f, (A.z + B.z) * 0.5f};
					float32 dist = Len(NkVec3f{mid.x - mCamPos.x, mid.y - mCamPos.y, mid.z - mCamPos.z});
					float32 wpp = (2.f * mThY * dist) / mVpH;
					NkVec3f perp = mRgt;
					if (oa && ob) {
						float32 sdx = bx - ax, sdy = by - ay, sl = sqrtf(sdx * sdx + sdy * sdy);
						if (sl > 1e-3f) {
							float32 pxn = -sdy / sl, pyn = sdx / sl;
							perp = mRgt * pxn - mUp * pyn;
						}
					}
					if (tPx < 1)
						tPx = 1;
					for (int32 i = 0; i < tPx; i++) {
						float32 off = ((float32)i - (tPx - 1) * 0.5f) * wpp;
						NkVec3f o = perp * off;
						dl(A + o, B + o, col);
					}
				}

				// ── Pick : poignée gizmo, sinon (dé)sélection objet ───────────────
				void DoPick(const NkGizmoInput &in) {
					int32 hit = -1;
					if (mHaveSel) {
						const float32 cur = in.mouseX, curY = in.mouseY;
						float32 cpx, cpy;
						bool cok = Project(mPivot, cpx, cpy);
						float32 pxPerW = mVpH / (2.f * mThY * NkGMax(mPivDist, 1e-3f));
						float32 best = 13.f;
						GH hs[32];
						int32 nh = BuildHandles(hs);
						for (int32 hi = 0; hi < nh; hi++) {
							float32 d = 1e30f;
							if (hs[hi].kind == 2) {
								if (cok) {
									float32 dc = sqrtf((cur - cpx) * (cur - cpx) + (curY - cpy) * (curY - cpy));
									float32 rpx = CenterR(hs[hi].op, mGL) * pxPerW;
									if (dc < rpx)
										d = dc * 0.4f;
								}
							} else
								ForEachSeg(hs[hi], mPivot, mGL, [&](NkVec3f P0, NkVec3f P1) {
									float32 ax, ay, bx, by;
									if (Project(P0, ax, ay) && Project(P1, bx, by)) {
										float32 dd = SegDist(cur, curY, ax, ay, bx, by);
										if (dd < d)
											d = dd;
									}
								});
							if (d < best) {
								best = d;
								hit = hi;
							}
						}
						if (hit >= 0) {
							mDragging = true;
							mGOp = hs[hit].op;
							mGMask = hs[hit].mask;
							mGKind = hs[hit].kind;
							mResT = {0, 0, 0};
							mResR = 0.f;
							mResS = {0, 0, 0}; // reset des résidus de snap
							if (hs[hit].op == 1) {
								float32 cx, cy;
								if (Project(mPivot, cx, cy))
									mLastAngle = atan2f(in.mouseY - cy, in.mouseX - cx);
							}
							return;
						}
					}
					// Ray-pick objet.
					float32 ndcX = 2.f * in.mouseX / mVpW - 1.f, ndcY = 1.f - 2.f * in.mouseY / mVpH;
					NkVec3f rd = Norm(NkVec3f{mFwd.x + mRgt.x * (ndcX * mThX) + mUp.x * (ndcY * mThY),
											  mFwd.y + mRgt.y * (ndcX * mThX) + mUp.y * (ndcY * mThY),
											  mFwd.z + mRgt.z * (ndcX * mThX) + mUp.z * (ndcY * mThY)});
					float32 bestT = 1e30f;
					int32 bestId = -1;
					for (int32 i = 0; i < mCount; i++) {
						NkVec3f c = Ctr(i);
						NkVec3f oc = {mCamPos.x - c.x, mCamPos.y - c.y, mCamPos.z - c.z};
						float32 b = Dot(oc, rd), cc = Dot(oc, oc) - mPickR[i] * mPickR[i], disc = b * b - cc;
						if (disc >= 0.f) {
							float32 t = -b - sqrtf(disc);
							if (t > 0.f && t < bestT) {
								bestT = t;
								bestId = i;
							}
						}
					}
					if (bestId >= 0) {
						if (!in.shiftDown) {
							for (int32 i = 0; i < mCount; i++)
								mSel[i] = false;
							mSel[bestId] = true;
							mSelId = bestId;
						} else {
							mSel[bestId] = !mSel[bestId];
							if (mSel[bestId])
								mSelId = bestId;
							else {
								mSelId = -1;
								for (int32 i = 0; i < mCount; i++)
									if (mSel[i]) {
										mSelId = i;
										break;
									}
							}
						}
					} else if (!in.shiftDown) {
						for (int32 i = 0; i < mCount; i++)
							mSel[i] = false;
						mSelId = -1;
					}
				}

				// Accumule `raw` dans `res` ; si `on` (Ctrl), ne restitue que les multiples
				// ENTIERS de `step` (garde le reste) -> déplacement par pas. Sinon = `raw`.
				static float32 SnapAmt(float32 &res, float32 raw, float32 step, bool on) {
					if (!on || step <= 0.f) {
						res = 0.f;
						return raw;
					}
					res += raw;
					float32 q = truncf(res / step) * step;
					res -= q;
					return q;
				}

				// ── Drag : applique à tous les sélectionnés (repère/pivot par orientation) ──
				void DoDrag(const NkGizmoInput &in) {
					const int32 op = mGOp;
					int32 mask = mGMask, kind = mGKind;
					// VERROU d'axe (X/Y/Z) : force l'axe unique + chemin par-axe.
					if (in.lockAxis >= 0 && in.lockAxis < 3) {
						mask = (1 << in.lockAxis);
						if (op != 1)
							kind = 0;
					}
					const bool snap = in.ctrlDown;
					const float32 mdx = in.mouseDX, mdy = in.mouseDY, mx = in.mouseX, my = in.mouseY;
					const bool localOri = (mOrient != ORIENT_GLOBAL);
					float32 cpx, cpy;
					bool cok = Project(mPivot, cpx, cpy);
					if (op == 0) {
						if (kind == 2) {
							float32 wpp = (2.f * mThY * mPivDist) / mVpH;
							NkVec3f wd = mRgt * (mdx * wpp) + mUp * (-mdy * wpp);
							for (int32 i = 0; i < mCount; i++)
								if (mSel[i])
									mTr[i] = mTr[i] + wd;
						} else {
							float32 amtT[3] = {0.f, 0.f, 0.f};
							for (int32 a = 0; a < 3; a++)
								if (mask & (1 << a)) {
									NkVec3f E = mPivot + mGB[a] * mGL;
									float32 epx, epy;
									if (cok && Project(E, epx, epy)) {
										float32 sdx = epx - cpx, sdy = epy - cpy, sl = sqrtf(sdx * sdx + sdy * sdy);
										if (sl > 1e-3f) {
											sdx /= sl;
											sdy /= sl;
										}
										amtT[a] = (mdx * sdx + mdy * sdy) * (mGL / NkGMax(sl, 1.f));
									}
									float32 &r = (a == 0) ? mResT.x : (a == 1) ? mResT.y : mResT.z;
									amtT[a] = SnapAmt(r, amtT[a], mSnapT, snap);
								}
							for (int32 i = 0; i < mCount; i++)
								if (mSel[i]) {
									NkVec3f B[3], Pi;
									BasisPivot(i, localOri, B, Pi);
									for (int32 a = 0; a < 3; a++)
										if (mask & (1 << a))
											mTr[i] = mTr[i] + B[a] * amtT[a];
								}
						}
					} else if (op == 2) {
						const float32 k = 0.012f;
						float32 amt[3] = {0.f, 0.f, 0.f};
						if (kind == 2) {
							float32 rdx = mx - cpx, rdy = my - cpy, rl = sqrtf(rdx * rdx + rdy * rdy);
							if (rl > 1e-3f) {
								rdx /= rl;
								rdy /= rl;
							}
							float32 a = (mdx * rdx + mdy * rdy) * k;
							a = SnapAmt(mResS.x, a, mSnapS, snap);
							amt[0] = amt[1] = amt[2] = a;
						} else
							for (int32 a = 0; a < 3; a++)
								if (mask & (1 << a)) {
									NkVec3f E = mPivot + mGB[a] * mGL;
									float32 epx, epy;
									if (cok && Project(E, epx, epy)) {
										float32 sdx = epx - cpx, sdy = epy - cpy, sl = sqrtf(sdx * sdx + sdy * sdy);
										if (sl > 1e-3f) {
											sdx /= sl;
											sdy /= sl;
										}
										amt[a] = (mdx * sdx + mdy * sdy) * k;
									}
									float32 &r = (a == 0) ? mResS.x : (a == 1) ? mResS.y : mResS.z;
									amt[a] = SnapAmt(r, amt[a], mSnapS, snap);
								}
						for (int32 i = 0; i < mCount; i++)
							if (mSel[i]) {
								NkVec3f B[3], Pi;
								BasisPivot(i, localOri, B, Pi);
								NkVec3f rel = Ctr(i) - Pi;
								for (int32 a = 0; a < 3; a++)
									if (amt[a] != 0.f) {
										AddComp(mScale[i], a, amt[a]);
										float32 along = Dot(rel, B[a]);
										mTr[i] = mTr[i] + B[a] * (along * amt[a]);
									}
								if (mScale[i].x < -0.9f)
									mScale[i].x = -0.9f;
								if (mScale[i].y < -0.9f)
									mScale[i].y = -0.9f;
								if (mScale[i].z < -0.9f)
									mScale[i].z = -0.9f;
							}
					} else {
						if (cok) {
							const int32 a = AxisOf(mask);
							float32 ang = atan2f(my - cpy, mx - cpx), d = ang - mLastAngle;
							while (d > 3.14159265f)
								d -= 6.2831853f;
							while (d < -3.14159265f)
								d += 6.2831853f;
							mLastAngle = ang;
							float32 viewSign =
								(Dot(NkVec3f{mCamPos.x - mPivot.x, mCamPos.y - mPivot.y, mCamPos.z - mPivot.z},
									 mGB[a]) > 0.f)
									? 1.f
									: -1.f;
							float32 th = -d * viewSign;
							th = SnapAmt(mResR, th, mSnapRdeg * 3.14159265f / 180.f,
										 snap); // snap rotation (pas en degrés)
							if (th != 0.f)
								for (int32 i = 0; i < mCount; i++)
									if (mSel[i]) {
										NkVec3f B[3], Pi;
										BasisPivot(i, localOri, B, Pi);
										NkMat4f R = NkMat4f::Rotation(B[a], NkAngle::FromRad(th));
										mRot[i] = R * mRot[i];
										NkVec3f rel = Ctr(i) - Pi, relR = R * rel;
										mTr[i] = mTr[i] + (relR - rel);
									}
						}
					}
				}

				void BasisPivot(int32 i, bool localOri, NkVec3f B[3], NkVec3f &Pi) const {
					if (!localOri) {
						B[0] = {1, 0, 0};
						B[1] = {0, 1, 0};
						B[2] = {0, 0, 1};
						Pi = mPivot;
					} else {
						const NkMat4f &M = mComposed[i];
						NkVec3f o = M * NkVec3f{0.f, 0.f, 0.f};
						B[0] = Norm((M * NkVec3f{1, 0, 0}) - o);
						B[1] = Norm((M * NkVec3f{0, 1, 0}) - o);
						B[2] = Norm((M * NkVec3f{0, 0, 1}) - o);
						Pi = o;
					}
				}

				// ── État ──────────────────────────────────────────────────────────
				int32 mMode = 0, mOrient = 0;
				bool mDragging = false;
				int32 mGOp = 0, mGMask = 0, mGKind = 0;
				float32 mLastAngle = 0.f;
				// Snap (quand ctrlDown) : pas + résidus accumulés par drag (quantification).
				float32 mSnapT = 0.5f, mSnapRdeg = 15.f, mSnapS = 0.1f;
				NkVec3f mResT = {0, 0, 0};
				float32 mResR = 0.f;
				NkVec3f mResS = {0, 0, 0};
				int32 mSelId = -1;
				bool mSel[kMax] = {};
				NkVec3f mTr[kMax] = {};
				NkMat4f mRot[kMax]; // init identité (constructeur)
				NkVec3f mScale[kMax] = {};

				// Caméra + cache par frame.
				NkVec3f mCamPos = {0, 0, 0}, mFwd = {0, 0, -1}, mRgt = {1, 0, 0}, mUp = {0, 1, 0};
				float32 mThY = 0.577f, mThX = 1.f, mVpW = 1.f, mVpH = 1.f;
				int32 mCount = 0;
				NkMat4f mComposed[kMax];
				NkVec3f mHalf[kMax] = {};
				float32 mPickR[kMax] = {};
				NkVec3f mPivot = {0, 0, 0}, mGB[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
				float32 mPivDist = 1.f, mGL = 1.f;
				bool mHaveSel = false;
		};

	} // namespace renderer
} // namespace nkentseu
