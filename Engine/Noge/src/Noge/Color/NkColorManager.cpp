// =============================================================================
// Nkentseu/Color/NkColorManager.cpp
// =============================================================================
// [FUSION 2026-07-25] Ce fichier implémentait SRGBToLinear/LinearToSRGB/
// FromSRGB pour l'ancienne classe dupliquée `nkentseu::NkColor` (supprimée de
// NkColorManager.h -- voir le commentaire [FUSION 2026-07-25] en tête de ce
// header). Ces fonctions sont désormais fournies par `nkentseu::math::NkColorF`
// (NKMath/NkColor.h : FromSRGB, SRGBToLinearChannel/LinearToSRGBChannel
// privées + ToLinearRGB/FromLinearRGB publiques), implémentées dans
// Kernel/Foundation/NKMath/src/NKMath/NkColor.cpp.
//
// Plus rien à implémenter ici pour le chemin d'import SVG minimal
// (NkSVGIO::Import) : `NkGuide`/`NkGrid`/`NkArtboard` utilisent maintenant
// `math::NkColorF::FromSRGB(...)`, une fonction FORCE_INLINE définie dans le
// header NKMath -- aucune dépendance de lien vers ce .cpp.
//
// Toujours NON implémenté (déclaré dans NkColorManager.h, sans corps -- stubs
// honnêtes, non appelés par le chemin d'import SVG donc pas d'erreur de
// lien) : NkPalette::Find/Get/Material/Tailwind/Pastels/Monochrome/WebSafe/
// SaveToFile/LoadFromFile, NkHarmony::*, NkColorPicker::SetColor/SyncFromHSV/
// SyncFromRGB/SyncFromHex, NkColorManager::PushHistory/SampleScreen -- la
// gestion colorimétrique complète (palettes/pipette/historique) reste hors
// scope de cet incrément (dédié à l'import SVG).
// =============================================================================
#include "NkColorManager.h"

namespace nkentseu {
	// Intentionnellement vide : voir commentaire d'en-tête ci-dessus.
} // namespace nkentseu
