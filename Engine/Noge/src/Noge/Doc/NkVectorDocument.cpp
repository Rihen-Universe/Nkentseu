// =============================================================================
// Nkentseu/Doc/NkVectorDocument.cpp
// =============================================================================
// [AJOUT 2026-07-24] Implémentation MINIMALE de la hiérarchie
// NkVectorDocument -> NkArtboard -> NkVectorLayer -> NkVectorObject, limitée
// au sous-ensemble nécessaire à un import SVG basique (NkSVGIO::Import) :
//   - NkVectorDocument::AddArtboard
//   - NkArtboard::AddLayer
//   - NkVectorLayer::AddPath
//   - NkVectorDocument::~NkVectorDocument (déclaré dans le header sans corps
//     -- nécessaire pour que le type soit destructible ; symétrique de
//     NkArtboard::~NkArtboard / NkVectorLayer::~NkVectorLayer, déjà définis
//     inline dans NkVectorDocument.h)
//
// Convention d'allocation : `new`/`delete` bruts (PAS NKMemory::Create/
// Destroy). Choix délibéré, PAS un oubli : NkArtboard::~NkArtboard() et
// NkVectorLayer::~NkVectorLayer() (déjà écrits, inline dans le .h, hors
// scope de cet incrément) font `delete` brut sur leurs enfants. Allouer via
// NKMemory::Create<T>() ici créerait un mismatch allocateur/désallocateur
// (Create() enregistre l'allocation via NkAllocator + tracking ; un `delete`
// brut en face ne passe pas par ce chemin) -> corruption tas. Pour rester
// symétrique avec le code existant non modifiable dans ce scope, on utilise
// `new`/`delete` bruts partout dans ce fichier.
//
// Volontairement NON implémenté ici (déclarés dans NkVectorDocument.h,
// aucun corps -- stubs honnêtes, PAS appelés par le chemin d'import donc pas
// d'erreur de lien) : DeleteArtboard, AddSymbol/FindSymbol, SaveToFile/
// LoadFromFile/ExportSVG/ExportPDF/ExportPNG, Copy/Cut/Paste/Duplicate
// (presse-papier), DeleteLayer/MoveLayer/ActiveLayer/Select*/HitTest/Draw
// (édition + rendu), NkVectorObject::GetBoundingBox/GetTransformMatrix/
// Contains/Intersects/Draw.
// =============================================================================
#include "Noge/Doc/NkVectorDocument.h"

namespace nkentseu {

	// ── NkVectorDocument ────────────────────────────────────────────────────

	NkArtboard &NkVectorDocument::AddArtboard(const char *name, float32 w, float32 h) noexcept {
		NkArtboard *artboard = new NkArtboard();
		artboard->name = name;
		artboard->width = w;
		artboard->height = h;
		artboards.PushBack(artboard);
		return *artboards.Back();
	}

	NkVectorDocument::~NkVectorDocument() noexcept {
		for (auto *artboard : artboards)
			delete artboard;
		for (auto *symbol : symbols)
			delete symbol;
		for (auto *obj : clipboard)
			delete obj;
	}

	// ── NkArtboard ──────────────────────────────────────────────────────────

	NkVectorLayer &NkArtboard::AddLayer(const char *name, uint32 parentId) noexcept {
		NkVectorLayer *layer = new NkVectorLayer();
		layer->name = name;
		layer->parentLayerId = parentId;
		layers.PushBack(layer);
		return *layers.Back();
	}

	// ── NkVectorLayer ───────────────────────────────────────────────────────

	NkVectorObject &NkVectorLayer::AddPath(const NkVectorPath &path, const NkPaint &fill, const char *name) noexcept {
		NkVectorObject *obj = new NkVectorObject();
		obj->type = NkVectorObjectType::Path;
		obj->name = name;
		obj->path = path;
		obj->fill = fill;
		objects.PushBack(obj);
		return *objects.Back();
	}

} // namespace nkentseu
