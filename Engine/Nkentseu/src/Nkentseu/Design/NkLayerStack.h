#pragma once
// =============================================================================
// Nkentseu/Design/NkLayerStack.h
// =============================================================================
// Pile de couches avec compositing GPU pour les applications de design.
//
// TYPES DE COUCHES :
//   Raster    — NkRasterCanvas (peinture numérique)
//   Vector    — NkVectorPath(s) (illustration vectorielle)
//   Text      — texte éditable avec style
//   Adjustment — filtre non-destructif (courbes, teinte, flou...)
//   Group     — groupe de couches avec pass-through optionnel
//
// BLEND MODES (24 modes) :
//   Normal, Dissolve, Darken, Multiply, ColorBurn, LinearBurn,
//   Lighten, Screen, ColorDodge, LinearDodge, Overlay,
//   SoftLight, HardLight, VividLight, LinearLight, PinLight,
//   Difference, Exclusion, Hue, Saturation, Color, Luminosity, Erase
//
// COMPOSITING :
//   Chaque couche est rendue dans un NkOffscreenTarget individuel.
//   Le compositing combine les couches de bas en haut via les blend modes.
//   Résultat final rendu via NkRender2D::DrawImage.
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Design/Raster/NkRasterCanvas.h"
#include "Design/Vector/NkVectorPath.h"

namespace nkentseu {
    using namespace math;

    // =========================================================================
    // NkBlendMode — 24 modes de fusion
    // =========================================================================
    enum class NkBlendMode : uint8 {
        // ── Groupe Normal ──────────────────────────────────────────────────
        Normal = 0,
        Dissolve,

        // ── Groupe Assombrir ──────────────────────────────────────────────
        Darken,
        Multiply,
        ColorBurn,
        LinearBurn,
        DarkerColor,

        // ── Groupe Éclaircir ──────────────────────────────────────────────
        Lighten,
        Screen,
        ColorDodge,
        LinearDodge,   ///< Add
        LighterColor,

        // ── Groupe Contraste ──────────────────────────────────────────────
        Overlay,
        SoftLight,
        HardLight,
        VividLight,
        LinearLight,
        PinLight,
        HardMix,

        // ── Groupe Inversion ──────────────────────────────────────────────
        Difference,
        Exclusion,
        Subtract,
        Divide,

        // ── Groupe Composantes HSL ────────────────────────────────────────
        Hue,
        Saturation,
        Color,
        Luminosity,

        // ── Spéciaux ─────────────────────────────────────────────────────
        Erase,     ///< Soustrait de l'alpha

        COUNT
    };

    // =========================================================================
    // NkAdjustmentType — types de filtres d'ajustement non-destructifs
    // =========================================================================
    enum class NkAdjustmentType : uint8 {
        BrightnessContrast,
        LevelsRGB,
        CurvesRGB,
        HueSaturation,
        ColorBalance,
        GradientMap,
        Exposure,
        Vibrance,
        Posterize,
        Threshold,
        Invert,
        GaussianBlur,
        Sharpen,
    };

    // =========================================================================
    // NkLayerAdjustment — filtre non-destructif
    // =========================================================================
    struct NkLayerAdjustment {
        NkAdjustmentType type = NkAdjustmentType::BrightnessContrast;

        // BrightnessContrast
        float32 brightness = 0.f;    ///< [-1..1]
        float32 contrast   = 0.f;    ///< [-1..1]

        // HueSaturation
        float32 hue        = 0.f;    ///< [-180..180]
        float32 saturation = 0.f;    ///< [-1..1]
        float32 lightness  = 0.f;    ///< [-1..1]

        // Curves (5 points de contrôle par canal)
        static constexpr uint32 kCurvePoints = 5u;
        NkVec2f curveR[kCurvePoints] = {{0,0},{0.25f,0.25f},{0.5f,0.5f},{0.75f,0.75f},{1,1}};
        NkVec2f curveG[kCurvePoints] = {{0,0},{0.25f,0.25f},{0.5f,0.5f},{0.75f,0.75f},{1,1}};
        NkVec2f curveB[kCurvePoints] = {{0,0},{0.25f,0.25f},{0.5f,0.5f},{0.75f,0.75f},{1,1}};

        // GaussianBlur
        float32 blurRadius = 2.f;    ///< Rayon en pixels
    };

    // =========================================================================
    // NkLayer — une couche dans la pile
    // =========================================================================
    struct NkLayer {
        // ── Identité ─────────────────────────────────────────────────────
        static constexpr uint32 kMaxName = 128u;
        char      name[kMaxName] = {};
        uint32    id             = 0;    ///< ID unique (auto-incrémenté)

        // ── Type de couche ────────────────────────────────────────────────
        enum class Type : uint8 {
            Raster, Vector, Text, Adjustment, Group, Reference
        };
        Type type = Type::Raster;

        // ── Compositing ───────────────────────────────────────────────────
        NkBlendMode blendMode   = NkBlendMode::Normal;
        float32     opacity     = 1.f;    ///< [0..1]
        bool        visible     = true;
        bool        locked      = false;  ///< Verrou d'édition
        bool        alphaSelf   = false;  ///< "Preserve transparency" Photoshop
        bool        clipped     = false;  ///< Clip par la couche dessous (calque écrêté)

        // ── Données selon le type ─────────────────────────────────────────
        NkRasterCanvas*  rasterCanvas = nullptr;  ///< Type::Raster (owned)

        struct VectorData {
            NkVector<NkVectorPath> paths;
            NkPaint  fill;
            NkPaint  stroke;
            NkStrokeStyle strokeStyle;
        };
        VectorData*      vectorData   = nullptr;  ///< Type::Vector (owned)

        NkLayerAdjustment* adjustment = nullptr;  ///< Type::Adjustment (owned)

        // ── Groupe ────────────────────────────────────────────────────────
        bool         isGroup      = false;
        bool         passThrough  = false;  ///< Groupe pass-through (ignore blend)
        bool         collapsed    = false;  ///< UI : groupe replié
        uint32       parentId     = 0;      ///< 0 = racine

        // ── Mask ──────────────────────────────────────────────────────────
        NkRasterCanvas* maskCanvas  = nullptr;  ///< Masque de couche (niveau de gris)
        bool            maskEnabled = false;
        bool            maskInvert  = false;

        // ── GPU ───────────────────────────────────────────────────────────
        nk_uint64    offscreenRT   = 0;  ///< NkOffscreenTarget handle
        bool         dirty         = true;  ///< Doit être re-rendu

        // ── Constructeur/Destructeur ──────────────────────────────────────
        NkLayer() noexcept = default;
        ~NkLayer() noexcept;
        NkLayer(const NkLayer&) = delete;
        NkLayer& operator=(const NkLayer&) = delete;
    };

    // =========================================================================
    // NkLayerStack — pile de couches avec compositing
    // =========================================================================
    class NkLayerStack {
        public:
            NkLayerStack()  noexcept = default;
            ~NkLayerStack() noexcept = default;

            NkLayerStack(const NkLayerStack&) = delete;
            NkLayerStack& operator=(const NkLayerStack&) = delete;

            // ── Création de couches ───────────────────────────────────────
            /**
             * @brief Ajoute une couche raster au-dessus de la sélection courante.
             */
            NkLayer* AddRasterLayer(const char* name = "Layer") noexcept;

            /**
             * @brief Ajoute une couche vectorielle.
             */
            NkLayer* AddVectorLayer(const char* name = "Vector") noexcept;

            /**
             * @brief Ajoute un calque d'ajustement.
             */
            NkLayer* AddAdjustmentLayer(NkAdjustmentType type,
                                         const char* name = nullptr) noexcept;

            /**
             * @brief Crée un groupe de couches.
             */
            NkLayer* AddGroup(const char* name = "Group") noexcept;

            // ── Gestion ──────────────────────────────────────────────────
            void DeleteLayer(uint32 layerId) noexcept;
            void MoveLayer  (uint32 layerId, uint32 toIndex) noexcept;
            void DuplicateLayer(uint32 layerId) noexcept;
            void MergeDown  (uint32 layerId) noexcept;   ///< Fusionne avec dessous
            void FlattenAll () noexcept;                  ///< Tout en une seule couche

            // ── Sélection ────────────────────────────────────────────────
            void SelectLayer(uint32 layerId) noexcept;
            [[nodiscard]] NkLayer*       GetActiveLayer()  noexcept;
            [[nodiscard]] const NkLayer* GetActiveLayer()  const noexcept;
            [[nodiscard]] NkLayer*       FindLayer(uint32 id) noexcept;
            [[nodiscard]] uint32         LayerCount() const noexcept;

            // ── Rendu / Compositing ──────────────────────────────────────
            /**
             * @brief Composite toutes les couches et rendu dans r2d.
             * @param r2d  Renderer 2D.
             * @param cmd  Command buffer.
             * @param dst  Rectangle destination écran.
             */
            void Composite(renderer::NkRender2D& r2d,
                            NkICommandBuffer* cmd,
                            const NkRectF& dst) noexcept;

            /**
             * @brief Marque une couche comme dirty (recalcul du composite).
             */
            void MarkDirty(uint32 layerId) noexcept;
            void MarkAllDirty() noexcept;

            // ── Accès itérable (pour l'UI) ────────────────────────────────
            [[nodiscard]] NkLayer* GetLayer(uint32 index) noexcept;
            [[nodiscard]] const NkLayer* GetLayer(uint32 index) const noexcept;

        private:
            void CompositeGroup(NkLayer* group,
                                 renderer::NkRender2D& r2d,
                                 NkICommandBuffer* cmd) noexcept;

            NkVector<NkLayer*> mLayers;     ///< Ordre bottom-to-top
            uint32             mNextId      = 1;
            uint32             mActiveLayer = 0;
    };

} // namespace nkentseu
