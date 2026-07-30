//
// NkPdfRaster.h — rasterisation de traces, anti-aliasee.
//
// PIECE CENTRALE du lecteur PDF : elle sert AUX DEUX usages, et c'est
// deliberé. Les graphiques vectoriels d'une page (`re`, `m`, `l`, `c`, `f`,
// `S`...) et le TEXTE sont le meme probleme : NKFont rend les glyphes sous
// forme de contours (NkGetGlyphShape -> segments, beziers quadratiques et
// cubiques), exactement ce que decrit un trace PDF. Un seul rastériseur, donc,
// plutot qu'un chemin pour les formes et un autre pour les caracteres.
//
// CHOIX : on rasterise vers un BITMAP RGBA plutot que de traduire en
// primitives NKGui. C'est ce que fait tout lecteur PDF, et pour une bonne
// raison : le decoupage (clipping) et les transformations arbitraires du
// format se pretent mal a une pile d'interface immediate. La page devient une
// texture, affichee comme le fait deja la visionneuse video.
//
// Les coordonnees sont en PIXELS DE SORTIE : l'interpreteur de flux applique
// la matrice courante AVANT d'ajouter les points. Le rastériseur ignore donc
// tout des transformations PDF.
//
#pragma once

#include "NKContainers/Sequential/NkVector.h"
#include "NKCore/NkTypes.h"

namespace nkentseu {
	namespace nkcode {
		namespace pdf {

			struct NkPdfPt {
					double x = 0.0, y = 0.0;
			};

			// Trace : suite de sous-traces, chacun une polyligne FERMEE pour le
			// remplissage. Les courbes sont aplaties a l'ajout (voir CurveTo) : le
			// rastériseur ne manipule que des segments.
			class NkPdfPath {
				public:
					void Clear() {
						mPts.Clear();
						mStarts.Clear();
						mHasCur = false;
					}
					bool Empty() const { return mStarts.Empty(); }

					void MoveTo(double x, double y);
					void LineTo(double x, double y);
					// Bezier CUBIQUE (operateur `c` du PDF, et contours CFF).
					void CurveTo(double x1, double y1, double x2, double y2, double x3, double y3);
					// Bezier QUADRATIQUE (contours TrueType).
					void QuadTo(double cx, double cy, double x, double y);
					void Close();

					// Rectangle (operateur `re`) : raccourci frequent.
					void Rect(double x, double y, double w, double h);

					const NkVector<NkPdfPt> &Points() const { return mPts; }
					const NkVector<int32> &Starts() const { return mStarts; }

					// Boite englobante ; false si le trace est vide.
					bool Bounds(double *x0, double *y0, double *x1, double *y1) const;

					// Tolerance d'aplatissement, en pixels. Plus petit = plus de
					// segments. 0,25 px est le compromis usuel : invisible a l'oeil,
					// sans faire exploser le nombre de segments sur une page chargee.
					static double FlatTolerance() { return 0.25; }

				private:
					void Flatten(double x0, double y0, double x1, double y1, double x2, double y2, double x3,
								 double y3, int32 depth);

					NkVector<NkPdfPt> mPts;
					NkVector<int32> mStarts; // index de debut de chaque sous-trace
					bool mHasCur = false;
					double mCurX = 0.0, mCurY = 0.0;	 // point courant
					double mStartX = 0.0, mStartY = 0.0; // debut du sous-trace (pour Close)
			};

			// Cible de rendu : RGBA 8 bits par canal, non premultipliee.
			class NkPdfCanvas {
				public:
					bool Create(int32 w, int32 h);
					void Destroy();
					bool Valid() const { return mW > 0 && mH > 0 && !mPix.Empty(); }

					int32 Width() const { return mW; }
					int32 Height() const { return mH; }
					const uint8 *Pixels() const { return mPix.Data(); }
					uint8 *Pixels() { return mPix.Data(); }

					void Clear(uint8 r, uint8 g, uint8 b, uint8 a);

					// Remplit `path`. `evenOdd` choisit la regle de remplissage :
					// false = non-nul (operateur `f`), true = pair-impair (`f*`).
					// La couleur est melangee avec le fond selon `a` ET la couverture
					// calculee : c'est l'anti-aliasing.
					void FillPath(const NkPdfPath &path, bool evenOdd, uint8 r, uint8 g, uint8 b, uint8 a);

					// Trace le contour de `path` avec une epaisseur donnee (operateur
					// `S`). Implemente en CONSTRUISANT le trace du contour puis en le
					// remplissant : un seul moteur de rasterisation a maintenir.
					void StrokePath(const NkPdfPath &path, double width, uint8 r, uint8 g, uint8 b, uint8 a);

					// ── Decoupage (operateurs W / W*) ──
					// Le masque est une couverture 8 bits par pixel. Absent = pas de
					// decoupage. Intersecter plutot que remplacer : les `q`/`Q` du PDF
					// empilent des decoupages successifs.
					void SetClipFromPath(const NkPdfPath &path, bool evenOdd);
					void ClearClip();
					bool HasClip() const { return !mClip.Empty(); }
					// Sauvegarde/restauration, pour l'empilement q/Q de l'interpreteur.
					NkVector<uint8> TakeClip() { return mClip; }
					void RestoreClip(const NkVector<uint8> &c) { mClip = c; }

				private:
					// Calcule la couverture de `path` dans `cov` (mW*mH octets).
					void Rasterize(const NkPdfPath &path, bool evenOdd, NkVector<uint8> &cov) const;

					int32 mW = 0, mH = 0;
					NkVector<uint8> mPix;  // RGBA, mW*mH*4
					NkVector<uint8> mClip; // couverture de decoupage, mW*mH (vide = aucun)
			};

		} // namespace pdf
	} // namespace nkcode
} // namespace nkentseu
