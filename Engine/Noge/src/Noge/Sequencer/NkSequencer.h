#pragma once
// =============================================================================
// Nkentseu/Sequencer/NkSequencer.h
// =============================================================================
// Séquenceur d'animation multi-piste — style Unreal Sequencer / DaVinci Resolve.
//
// CONCEPTS :
//   NkSequence     — conteneur racine (durée, FPS, entités trackées)
//   NkTrack        — piste pour une entité et une propriété
//   NkClipOnTrack  — instance d'un NkAnimationClip sur une piste (avec offset)
//   NkKeyframeSet  — ensemble de keyframes sur un canal scalaire
//   NkPlaybackCtrl — contrôle de lecture (play/pause/scrub/loop/speed)
//   NkNLATrack     — piste Non-Linear Animation (blend de clips)
//   NkCameraShot   — plan caméra (cut/blend entre caméras)
//   NkMarker       — marqueur de timeline (scène, son, event)
//   NkRenderOutput — configuration du rendu de séquence
//
// USAGE :
//   NkSequence seq;
//   seq.name = "OpeningCinematic";
//   seq.fps = 24.f;
//   seq.duration = 120.f;  // 5 secondes
//
//   // Piste d'animation sur un personnage
//   auto& track = seq.AddTrack(heroEntity, NkTrackType::Animation);
//   track.AddClip(runClipHandle, 0.f, 60.f);   // run pendant 2.5s
//   track.AddClip(idleClipHandle, 60.f, 60.f); // idle pendant 2.5s
//
//   // Piste de caméra
//   auto& camTrack = seq.AddCameraTrack();
//   camTrack.AddShot(cam1Entity, 0.f, 72.f, NkCutType::Cut);
//   camTrack.AddShot(cam2Entity, 72.f, 48.f, NkCutType::Blend);
//
//   // Lecture
//   NkPlaybackCtrl ctrl;
//   ctrl.Play();
//   // Chaque frame : seq.Evaluate(ctrl.time, world);
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKECS/World/NkWorld.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "Nkentseu/ECS/Components/Animation/NkAnimation.h"
#include "Nkentseu/ECS/Components/Rendering/NkRenderComponents.h"

namespace nkentseu {
    using namespace math;
    using namespace ecs;

    // =========================================================================
    // NkTrackType — type de piste
    // =========================================================================
    enum class NkTrackType : uint8 {
        Animation,       ///< Clips d'animation squelettique
        Transform,       ///< TRS animé (position/rotation/scale)
        Property,        ///< Propriété ECS quelconque (float/vec3/color)
        Camera,          ///< Cuts et blends de caméra
        Audio,           ///< Clips audio
        Event,           ///< Événements scripts (callbacks)
        FacialAnim,      ///< Piste facial FACS (AU weights)
        BlendShape,      ///< Weights de blend shapes
        Light,           ///< Intensité/couleur d'une lumière
        PostProcess,     ///< Paramètres de post-processing
        Particle,        ///< Système de particules
        Visibility,      ///< Show/hide d'entités
        NLA,             ///< Non-Linear Animation (blend de clips)
    };

    // =========================================================================
    // NkInterpolation — type d'interpolation entre keyframes
    // =========================================================================
    enum class NkInterpolation : uint8 {
        Constant,   ///< Stepped (valeur constante jusqu'à la prochaine key)
        Linear,     ///< Interpolation linéaire
        Bezier,     ///< Courbe de Bézier avec handles
        Auto,       ///< Lissage automatique (Catmull-Rom)
        EaseIn,
        EaseOut,
        EaseInOut,
    };

    // =========================================================================
    // NkKeyframe — keyframe générique sur un canal scalaire
    // =========================================================================
    struct NkKeyframe {
        float32        time          = 0.f;
        float32        value         = 0.f;
        float32        inTangent     = 0.f;
        float32        outTangent    = 0.f;
        NkInterpolation interpolation = NkInterpolation::Bezier;
        bool           selected      = false;

        [[nodiscard]] float32 Evaluate(const NkKeyframe& next, float32 t) const noexcept;
    };

    // =========================================================================
    // NkAnimChannel — canal de valeur (courbe d'animation sur une propriété)
    // =========================================================================
    struct NkAnimChannel {
        static constexpr uint32 kMaxName = 64u;
        char    propertyName[kMaxName] = {};  ///< "localPosition.x", "opacity", etc.
        uint32  componentOffset = 0;          ///< Offset dans le composant ECS cible

        NkVector<NkKeyframe> keyframes;

        // Flags
        bool locked   = false;
        bool muted    = false;
        bool selected = false;

        void AddKey(float32 time, float32 value,
                    NkInterpolation interp = NkInterpolation::Bezier) noexcept;
        void RemoveKey(uint32 idx) noexcept;
        [[nodiscard]] float32 Evaluate(float32 time) const noexcept;
        void SortByTime() noexcept;
        void AutoSmooth() noexcept;   ///< Recalcule les tangentes automatiquement
    };

    // =========================================================================
    // NkClipOnTrack — clip positionné sur une piste
    // =========================================================================
    struct NkClipOnTrack {
        nk_uint64 clipHandle  = 0;     ///< Handle du NkAnimationClip (GPU asset)
        float32   startTime   = 0.f;   ///< Début sur la timeline (frames)
        float32   duration    = 0.f;   ///< Durée sur la timeline
        float32   clipOffset  = 0.f;   ///< Offset dans le clip source
        float32   speed       = 1.f;   ///< Vitesse de lecture
        float32   blendIn     = 0.f;   ///< Durée du fondu entrant
        float32   blendOut    = 0.f;   ///< Durée du fondu sortant
        float32   weight      = 1.f;   ///< Influence [0..1]
        bool      loop        = false;
        bool      reverse     = false;

        [[nodiscard]] float32 EndTime()     const noexcept { return startTime + duration; }
        [[nodiscard]] bool IsActive(float32 t) const noexcept {
            return t >= startTime && t <= EndTime();
        }
        [[nodiscard]] float32 GetLocalTime(float32 t) const noexcept {
            return (t - startTime) * speed + clipOffset;
        }
        [[nodiscard]] float32 GetWeight(float32 t) const noexcept;  ///< Avec fade
    };

    // =========================================================================
    // NkTrack — piste pour une entité + type de propriété
    // =========================================================================
    struct NkTrack {
        NkTrackType  type       = NkTrackType::Animation;
        NkEntityId   entity     = NkEntityId::Invalid();
        NkString     name;
        bool         muted      = false;
        bool         locked     = false;
        bool         selected   = false;
        float32      weight     = 1.f;        ///< Influence globale de la piste

        // Clips posés sur la timeline
        NkVector<NkClipOnTrack> clips;

        // Canaux de propriétés (pour Type=Transform/Property/FacialAnim...)
        NkVector<NkAnimChannel> channels;

        // ── API ───────────────────────────────────────────────────────────
        NkClipOnTrack& AddClip(nk_uint64 clipHandle,
                                float32 startTime, float32 duration) noexcept {
            NkClipOnTrack clip;
            clip.clipHandle = clipHandle;
            clip.startTime  = startTime;
            clip.duration   = duration;
            clips.PushBack(static_cast<NkClipOnTrack&&>(clip));
            return clips.Back();
        }

        NkAnimChannel& AddChannel(const char* propName) noexcept {
            NkAnimChannel ch;
            NkStrNCpy(ch.propertyName, propName, 63);
            channels.PushBack(static_cast<NkAnimChannel&&>(ch));
            return channels.Back();
        }

        /**
         * @brief Évalue la piste à un instant donné et applique à world.
         */
        void Evaluate(float32 time, NkWorld& world) const noexcept;
    };

    // =========================================================================
    // NkCameraShot — plan caméra dans la séquence
    // =========================================================================
    enum class NkCutType : uint8 { Cut, Blend, Wipe };

    struct NkCameraShot {
        NkEntityId  cameraEntity = NkEntityId::Invalid();
        float32     startTime    = 0.f;
        float32     duration     = 0.f;
        NkCutType   cutType      = NkCutType::Cut;
        float32     blendDuration = 0.f;  ///< Pour NkCutType::Blend

        [[nodiscard]] float32 EndTime() const noexcept { return startTime + duration; }
    };

    struct NkCameraTrack {
        NkVector<NkCameraShot> shots;
        bool muted  = false;
        bool locked = false;

        NkCameraShot& AddShot(NkEntityId cam, float32 start, float32 dur,
                               NkCutType cut = NkCutType::Cut) noexcept {
            NkCameraShot s;
            s.cameraEntity = cam;
            s.startTime    = start;
            s.duration     = dur;
            s.cutType      = cut;
            shots.PushBack(static_cast<NkCameraShot&&>(s));
            return shots.Back();
        }

        [[nodiscard]] const NkCameraShot* GetActiveShot(float32 t) const noexcept;
    };

    // =========================================================================
    // NkMarker — marqueur de timeline
    // =========================================================================
    struct NkMarker {
        static constexpr uint32 kMaxLabel = 128u;
        char    label[kMaxLabel] = {};
        float32 time             = 0.f;
        NkVec3f color            = {1.f, 0.8f, 0.f};  ///< Couleur dans l'UI

        enum class Type : uint8 { Scene, Audio, Event, Note };
        Type type = Type::Note;

        // Pour Type::Event — nom de fonction à appeler
        char    eventFunction[128] = {};
    };

    // =========================================================================
    // NkNLAClip — clip dans une piste NLA
    // =========================================================================
    struct NkNLAClip {
        nk_uint64 clipHandle  = 0;
        float32   startTime   = 0.f;
        float32   duration    = 0.f;
        float32   clipOffset  = 0.f;
        float32   speed       = 1.f;
        float32   influence   = 1.f;   ///< [0..1] blend d'influence
        bool      repeat      = false;
        uint32    repeatCount = 1;
        bool      reverse     = false;
        bool      muted       = false;

        enum class BlendType : uint8 {
            Replace,     ///< Remplace l'action de dessous
            Add,         ///< Ajoute (additif)
            Multiply,    ///< Multiplie
        };
        BlendType blendType = BlendType::Replace;
    };

    struct NkNLATrack {
        NkEntityId entity = NkEntityId::Invalid();
        NkString   name;
        NkVector<NkNLAClip> clips;
        bool muted  = false;
        bool locked = false;
        float32 weight = 1.f;

        void Evaluate(float32 time, NkWorld& world) const noexcept;
    };

    // =========================================================================
    // NkRenderOutput — configuration du rendu de séquence
    // =========================================================================
    struct NkRenderOutput {
        uint32  width       = 1920;
        uint32  height      = 1080;
        float32 fps         = 24.f;
        float32 startTime   = 0.f;
        float32 endTime     = 0.f;     ///< 0 = jusqu'à la fin de la séquence

        enum class Format : uint8 { PNG, EXR, JPEG, WebP };
        Format  format      = Format::PNG;
        uint32  jpegQuality = 90;      ///< Pour Format::JPEG
        bool    exrHDR      = true;    ///< Pour Format::EXR (float32)

        NkString outputDirectory;
        NkString filePrefix    = "frame_";  ///< ex: "frame_0001.png"

        bool renderMotionBlur  = false;
        uint32 motionBlurSamples = 4;

        bool renderDOF         = false;
        bool renderSSAO        = true;
        bool renderShadows     = true;
    };

    // =========================================================================
    // NkPlaybackCtrl — contrôle de lecture
    // =========================================================================
    struct NkPlaybackCtrl {
        float32 time     = 0.f;   ///< Temps courant (frames ou secondes selon config)
        float32 speed    = 1.f;   ///< Vitesse (négatif = rebours)
        bool    playing  = false;
        bool    loop     = true;
        float32 loopStart = 0.f;
        float32 loopEnd   = 0.f;  ///< 0 = fin de la séquence

        void Play()    noexcept { playing = true; }
        void Pause()   noexcept { playing = false; }
        void Stop()    noexcept { playing = false; time = loopStart; }
        void Rewind()  noexcept { time = loopStart; }
        void JumpTo(float32 t) noexcept { time = t; }

        /**
         * @brief Avance la lecture et gère les boucles.
         * @return true si la lecture a atteint la fin.
         */
        bool Update(float32 dt, float32 sequenceDuration) noexcept;
    };

    // =========================================================================
    // NkSequence — conteneur racine de la séquence
    // =========================================================================
    class NkSequence {
    public:
        NkString name;
        float32  fps       = 24.f;
        float32  duration  = 0.f;    ///< Durée totale en secondes

        // ── Pistes ────────────────────────────────────────────────────────
        NkVector<NkTrack>     tracks;
        NkVector<NkNLATrack>  nlaTracks;
        NkCameraTrack         cameraTrack;
        NkVector<NkMarker>    markers;
        NkRenderOutput        renderOutput;

        // ── API pistes ────────────────────────────────────────────────────
        NkTrack& AddTrack(NkEntityId entity, NkTrackType type,
                           const char* name = nullptr) noexcept {
            NkTrack t;
            t.entity = entity;
            t.type   = type;
            t.name   = name ? name : "Track";
            tracks.PushBack(static_cast<NkTrack&&>(t));
            return tracks.Back();
        }

        NkNLATrack& AddNLATrack(NkEntityId entity,
                                  const char* name = nullptr) noexcept {
            NkNLATrack t;
            t.entity = entity;
            t.name   = name ? name : "NLA";
            nlaTracks.PushBack(static_cast<NkNLATrack&&>(t));
            return nlaTracks.Back();
        }

        NkMarker& AddMarker(float32 time, const char* label,
                             NkMarker::Type type = NkMarker::Type::Note) noexcept {
            NkMarker m;
            m.time = time;
            m.type = type;
            NkStrNCpy(m.label, label, 127);
            markers.PushBack(static_cast<NkMarker&&>(m));
            return markers.Back();
        }

        // ── Évaluation ────────────────────────────────────────────────────
        /**
         * @brief Évalue toutes les pistes à l'instant donné.
         * Applique les transformations, animations, lumières... dans world.
         */
        void Evaluate(float32 time, NkWorld& world) const noexcept;

        /**
         * @brief Retourne le plan de caméra actif à l'instant donné.
         */
        [[nodiscard]] NkEntityId GetActiveCameraAt(float32 time) const noexcept;

        /**
         * @brief Recalcule la durée à partir des pistes.
         */
        void RecalcDuration() noexcept;

        // ── Sérialisation ────────────────────────────────────────────────
        [[nodiscard]] bool SaveToFile(const char* path) const noexcept;
        [[nodiscard]] bool LoadFromFile(const char* path) noexcept;
    };

} // namespace nkentseu
