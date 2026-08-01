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

				// ── POINTS DE PIVOT (Blender : « Transform Pivot Point », touche `.`) ──
				// Détermine AUTOUR DE QUOI s'appliquent la ROTATION et l'ÉCHELLE, et où le
				// gizmo se DESSINE :
				//   MEDIAN     : barycentre des éléments sélectionnés (défaut Blender).
				//   BBOX       : centre de la BOÎTE ENGLOBANTE de la sélection (≠ barycentre
				//                dès que la répartition est inégale).
				//   CURSOR     : le CURSEUR 3D (position monde libre, cf. Set3DCursor).
				//   INDIVIDUAL : chaque élément est transformé autour de SON PROPRE centre
				//                (le gizmo, lui, reste dessiné au barycentre — comme Blender).
				//   ACTIVE     : tout est transformé autour de l'élément ACTIF (dernier
				//                sélectionné = ActiveIndex()).
				enum {
					PIVOT_MEDIAN = 0,
					PIVOT_BBOX = 1,
					PIVOT_CURSOR = 2,
					PIVOT_INDIVIDUAL = 3,
					PIVOT_ACTIVE = 4
				};

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

				// Marqueur OBB (cage englobante) autour de chaque objet sélectionné. Discret
				// par défaut (fin liseré) ; peut être MASQUÉ (façon Blender épuré, ou pour des
				// captures où l'on veut voir le gizmo SEUL). Défaut : affiché mais discret.
				void SetDrawObjectBounds(bool e) {
					mDrawOBB = e;
				}

				bool DrawObjectBounds() const {
					return mDrawOBB;
				}

				void CycleOrientation() {
					mOrient = (mOrient + 1) % 3;
				}

				// ── Point de pivot (façon Blender, touche `.`) ────────────────────
				void CyclePivot() {
					mPivotMode = (mPivotMode + 1) % 5;
				}

				void SetPivotMode(int32 p) {
					mPivotMode = ((p % 5) + 5) % 5;
				}

				int32 PivotMode() const {
					return mPivotMode;
				}

				static const char *PivotName(int32 p) {
					static const char *kN[5] = {"MEDIAN", "BOITE ENGLOBANTE", "CURSEUR 3D",
												"ORIGINES INDIVIDUELLES", "ELEMENT ACTIF"};
					return kN[((p % 5) + 5) % 5];
				}

				const char *PivotName() const {
					return PivotName(mPivotMode);
				}

				// CURSEUR 3D (position MONDE). Sert de pivot en mode PIVOT_CURSOR ; c'est
				// l'application qui le place (clic) et qui le DESSINE dans la scène.
				void Set3DCursor(NkVec3f p) {
					mCursor = p;
				}

				NkVec3f Get3DCursor() const {
					return mCursor;
				}

				void Reset3DCursor() {
					mCursor = {0.f, 0.f, 0.f};
				}

				// Choix EXPLICITE du mode/orientation par valeur (enum/entier) — pas de cycle.
				void SetMode(int32 m) {
					mMode = m & 3;
				}

				void SetOrientation(int32 o) {
					mOrient = ((o % 3) + 3) % 3;
				}

				// ── Orientation « NORMAL » (façon Blender, Edit Mode) ─────────────
				// L'éditeur de maillage fournit le REPÈRE de l'élément sélectionné :
				//   normal  -> axe Z du gizmo (direction d'extrusion),
				//   tangent -> axe X (une arête de la face / la direction de l'arête).
				// Y est reconstruit (Z x X). Tant qu'un repère est posé, ORIENT_NORMAL
				// l'utilise ; sans repère, ORIENT_NORMAL retombe sur le comportement
				// Local (base de la matrice composée de la cible active).
				void SetNormalFrame(NkVec3f normal, NkVec3f tangent) {
					NkVec3f z = Norm(normal);
					if (Len(z) < 0.5f)
						return; // normale dégénérée -> on garde l'ancien repère
					NkVec3f x = tangent;
					// Orthogonalisation de Gram-Schmidt de la tangente contre la normale.
					const float32 d = Dot(x, z);
					x = NkVec3f{x.x - z.x * d, x.y - z.y * d, x.z - z.z * d};
					if (Len(x) < 1e-5f) { // tangente colinéaire -> axe de secours
						NkVec3f a = (fabsf(z.y) < 0.9f) ? NkVec3f{0.f, 1.f, 0.f} : NkVec3f{1.f, 0.f, 0.f};
						x = Cross(a, z);
					}
					x = Norm(x);
					mNFrameX = x;
					mNFrameZ = z;
					mNFrameY = Norm(Cross(z, x));
					mHasNFrame = true;
				}

				void ClearNormalFrame() {
					mHasNFrame = false;
				}

				bool HasNormalFrame() const {
					return mHasNFrame;
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

				// ── AIMANTATION PERSISTANTE (Blender : Shift+Tab) ───────────────
				// Blender ne fait pas « Ctrl = aimanter ». Il a une bascule
				// PERSISTANTE dans l'en-tete, et Ctrl l'INVERSE le temps du geste :
				// aimantation eteinte -> Ctrl aimante ; aimantation allumee -> Ctrl
				// desaimante. C'est ce qui permet de travailler aimante en permanence
				// tout en s'echappant ponctuellement — impossible avec un simple
				// « Ctrl = aimanter », ou l'echappement n'existe pas.
				// Par defaut la bascule est ETEINTE : le comportement observable reste
				// donc celui d'avant (Ctrl aimante), l'inversion n'apparaissant que si
				// l'application allume l'aimantation.
				void SetSnapEnabled(bool on) {
					mSnapOn = on;
				}
				bool IsSnapEnabled() const {
					return mSnapOn;
				}
				// Etat EFFECTIF pour ce geste : la bascule, inversee par Ctrl.
				bool SnapActive(bool ctrlDown) const {
					return mSnapOn != ctrlDown;
				}

				// ── GRILLE ABSOLUE (Blender : « Absolute Grid Snap ») ───────────
				// false (defaut) = increment RELATIF : on avance par pas depuis la
				//   position de depart, quelle qu'elle soit. Un objet a x = 0,3 avec un
				//   pas de 0,5 ira a 0,8 — jamais a 0,5.
				// true = grille ABSOLUE : la position finale est un MULTIPLE du pas.
				//   Le meme objet ira a 0,5. C'est ce qu'on attend quand on dit
				//   « aligner sur la grille », et c'est une option distincte chez
				//   Blender precisement parce que les deux usages existent.
				// N'a de sens qu'en orientation GLOBALE : une grille absolue le long
				// d'axes locaux tournes ne serait plus une grille. On l'ignore donc
				// hors ORIENT_GLOBAL plutot que de produire un alignement faux.
				void SetSnapAbsolute(bool on) {
					mSnapAbsolute = on;
				}
				bool IsSnapAbsolute() const {
					return mSnapAbsolute;
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

				// Centroïde (barycentre) des cibles sélectionnées, calculé au dernier
				// Update(). Sert de PIVOT d'orbite caméra « autour de la sélection »
				// (façon Blender). Valeur d'une frame de retard (Update tourne après la
				// navigation caméra dans la démo) — sans impact perceptible.
				NkVec3f GetPivot() const {
					return mPivot;
				}

				// TEST/API : injecte un décalage utilisateur FIGÉ sur la sélection, par
				// le MÊME chemin interne que le drag (mTr/mRot/mScale). Utilisé par la
				// capture headless de non-régression du contour de sélection
				// (NK_SEL_TEST_XFORM) : reproduit un objet transformé sans souris.
				void SetSelectedTransform(NkVec3f translate, NkMat4f rot, NkVec3f scaleDelta) {
					for (int32 i = 0; i < kMax; i++)
						if (mSel[i]) {
							mTr[i] = translate;
							mRot[i] = rot;
							mScale[i] = scaleDelta;
						}
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

				// Sélection EXCLUSIVE d'une seule cible par index (désélectionne les autres).
				// Pratique pour piloter le gizmo par programme (tests, captures headless, API éditeur).
				void Select(int32 i) {
					for (int32 k = 0; k < kMax; k++)
						mSel[k] = false;
					if (i >= 0 && i < kMax) {
						mSel[i] = true;
						mSelId = i;
					} else
						mSelId = -1;
				}

				// AJOUTE une cible à la sélection sans vider les autres — l'équivalent
				// programmatique de Shift+clic. Select() est EXCLUSIF : sans ce pendant,
				// aucun moyen de construire une sélection multiple par API (tests,
				// captures headless, futur éditeur qui restaure une sélection sauvegardée).
				// La cible ajoutée devient l'ACTIVE, comme dans Blender.
				void AddToSelection(int32 i) {
					if (i >= 0 && i < kMax) {
						mSel[i] = true;
						mSelId = i;
					}
				}

				// Bascule l'état d'une cible (Shift+clic sur un objet déjà sélectionné le
				// retire). Si la cible active est retirée, l'index actif est réaffecté au
				// premier sélectionné restant, ou -1 s'il n'en reste aucun.
				void ToggleSelection(int32 i) {
					if (i < 0 || i >= kMax)
						return;
					mSel[i] = !mSel[i];
					if (mSel[i]) {
						mSelId = i;
						return;
					}
					if (mSelId == i) {
						mSelId = -1;
						for (int32 k = 0; k < kMax; k++)
							if (mSel[k]) {
								mSelId = k;
								break;
							}
					}
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

				// ── Acces PAR CIBLE aux decalages utilisateur ────────────────────
				// Le panneau Proprietes d'un editeur edite la transformation par
				// CHAMPS : il lit et ecrit les decalages de la cible active. Les
				// Clear* ci-dessus restent les remises a zero de la SELECTION.
				NkVec3f TranslateOf(int32 i) const {
					return (i >= 0 && i < kMax) ? mTr[i] : NkVec3f{0.f, 0.f, 0.f};
				}
				void SetTranslateOf(int32 i, NkVec3f v) {
					if (i >= 0 && i < kMax)
						mTr[i] = v;
				}
				NkMat4f RotationOf(int32 i) const {
					return (i >= 0 && i < kMax) ? mRot[i] : NkMat4f::Identity();
				}
				void SetRotationOf(int32 i, const NkMat4f &m) {
					if (i >= 0 && i < kMax)
						mRot[i] = m;
				}
				NkVec3f ScaleOf(int32 i) const {
					return (i >= 0 && i < kMax) ? mScale[i] : NkVec3f{0.f, 0.f, 0.f};
				}
				void SetScaleOf(int32 i, NkVec3f v) {
					if (i >= 0 && i < kMax)
						mScale[i] = v;
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

				// Même décalage utilisateur (translation, rotation, échelle) que Apply(),
				// mais composé AUTOUR D'UN POINT MONDE ARBITRAIRE au lieu du centre de la
				// cible i. La matrice renvoyée s'applique à des POINTS MONDE (pas à une
				// matrice de base) : P' = ApplyAbout(i, about) * P.
				// C'est ce que consomme l'Edit Mode : pivot commun (median/bbox/curseur/
				// actif) en passant `about` = ce pivot, ou pivot « ORIGINES INDIVIDUELLES »
				// en passant l'origine PROPRE de chaque élément.
				NkMat4f ApplyAbout(int32 i, NkVec3f about) const {
					if (i < 0 || i >= kMax)
						return NkMat4f::Identity();
					const NkVec3f t = mTr[i], s = mScale[i];
					return NkMat4f::Translate(t) * NkMat4f::Translate(about) * mRot[i] *
						   NkMat4f::Scale({1.f + s.x, 1.f + s.y, 1.f + s.z}) *
						   NkMat4f::Translate({-about.x, -about.y, -about.z});
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
					// ── PIVOT (façon Blender) : dépend du MODE DE PIVOT courant ───────
					// MEDIAN = barycentre des centres sélectionnés ; BBOX = centre de la
					// boîte englobante (coins OBB de chaque sélectionné) ; CURSOR = curseur
					// 3D ; ACTIVE = centre de l'élément actif ; INDIVIDUAL = le gizmo se
					// DESSINE au barycentre (comme Blender) et chaque objet est transformé
					// autour de son propre centre (cf. PivotOf).
					int32 selCount = 0;
					NkVec3f pivot = {0.f, 0.f, 0.f};
					NkVec3f bbMin = {1e30f, 1e30f, 1e30f}, bbMax = {-1e30f, -1e30f, -1e30f};
					for (int32 i = 0; i < count; i++)
						if (mSel[i]) {
							selCount++;
							pivot = pivot + Ctr(i);
							for (int32 c = 0; c < 8; c++) {
								const NkVec3f p = Corner(mComposed[i], mHalf[i], (c & 1) ? 1.f : -1.f,
														 (c & 2) ? 1.f : -1.f, (c & 4) ? 1.f : -1.f);
								bbMin.x = (p.x < bbMin.x) ? p.x : bbMin.x;
								bbMin.y = (p.y < bbMin.y) ? p.y : bbMin.y;
								bbMin.z = (p.z < bbMin.z) ? p.z : bbMin.z;
								bbMax.x = (p.x > bbMax.x) ? p.x : bbMax.x;
								bbMax.y = (p.y > bbMax.y) ? p.y : bbMax.y;
								bbMax.z = (p.z > bbMax.z) ? p.z : bbMax.z;
							}
						}
					if (selCount > 0)
						pivot = pivot * (1.f / (float32)selCount);
					if (selCount > 0) {
						if (mPivotMode == PIVOT_BBOX)
							pivot = (bbMin + bbMax) * 0.5f;
						else if (mPivotMode == PIVOT_ACTIVE && mSelId >= 0 && mSelId < count)
							pivot = Ctr(mSelId);
					}
					if (mPivotMode == PIVOT_CURSOR)
						pivot = mCursor; // le curseur 3D existe même sans sélection
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
					if (mOrient == ORIENT_NORMAL && mHasNFrame) {
						// Repère fourni par l'éditeur de maillage (normale de l'élément).
						mGB[0] = mNFrameX;
						mGB[1] = mNFrameY;
						mGB[2] = mNFrameZ;
					} else if (mOrient != ORIENT_GLOBAL && mSelId >= 0 && mSelId < count) {
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
					// Survol (feedback Blender) : calculé hors drag, sinon suit la poignée tirée.
					if (mHaveSel && !mDragging)
						ComputeHover(in);
					else
						mHovValid = false;
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
						const bool hovered =
							mHovValid && mHovOp == hs[hi].op && mHovMask == hs[hi].mask && mHovKind == hs[hi].kind;
						// Lignes FINES par défaut (façon Blender épuré) ; épaissies seulement
						// à l'interaction (survol -> 3, drag -> 5).
						const int32 tPx = active ? 5 : (hovered ? 3 : 2);
						ForEachSeg(hs[hi], mPivot, mGL,
								   [&](NkVec3f A, NkVec3f B) { ThickLine(A, B, col, tPx, drawLine); });
					}
					// Cercle de VUE blanc (Blender) : anneau extérieur face-caméra en mode
					// Rotate/Combine. Purement visuel (non pickable), fin et discret.
					if (mMode == MODE_ROTATE || mMode == MODE_COMBINE) {
						const float32 Rv = ((mMode == MODE_COMBINE) ? 0.90f : 1.12f) * mGL;
						const NkVec4f wc = {0.90f, 0.90f, 0.90f, 0.55f};
						NkVec3f prev{};
						for (int32 k = 0; k <= 64; k++) {
							float32 t = (float32)k / 64.f * 6.2831853f;
							NkVec3f P = mPivot + (mRgt * cosf(t) + mUp * sinf(t)) * Rv;
							if (k > 0)
								ThickLine(prev, P, wc, 2, drawLine);
							prev = P;
						}
					}
					// Marqueur OBB discret (fin liseré), masquable via SetDrawObjectBounds(false).
					DrawOBBMarkers(drawLine);
				}

				// ── Dessin PLEIN (façon Blender) : pointes/cubes/anneaux SOLIDES ──────
				// Surcharge à 2 callbacks : drawLine(a,b,color) pour les tiges/liserés fins,
				// drawTri(a,b,c,color) pour les formes PLEINES (branché sur DrawDebugTriangle).
				// L'app fournit drawTri ; les call-sites qui ne passent QUE drawLine tombent sur
				// la surcharge à 1 argument (rendu FIL DE FER, inchangé) -> zéro régression.
				template <class DrawLine, class DrawTri> void Draw(DrawLine drawLine, DrawTri drawTri) const {
					if (!mHaveSel)
						return;
					GH hs[32];
					int32 nh = BuildHandles(hs);
					for (int32 hi = 0; hi < nh; hi++) {
						const NkVec4f col = HandleColor(hs[hi]);
						DrawHandleSolid(hs[hi], mPivot, mGL, col, drawLine, drawTri);
					}
					// Cercle de VUE blanc (Blender) : reste une FINE ligne (pas de plein).
					if (mMode == MODE_ROTATE || mMode == MODE_COMBINE) {
						const float32 Rv = ((mMode == MODE_COMBINE) ? 0.90f : 1.12f) * mGL;
						const NkVec4f wc = {0.90f, 0.90f, 0.90f, 0.55f};
						NkVec3f prev{};
						for (int32 k = 0; k <= 64; k++) {
							float32 t = (float32)k / 64.f * 6.2831853f;
							NkVec3f P = mPivot + (mRgt * cosf(t) + mUp * sinf(t)) * Rv;
							if (k > 0)
								ThickLine(prev, P, wc, 2, drawLine);
							prev = P;
						}
					}
					DrawOBBMarkers(drawLine);
				}

			private:
				struct GH {
						int32 op, mask, kind;
				}; // op:0=T 1=R 2=S ; kind:0=axe 1=plan 2=centre 3=anneau

				// Cage OBB : fin liseré DISCRET (1 px, alpha bas, orange atténué) autour de
				// chaque objet sélectionné. Masquée si mDrawOBB=false.
				template <class DrawLine> void DrawOBBMarkers(DrawLine &drawLine) const {
					if (!mDrawOBB)
						return;
					for (int32 i = 0; i < mCount; i++)
						if (mSel[i]) {
							const NkMat4f &M = mComposed[i];
							NkVec3f hh = mHalf[i];
							const NkVec4f Y = (i == mSelId) ? NkVec4f{1.f, 0.55f, 0.05f, 0.30f}
															: NkVec4f{1.f, 0.75f, 0.1f, 0.25f};
							NkVec3f c000 = Corner(M, hh, -1, -1, -1), c100 = Corner(M, hh, +1, -1, -1),
									c010 = Corner(M, hh, -1, +1, -1), c110 = Corner(M, hh, +1, +1, -1);
							NkVec3f c001 = Corner(M, hh, -1, -1, +1), c101 = Corner(M, hh, +1, -1, +1),
									c011 = Corner(M, hh, -1, +1, +1), c111 = Corner(M, hh, +1, +1, +1);
							ThickLine(c000, c100, Y, 1, drawLine);
							ThickLine(c010, c110, Y, 1, drawLine);
							ThickLine(c001, c101, Y, 1, drawLine);
							ThickLine(c011, c111, Y, 1, drawLine);
							ThickLine(c000, c010, Y, 1, drawLine);
							ThickLine(c100, c110, Y, 1, drawLine);
							ThickLine(c001, c011, Y, 1, drawLine);
							ThickLine(c101, c111, Y, 1, drawLine);
							ThickLine(c000, c001, Y, 1, drawLine);
							ThickLine(c100, c101, Y, 1, drawLine);
							ThickLine(c010, c011, Y, 1, drawLine);
							ThickLine(c110, c111, Y, 1, drawLine);
						}
				}

				// Rendu PLEIN d'une poignée via triangles (drawTri), façon Blender solide.
				// - back-face culling + ombrage par la normale -> aspect SOLIDE correct SANS
				//   depth-test (overlay), et alpha ~opaque -> indépendant de l'ordre de tri.
				template <class DrawLine, class DrawTri>
				void DrawHandleSolid(const GH &h, NkVec3f C, float32 L, NkVec4f col, DrawLine &drawLine,
									 DrawTri &drawTri) const {
					// Triangle PLEIN : ref = point interne -> normale sortante ; cull si face arrière.
					auto emitSolid = [&](NkVec3f a, NkVec3f b, NkVec3f c3, NkVec3f ref) {
						NkVec3f n = Norm(Cross(b - a, c3 - a));
						NkVec3f ctr = (a + b + c3) * (1.f / 3.f);
						if (Dot(n, ctr - ref) < 0.f) {
							n = NkVec3f{-n.x, -n.y, -n.z};
							NkVec3f tmp = b;
							b = c3;
							c3 = tmp;
						}
						NkVec3f toCam = mCamPos - ctr;
						if (Dot(n, toCam) <= 0.f)
							return; // face arrière -> cull
						float32 sh = 0.66f + 0.34f * Clamp01(Dot(n, Norm(toCam)));
						drawTri(a, b, c3, NkVec4f{col.x * sh, col.y * sh, col.z * sh, col.w});
					};
					// Triangle PLAT bi-face (plans/disques/rubans) : pas de cull, ombrage constant.
					auto emitFlat = [&](NkVec3f a, NkVec3f b, NkVec3f c3, float32 sh, float32 alpha) {
						drawTri(a, b, c3, NkVec4f{col.x * sh, col.y * sh, col.z * sh, alpha});
					};
					const bool combine = (mMode == 3);

					if (h.kind == 0) {
						int32 a = AxisOf(h.mask);
						NkVec3f dir = mGB[a], u = mGB[(a + 1) % 3], w = mGB[(a + 2) % 3];
						float32 Lt = L, Ls = combine ? 0.55f * L : L;
						float32 len = (h.op == 0) ? Lt : Ls;
						NkVec3f E = C + dir * len;
						ThickLine(C, E, col, 2, drawLine); // tige fine
						if (h.op == 0) {
							// TÊTE DE FLÈCHE CONIQUE PLEINE (côtés + base fermée), PETITE.
							const float32 hl = 0.22f * L, hw = 0.075f * L;
							NkVec3f b = E - dir * hl;	  // centre de la base
							NkVec3f ref = (E + b) * 0.5f;  // milieu de l'axe du cône
							const int32 N = 14;
							NkVec3f prev{};
							for (int32 k = 0; k <= N; k++) {
								float32 t = (float32)k / (float32)N * 6.2831853f;
								NkVec3f p = b + (u * cosf(t) + w * sinf(t)) * hw;
								if (k > 0) {
									emitSolid(E, prev, p, ref); // face latérale
									emitSolid(b, prev, p, ref); // base (cap)
								}
								prev = p;
							}
						} else {
							// PETIT CUBE PLEIN (6 faces = 12 triangles).
							const float32 hb = 0.06f * L;
							NkVec3f cc[8];
							for (int32 j = 0; j < 8; j++)
								cc[j] = E + u * ((j & 1) ? hb : -hb) + w * ((j & 2) ? hb : -hb) +
										dir * ((j & 4) ? hb : -hb);
							static const int32 fq[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1},
														   {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
							for (int32 f = 0; f < 6; f++) {
								emitSolid(cc[fq[f][0]], cc[fq[f][1]], cc[fq[f][2]], E);
								emitSolid(cc[fq[f][0]], cc[fq[f][2]], cc[fq[f][3]], E);
							}
						}
					} else if (h.kind == 1) {
						// POIGNÉE DE PLAN : quad PLEIN semi-transparent + fin liseré.
						int32 i, j;
						PlaneAx(h.mask, i, j);
						NkVec3f di = mGB[i], dj = mGB[j];
						float32 a0 = (h.op == 0) ? 0.22f * L : 0.32f * L,
								b0 = (h.op == 0) ? 0.45f * L : 0.58f * L;
						NkVec3f p00 = C + di * a0 + dj * a0, p10 = C + di * b0 + dj * a0,
								p11 = C + di * b0 + dj * b0, p01 = C + di * a0 + dj * b0;
						float32 al = col.w; // ~0.55 (semi-transparent, cf. HandleColor)
						emitFlat(p00, p10, p11, 0.9f, al);
						emitFlat(p00, p11, p01, 0.9f, al);
						NkVec4f edge = {col.x, col.y, col.z, 0.9f};
						ThickLine(p00, p10, edge, 1, drawLine);
						ThickLine(p10, p11, edge, 1, drawLine);
						ThickLine(p11, p01, edge, 1, drawLine);
						ThickLine(p01, p00, edge, 1, drawLine);
					} else if (h.kind == 2) {
						// CENTRE : petit DISQUE PLEIN face-caméra (déplacement/échelle libre).
						float32 rU = CenterR(h.op, L);
						NkVec3f prev{};
						for (int32 k = 0; k <= 24; k++) {
							float32 t = (float32)k / 24.f * 6.2831853f;
							NkVec3f P = C + (mRgt * cosf(t) + mUp * sinf(t)) * rU;
							if (k > 0)
								emitFlat(C, prev, P, 1.f, 0.35f);
							prev = P;
						}
					} else {
						// ROTATE : RUBAN PLEIN (bande de triangles) le long du cercle de l'axe.
						int32 a = AxisOf(h.mask);
						NkVec3f u = mGB[(a + 1) % 3], w = mGB[(a + 2) % 3];
						float32 Lr = combine ? 0.90f * L : L;
						float32 bw = 0.022f * L; // demi-largeur du ruban
						NkVec3f pin{}, pout{};
						for (int32 k = 0; k <= 64; k++) {
							float32 t = (float32)k / 64.f * 6.2831853f;
							NkVec3f rad = u * cosf(t) + w * sinf(t);
							NkVec3f Ri = C + rad * (Lr - bw), Ro = C + rad * (Lr + bw);
							if (k > 0) {
								emitFlat(pin, pout, Ro, 0.92f, col.w);
								emitFlat(pin, Ro, Ri, 0.92f, col.w);
							}
							pin = Ri;
							pout = Ro;
						}
					}
				}

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

				static float32 Clamp01(float32 v) {
					return v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
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
							// TRANSLATE : petite pointe de flèche CONIQUE (silhouette + cercle de
							// base), tessellée ~10 segments façon Blender. Discrète, pas énorme.
							const float32 hl = 0.18f * L, hw = 0.055f * L;
							NkVec3f b = E - dir * hl; // centre de la base du cône
							const int32 N = 10;
							NkVec3f prevB{};
							for (int32 k = 0; k <= N; k++) {
								float32 t = (float32)k / (float32)N * 6.2831853f;
								NkVec3f Pb = b + (u * cosf(t) + w * sinf(t)) * hw;
								if (k > 0)
									cb(prevB, Pb); // cercle de base du cône
								cb(E, Pb);		   // arête apex->base (silhouette conique)
								prevB = Pb;
							}
						} else {
							// SCALE : petit CUBE plein au bout (12 arêtes), discret.
							float32 hb = 0.05f * L;
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
						// ROTATE : anneau LISSE (bien tessellé, 64 segments) autour de l'axe.
						int32 a = AxisOf(h.mask);
						NkVec3f u = mGB[(a + 1) % 3], w = mGB[(a + 2) % 3];
						NkVec3f prev{};
						for (int32 k = 0; k <= 64; k++) {
							float32 t = (float32)k / 64.f * 6.2831853f;
							NkVec3f P = C + (u * cosf(t) + w * sinf(t)) * Lr;
							if (k > 0)
								cb(prev, P);
							prev = P;
						}
					}
				}

				NkVec4f HandleColor(const GH &h) const {
					// Couleurs d'axe FRANCHES façon Blender : X ROUGE, Y VERT, Z BLEU.
					const NkVec4f ACOL[3] = {
						{0.90f, 0.20f, 0.25f, 1.f}, {0.40f, 0.80f, 0.20f, 1.f}, {0.20f, 0.45f, 0.90f, 1.f}};
					// Drag actif -> JAUNE vif (priorité max).
					if (mDragging && mGOp == h.op && mGMask == h.mask && mGKind == h.kind)
						return {1.f, 0.92f, 0.15f, 1.f};
					// Survol (hover) -> blanc-jaune lumineux (feedback Blender).
					if (mHovValid && mHovOp == h.op && mHovMask == h.mask && mHovKind == h.kind)
						return {1.f, 1.f, 0.55f, 1.f};
					if (h.kind == 2)
						return {0.95f, 0.95f, 0.95f, 1.f};
					if (h.kind == 1) {
						// Poignées de PLAN : couleur de l'axe normal, SEMI-TRANSPARENTES (discrètes).
						int32 nrm = (h.mask == 3) ? 2 : (h.mask == 6) ? 0 : 1;
						NkVec4f c = ACOL[nrm];
						c.w = 0.55f;
						return c;
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
				// Renvoie l'index (dans l'ordre BuildHandles) de la poignée la plus proche
				// du curseur (px), ou -1 si aucune sous le seuil. Partagé par le pick (clic)
				// ET le hover (survol) -> géométrie/seuil strictement identiques.
				// SEUIL DE PICK PAR TYPE DE POIGNEE (pixels ecran).
				// Les AXES (tiges/fleches/cubes) et le petit DISQUE central sont les poignees
				// que l'on vise reellement -> seuil genereux (13 px, ordre de grandeur Blender).
				// Les POIGNEES DE PLAN (lisere du quad) et les RUBANS DE ROTATION sont de grandes
				// courbes qui TRAVERSENT le maillage : a 13 px elles avalaient des clics destines
				// a un sommet/arete situe dessous. Resserrees a 9 px -> toujours confortables a
				// attraper, mais elles laissent passer la selection de maillage.
				static float32 KindPickPx(int32 kind) {
					return (kind == 1 || kind == 3) ? 9.f : 13.f;
				}

				int32 PickHandle(float32 cur, float32 curY, const GH *hs, int32 nh,
									 float32 *outDist = nullptr) const {
					float32 cpx, cpy;
					bool cok = Project(mPivot, cpx, cpy);
					float32 pxPerW = mVpH / (2.f * mThY * NkGMax(mPivDist, 1e-3f));
					float32 best = 1e30f;
					int32 hit = -1;
					for (int32 hi = 0; hi < nh; hi++) {
						float32 d = 1e30f;
						if (hs[hi].kind == 2) {
							if (cok) {
								float32 dc = sqrtf((cur - cpx) * (cur - cpx) + (curY - cpy) * (curY - cpy));
								float32 rpx = CenterR(hs[hi].op, mGL) * pxPerW;
								// ⚠ Le gizmo ne doit JAMAIS avaler un clic destiné au maillage
								// DERRIÈRE lui (sélection sommet/arête/face en Edit Mode).
								// AVANT : tout point situé DANS le disque était capturé, avec en
								// prime un bonus (dc * 0.4) qui le faisait gagner contre toutes
								// les autres poignées -> un grand disque autour du pivot bloquait
								// la sélection sur une bonne partie de l'écran.
								// MAINTENANT :
								//  • petit disque central (translate/scale uniforme) : reste
								//    plein mais SANS bonus — il ne gagne que s'il est réellement
								//    le plus proche du curseur ;
								//  • grand cercle (rotation / cercle de vue) : COURONNE — seul le
								//    voisinage du CONTOUR est pickable, l'intérieur laisse passer
								//    le clic (comportement Blender).
								const float32 kSmallDiscPx = 18.f;
								d = (rpx <= kSmallDiscPx) ? ((dc < rpx) ? dc : 1e30f) : fabsf(dc - rpx);
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
						if (d < KindPickPx(hs[hi].kind) && d < best) {
							best = d;
							hit = hi;
						}
					}
					if (outDist)
						*outDist = (hit >= 0) ? best : 1e30f;
					return hit;
				}

			public:
				// DISTANCE ECRAN (px) a la poignee de gizmo la plus proche du curseur, ou 1e30
				// si aucune n'est sous son seuil. Sert a ARBITRER gizmo vs maillage : l'editeur
				// compare cette distance a celle de son meilleur candidat sommet/arete et ne
				// laisse le clic au gizmo que si le gizmo est REELLEMENT plus proche.
				float32 HandlePickDistPx(float32 mouseX, float32 mouseY) const {
					if (!mHaveSel)
						return 1e30f;
					GH hs[32];
					int32 nh = BuildHandles(hs);
					float32 d = 1e30f;
					PickHandle(mouseX, mouseY, hs, nh, &d);
					return d;
				}

			private:
				// Survol : calcule la poignée sous le curseur CHAQUE frame (hors drag) pour le
				// feedback visuel (surbrillance jaune/blanc). N'altère PAS la sélection/drag.
				void ComputeHover(const NkGizmoInput &in) {
					GH hs[32];
					int32 nh = BuildHandles(hs);
					int32 hit = PickHandle(in.mouseX, in.mouseY, hs, nh);
					if (hit >= 0) {
						mHovValid = true;
						mHovOp = hs[hit].op;
						mHovMask = hs[hit].mask;
						mHovKind = hs[hit].kind;
					} else
						mHovValid = false;
				}

				void DoPick(const NkGizmoInput &in) {
					if (mHaveSel) {
						GH hs[32];
						int32 nh = BuildHandles(hs);
						int32 hit = PickHandle(in.mouseX, in.mouseY, hs, nh);
						if (hit >= 0) {
							mDragging = true;
							// Debut de drag : la position libre repart du pivot courant.
							mDragFree = mPivot;
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
					// 1) Pick sur la BOÎTE de l'objet (OBB monde = mComposed × mHalf).
					// La sphère seule ne peut pas décrire un objet PLAT ou très allongé :
					// un sol de 80×80 aurait un rayon englobant énorme et volerait tous
					// les clics, y compris ceux des objets posés dessus. La boîte épouse
					// la forme réelle, donc chaque objet ne capte que sa propre surface.
					for (int32 i = 0; i < mCount; i++) {
						const NkVec3f h = mHalf[i];
						if (h.x <= 0.f && h.y <= 0.f && h.z <= 0.f)
							continue; // pas d'extent connu -> laissé à la sphère
						const NkMat4f &M = mComposed[i];
						const NkVec3f c = M * NkVec3f{0.f, 0.f, 0.f};
						// Axes de l'OBB obtenus en transformant les axes unitaires : la
						// longueur obtenue PORTE l'échelle, et on reste indépendant de la
						// convention de stockage de NkMat4f (même opérateur que Corner()).
						const NkVec3f ex = M * NkVec3f{1.f, 0.f, 0.f}, ey = M * NkVec3f{0.f, 1.f, 0.f},
									  ez = M * NkVec3f{0.f, 0.f, 1.f};
						const NkVec3f ax[3] = {{ex.x - c.x, ex.y - c.y, ex.z - c.z},
											   {ey.x - c.x, ey.y - c.y, ey.z - c.z},
											   {ez.x - c.x, ez.y - c.y, ez.z - c.z}};
						const float32 hl[3] = {h.x, h.y, h.z};
						const NkVec3f p{c.x - mCamPos.x, c.y - mCamPos.y, c.z - mCamPos.z};
						float32 tMin = -1e30f, tMax = 1e30f;
						bool hit = true;
						for (int32 a = 0; a < 3; a++) {
							const float32 len = sqrtf(Dot(ax[a], ax[a]));
							if (len <= 1e-6f)
								continue;
							const NkVec3f n{ax[a].x / len, ax[a].y / len, ax[a].z / len};
							// Demi-extent MONDE, avec une épaisseur PLANCHER : un plan a un
							// extent nul sur sa normale, et un slab d'épaisseur zéro rend le
							// test dégénéré (t1 == t2) donc fragile en flottant. Ce plancher
							// ne concerne QUE le pick — le marqueur garde l'extent réel et
							// reste donc plat sur un plan.
							float32 e = hl[a] * len;
							if (e < 1e-3f)
								e = 1e-3f;
							const float32 s = Dot(n, p), f = Dot(n, rd);
							if (f > 1e-6f || f < -1e-6f) {
								float32 t1 = (s - e) / f, t2 = (s + e) / f;
								if (t1 > t2) {
									const float32 tmp = t1;
									t1 = t2;
									t2 = tmp;
								}
								if (t1 > tMin)
									tMin = t1;
								if (t2 < tMax)
									tMax = t2;
								if (tMin > tMax) {
									hit = false;
									break;
								}
							} else if (-s - e > 0.f || -s + e < 0.f) {
								hit = false; // rayon parallèle et hors du slab
								break;
							}
						}
						if (hit && tMax >= 0.f) {
							const float32 t = tMin > 0.f ? tMin : tMax;
							if (t > 0.f && t < bestT) {
								bestT = t;
								bestId = i;
							}
						}
					}
					// 2) Repli SPHÈRE si le clic n'a touché aucune boîte : conserve la
					// tolérance historique (viser à côté d'un petit objet le sélectionne
					// quand même) sans laisser un objet plat capter ce qui ne le concerne pas.
					if (bestId < 0) {
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

				// Multiple de `step` le PLUS PROCHE de v (grille absolue). floorf(x+0.5)
				// et non truncf : truncf arrondit vers zero, donc -0,4 tomberait sur 0
				// alors que -0,5 est aussi proche — la grille serait dissymetrique autour
				// de l'origine, ce qui se voit des qu'on travaille des deux cotes.
				static float32 SnapToGrid(float32 v, float32 step) {
					if (step <= 0.f)
						return v;
					return floorf(v / step + 0.5f) * step;
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
					// GARDE ANTI-DÉRIVE : si la souris n'a PAS bougé cette frame (delta nul),
					// AUCUNE transformation ne doit avancer — même poignée pressée/maintenue.
					// La transfo ne progresse QUE sur un vrai mouvement. Corrige le bug « le
					// modèle continue de bouger alors que la souris est immobile mais le bouton
					// reste pressé » (delta périmé conservé entre frames côté input). Vaut pour
					// translate ET scale (basés sur mouseDX/DY) ET rotate (garde l'angle figé).
					if (in.mouseDX == 0.f && in.mouseDY == 0.f)
						return;
					const int32 op = mGOp;
					int32 mask = mGMask, kind = mGKind;
					// VERROU d'axe (X/Y/Z) : force l'axe unique + chemin par-axe.
					if (in.lockAxis >= 0 && in.lockAxis < 3) {
						mask = (1 << in.lockAxis);
						if (op != 1)
							kind = 0;
					}
					// Etat EFFECTIF : la bascule persistante, INVERSEE par Ctrl (Blender).
					const bool snap = SnapActive(in.ctrlDown);
					// Quantification RELATIVE et grille ABSOLUE ne doivent pas se cumuler :
					// la premiere avance deja par pas, la seconde realignerait ensuite — le
					// deplacement ferait des sauts de deux pas. La grille absolue n'a par
					// ailleurs de sens qu'en repere GLOBAL.
					const bool snapAbs = snap && mSnapAbsolute && (mOrient == ORIENT_GLOBAL);
					const bool snapRel = snap && !snapAbs;
					const float32 mdx = in.mouseDX, mdy = in.mouseDY, mx = in.mouseX, my = in.mouseY;
					const bool localOri = (mOrient != ORIENT_GLOBAL);
					float32 cpx, cpy;
					bool cok = Project(mPivot, cpx, cpy);
					if (op == 0) {
						if (kind == 2) {
							float32 wpp = (2.f * mThY * mPivDist) / mVpH;
							NkVec3f wd = mRgt * (mdx * wpp) + mUp * (-mdy * wpp);
							// Deplacement LIBRE dans le plan ecran : la grille absolue s'y applique
							// sur les trois axes (aucun n'est porteur), la quantification relative
							// n'aurait pas de sens faute d'axe de reference.
							if (snapAbs && mSnapT > 0.f) {
								mDragFree = mDragFree + wd; // position LIBRE cumulee
								const NkVec3f tgt{SnapToGrid(mDragFree.x, mSnapT), SnapToGrid(mDragFree.y, mSnapT),
												  SnapToGrid(mDragFree.z, mSnapT)};
								wd = tgt - mPivot; // amene le pivot PILE sur la case visee
							}
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
									amtT[a] = SnapAmt(r, amtT[a], mSnapT, snapRel);
								}
							for (int32 i = 0; i < mCount; i++)
								if (mSel[i]) {
									NkVec3f B[3], Pi;
									BasisPivot(i, localOri, B, Pi);
									for (int32 a = 0; a < 3; a++)
										if (mask & (1 << a))
											mTr[i] = mTr[i] + B[a] * amtT[a];
								}
							// GRILLE ABSOLUE : on aligne le PIVOT sur des multiples du pas et on
							// reporte la MEME correction sur toute la selection — sinon des objets
							// selectionnes ensemble se desolidariseraient, chacun tombant sur sa
							// propre case. Seuls les axes ACTIFS sont alignes : tirer la fleche X ne
							// doit pas realigner Y et Z au passage.
							if (snapAbs && mSnapT > 0.f) {
								// La position LIBRE avance du deplacement brut ; la case visee en
								// decoule. La correction ramene le pivot dessus. Seuls les axes
								// ACTIFS sont alignes : tirer la fleche X ne doit pas realigner Y
								// et Z au passage.
								mDragFree = mDragFree + NkVec3f{amtT[0], amtT[1], amtT[2]};
								const NkVec3f now{mPivot.x + amtT[0], mPivot.y + amtT[1], mPivot.z + amtT[2]};
								NkVec3f corr{0.f, 0.f, 0.f};
								if (mask & 1)
									corr.x = SnapToGrid(mDragFree.x, mSnapT) - now.x;
								if (mask & 2)
									corr.y = SnapToGrid(mDragFree.y, mSnapT) - now.y;
								if (mask & 4)
									corr.z = SnapToGrid(mDragFree.z, mSnapT) - now.z;
								for (int32 i = 0; i < mCount; i++)
									if (mSel[i])
										mTr[i] = mTr[i] + corr;
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
							a = SnapAmt(mResS.x, a, mSnapS, snapRel);
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
									amt[a] = SnapAmt(r, amt[a], mSnapS, snapRel);
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

				// Pivot RÉEL appliqué à la cible i (rotation / échelle). Séparé de
				// l'ORIENTATION : c'est le MODE DE PIVOT qui décide, exactement comme dans
				// Blender (avant, « Local » impliquait implicitement des origines
				// individuelles — les deux notions sont désormais indépendantes ; pour une
				// sélection UNIQUE le résultat est identique, Ctr(i) == mPivot).
				NkVec3f PivotOf(int32 i) const {
					return (mPivotMode == PIVOT_INDIVIDUAL) ? Ctr(i) : mPivot;
				}

				void BasisPivot(int32 i, bool localOri, NkVec3f B[3], NkVec3f &Pi) const {
					// Orientation NORMAL avec repère fourni (Edit Mode) : le DRAG suit les
					// mêmes axes que ceux DESSINÉS (sinon tirer la flèche Z ne déplacerait
					// pas le long de la normale).
					if (localOri && mOrient == ORIENT_NORMAL && mHasNFrame) {
						B[0] = mNFrameX;
						B[1] = mNFrameY;
						B[2] = mNFrameZ;
					} else if (!localOri) {
						B[0] = {1, 0, 0};
						B[1] = {0, 1, 0};
						B[2] = {0, 0, 1};
					} else {
						const NkMat4f &M = mComposed[i];
						NkVec3f o = M * NkVec3f{0.f, 0.f, 0.f};
						B[0] = Norm((M * NkVec3f{1, 0, 0}) - o);
						B[1] = Norm((M * NkVec3f{0, 1, 0}) - o);
						B[2] = Norm((M * NkVec3f{0, 0, 1}) - o);
					}
					Pi = PivotOf(i);
				}

				// ── État ──────────────────────────────────────────────────────────
				int32 mMode = 0, mOrient = 0;
				// Point de pivot (Blender) + curseur 3D monde. Défaut = MEDIAN (barycentre),
				// comportement historique.
				int32 mPivotMode = PIVOT_MEDIAN;
				NkVec3f mCursor = {0.f, 0.f, 0.f};
				// Repère « Normal » posé par l'éditeur de maillage (SetNormalFrame) :
				// Z = normale de l'élément, X = tangente, Y = Z x X.
				bool mHasNFrame = false;
				NkVec3f mNFrameX = {1.f, 0.f, 0.f}, mNFrameY = {0.f, 1.f, 0.f}, mNFrameZ = {0.f, 0.f, 1.f};
				bool mDragging = false;
				int32 mGOp = 0, mGMask = 0, mGKind = 0;
				// Survol (hover) : poignée sous le curseur hors drag (surbrillance visuelle).
				bool mHovValid = false;
				int32 mHovOp = -1, mHovMask = 0, mHovKind = 0;
				// OBB/AABB : marqueur de boîte du gizmo — désormais OPT-IN (défaut OFF).
				// L'indicateur de sélection PAR DÉFAUT est le liseré silhouette qui épouse
				// le mesh (NkRender3D::SetSelectionOutline, ON par défaut). Réactiver la
				// boîte explicitement via SetDrawObjectBounds(true) si souhaité.
				bool mDrawOBB = false; // marqueur OBB discret, opt-in (SetDrawObjectBounds)
				float32 mLastAngle = 0.f;
				// Snap (quand ctrlDown) : pas + résidus accumulés par drag (quantification).
				float32 mSnapT = 0.5f, mSnapRdeg = 15.f, mSnapS = 0.1f;
				// Bascule persistante (cf. SetSnapEnabled) et mode grille absolue.
				bool mSnapOn = false, mSnapAbsolute = false;
				// Position du pivot SANS aimantation, cumulee depuis le debut du drag.
				// Indispensable a la grille absolue : quantifier une position DEJA
				// quantifiee bloque l'objet sur la premiere case atteinte — chaque frame
				// le petit deplacement de souris se rearrondit sur la meme case et rien
				// n'avance plus. Mesure du defaut : depart 0,3 pas 0,5, drag libre menant
				// a 1,849 ; la grille absolue s'arretait a 0,500 au lieu de 2,000.
				NkVec3f mDragFree = {0.f, 0.f, 0.f};
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
