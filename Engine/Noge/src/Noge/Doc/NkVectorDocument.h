#pragma once
// =============================================================================
// Nkentseu/Design/Vector/NkVectorDocument.h
// =============================================================================
// Document vectoriel multi-artboards — style Illustrator/Affinity Designer.
//
// STRUCTURE :
//   NkVectorDocument
//     └── NkArtboard (1..N)
//           └── NkVectorLayer (pile)
//                 └── NkVectorObject (formes, textes, images)
//
// FONCTIONNALITÉS :
//   • Multi-artboards dans un même document
//   • Calques vectoriels avec blend mode et opacité
//   • Groupes, sous-groupes
//   • Masques d'écrêtage (clip masks)
//   • Masques d'opacité
//   • Guides et grilles configurables
//   • Symboles (instances réutilisables)
//   • Sérialisation JSON + SVG
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
// [FIX 2026-07-24] Les 3 includes ci-dessous étaient cassés (chemins jamais
// résolus par le compilateur -- includedirs Noge.jenga = "src", racine réelle
// des fichiers = "src/Noge/...") :
//   - "NkVectorPath.h" (relatif, mais le fichier n'est pas dans Doc/) ->
//     "Noge/Design/Vector/NkVectorPath.h"
//   - "Nkentseu/Design/NkLayerStack.h" (préfixe "Nkentseu/" inexistant) ->
//     "Noge/Design/NkLayerStack.h"
//   - "Nkentseu/Design/Color/NkColorManager.h" (double erreur : préfixe +
//     "Design/Color/" alors que le fichier réel est sous "Noge/Color/") ->
//     "Noge/Color/NkColorManager.h"
#include "Noge/Design/Vector/NkVectorPath.h"
#include "Noge/Design/NkLayerStack.h"
#include "Noge/Color/NkColorManager.h"
// [FIX 2026-07-24] Include manquant : `undoStack` (membre par valeur plus
// bas) est de type NkUndoStack, jamais inclus nulle part dans ce fichier ni
// dans sa chaîne d'includes -> type incomplet à la compilation sans cette
// ligne.
#include "Noge/Modeling/NkUndoStack.h"

namespace nkentseu {
	// [FIX 2026-07-24] `using namespace math;` remplacé par des `using`
	// ciblés (voir le commentaire complet dans Noge/Color/NkColorManager.h) :
	// un using-directive en portée namespace fuit vers le reste de la unité
	// de compilation, y compris VERS L'ARRIÈRE dans les fichiers inclus après
	// lui -- c'est précisément ce qui rendait `NkColor` ambigu à l'intérieur
	// même de Noge/Color/NkColorManager.h (inclus juste au-dessus) tant que
	// NkVectorPath.h/NkLayerStack.h faisaient encore `using namespace math;`
	// avant lui dans la même unité de compilation. `NkColor` lui-même N'EST
	// PAS importé ici : toutes ses utilisations dans ce fichier (NkGuide,
	// NkGrid, NkArtboard::background) sont qualifiées explicitement en
	// `nkentseu::NkColor`.
	using math::NkVec2f;
	using math::NkMat3f;
	using math::NkAABB2f;

	// =========================================================================
	// NkVectorObject — objet vectoriel (chemin, texte, image)
	// =========================================================================
	enum class NkVectorObjectType : uint8 {
		Path,	///< Chemin vectoriel (NkVectorPath)
		Text,	///< Texte (NkVectorText)
		Image,	///< Image raster embarquée
		Group,	///< Groupe d'objets
		Symbol, ///< Instance d'un symbole
	};

	struct NkVectorObject {
			NkVectorObjectType type = NkVectorObjectType::Path;
			NkString name;
			bool visible = true;
			bool locked = false;
			bool selected = false;

			// ── Transform ─────────────────────────────────────────────────────
			NkVec2f position = {};
			float32 rotation = 0.f; ///< Degrés
			NkVec2f scale = {1, 1};
			NkVec2f pivot = {0, 0}; ///< Point de rotation en unités locales

			// ── Apparence ─────────────────────────────────────────────────────
			NkPaint fill;
			NkPaint stroke;
			NkStrokeStyle strokeStyle;
			NkBlendMode blendMode = NkBlendMode::Normal;
			float32 opacity = 1.f;

			// ── Path (si type==Path) ──────────────────────────────────────────
			NkVectorPath path;
			NkFillRule fillRule = NkFillRule::NonZero;

			// ── Image (si type==Image) ────────────────────────────────────────
			nk_uint64 imageHandle = 0;
			NkVec2f imageSize = {};
			bool imagePreserveAspect = true;

			// ── Groupe (si type==Group) ───────────────────────────────────────
			NkVector<NkVectorObject *> children; ///< Enfants (non-owning, gérés par layer)

			// ── Masques ───────────────────────────────────────────────────────
			NkVectorPath *clipMask = nullptr; ///< Masque d'écrêtage vectoriel
			bool hasClipMask = false;

			// ── Symbole ───────────────────────────────────────────────────────
			uint32 symbolId = 0;
			NkVec2f symbolSize = {};

			// ── Bounding box (calculée) ───────────────────────────────────────
			[[nodiscard]] NkAABB2f GetBoundingBox() const noexcept;

			// ── Transform helpers ─────────────────────────────────────────────
			[[nodiscard]] NkMat3f GetTransformMatrix() const noexcept;

			void TranslateTo(NkVec2f pos) noexcept {
				position = pos;
			}

			void RotateTo(float32 deg) noexcept {
				rotation = deg;
			}

			void ScaleTo(NkVec2f s) noexcept {
				scale = s;
			}

			void SetPivot(NkVec2f p) noexcept {
				pivot = p;
			}

			// ── Hit testing ───────────────────────────────────────────────────
			[[nodiscard]] bool Contains(NkVec2f pt) const noexcept;
			[[nodiscard]] bool Intersects(const NkAABB2f &rect) const noexcept;

			// ── Rendu ─────────────────────────────────────────────────────────
			void Draw(renderer::NkRender2D &r2d, float32 parentOpacity = 1.f) const noexcept;
	};

	// =========================================================================
	// NkVectorLayer — calque vectoriel dans un artboard
	// =========================================================================
	struct NkVectorLayer {
			NkString name;
			bool visible = true;
			bool locked = false;
			NkBlendMode blendMode = NkBlendMode::Normal;
			float32 opacity = 1.f;
			bool expanded = true; ///< UI : calque déplié

			NkVector<NkVectorObject *> objects; ///< Owned
			uint32 parentLayerId = 0;			///< 0 = racine

			~NkVectorLayer() noexcept {
				for (auto *obj : objects)
					delete obj;
			}

			NkVectorObject &AddPath(const NkVectorPath &path, const NkPaint &fill, const char *name = "Path") noexcept;

			NkVectorObject &AddGroup(const char *name = "Group") noexcept;

			void Remove(NkVectorObject *obj) noexcept;
			void MoveUp(NkVectorObject *obj) noexcept;
			void MoveDown(NkVectorObject *obj) noexcept;

			void Draw(renderer::NkRender2D &r2d) const noexcept;
	};

	// =========================================================================
	// NkGuide — guide de référence
	// =========================================================================
	struct NkGuide {
			enum class Orientation : uint8 { Horizontal, Vertical };
			Orientation orientation = Orientation::Horizontal;
			float32 position = 0.f; ///< Position en unités document
			// [FIX 2026-07-24] `NkColor` qualifié explicitement : ambigu sinon
			// entre `nkentseu::NkColor` (Color/NkColorManager.h, inclus plus
			// haut) et `nkentseu::math::NkColor` (NKMath/NkColor.h, visible via
			// `using namespace math;` ligne ~48 de ce fichier) -- seul
			// `nkentseu::NkColor` a `FromSRGB()`, mais la résolution du NOM
			// `NkColor` lui-même échoue (ambigu) avant même de regarder ses
			// membres, d'où la qualification explicite.
			nkentseu::NkColor color = nkentseu::NkColor::FromSRGB(0.3f, 0.6f, 1.f);
			bool locked = false;
			bool visible = true;
	};

	// =========================================================================
	// NkGrid — grille de référence
	// =========================================================================
	struct NkGrid {
			enum class Type : uint8 { Lines, Dots, Isometric };
			Type type = Type::Lines;
			float32 spacing = 10.f; ///< Espacement en unités document
			uint32 divisions = 4;	///< Subdivisions de la grille principale
			// [FIX 2026-07-24] Qualification explicite -- même raison que NkGuide::color ci-dessus.
			nkentseu::NkColor color = nkentseu::NkColor::FromSRGB(0.7f, 0.7f, 0.9f, 0.5f);
			nkentseu::NkColor subColor = nkentseu::NkColor::FromSRGB(0.8f, 0.8f, 0.95f, 0.3f);
			bool visible = true;
			bool snapEnabled = true;
			float32 snapRadius = 8.f; ///< Rayon de snap en pixels écran
	};

	// =========================================================================
	// NkSymbol — composant réutilisable (style Illustrator/Sketch)
	// =========================================================================
	struct NkSymbol {
			uint32 id = 0;
			NkString name;
			NkAABB2f bounds;

			NkVector<NkVectorObject *> objects; ///< Owned

			~NkSymbol() noexcept {
				for (auto *obj : objects)
					delete obj;
			}
	};

	// =========================================================================
	// NkArtboard — page / artboard dans le document
	// =========================================================================
	class NkArtboard {
		public:
			NkString name;
			float32 width = 1920.f;
			float32 height = 1080.f;
			// [FIX 2026-07-24] Qualification explicite -- même raison que NkGuide::color plus haut.
			nkentseu::NkColor background = nkentseu::NkColor::White();
			bool clipContent = true; ///< Écrête le contenu aux bords

			// ── Calques ──────────────────────────────────────────────────────
			NkVector<NkVectorLayer *> layers; ///< Owned (bottom → top)

			NkVectorLayer &AddLayer(const char *name = "Layer", uint32 parentId = 0) noexcept;
			void DeleteLayer(uint32 idx) noexcept;
			void MoveLayer(uint32 from, uint32 to) noexcept;

			[[nodiscard]] NkVectorLayer *ActiveLayer() noexcept;

			void SetActiveLayer(uint32 idx) noexcept {
				mActiveLayer = idx;
			}

			// ── Guides ────────────────────────────────────────────────────────
			NkVector<NkGuide> guides;
			NkGrid grid;

			void AddGuide(NkGuide::Orientation ori, float32 pos) noexcept {
				guides.PushBack({ori, pos});
			}

			// ── Sélection ─────────────────────────────────────────────────────
			NkVector<NkVectorObject *> selection; ///< Non-owning

			void Select(NkVectorObject *obj, bool addToSel = false) noexcept;
			void Deselect(NkVectorObject *obj) noexcept;
			void SelectAll() noexcept;
			void DeselectAll() noexcept;
			void SelectInRect(const NkAABB2f &rect) noexcept;

			// ── Hit test ──────────────────────────────────────────────────────
			[[nodiscard]] NkVectorObject *HitTest(NkVec2f pt) const noexcept;

			// ── Rendu ─────────────────────────────────────────────────────────
			void Draw(renderer::NkRender2D &r2d, const NkAABB2f &viewport, float32 zoom = 1.f) const noexcept;

			~NkArtboard() noexcept {
				for (auto *l : layers)
					delete l;
			}

		private:
			uint32 mActiveLayer = 0;
	};

	// =========================================================================
	// NkVectorDocument — document vectoriel complet
	// =========================================================================
	class NkVectorDocument {
		public:
			NkString name;
			NkString filePath;
			float32 unitsPerInch = 96.f; ///< PPI (96 pour web, 300 pour print)

			enum class Units : uint8 { Pixels, Millimeters, Centimeters, Inches, Points };
			Units units = Units::Pixels;

			// ── Artboards ────────────────────────────────────────────────────
			NkVector<NkArtboard *> artboards; ///< Owned
			uint32 activeArtboard = 0;

			NkArtboard &AddArtboard(const char *name, float32 w = 1920, float32 h = 1080) noexcept;
			void DeleteArtboard(uint32 idx) noexcept;

			[[nodiscard]] NkArtboard *GetActive() noexcept {
				return activeArtboard < artboards.Size() ? artboards[activeArtboard] : nullptr;
			}

			// ── Symboles ─────────────────────────────────────────────────────
			NkVector<NkSymbol *> symbols;

			NkSymbol &AddSymbol(const char *name) noexcept;
			[[nodiscard]] NkSymbol *FindSymbol(uint32 id) const noexcept;

			// ── Palettes de couleurs du document ─────────────────────────────
			NkVector<NkPalette> colorPalettes;

			// ── Historique undo/redo ──────────────────────────────────────────
			NkUndoStack undoStack{100};

			// ── Métadonnées ───────────────────────────────────────────────────
			NkString author;
			NkString description;
			NkString version = "1.0";

			// ── Sérialisation ─────────────────────────────────────────────────
			[[nodiscard]] bool SaveToFile(const char *path) const noexcept;
			[[nodiscard]] bool LoadFromFile(const char *path) noexcept;
			[[nodiscard]] bool ExportSVG(const char *path, uint32 artboardIdx = 0) const noexcept;
			[[nodiscard]] bool ExportPDF(const char *path) const noexcept;
			[[nodiscard]] bool ExportPNG(const char *path, uint32 artboardIdx = 0, float32 scale = 1.f) const noexcept;

			// ── Presse-papier interne ──────────────────────────────────────────
			NkVector<NkVectorObject *> clipboard; ///< Owned

			void Copy() noexcept;
			void Cut() noexcept;
			void Paste() noexcept;
			void Duplicate() noexcept;

			~NkVectorDocument() noexcept;
	};

} // namespace nkentseu
