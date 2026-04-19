#pragma once
// =============================================================================
// Nkentseu/Design/Hybrid/NkHybridDocument.h
// =============================================================================
// Document hybride vectoriel + raster (style Procreate / Clip Studio Paint).
//
// CONCEPT :
//   Un NkHybridDocument contient une pile de couches mixtes.
//   Chaque couche peut être vectorielle OU raster OU ajustement.
//   Le compositing fusionne tout en une image finale.
//   C'est la base pour les applications d'illustration.
//
// FONCTIONNALITÉS SPÉCIFIQUES :
//   • NkSymmetryTool — symétrie radiale, miroir, kaleidoscope
//   • NkPerspectiveGuide — guides de perspective (1/2/3 points de fuite)
//   • NkStabilizer — stabilisation du trait (Lazy Nezumi-style)
//   • NkAnimationAssist — animation image-par-image avec onion skin
//   • NkTimelapseRecorder — enregistrement du processus de création
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Nkentseu/Design/NkLayerStack.h"
#include "Nkentseu/Design/Raster/NkRasterCanvas.h"
#include "Nkentseu/Design/Vector/NkVectorPath.h"
#include "Nkentseu/Design/Color/NkColorManager.h"

namespace nkentseu {
    using namespace math;

    // =========================================================================
    // NkSymmetryTool — dessin symétrique
    // =========================================================================
    class NkSymmetryTool {
    public:
        enum class Mode : uint8 {
            None,
            MirrorH,         ///< Miroir horizontal (axe vertical)
            MirrorV,         ///< Miroir vertical (axe horizontal)
            MirrorBoth,      ///< Les deux axes
            Radial,          ///< N secteurs radiaux autour d'un centre
            Kaleidoscope,    ///< Radial + miroir dans chaque secteur
            Mandala,         ///< Radial + miroir + symétrie de rotation
            Point,           ///< Symétrie de point (rotation de 180°)
        };

        Mode    mode         = Mode::None;
        NkVec2f center       = {0.f, 0.f};  ///< Centre de symétrie (espace canvas)
        uint32  radialCount  = 6;            ///< Nb de secteurs (mode Radial/Mandala)
        float32 axisAngle    = 0.f;          ///< Angle de l'axe (degrés)
        bool    showGuide    = true;         ///< Afficher les axes de symétrie

        /**
         * @brief Retourne toutes les positions symétriques d'un point.
         * @param pos   Position originale dans le canvas.
         * @param out   Points miroirs à peindre aussi.
         */
        void GetMirrorPoints(NkVec2f pos,
                              NkVector<NkVec2f>& out) const noexcept;

        /**
         * @brief Même chose pour un NkBrushDab (avec angles miroirs).
         */
        void GetMirrorDabs(const NkBrushDab& dab, NkVec2f center,
                            NkVector<std::pair<NkBrushDab, NkVec2f>>& out) const noexcept;

        /**
         * @brief Dessine les guides de symétrie via NkRender2D.
         */
        void DrawGuides(renderer::NkRender2D& r2d,
                         const NkRectF& canvasRect,
                         float32 zoom) const noexcept;
    };

    // =========================================================================
    // NkPerspectiveGuide — guides de perspective
    // =========================================================================
    struct NkVanishingPoint {
        NkVec2f  position;
        NkColor  color      = NkColor::FromSRGB(0.3f, 0.7f, 1.f, 0.6f);
        bool     enabled    = true;
        uint32   lineCount  = 12;  ///< Nombre de lignes rayonnantes
    };

    class NkPerspectiveGuide {
    public:
        enum class Mode : uint8 { OnePoint, TwoPoint, ThreePoint, Isometric };
        Mode mode = Mode::TwoPoint;

        NkVanishingPoint points[3];  ///< Max 3 points de fuite

        bool  visible        = true;
        bool  snapEnabled    = true;
        float32 snapAngle    = 5.f;  ///< Angle de snap vers les lignes (°)
        bool  lockHorizon    = true; ///< Verrouille la ligne d'horizon

        NkVec2f horizon;  ///< Ligne d'horizon (coordonnées Y canvas)

        /**
         * @brief Retourne l'angle de snap le plus proche du trait actuel.
         */
        [[nodiscard]] float32 SnapStrokeAngle(NkVec2f from, NkVec2f to) const noexcept;

        /**
         * @brief Dessine les guides via NkRender2D.
         */
        void Draw(renderer::NkRender2D& r2d,
                   const NkRectF& canvasRect,
                   float32 zoom) const noexcept;
    };

    // =========================================================================
    // NkStabilizer — stabilisation du trait
    // =========================================================================
    /**
     * @class NkStabilizer
     * @brief Lisse le mouvement du stylet pour des traits propres.
     *
     * MODES :
     *   Chaikin      — lissage récursif de Chaikin (simple et rapide)
     *   LazyNezumi   — le stylet "traîne" derrière le curseur avec une laisse
     *   Average      — moyenne glissante des N dernières positions
     *   Kalman       — filtre de Kalman (optimal pour le bruit gaussien)
     */
    class NkStabilizer {
    public:
        enum class Mode : uint8 { None, Chaikin, LazyNezumi, Average, Kalman };

        Mode    mode          = Mode::LazyNezumi;
        float32 strength      = 0.5f;   ///< Force de stabilisation [0..1]
        uint32  windowSize    = 8;      ///< Pour Average
        float32 lazyRadius    = 50.f;   ///< Laisse LazyNezumi (pixels)

        /**
         * @brief Ajoute un point brut et retourne le point stabilisé.
         * @param rawPos    Position brute du stylet.
         * @param pressure  Pression du stylet [0..1].
         * @return Position stabilisée (peut être derrière rawPos pour LazyNezumi).
         */
        [[nodiscard]] NkVec2f Process(NkVec2f rawPos, float32 pressure = 1.f) noexcept;

        /**
         * @brief Finalise le trait (vide le buffer et émet les points restants).
         * @param out Points restants à émettre.
         */
        void Flush(NkVector<NkVec2f>& out) noexcept;

        void Reset() noexcept;

        [[nodiscard]] bool IsActive()    const noexcept { return mode != Mode::None; }
        [[nodiscard]] NkVec2f LastPos()  const noexcept { return mLastStabilized; }

    private:
        // Chaikin
        void ChaikinSmooth(NkVector<NkVec2f>& pts, uint32 iterations) noexcept;
        // LazyNezumi
        NkVec2f mLazyPos  = {};
        // Average
        NkVector<NkVec2f> mWindow;
        // Kalman
        NkVec2f mKalmanPos = {}, mKalmanVel = {};
        float32 mKalmanP   = 1.f;

        NkVec2f mLastRaw         = {};
        NkVec2f mLastStabilized  = {};
        bool    mFirstPoint      = true;

        NkVector<NkVec2f> mBuffer;  ///< Buffer de points bruts
    };

    // =========================================================================
    // NkAnimationFrame — frame d'animation image-par-image
    // =========================================================================
    struct NkAnimationFrame {
        uint32          index   = 0;    ///< Index de la frame (0-based)
        float32         duration = 1.f; ///< Durée de cette frame (1 = 1 frame standard)
        NkRasterCanvas* canvas  = nullptr;  ///< Canvas de cette frame (owned)
        bool            visible = true;

        NkAnimationFrame() noexcept = default;
        ~NkAnimationFrame() noexcept { delete canvas; }
        NkAnimationFrame(const NkAnimationFrame&) = delete;
        NkAnimationFrame& operator=(const NkAnimationFrame&) = delete;
    };

    // =========================================================================
    // NkAnimationAssist — animation image-par-image
    // =========================================================================
    class NkAnimationAssist {
    public:
        float32 fps          = 12.f;     ///< Images par seconde
        bool    loop         = true;
        uint32  onionSkinBefore = 2;     ///< Frames précédentes visibles
        uint32  onionSkinAfter  = 1;     ///< Frames suivantes visibles
        float32 onionOpacity    = 0.3f;  ///< Opacité des frames fantômes
        NkVec3f onionColorBefore = {1, 0.3f, 0.3f};  ///< Rouge = avant
        NkVec3f onionColorAfter  = {0.3f, 0.3f, 1};  ///< Bleu = après

        NkVector<NkAnimationFrame*> frames;  ///< Owned
        uint32                      currentFrame = 0;

        // ── Lecture ───────────────────────────────────────────────────────
        bool    playing       = false;
        float32 playbackTime  = 0.f;

        void Play()    noexcept { playing = true; }
        void Stop()    noexcept { playing = false; playbackTime = 0.f; }
        void Pause()   noexcept { playing = false; }
        void Update(float32 dt) noexcept;

        // ── Frames ────────────────────────────────────────────────────────
        NkAnimationFrame& AddFrame(uint32 width, uint32 height) noexcept;
        void DeleteFrame   (uint32 idx) noexcept;
        void DuplicateFrame(uint32 idx) noexcept;
        void MoveFrame     (uint32 from, uint32 to) noexcept;

        [[nodiscard]] NkAnimationFrame* GetCurrent() noexcept;
        [[nodiscard]] float32 GetTotalDuration() const noexcept;

        // ── Rendu avec onion skin ─────────────────────────────────────────
        void DrawWithOnionSkin(renderer::NkRender2D& r2d,
                                const NkRectF& dst) const noexcept;

        // ── Export ────────────────────────────────────────────────────────
        bool ExportGIF   (const char* path) const noexcept;
        bool ExportWebP  (const char* path) const noexcept;
        bool ExportMp4   (const char* path) const noexcept;   ///< Nécessite ffmpeg
        bool ExportFrames(const char* dir, const char* prefix = "frame") const noexcept;

        ~NkAnimationAssist() noexcept {
            for (auto* f : frames) delete f;
        }
    };

    // =========================================================================
    // NkHybridDocument — document hybride vectoriel + raster
    // =========================================================================
    class NkHybridDocument {
    public:
        NkString  name;
        NkString  filePath;
        uint32    width  = 2048;
        uint32    height = 2048;
        float32   dpi    = 150.f;   ///< Pour l'export print

        // ── Pile de couches mixtes ────────────────────────────────────────
        NkLayerStack layerStack;    ///< Gère tout type de couche

        // ── Outils d'illustration ─────────────────────────────────────────
        NkSymmetryTool      symmetry;
        NkPerspectiveGuide  perspective;
        NkStabilizer        stabilizer;
        NkAnimationAssist   animation;

        // ── Couleurs ──────────────────────────────────────────────────────
        NkVector<NkPalette> palettes;
        NkColor foreground = NkColor::Black();
        NkColor background = NkColor::White();

        // ── Historique ───────────────────────────────────────────────────
        NkUndoStack undoStack{200};

        // ── Métadonnées ───────────────────────────────────────────────────
        NkString author;
        NkString tags;           ///< Mots-clés séparés par des virgules
        NkString license;

        // ── Sérialisation ─────────────────────────────────────────────────
        [[nodiscard]] bool SaveToFile   (const char* path) const noexcept;  ///< .nkart
        [[nodiscard]] bool LoadFromFile (const char* path) noexcept;
        [[nodiscard]] bool ExportPNG    (const char* path, float32 scale = 1.f) const noexcept;
        [[nodiscard]] bool ExportPSD    (const char* path) const noexcept;  ///< Limité

        // ── Composition finale ────────────────────────────────────────────
        void Composite(renderer::NkRender2D& r2d,
                        NkICommandBuffer* cmd,
                        const NkRectF& dst) noexcept;
    };

    // =========================================================================
    // NkExportPreset — preset d'export multi-format
    // =========================================================================
    struct NkExportPreset {
        NkString name;

        enum class Target : uint8 {
            Web,        ///< PNG 72dpi, sRGB
            Print,      ///< TIFF 300dpi, CMYK
            Social,     ///< JPEG 80%, 1080x1080
            Animation,  ///< GIF ou WebP animé
            Vector,     ///< SVG vectoriel pur
            HighRes,    ///< PNG 300dpi, LinearRGB
            Custom,     ///< Configuration manuelle
        };

        Target target = Target::Web;

        // Paramètres
        uint32  targetWidth  = 0;    ///< 0 = taille du document
        uint32  targetHeight = 0;
        float32 targetDPI    = 72.f;

        enum class OutputFormat : uint8 { PNG, JPEG, TIFF, SVG, PDF, WebP, GIF, EXR };
        OutputFormat format   = OutputFormat::PNG;
        uint32 jpegQuality    = 85;

        bool   embedColorProfile = true;
        bool   flattenAllLayers  = true;
        bool   exportAlpha       = true;
        bool   includeMetadata   = true;

        NkString outputPath;
        NkString filenameTemplate = "{name}_{date}";

        // Factory presets
        [[nodiscard]] static NkExportPreset Web()    noexcept;
        [[nodiscard]] static NkExportPreset Print()  noexcept;
        [[nodiscard]] static NkExportPreset Social() noexcept;
    };

} // namespace nkentseu
