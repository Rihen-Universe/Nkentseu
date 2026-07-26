// =============================================================================
// Nkentseu/Design/Vector/NkVectorPath.cpp
// =============================================================================
// [AJOUT 2026-07-24] Implémentation MINIMALE de NkVectorPath, limitée au
// sous-ensemble nécessaire à un import SVG basique (NkSVGIO::Import) :
//   - MoveTo / LineTo / Close : push simple d'une commande dans `mCmds`.
//
// Volontairement NON implémenté ici (déclarés dans NkVectorPath.h, aucun
// corps -- stubs honnêtes, PAS appelés par ce chemin donc pas d'erreur de
// lien) : CubicTo/QuadTo/ArcTo (aucun contour SVG source n'en a besoin, le
// codec NkSVGCodec aplatit déjà tout en polylignes), AddRect/AddCircle/
// AddEllipse/AddStar/AddRoundedRect (formes de haut niveau), Transform/
// Translate/Scale/Rotate, Union/Subtract/Intersect/Exclude (opérations
// booléennes), GetBoundingBox/GetLength/PointAtLength/TangentAtLength/
// Contains (métriques), DrawFill/DrawStroke/Draw (tessellation + rendu),
// ToSVGPath/FromSVGPath (sérialisation SVG).
// =============================================================================
#include "NkVectorPath.h"

namespace nkentseu {

	NkVectorPath &NkVectorPath::MoveTo(float32 x, float32 y) noexcept {
		mCmds.PushBack(NkPathCmd::Move(x, y));
		mBoundsDirty = true;
		mLengthDirty = true;
		return *this;
	}

	NkVectorPath &NkVectorPath::LineTo(float32 x, float32 y) noexcept {
		mCmds.PushBack(NkPathCmd::Line(x, y));
		mBoundsDirty = true;
		mLengthDirty = true;
		return *this;
	}

	NkVectorPath &NkVectorPath::Close() noexcept {
		mCmds.PushBack(NkPathCmd::CloseCmd());
		return *this;
	}

} // namespace nkentseu
